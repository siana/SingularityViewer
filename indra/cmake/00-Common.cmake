# -*- cmake -*-
#
# Compilation options shared by all Second Life components.

if(NOT DEFINED ${CMAKE_CURRENT_LIST_FILE}_INCLUDED)
set(${CMAKE_CURRENT_LIST_FILE}_INCLUDED "YES")

include(Variables)


# Portable compilation flags.

set(CMAKE_CXX_FLAGS_DEBUG "-D_DEBUG -DLL_DEBUG=1")
set(CMAKE_CXX_FLAGS_RELEASE
    "-DLL_RELEASE=1 -DLL_RELEASE_FOR_DOWNLOAD=1 -D_SECURE_SCL=0 -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE
    "${CMAKE_CXX_FLAGS_RELEASE}")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO 
    "-DLL_RELEASE=1 -D_SECURE_SCL=0 -DNDEBUG -DLL_RELEASE_WITH_DEBUG_INFO=1")

# Configure crash reporting
set(RELEASE_CRASH_REPORTING OFF CACHE BOOL "Enable use of crash reporting in release builds")
set(NON_RELEASE_CRASH_REPORTING OFF CACHE BOOL "Enable use of crash reporting in developer builds")

if(RELEASE_CRASH_REPORTING)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DLL_SEND_CRASH_REPORTS=1")
endif()

if(NON_RELEASE_CRASH_REPORTING)
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -DLL_SEND_CRASH_REPORTS=1")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DLL_SEND_CRASH_REPORTS=1")
endif()


# Don't bother with a MinSizeRel build.

set(CMAKE_CONFIGURATION_TYPES "RelWithDebInfo;Release;Debug" CACHE STRING
    "Supported build types." FORCE)

# Platform-specific compilation flags.

