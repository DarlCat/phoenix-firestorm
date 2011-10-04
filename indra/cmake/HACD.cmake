# -*- cmake -*-
include(Prebuilt)

set(HACD_FIND_QUIETLY ON)
set(HACD_FIND_REQUIRED ON)

if (STANDALONE)
  include(FindHACD)
else (STANDALONE)
#  use_prebuilt_binary(hacd)
  use_prebuilt_binary(nd_hacdConvexDecomposition)
  if (WINDOWS)
    set(HACD_LIBRARY hacd)
    set(LLCONVEXDECOMP_LIBRARY nd_hacdConvexDecomposition )
    set(HACD_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/hacd)
  elseif(DARWIN)
    set(HACD_LIBRARY hacd)
    set(LLCONVEXDECOMP_LIBRARY nd_hacdConvexDecomposition )
    set(HACD_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/hacd)
  else()
    set(HACD_LIBRARY hacd)
    set(LLCONVEXDECOMP_LIBRARY nd_hacdConvexDecomposition )
    set(HACD_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/hacd)
  endif()
endif (STANDALONE)
