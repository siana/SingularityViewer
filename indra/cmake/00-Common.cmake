# -*- cmake -*-
#
# Compilation options shared by all Second Life components.

if(NOT DEFINED ${CMAKE_CURRENT_LIST_FILE}_INCLUDED)
set(${CMAKE_CURRENT_LIST_FILE}_INCLUDED "YES")

include(CheckCCompilerFlag)
include(Variables)

# Portable compilation flags.

set(CMAKE_CXX_FLAGS_DEBUG "-D_DEBUG -DLL_DEBUG=1")
set(CMAKE_CXX_FLAGS_RELEASE
    "-DLL_RELEASE=1 -DLL_RELEASE_FOR_DOWNLOAD=1 -D_SECURE_SCL=0 -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE
    "${CMAKE_CXX_FLAGS_RELEASE}")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO 
    "-DLL_RELEASE=1 -D_SECURE_SCL=0 -DNDEBUG -DLL_RELEASE_WITH_DEBUG_INFO=1")

# Don't bother with a MinSizeRel build.
set(CMAKE_CONFIGURATION_TYPES "RelWithDebInfo;Release;Debug" CACHE STRING
    "Supported build types." FORCE)

# Platform-specific compilation flags.
if (WINDOWS)
  # Don't build DLLs.
  set(BUILD_SHARED_LIBS OFF)

  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /Zi /MDd /MP"
      CACHE STRING "C++ compiler debug options" FORCE)
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO 
      "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Od /Zi /MD /MP"
      CACHE STRING "C++ compiler release-with-debug options" FORCE)
  set(CMAKE_CXX_FLAGS_RELEASE
      "${CMAKE_CXX_FLAGS_RELEASE} ${LL_CXX_FLAGS} /O2 /Zi /Zo /MD /MP /Ob2 /Zc:inline /fp:fast -D_ITERATOR_DEBUG_LEVEL=0"
      CACHE STRING "C++ compiler release options" FORCE)
  set(CMAKE_C_FLAGS_RELEASE
      "${CMAKE_C_FLAGS_RELEASE} ${LL_C_FLAGS} /O2 /Zi /MD /MP /fp:fast"
      CACHE STRING "C compiler release options" FORCE)

  if (ADDRESS_SIZE EQUAL 32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LARGEADDRESSAWARE")
  endif (ADDRESS_SIZE EQUAL 32)

  if (FULL_DEBUG_SYMS OR USE_CRASHPAD)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /DEBUG:FULL")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEBUG:FULL")
  else ()
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /DEBUG:FASTLINK")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEBUG:FASTLINK")
  endif ()

  if (USE_LTO)
    if(INCREMENTAL_LINK)
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LTCG:INCREMENTAL")
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /LTCG:INCREMENTAL")
      set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /LTCG:INCREMENTAL")
    else(INCREMENTAL_LINK)
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LTCG")
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /LTCG")
      set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /LTCG")
    endif(INCREMENTAL_LINK)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /OPT:REF /OPT:ICF /LTCG")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /OPT:REF /OPT:ICF /LTCG")
    set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /LTCG")
  elseif (INCREMENTAL_LINK)
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS} /INCREMENTAL /VERBOSE:INCR")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS} /INCREMENTAL /VERBOSE:INCR")
  else ()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /OPT:REF /INCREMENTAL:NO")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /OPT:REF /INCREMENTAL:NO")
  endif ()

  set(CMAKE_CXX_STANDARD_LIBRARIES "")
  set(CMAKE_C_STANDARD_LIBRARIES "")

  add_definitions(
      /DLL_WINDOWS=1
      /DNOMINMAX
      /DUNICODE
      /D_UNICODE
      /DBOOST_CONFIG_SUPPRESS_OUTDATED_MESSAGE
      /GS
      /TP
      /W3
      /c
      /Zc:__cplusplus
      /Zc:forScope
      /Zc:rvalueCast
      /Zc:wchar_t
      /nologo
      /Oy-
      /Zm140
      /wd4267
      /wd4244
      )

  if (USE_LTO)
    add_compile_options(
        /GL
        /Gy
        /Gw
        )
  endif (USE_LTO)

  if (ADDRESS_SIZE EQUAL 32)
    add_compile_options(/arch:SSE2)
  endif (ADDRESS_SIZE EQUAL 32)

  if (NOT DISABLE_FATAL_WARNINGS)
    add_definitions(/WX)
  endif (NOT DISABLE_FATAL_WARNINGS)

  # configure win32 API for windows Vista+ compatibility
  set(WINVER "0x0600" CACHE STRING "Win32 API Target version (see http://msdn.microsoft.com/en-us/library/aa383745%28v=VS.85%29.aspx)")
  add_definitions("/DWINVER=${WINVER}" "/D_WIN32_WINNT=${WINVER}")
endif (WINDOWS)

set (GCC_EXTRA_OPTIMIZATIONS "-ffast-math")

