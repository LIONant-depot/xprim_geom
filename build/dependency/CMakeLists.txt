include(${CMAKE_BINARY_DIR}/_deps/xcmake_tools/Common.cmake)

DefineInterfaceComponent(xprim_geom "dependencies/xtools"
  "source/xprim_geom.h"
  "source/xprim_geom_capsule.h"
  "source/xprim_geom_cube.h"
  "source/xprim_geom_cylinder.h"
  "source/xprim_geom_grid.h"
  "source/xprim_geom_hemisphere.h"
  "source/xprim_geom_prism.h"
  "source/xprim_geom_quad.h"
  "source/xprim_geom_rounded_cube.h"
  "source/xprim_geom_uvsphere.h"
  "README.md"
)