if (WINDOWS)
  # Various libs are compiler specific, generate some variables here we can just use
  # when we require them instead of reimplementing the test each time.
  if (MSVC10)
    set(MSVC_DIR 10.0)
    set(MSVC_SUFFIX 100)
  endif (MSVC10)

  # Remove default /Zm1000 flag that cmake inserts
  string (REPLACE "/Zm1000" " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

  # Don't build DLLs.
  set(BUILD_SHARED_LIBS OFF)

  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /Zi /MDd /MP"
      CACHE STRING "C++ compiler debug options" FORCE)
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO 
      "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Od /Zi /MD /MP"
      CACHE STRING "C++ compiler release-with-debug options" FORCE)
  set(CMAKE_CXX_FLAGS_RELEASE
      "${CMAKE_CXX_FLAGS_RELEASE} ${LL_CXX_FLAGS} /O2 /Zi /MD /MP /fp:fast -D_SECURE_STL=0 -D_HAS_ITERATOR_DEBUGGING=0"
      CACHE STRING "C++ compiler release options" FORCE)
  set(CMAKE_C_FLAGS_RELEASE
      "${CMAKE_C_FLAGS_RELEASE} ${LL_C_FLAGS} /O2 /Zi /MD /MP /fp:fast"
      CACHE STRING "C compiler release options" FORCE)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LARGEADDRESSAWARE")

  set(CMAKE_CXX_STANDARD_LIBRARIES "")
  set(CMAKE_C_STANDARD_LIBRARIES "")

  add_definitions(
      /DLL_WINDOWS=1
      /DUNICODE
      /D_UNICODE 
      /GS
      /TP
      /W3
      /c
      /Zc:forScope
	  /Zc:wchar_t-
      /nologo
      /Oy-
	  /arch:SSE2
      )

  # configure win32 API for windows XP+ compatibility
  set(WINVER "0x0501" CACHE STRING "Win32 API Target version (see http://msdn.microsoft.com/en-us/library/aa383745%28v=VS.85%29.aspx)")
  add_definitions("/DWINVER=${WINVER}" "/D_WIN32_WINNT=${WINVER}")

  # Are we using the crummy Visual Studio KDU build workaround?
  if (NOT DISABLE_FATAL_WARNINGS)
    add_definitions(/WX)
  endif (NOT DISABLE_FATAL_WARNINGS)

  SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")
  SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /MANIFEST:NO")
  SET(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /MANIFEST:NO")

endif (WINDOWS)

set (GCC_EXTRA_OPTIMIZATIONS "-ffast-math")

if (LINUX)
  set(CMAKE_SKIP_RPATH TRUE)

  add_definitions(
      -DLL_LINUX=1
      -DAPPID=secondlife
      -D_REENTRANT
      -fexceptions
      -fno-math-errno
      -fno-strict-aliasing
      -fsigned-char
      -fvisibility=hidden
      -g
      -pthread
      )

  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -D_FORTIFY_SOURCE=2 ")

  # Don't catch SIGCHLD in our base application class for the viewer
  # some of our 3rd party libs may need their *own* SIGCHLD handler to work.  Sigh!
  # The viewer doesn't need to catch SIGCHLD anyway.
  add_definitions(-DLL_IGNORE_SIGCHLD)

  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    find_program(GXX g++)
    mark_as_advanced(GXX)

    if (GXX)
      execute_process(
          COMMAND ${GXX} --version
          COMMAND sed "s/^[gc+ ]*//"
          COMMAND head -1
          OUTPUT_VARIABLE GXX_VERSION
          OUTPUT_STRIP_TRAILING_WHITESPACE
          )
    else (GXX)
      set(GXX_VERSION x)
    endif (GXX)

    # The quoting hack here is necessary in case we're using distcc or
    # ccache as our compiler.  CMake doesn't pass the command line
    # through the shell by default, so we end up trying to run "distcc"
    # " g++" - notice the leading space.  Ugh.

    execute_process(
        COMMAND sh -c "${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_ARG1} --version"
        COMMAND sed "s/^[gc+ ]*//"
        COMMAND head -1
        OUTPUT_VARIABLE CXX_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    #Lets actually get a numerical version of gxx's version
    STRING(REGEX REPLACE ".* ([0-9])\\.([0-9])\\.([0-9]).*" "\\1\\2\\3" CXX_VERSION ${CXX_VERSION})

    #gcc 4.3 and above doesn't like the LL boost
    if(${CXX_VERSION} GREATER 429)
      add_definitions(-Wno-parentheses)
    endif (${CXX_VERSION} GREATER 429)

    #gcc 4.6 has a new spammy warning
    if(NOT ${CXX_VERSION} LESS 460)
      add_definitions(-Wno-unused-but-set-variable)
    endif (NOT ${CXX_VERSION} LESS 460)

    #gcc 4.8 boost spam wall
    if(NOT ${CXX_VERSION} LESS 480)
      add_definitions(-Wno-unused-local-typedefs)
    endif (NOT ${CXX_VERSION} LESS 480)

    # End of hacks.

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")

    if (NOT STANDALONE)
      # this stops us requiring a really recent glibc at runtime
      add_definitions(-fno-stack-protector)
    endif (NOT STANDALONE)
    if (${ARCH} STREQUAL "x86_64")
      add_definitions(-DLINUX64=1 -pipe)
      set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fomit-frame-pointer -ffast-math -funroll-loops")
      set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fomit-frame-pointer -ffast-math -funroll-loops")
      set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -ffast-math")
      set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -ffast-math")
    else (${ARCH} STREQUAL "x86_64")
      if (NOT STANDALONE)
        set(MARCH_FLAG " -march=pentium4")
      endif (NOT STANDALONE)
      set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}${MARCH_FLAG} -fno-inline -msse2")
      set(CMAKE_C_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}${MARCH_FLAG} -fno-inline -msse2")
      set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}${MARCH_FLAG} -mfpmath=sse,387 -msse2 ${GCC_EXTRA_OPTIMIZATIONS}")
      set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}${MARCH_FLAG} -mfpmath=sse,387 -msse2 ${GCC_EXTRA_OPTIMIZATIONS}")
      set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}${MARCH_FLAG} -mfpmath=sse,387 -msse2 ${GCC_EXTRA_OPTIMIZATIONS}")
      set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}${MARCH_FLAG} -mfpmath=sse,387 -msse2 ${GCC_EXTRA_OPTIMIZATIONS}")
    endif (${ARCH} STREQUAL "x86_64")
  elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    if (NOT STANDALONE)
      # this stops us requiring a really recent glibc at runtime
      add_definitions(-fno-stack-protector)
    endif (NOT STANDALONE)

    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}${MARCH_FLAG} -fno-inline -msse2")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}${MARCH_FLAG} -fno-inline -msse2")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}${MARCH_FLAG} -msse2")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}${MARCH_FLAG} -msse2")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}${MARCH_FLAG} -msse2")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}${MARCH_FLAG} -msse2")
  elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")

    if (NOT STANDALONE)
      # this stops us requiring a really recent glibc at runtime
      add_definitions(-fno-stack-protector)
    endif (NOT STANDALONE)

    if (NOT STANDALONE)
      set(MARCH_FLAG " -axsse4.1 -msse2")
    endif (NOT STANDALONE)

    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}${MARCH_FLAG} -fno-inline-functions")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}${MARCH_FLAG} -fno-inline-functions")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}${MARCH_FLAG} -parallel -fp-model fast=1")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}${MARCH_FLAG} -parallel -fp-model fast=1")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}${MARCH_FLAG} -parallel -fp-model fast=1")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}${MARCH_FLAG} -parallel -fp-model fast=1")
  endif()

  set(CMAKE_CXX_FLAGS_DEBUG "-O0 ${CMAKE_CXX_FLAGS_DEBUG}")
  set(CMAKE_C_FLAGS_DEBUG "-O0 ${CMAKE_CXX_FLAGS_DEBUG}")
  set(CMAKE_CXX_FLAGS_RELEASE "-O3 ${CMAKE_CXX_FLAGS_RELEASE}")
  set(CMAKE_C_FLAGS_RELEASE "-O3 ${CMAKE_C_FLAGS_RELEASE}")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 ${CMAKE_C_FLAGS_RELWITHDEBINFO}")  