if (LINUX)
  set(CMAKE_SKIP_RPATH TRUE)

  add_compile_options(
    -fvisibility=hidden
    -fexceptions
    -fno-math-errno
    -fno-strict-aliasing
    -fsigned-char
    -g
    -pthread
    )

  add_definitions(
    -DLL_LINUX=1
    -DAPPID=secondlife
    -D_REENTRANT
  )

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++14")

  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2")

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

    CHECK_C_COMPILER_FLAG(-fstack-protector-strong HAS_STRONG_STACK_PROTECTOR)
    if (${CMAKE_BUILD_TYPE} STREQUAL "Release")
      if(HAS_STRONG_STACK_PROTECTOR)
        add_compile_options(-fstack-protector-strong)
      endif(HAS_STRONG_STACK_PROTECTOR)
    endif (${CMAKE_BUILD_TYPE} STREQUAL "Release")

    if (${ARCH} STREQUAL "x86_64")
      add_definitions(-pipe)
      set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ffast-math")
      set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -ffast-math")
      set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -ffast-math")
      set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -ffast-math")
    else (${ARCH} STREQUAL "x86_64")
      if (NOT STANDALONE)
        set(MARCH_FLAG " -march=pentium4")
      endif (NOT STANDALONE)
      set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}${MARCH_FLAG} -fno-inline -msse3")
      set(CMAKE_C_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}${MARCH_FLAG} -fno-inline -msse3")
      set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}${MARCH_FLAG} -mfpmath=sse,387 -msse3 ${GCC_EXTRA_OPTIMIZATIONS}")
      set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}${MARCH_FLAG} -mfpmath=sse,387 -msse3 ${GCC_EXTRA_OPTIMIZATIONS}")
      set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}${MARCH_FLAG} -mfpmath=sse,387 -msse3 ${GCC_EXTRA_OPTIMIZATIONS}")
      set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}${MARCH_FLAG} -mfpmath=sse,387 -msse3 ${GCC_EXTRA_OPTIMIZATIONS}")
    endif (${ARCH} STREQUAL "x86_64")
  elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}${MARCH_FLAG} -fno-inline -msse3")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}${MARCH_FLAG} -fno-inline -msse3")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}${MARCH_FLAG} -msse3")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}${MARCH_FLAG} -msse3")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}${MARCH_FLAG} -msse3")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}${MARCH_FLAG} -msse3")
  elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
    if (NOT STANDALONE)
      set(MARCH_FLAG " -axsse4.1 -msse3")
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
  add_definitions(-DLL_DARWIN=1)
  set(CMAKE_CXX_LINK_FLAGS "-Wl,-no_compact_unwind -Wl,-headerpad_max_install_names,-search_paths_first")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_CXX_LINK_FLAGS}")
  set(DARWIN_extra_cstar_flags "-g")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${DARWIN_extra_cstar_flags} -ftemplate-depth=256")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  ${DARWIN_extra_cstar_flags}")
  # NOTE: it's critical that the optimization flag is put in front.
  # NOTE: it's critical to have both CXX_FLAGS and C_FLAGS covered.
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O0 ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O0 ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
  set(CMAKE_XCODE_ATTRIBUTE_CLANG_X86_VECTOR_INSTRUCTIONS SSE3)
  set(CMAKE_XCODE_ATTRIBUTE_GCC_OPTIMIZATION_LEVEL -O3)
  set(CMAKE_CXX_FLAGS_RELEASE "-O3 -msse3 ${CMAKE_CXX_FLAGS_RELEASE}")
  set(CMAKE_C_FLAGS_RELEASE "-O3 -msse3 ${CMAKE_C_FLAGS_RELEASE}")
  if (XCODE_VERSION GREATER 4.2)
    set(ENABLE_SIGNING TRUE)
    set(SIGNING_IDENTITY "Developer ID Application: Linden Research, Inc.")
  endif (XCODE_VERSION GREATER 4.2)
endif (DARWIN)



if (LINUX OR DARWIN)
  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_definitions(-DLL_GNUC=1)
    set(UNIX_WARNINGS "-Wall -Wno-sign-compare -Wno-trigraphs")
    set(UNIX_CXX_WARNINGS "${UNIX_WARNINGS} -Wno-reorder -Wno-non-virtual-dtor -Woverloaded-virtual")
  elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_definitions(-DLL_CLANG=1)
    set(UNIX_WARNINGS "-Wall -Wno-sign-compare -Wno-trigraphs -Wno-tautological-compare -Wno-char-subscripts -Wno-gnu -Wno-logical-op-parentheses -Wno-logical-not-parentheses -Wno-non-virtual-dtor -Wno-deprecated")
    set(UNIX_WARNINGS "${UNIX_WARNINGS} -Woverloaded-virtual -Wno-parentheses-equality -Wno-reorder -Wno-unused-function -Wno-unused-value -Wno-unused-variable -Wno-unused-private-field -Wno-parentheses")
    set(UNIX_CXX_WARNINGS "${UNIX_WARNINGS}")
  elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
    add_definitions(-DLL_ICC=1)
  endif ()
  
  if (NOT DISABLE_FATAL_WARNINGS)
    set(UNIX_WARNINGS "${UNIX_WARNINGS} -Werror")
    set(UNIX_CXX_WARNINGS "${UNIX_CXX_WARNINGS} -Werror")
  endif (NOT DISABLE_FATAL_WARNINGS)

  set(CMAKE_C_FLAGS "${UNIX_WARNINGS} ${CMAKE_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${UNIX_CXX_WARNINGS} ${CMAKE_CXX_FLAGS}")
  if (ADDRESS_SIZE EQUAL 32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
  elseif (ADDRESS_SIZE EQUAL 64)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64")
  endif (ADDRESS_SIZE EQUAL 32)
endif (LINUX OR DARWIN)


if (STANDALONE)
  add_definitions(-DLL_STANDALONE=1)
else (STANDALONE)
  #Enforce compile-time correctness for fmt strings
  add_definitions(-DFMT_STRING_ALIAS=1)

  if(USE_CRASHPAD)
    add_definitions(-DUSE_CRASHPAD=1 -DCRASHPAD_URL="${CRASHPAD_URL}")
  endif()

  set(${ARCH}_linux_INCLUDES
      atk-1.0
      cairo
      glib-2.0
      gdk-pixbuf-2.0
      gstreamer-0.10
      gtk-2.0
      pango-1.0
      pixman-1
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
