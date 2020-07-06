# -*- cmake -*-

include(Variables)
if (USE_FMODSTUDIO)
  use_prebuilt_binary(fmodstudio)
  if(WINDOWS)
    set(lib_suffix .dll)
  elseif(DARWIN)
    set(lib_suffix .dylib)
  else(WINDOWS)
    set(lib_suffix .so)
  endif(WINDOWS)
  if(WINDOWS)
    set(FMOD_LIBRARY_RELEASE ${LIBS_PREBUILT_DIR}/lib/release/fmod${lib_suffix})
    set(FMOD_LIBRARY_DEBUG ${LIBS_PREBUILT_DIR}/lib/debug/fmodL${lib_suffix})
  else(WINDOWS)
    set(FMOD_LIBRARY_RELEASE ${LIBS_PREBUILT_DIR}/lib/release/libfmod${lib_suffix})
    set(FMOD_LIBRARY_DEBUG ${LIBS_PREBUILT_DIR}/lib/debug/libfmodL${lib_suffix})
  endif(WINDOWS)
  set(FMOD_LINK_LIBRARY_RELEASE ${FMOD_LIBRARY_RELEASE})
  set(FMOD_LINK_LIBRARY_DEBUG ${FMOD_LIBRARY_DEBUG})

  if(WINDOWS)
    string(REPLACE ".dll" "_vc.lib" FMOD_LINK_LIBRARY_RELEASE ${FMOD_LIBRARY_RELEASE})
    string(REPLACE ".dll" "_vc.lib" FMOD_LINK_LIBRARY_DEBUG ${FMOD_LIBRARY_DEBUG})
  endif(WINDOWS)

  set (FMOD_LIBRARY
    debug ${FMOD_LINK_LIBRARY_DEBUG}
    optimized ${FMOD_LINK_LIBRARY_RELEASE}
    )

  set(FMODSTUDIO_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include/fmodstudio)
endif(USE_FMODSTUDIO)

if(FMOD_LIBRARY_RELEASE AND FMODSTUDIO_INCLUDE_DIR)
  set(FMOD ON)
  if (NOT FMOD_LIBRARY_DEBUG) #Use release library in debug configuration if debug library is absent.
    set(FMOD_LIBRARY_DEBUG ${FMOD_LIBRARY_RELEASE})
  endif (NOT FMOD_LIBRARY_DEBUG)
else (FMOD_LIBRARY_RELEASE AND FMODSTUDIO_INCLUDE_DIR)
  message(STATUS "No support for FMOD Studio audio (need to set FMODSTUDIO_SDK_DIR?)")
  set(FMOD OFF)
  set(USE_FMODSTUDIO OFF)
endif (FMOD_LIBRARY_RELEASE AND FMODSTUDIO_INCLUDE_DIR)

if (FMOD)
  message(STATUS "Building with FMOD Studio audio support")
endif (FMOD)

