// obj_to_xprim: turns a mesh (an .obj: positions, UVs, normals, any polygons) into a header of xprim_geom, so a primitive library user gets it without a file.
// The mesh is welded, simplified to a triangle budget (meshoptimizer, keeping UV and normal seams), given MikkTSpace tangents, centred and scaled so its
// largest side is 1 (like the other primitives), and stored quantized (16 bits a value) in a header only its generator includes.
//
//   obj_to_xprim <in.obj> <out.h> <namespace name> <target triangles> [degrees about Y, counter clockwise seen from above] [degrees about X, the top towards +Z]
//   e.g. obj_to_xprim untitled.obj ../../source/xprim_geom_material_ball_data.h material_ball 20000 90 20
//
// Build (from a VS developer prompt, in this folder):
//   cl /nologo /std:c++20 /O2 /EHsc obj_to_xprim.cpp ../../../meshoptimizer/src/*.cpp ../../../MikkTSpace/mikktspace.c /I../../../meshoptimizer/src /I../../../MikkTSpace
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>
#include "meshoptimizer.h"
extern "C" {
#include "mikktspace.h"
}

struct vert { float p[3], n[3], uv[2], t[4]; };

static std::vector<vert>          g_V;
static std::vector<std::uint32_t> g_I;

static int  MkFaces   (const SMikkTSpaceContext*)                                      { return int(g_I.size() / 3); }
static int  MkVerts   (const SMikkTSpaceContext*, int)                                 { return 3; }
static void MkPos     (const SMikkTSpaceContext*, float o[], int f, int v)             { const auto& V = g_V[g_I[f * 3 + v]]; o[0] = V.p[0]; o[1] = V.p[1]; o[2] = V.p[2]; }
static void MkNormal  (const SMikkTSpaceContext*, float o[], int f, int v)             { const auto& V = g_V[g_I[f * 3 + v]]; o[0] = V.n[0]; o[1] = V.n[1]; o[2] = V.n[2]; }
static void MkUV      (const SMikkTSpaceContext*, float o[], int f, int v)             { const auto& V = g_V[g_I[f * 3 + v]]; o[0] = V.uv[0]; o[1] = V.uv[1]; }
static void MkTangent (const SMikkTSpaceContext*, const float t[], float s, int f, int v)
{
    auto& V = g_V[g_I[f * 3 + v]];                  // a vertex shared by faces takes the last one's: MikkTSpace gives one tangent per welded vertex here
    V.t[0] = t[0]; V.t[1] = t[1]; V.t[2] = t[2]; V.t[3] = s;
}

static std::int16_t  SNorm(float x) { return std::int16_t(std::lround(std::clamp(x, -1.0f, 1.0f) * 32767.0f)); }
static std::uint16_t UNorm(float x) { return std::uint16_t(std::lround(std::clamp(x, 0.0f, 1.0f) * 65535.0f)); }

