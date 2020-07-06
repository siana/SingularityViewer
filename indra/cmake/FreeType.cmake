# -*- cmake -*-
include(Prebuilt)

if (LINUX)
  include(FindPkgConfig)

  pkg_check_modules(FREETYPE REQUIRED freetype2)
else (LINUX)
  use_prebuilt_binary(freetype)
  set(FREETYPE_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/freetype2)
  set(FREETYPE_LIBRARIES freetype)
endif (LINUX)

link_directories(${FREETYPE_LIBRARY_DIRS})
