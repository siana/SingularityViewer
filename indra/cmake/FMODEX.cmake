# -*- cmake -*-

include(Linking)

if(INSTALL_PROPRIETARY)
  include(Prebuilt)
  use_prebuilt_binary(fmodex)
endif(INSTALL_PROPRIETARY)

find_library(FMODEX_LIBRARY_RELEASE
             NAMES fmodex fmodex_vc fmodexL_vc
             PATHS
             ${ARCH_PREBUILT_DIRS_RELEASE}
             )

find_library(FMODEX_LIBRARY_DEBUG
             NAMES fmodex fmodex_vc fmodexL_vc
             PATHS
             ${ARCH_PREBUILT_DIRS_DEBUG}
             )

if (FMODEX_LIBRARY_RELEASE AND FMODEX_LIBRARY_DEBUG)
  set(FMODEX_LIBRARY
      debug ${FMODEX_LIBRARY_DEBUG}
      optimized ${FMODEX_LIBRARY_RELEASE})
elseif (FMODEX_LIBRARY_RELEASE)
  set(FMODEX_LIBRARY ${FMODEX_LIBRARY_RELEASE})
endif (FMODEX_LIBRARY_RELEASE AND FMODEX_LIBRARY_DEBUG)

if (NOT FMODEX_LIBRARY)
  set(FMODEX_SDK_DIR CACHE PATH "Path to the FMOD Ex SDK.")
  if (FMODEX_SDK_DIR)
    find_library(FMODEX_LIBRARY
                 fmodex fmodex_vc fmodexL_vc
                 PATHS
                 ${FMODEX_SDK_DIR}/api/lib
                 ${FMODEX_SDK_DIR}/api
                 ${FMODEX_SDK_DIR}/lib
                 ${FMODEX_SDK_DIR}
                 )

				 
  endif(FMODEX_SDK_DIR)
  if(WINDOWS AND NOT FMODEX_LIBRARY)
	set(FMODEX_PROG_DIR "$ENV{PROGRAMFILES}/FMOD SoundSystem/FMOD Programmers API Windows")
	find_library(FMODEX_LIBRARY
                 fmodex_vc fmodexL_vc 
                 PATHS
                 ${FMODEX_PROG_DIR}/api/lib
				 ${FMODEX_PROG_DIR}/api
				 ${FMODEX_PROG_DIR}
                 )
	if(FMODEX_LIBRARY)
		message(STATUS "Found fmodex in ${FMODEX_PROG_DIR}")
		set(FMODEX_SDK_DIR ${FMODEX_PROG_DIR})
		set(FMODEX_SDK_DIR ${FMODEX_PROG_DIR} CACHE PATH "Path to the FMOD Ex SDK." FORCE)
	endif(FMODEX_LIBRARY)
  endif(WINDOWS AND NOT FMODEX_LIBRARY)
endif (NOT FMODEX_LIBRARY)

find_path(FMODEX_INCLUDE_DIR fmod.h
          ${LIBS_PREBUILT_DIR}/include/fmodex
          ${LIBS_PREBUILT_DIR}/${LL_ARCH_DIR}/fmodex
          ${FMODEX_SDK_DIR}/api/inc
          ${FMODEX_SDK_DIR}/inc
          ${FMODEX_SDK_DIR}
          )

if (FMODEX_LIBRARY AND FMODEX_INCLUDE_DIR)
  set(FMODEX ON CACHE BOOL "Use closed source FMOD Ex sound library.")
else (FMODEX_LIBRARY AND FMODEX_INCLUDE_DIR)
  set(FMODEX_LIBRARY "")
  set(FMODEX_INCLUDE_DIR "")
  if (FMODEX)
    message(STATUS "No support for FMOD Ex audio (need to set FMODEX_SDK_DIR?)")
  endif (FMODEX)
  set(FMODEX OFF CACHE BOOL "Use closed source FMOD Ex sound library.")
  set(FMODEX OFF)
endif (FMODEX_LIBRARY AND FMODEX_INCLUDE_DIR)

if (FMODEX)
  message(STATUS "Building with FMOD Ex audio support")
endif (FMODEX)
