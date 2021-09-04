namespace xprim_geom::uvsphere
{
    //-----------------------------------------------------------------------------
    inline
    mesh Generate(int Rings, float RadicalSegments, float Height, float Radius ) noexcept
    {
        mesh Mesh;
    
	    const float Scale	= Height * 0.5f;
	    int			thisrow = 0;
	    int			prevrow = 0;
		int			Point   = 0;

	    for( int j = 0, end_j = Rings + 1; j <= end_j; ++j ) 
		{
		    const float v = j / float(Rings + 1.0f);
		    const float w = std::sinf( pi_v * v);
		    const float y = Scale * std::cosf( pi_v * v );

		    for( int i = 0; i <= RadicalSegments; ++i )
			{
			    const float u = i/float{ RadicalSegments };
			    const float	x = std::sinf(u * tau_v);
			    const float	z = std::cosf(u * tau_v);

				Mesh.m_Vertices.push_back
				( vertex
				{ .m_Position = {x * Radius * w, y, z * Radius * w}
				, .m_Normal   = { x * Radius * w * Scale, y / Scale, z * Radius * w * Scale }
				, .m_Tangent  = { z, 0.0f, -x, 1.0f }
				, .m_Texcoord = { u, v }
				});
			    Point++;

			    if( i > 0 && j > 0 ) 
				{
				    Mesh.m_Indices.push_back(prevrow + i - 1);
				    Mesh.m_Indices.push_back(thisrow + i - 1);
					Mesh.m_Indices.push_back(prevrow + i);

				    Mesh.m_Indices.push_back(prevrow + i);
				    Mesh.m_Indices.push_back(thisrow + i - 1);
					Mesh.m_Indices.push_back(thisrow + i);
				}
		    }

		    prevrow = thisrow;
		    thisrow = Point;
	    }

        return Mesh;
    }


/*
	//-----------------------------------------------------------------------------
	inline
    mesh Generate( const int nRings, const int nSectors, const float Radius = 1.0f ) noexcept
	{
		mesh Mesh;

        const auto R = 1.0f / static_cast<float>(nRings   - 1);
        const auto S = 1.0f / static_cast<float>(nSectors - 1);

		Mesh.m_Vertices = std::vector<vertex>( nRings * nSectors );
		Mesh.m_Indices  = std::vector<std::uint32_t>( nRings * nSectors * 6 );

        //
        // Copy Vertices
        //
        for (int r = 0, iVert = 0; r < nRings; ++r)
        for (int s = 0; s < nSectors; ++s)
        {
            const float r2  = r * R;
            const float s2  = s * S;
            const float Sr  = std::sinf(std::numbers::pi_v<float> * r2);
            const float y   = std::sinf(-0.5f * std::numbers::pi_v<float> + std::numbers::pi_v<float> * r2);
            const float x   = std::cosf(2*std::numbers::pi_v<float> * s2) * Sr;
            const float z   = std::sinf(2*std::numbers::pi_v<float> * s2) * Sr;
            auto&       V   = Mesh.m_Vertices[iVert++];

            V.m_Texcoord = float2{ s2, 1 - r2 };
            V.m_Position = float3{ x * Radius, y * Radius, z * Radius };
            V.m_Normal   = float3{ x, y, z };
        }

        //
        // Copy Indices
        // 
        for( int r = 0, iIndex = -1; r < nRings   - 1; ++r )
        for( int s = 0; s < nSectors - 1; ++s )
        {
            const int curRow  = r       * nSectors;
            const int nextRow = (r + 1) * nSectors;

            Mesh.m_Indices[++iIndex] = curRow  + s;
            Mesh.m_Indices[++iIndex] = nextRow + s;
            Mesh.m_Indices[++iIndex] = nextRow + (s + 1);
            Mesh.m_Indices[++iIndex] = curRow  + s;
            Mesh.m_Indices[++iIndex] = nextRow + (s + 1);
            Mesh.m_Indices[++iIndex] = curRow  + (s + 1);
        }

		return Mesh;
	}
    */
}





