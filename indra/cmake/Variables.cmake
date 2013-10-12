# -*- cmake -*-
#
# Definitions of variables used throughout the Second Life build
# process.
#
# Platform variables:
#
#   DARWIN  - Mac OS X
#   LINUX   - Linux
#   WINDOWS - Windows


# Relative and absolute paths to subtrees.

if(NOT DEFINED ${CMAKE_CURRENT_LIST_FILE}_INCLUDED)
set(${CMAKE_CURRENT_LIST_FILE}_INCLUDED "YES")

if(NOT DEFINED COMMON_CMAKE_DIR)
    set(COMMON_CMAKE_DIR "${CMAKE_SOURCE_DIR}/cmake")
endif(NOT DEFINED COMMON_CMAKE_DIR)

set(LIBS_CLOSED_PREFIX)
set(LIBS_OPEN_PREFIX)
set(SCRIPTS_PREFIX ../scripts)
set(VIEWER_PREFIX)

set(LIBS_CLOSED_DIR ${CMAKE_SOURCE_DIR}/${LIBS_CLOSED_PREFIX})
set(LIBS_OPEN_DIR ${CMAKE_SOURCE_DIR}/${LIBS_OPEN_PREFIX})
set(SCRIPTS_DIR ${CMAKE_SOURCE_DIR}/${SCRIPTS_PREFIX})
set(VIEWER_DIR ${CMAKE_SOURCE_DIR}/${VIEWER_PREFIX})
set(DISABLE_TCMALLOC OFF CACHE BOOL "Disable linkage of TCMalloc. (64bit builds automatically disable TCMalloc)")
set(LL_TESTS OFF CACHE BOOL "Build and run unit and integration tests (disable for build timing runs to reduce variation)")
set(DISABLE_FATAL_WARNINGS TRUE CACHE BOOL "Set this to FALSE to enable fatal warnings.")

set(LIBS_PREBUILT_DIR ${CMAKE_SOURCE_DIR}/../libraries CACHE PATH
    "Location of prebuilt libraries.")

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  set(WINDOWS ON BOOL FORCE)
  set(ARCH i686)
  set(LL_ARCH ${ARCH}_win32)
  set(LL_ARCH_DIR ${ARCH}-win32)
  set(WORD_SIZE 32)