int main(int argc, char** argv)
{
    if (argc < 5) { std::printf("obj_to_xprim <in.obj> <out.h> <namespace name> <target triangles> [degrees about Y] [degrees about X]\n"); return 2; }
    const std::string Name   = argv[3];
    const std::size_t Target = std::size_t(std::atoi(argv[4]));
    const float       Angle  = argc > 5 ? float(std::atof(argv[5])) * 3.14159265f / 180.0f : 0.0f;
    const float       Cos    = std::cos(Angle), Sin = std::sin(Angle);
    const auto        RotY   = [&](float* v) { const float x = v[0], z = v[2]; v[0] = x * Cos + z * Sin; v[2] = z * Cos - x * Sin; };
    const float       Tilt   = argc > 6 ? float(std::atof(argv[6])) * 3.14159265f / 180.0f : 0.0f;
    const float       CosX   = std::cos(Tilt), SinX = std::sin(Tilt);
    const auto        RotX   = [&](float* v) { const float y = v[1], z = v[2]; v[1] = y * CosX - z * SinX; v[2] = y * SinX + z * CosX; };

    // Read: v, vt, vn, f (any polygon: a fan); one vertex per distinct v/vt/vn
    std::vector<std::array<float, 3>> P, N;
    std::vector<std::array<float, 2>> T;
    std::map<std::tuple<int, int, int>, std::uint32_t> Weld;
    std::ifstream In(argv[1]);
    if (!In) { std::printf("can not read %s\n", argv[1]); return 1; }
    std::string Line;
    while (std::getline(In, Line))
    {
        std::istringstream S(Line);
        std::string K; S >> K;
        if      (K == "v")  { std::array<float, 3> a{}; S >> a[0] >> a[1] >> a[2]; P.push_back(a); }
        else if (K == "vn") { std::array<float, 3> a{}; S >> a[0] >> a[1] >> a[2]; N.push_back(a); }
        else if (K == "vt") { std::array<float, 2> a{}; S >> a[0] >> a[1]; T.push_back(a); }
        else if (K == "f")
        {
            std::vector<std::uint32_t> Poly;
            std::string W;
            while (S >> W)
            {
                int vi = 0, ti = 0, ni = 0;
                std::sscanf(W.c_str(), "%d/%d/%d", &vi, &ti, &ni);
                if (W.find("//") != std::string::npos) std::sscanf(W.c_str(), "%d//%d", &vi, &ni);
                auto Key = std::make_tuple(vi, ti, ni);
                auto It  = Weld.find(Key);
                if (It == Weld.end())
                {
                    vert V{};
                    const auto& p = P[vi - 1];                         V.p[0] = p[0]; V.p[1] = p[1]; V.p[2] = p[2];
                    if (ni) { const auto& n = N[ni - 1];               V.n[0] = n[0]; V.n[1] = n[1]; V.n[2] = n[2]; }
                    RotY(V.p); RotY(V.n); RotX(V.p); RotX(V.n);
                    if (ti) { const auto& t = T[ti - 1];               V.uv[0] = t[0]; V.uv[1] = 1.0f - t[1]; }       // the OBJ's v goes up, ours down
                    It = Weld.emplace(Key, std::uint32_t(g_V.size())).first;
                    g_V.push_back(V);
                }
                Poly.push_back(It->second);
            }
            for (std::size_t i = 1; i + 1 < Poly.size(); ++i) { g_I.push_back(Poly[0]); g_I.push_back(Poly[i]); g_I.push_back(Poly[i + 1]); }
        }
    }
    std::printf("read: %zu vertices, %zu triangles\n", g_V.size(), g_I.size() / 3);

    // Simplify to the budget, the UVs and normals weighed so their seams stay
    if (g_I.size() / 3 > Target)
    {
        std::vector<float> Attr(g_V.size() * 5);
        for (std::size_t i = 0; i < g_V.size(); ++i) { Attr[i * 5 + 0] = g_V[i].n[0]; Attr[i * 5 + 1] = g_V[i].n[1]; Attr[i * 5 + 2] = g_V[i].n[2]; Attr[i * 5 + 3] = g_V[i].uv[0]; Attr[i * 5 + 4] = g_V[i].uv[1]; }
        const float Weights[5] = { 0.5f, 0.5f, 0.5f, 1.0f, 1.0f };
        std::vector<std::uint32_t> Out(g_I.size());
        float Err = 0;
        Out.resize(meshopt_simplifyWithAttributes(Out.data(), g_I.data(), g_I.size(), &g_V[0].p[0], g_V.size(), sizeof(vert), Attr.data(), sizeof(float) * 5, Weights, 5, nullptr, Target * 3, 0.05f, 0, &Err));
        g_I = std::move(Out);
        std::printf("simplified: %zu triangles (error %.5f)\n", g_I.size() / 3, Err);
    }

    // Only the vertices used, in a cache friendly order
    {
        std::vector<std::uint32_t> Remap(g_V.size());
        const auto Count = meshopt_optimizeVertexFetchRemap(Remap.data(), g_I.data(), g_I.size(), g_V.size());
        std::vector<vert> V(Count);
        meshopt_remapVertexBuffer(V.data(), g_V.data(), g_V.size(), sizeof(vert), Remap.data());
        meshopt_remapIndexBuffer(g_I.data(), g_I.data(), g_I.size(), Remap.data());
        g_V = std::move(V);
        meshopt_optimizeVertexCache(g_I.data(), g_I.data(), g_I.size(), g_V.size());
    }

    // Tangents
    {
        SMikkTSpaceInterface I{};
        I.m_getNumFaces = MkFaces; I.m_getNumVerticesOfFace = MkVerts; I.m_getPosition = MkPos; I.m_getNormal = MkNormal; I.m_getTexCoord = MkUV; I.m_setTSpaceBasic = MkTangent;
        SMikkTSpaceContext C{ &I, nullptr };
        genTangSpaceDefault(&C);
    }

    // Centred, the largest side 1
    float Lo[3] = { 1e30f, 1e30f, 1e30f }, Hi[3] = { -1e30f, -1e30f, -1e30f };
    for (const auto& V : g_V) for (int k = 0; k < 3; ++k) { Lo[k] = std::min(Lo[k], V.p[k]); Hi[k] = std::max(Hi[k], V.p[k]); }
    const float Side = std::max({ Hi[0] - Lo[0], Hi[1] - Lo[1], Hi[2] - Lo[2] });
    const float Mid[3] = { (Lo[0] + Hi[0]) * 0.5f, (Lo[1] + Hi[1]) * 0.5f, (Lo[2] + Hi[2]) * 0.5f };
    const float Ext[3] = { (Hi[0] - Lo[0]) / Side, (Hi[1] - Lo[1]) / Side, (Hi[2] - Lo[2]) / Side };       // the box after scaling, -Ext/2..Ext/2
    float UVLo[2] = { 1e30f, 1e30f }, UVHi[2] = { -1e30f, -1e30f };
    for (const auto& V : g_V) for (int k = 0; k < 2; ++k) { UVLo[k] = std::min(UVLo[k], V.uv[k]); UVHi[k] = std::max(UVHi[k], V.uv[k]); }

    // Write
    std::ofstream O(argv[2], std::ios::binary);
    O << "// Generated by dependencies/xprim_geom/tools/obj_to_xprim from " << std::string(argv[1]).substr(std::string(argv[1]).find_last_of("/\\\\") + 1) << ": do not edit (run the tool again).\n";
    O << "// " << g_V.size() << " vertices, " << g_I.size() / 3 << " triangles. Per vertex: position (unorm16 in the box), normal (snorm16 x3),\n";
    O << "// tangent (snorm16 x3, then 0/1 for the sign -1/+1), uv (unorm16 in the uv box x2): 12 values.\n";
    O << "#pragma once\n#include <cstdint>\n\nnamespace xprim_geom::" << Name << "::data\n{\n";
    char B[256];
    std::snprintf(B, sizeof(B), "    inline constexpr float box_v[3]    = { %#.9gf, %#.9gf, %#.9gf };       // the mesh's size (largest side 1), centred on 0\n", Ext[0], Ext[1], Ext[2]); O << B;
    std::snprintf(B, sizeof(B), "    inline constexpr float uv_lo_v[2]  = { %#.9gf, %#.9gf };\n", UVLo[0], UVLo[1]); O << B;
    std::snprintf(B, sizeof(B), "    inline constexpr float uv_hi_v[2]  = { %#.9gf, %#.9gf };\n", UVHi[0], UVHi[1]); O << B;
    O << "    inline constexpr std::uint16_t vertices_v[] =\n    {";
    for (std::size_t i = 0; i < g_V.size(); ++i)
    {
        const auto& V = g_V[i];
        std::uint16_t Q[10];
        for (int k = 0; k < 3; ++k) Q[k] = UNorm(((V.p[k] - Mid[k]) / Side + Ext[k] * 0.5f) / std::max(Ext[k], 1e-6f));
        float nl = std::sqrt(V.n[0] * V.n[0] + V.n[1] * V.n[1] + V.n[2] * V.n[2]); if (nl <= 0) nl = 1;
        for (int k = 0; k < 3; ++k) Q[3 + k] = std::uint16_t(SNorm(V.n[k] / nl));
        for (int k = 0; k < 3; ++k) Q[6 + k] = std::uint16_t(SNorm(V.t[k]));
        Q[9] = V.t[3] < 0 ? 0 : 1;
        // the UV goes as two more values
        std::uint16_t U = UNorm((V.uv[0] - UVLo[0]) / std::max(UVHi[0] - UVLo[0], 1e-6f));
        std::uint16_t W = UNorm((V.uv[1] - UVLo[1]) / std::max(UVHi[1] - UVLo[1], 1e-6f));
        if (i % 2 == 0) O << "\n       ";
        for (auto q : Q) O << ' ' << q << ',';
        O << ' ' << U << ',' << ' ' << W << ',';
    }
    O << "\n    };\n    inline constexpr std::uint32_t indices_v[] =\n    {";
    for (std::size_t i = 0; i < g_I.size(); ++i) { if (i % 24 == 0) O << "\n       "; O << ' ' << g_I[i] << ','; }
    O << "\n    };\n}\n";
    std::printf("wrote %s: %zu vertices, %zu triangles\n", argv[2], g_V.size(), g_I.size() / 3);
    return 0;
}
