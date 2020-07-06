# -*- cmake -*-
include(Prebuilt)

if (NOT STANDALONE)
  use_prebuilt_binary(slvoice)
else (NOT STANDALONE)
  # Download there even when using standalone.
  set(STANDALONE OFF)
  use_prebuilt_binary(vivox)
  if(LINUX AND ${ARCH} STREQUAL "x86_64")
    use_prebuilt_binary(32bitcompatibilitylibs)
  endif(LINUX AND ${ARCH} STREQUAL "x86_64")
  set(STANDALONE ON)
endif(NOT STANDALONE)

if(LINUX)
  include(FindPkgConfig)
  pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
endif(LINUX)

use_prebuilt_binary(fonts)