endif (${CMAKE_SYSTEM_NAME} MATCHES "Windows")

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  set(LINUX ON BOOl FORCE)

  # If someone has specified a word size, use that to determine the
  # architecture.  Otherwise, let the architecture specify the word size.
  if (WORD_SIZE EQUAL 32)
    set(ARCH i686)
  elseif (WORD_SIZE EQUAL 64)
    set(ARCH x86_64)
  else (WORD_SIZE EQUAL 32)
    if(CMAKE_SIZEOF_VOID_P MATCHES 4)
      set(ARCH i686)
      set(WORD_SIZE 32)
    else(CMAKE_SIZEOF_VOID_P MATCHES 4)
      set(ARCH x86_64)
      set(WORD_SIZE 64)
    endif(CMAKE_SIZEOF_VOID_P MATCHES 4)
  endif (WORD_SIZE EQUAL 32)

  if (NOT STANDALONE AND MULTIARCH_HACK)
    if (WORD_SIZE EQUAL 32)
      set(DEB_ARCHITECTURE i386)
      set(FIND_LIBRARY_USE_LIB64_PATHS OFF)
      set(CMAKE_SYSTEM_LIBRARY_PATH /usr/lib32 ${CMAKE_SYSTEM_LIBRARY_PATH})
    else (WORD_SIZE EQUAL 32)
      set(DEB_ARCHITECTURE amd64)
      set(FIND_LIBRARY_USE_LIB64_PATHS ON)
    endif (WORD_SIZE EQUAL 32)

    execute_process(COMMAND dpkg-architecture -a${DEB_ARCHITECTURE} -qDEB_HOST_MULTIARCH 
        RESULT_VARIABLE DPKG_RESULT
        OUTPUT_VARIABLE DPKG_ARCH
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    #message (STATUS "DPKG_RESULT ${DPKG_RESULT}, DPKG_ARCH ${DPKG_ARCH}")
    if (DPKG_RESULT EQUAL 0)
      set(CMAKE_LIBRARY_ARCHITECTURE ${DPKG_ARCH})
      set(CMAKE_SYSTEM_LIBRARY_PATH /usr/lib/${DPKG_ARCH} /usr/local/lib/${DPKG_ARCH} ${CMAKE_SYSTEM_LIBRARY_PATH})
    endif (DPKG_RESULT EQUAL 0)

    include(ConfigurePkgConfig)
  endif (NOT STANDALONE AND MULTIARCH_HACK)

  set(LL_ARCH ${ARCH}_linux)
  set(LL_ARCH_DIR ${ARCH}-linux)
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(DARWIN 1)

  if(${CMAKE_GENERATOR} MATCHES Xcode)
    #SDK Compiler and Deployment targets for XCode
    if (${XCODE_VERSION} VERSION_LESS 4.0.0)
      set(CMAKE_OSX_SYSROOT /Developer/SDKs/MacOSX10.5.sdk)
      set(CMAKE_OSX_DEPLOYMENT_TARGET 10.5)
    else (${XCODE_VERSION} VERSION_LESS 4.0.0)
      set(CMAKE_OSX_SYSROOT /Developer/SDKs/MacOSX10.6.sdk)
      set(CMAKE_OSX_DEPLOYMENT_TARGET 10.6)
    endif (${XCODE_VERSION} VERSION_LESS 4.0.0)
  else(${CMAKE_GENERATOR} MATCHES Xcode)
    set(CMAKE_OSX_SYSROOT /Developer/SDKs/MacOSX10.6.sdk)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 10.6)
  endif(${CMAKE_GENERATOR} MATCHES Xcode)

  set(CMAKE_XCODE_ATTRIBUTE_GCC_VERSION "com.apple.compilers.llvmgcc42")

  set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT dwarf-with-dsym)

  # Build only for i386 by default, system default on MacOSX 10.6 is x86_64
  set(CMAKE_OSX_ARCHITECTURES i386)
  set(ARCH i386)
  set(WORD_SIZE 32)

  set(LL_ARCH ${ARCH}_darwin)
  set(LL_ARCH_DIR universal-darwin)
endif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

if (WINDOWS)
  set(PREBUILT_TYPE windows)
elseif(DARWIN)
  set(PREBUILT_TYPE darwin)
elseif(LINUX AND WORD_SIZE EQUAL 32)
  set(PREBUILT_TYPE linux)
elseif(LINUX AND WORD_SIZE EQUAL 64)
  set(PREBUILT_TYPE linux64)
endif(WINDOWS)

# Default deploy grid
set(GRID agni CACHE STRING "Target Grid")

set(VIEWER_CHANNEL "Singularity" CACHE STRING "Viewer Channel Name")
set(VIEWER_LOGIN_CHANNEL ${VIEWER_CHANNEL} CACHE STRING "Fake login channel for A/B Testing")
set(VIEWER_BRANDING_ID "singularity" CACHE STRING "Viewer branding id (currently secondlife|snowglobe)")

# *TODO: break out proper Branding-secondlife.cmake, Branding-snowglobe.cmake, etc
set(VIEWER_BRANDING_NAME "Singularity")
set(VIEWER_BRANDING_NAME_CAMELCASE "Singularity")

set(STANDALONE OFF CACHE BOOL "Do not use Linden-supplied prebuilt libraries.")

source_group("CMake Rules" FILES CMakeLists.txt)

endif(NOT DEFINED ${CMAKE_CURRENT_LIST_FILE}_INCLUDED)
