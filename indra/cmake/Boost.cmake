# -*- cmake -*-
include(Prebuilt)

set(Boost_FIND_QUIETLY ON)
set(Boost_FIND_REQUIRED ON)

if (STANDALONE)
  include(FindBoost)

  set(Boost_USE_MULTITHREADED ON)
  find_package(Boost 1.51.0 COMPONENTS date_time filesystem program_options regex system thread wave context)
else (STANDALONE)
  use_prebuilt_binary(boost)
  set(Boost_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/${LL_ARCH_DIR}/include)
  set(Boost_VERSION "1.52")

  if (WINDOWS)
    set(Boost_CONTEXT_LIBRARY 
        optimized libboost_context-mt
        debug libboost_context-mt-gd)
    set(Boost_FILESYSTEM_LIBRARY 
        optimized libboost_filesystem-mt
        debug libboost_filesystem-mt-gd)
    set(Boost_PROGRAM_OPTIONS_LIBRARY 
        optimized libboost_program_options-mt
        debug libboost_program_options-mt-gd)
    set(Boost_REGEX_LIBRARY
        optimized libboost_regex-mt
        debug libboost_regex-mt-gd)
    set(Boost_SIGNALS_LIBRARY 
        optimized libboost_signals-mt
        debug libboost_signals-mt-gd)
    set(Boost_SYSTEM_LIBRARY 
        optimized libboost_system-mt
        debug libboost_system-mt-gd)
    set(Boost_THREAD_LIBRARY 
        optimized libboost_thread-mt
        debug libboost_thread-mt-gd)
  elseif (LINUX)
    set(Boost_CONTEXT_LIBRARY
        optimized boost_context-mt.a
        debug boost_context-mt-d.a)
    set(Boost_FILESYSTEM_LIBRARY
        optimized boost_filesystem-mt.a
        debug boost_filesystem-mt-d.a)
    set(Boost_PROGRAM_OPTIONS_LIBRARY
        optimized boost_program_options-mt.a
        debug boost_program_options-mt-d.a)
    set(Boost_REGEX_LIBRARY
        optimized boost_regex-mt.a
        debug boost_regex-mt-d.a)
    set(Boost_SIGNALS_LIBRARY
        optimized boost_signals-mt.a
        debug boost_signals-mt-d.a)
    set(Boost_SYSTEM_LIBRARY
        optimized boost_system-mt.a
        debug boost_system-mt-d.a)
    set(Boost_THREAD_LIBRARY
        optimized boost_thread-mt.a
        debug boost_thread-mt-d.a)
  elseif (DARWIN)
    set(Boost_CONTEXT_LIBRARY
        optimized boost_context-mt
        debug boost_context-mt-d)
    set(Boost_FILESYSTEM_LIBRARY
        optimized boost_filesystem-mt
        debug boost_filesystem-mt-d)
    set(Boost_PROGRAM_OPTIONS_LIBRARY
        optimized boost_program_options-mt
        debug boost_program_options-mt-d)
    set(Boost_REGEX_LIBRARY
        optimized boost_regex-mt
        debug boost_regex-mt-d)
    set(Boost_SIGNALS_LIBRARY
        optimized boost_signals-mt
        debug boost_signals-mt-d)
    set(Boost_SYSTEM_LIBRARY
        optimized boost_system-mt
        debug boost_system-mt-d)
    set(Boost_THREAD_LIBRARY
        optimized boost_thread-mt
        debug boost_thread-mt-d)
  endif (WINDOWS)
endif (STANDALONE)