endif (LINUX)


if (DARWIN)
  add_definitions(-DLL_DARWIN=1 -D_XOPEN_SOURCE)
  set(CMAKE_CXX_LINK_FLAGS "-Wl,-headerpad_max_install_names,-search_paths_first")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_CXX_LINK_FLAGS}")

  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mlong-branch")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mlong-branch")
    # NOTE: it's critical that the optimization flag is put in front.
    # NOTE: it's critical to have both CXX_FLAGS and C_FLAGS covered.
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -msse3 -mtune=generic -mfpmath=sse ${GCC_EXTRA_OPTIMIZATIONS}")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -msse3 -mtune=generic -mfpmath=sse ${GCC_EXTRA_OPTIMIZATIONS}")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O3 -msse3 -mtune=generic -mfpmath=sse ${GCC_EXTRA_OPTIMIZATIONS}")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -O3 -msse3 -mtune=generic -mfpmath=sse ${GCC_EXTRA_OPTIMIZATIONS}")
  elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    # NOTE: it's critical that the optimization flag is put in front.
    # NOTE: it's critical to have both CXX_FLAGS and C_FLAGS covered.
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -msse3")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -msse3")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O3 -msse3")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -O3 -msse3")
  endif()
endif (DARWIN)


if (LINUX OR DARWIN)
  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_definitions(-DLL_GNUC=1)
    set(UNIX_WARNINGS "-Wall -Wno-sign-compare -Wno-trigraphs")
    set(UNIX_CXX_WARNINGS "${UNIX_WARNINGS} -Wno-reorder -Wno-non-virtual-dtor -Woverloaded-virtual")
  elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_definitions(-DLL_CLANG=1)
    set(UNIX_WARNINGS "-Wall -Wno-sign-compare -Wno-trigraphs -Wno-tautological-compare -Wno-char-subscripts -Wno-gnu -Wno-logical-op-parentheses -Wno-non-virtual-dtor")
    set(UNIX_WARNINGS "${UNIX_WARNINGS} -Woverloaded-virtual -Wno-parentheses-equality -Wno-reorder -Wno-unused-function -Wno-unused-value -Wno-unused-variable")
    set(UNIX_CXX_WARNINGS "${UNIX_WARNINGS}")
  elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
    add_definitions(-DLL_ICC=1)
    set(UNIX_WARNINGS "-wd327 -wd597 -wd858")
    set(UNIX_CXX_WARNINGS "${UNIX_WARNINGS}")
  endif ()
  
  if (NOT DISABLE_FATAL_WARNINGS)
    set(UNIX_WARNINGS "${UNIX_WARNINGS} -Werror")
    set(UNIX_CXX_WARNINGS "${UNIX_CXX_WARNINGS} -Werror")
  endif (NOT DISABLE_FATAL_WARNINGS)

  set(CMAKE_C_FLAGS "${UNIX_WARNINGS} ${CMAKE_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${UNIX_CXX_WARNINGS} ${CMAKE_CXX_FLAGS}")
  if (WORD_SIZE EQUAL 32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
  elseif (WORD_SIZE EQUAL 64)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64")
  endif (WORD_SIZE EQUAL 32)
endif (LINUX OR DARWIN)


if (STANDALONE)
  add_definitions(-DLL_STANDALONE=1)
else (STANDALONE)
  set(${ARCH}_linux_INCLUDES
      ELFIO
      atk-1.0
      glib-2.0
      gstreamer-0.10
      gtk-2.0
      freetype2
      pango-1.0
      )
endif (STANDALONE)

if(1 EQUAL 1)
  add_definitions(-DENABLE_CLASSIC_CLOUDS=1)
  if (NOT "$ENV{SHY_MOD}" STREQUAL "")
    add_definitions(-DSHY_MOD=1)
  endif (NOT "$ENV{SHY_MOD}" STREQUAL "")
endif(1 EQUAL 1)

SET( CMAKE_EXE_LINKER_FLAGS_RELEASE
    "${CMAKE_EXE_LINKER_FLAGS_RELEASE}" CACHE STRING
    "Flags used for linking binaries under build."
    FORCE )
SET( CMAKE_SHARED_LINKER_FLAGS_RELEASE
    "${CMAKE_SHARED_LINKER_FLAGS_RELEASE}" CACHE STRING
    "Flags used by the shared libraries linker under build."
    FORCE )
MARK_AS_ADVANCED(
    CMAKE_CXX_FLAGS_RELEASE
    CMAKE_C_FLAGS_RELEASE
    CMAKE_EXE_LINKER_FLAGS_RELEASE
    CMAKE_SHARED_LINKER_FLAGS_RELEASE
    )

include(GooglePerfTools)

endif(NOT DEFINED ${CMAKE_CURRENT_LIST_FILE}_INCLUDED)
