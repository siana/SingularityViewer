# -*- cmake -*-

include(Variables)

add_subdirectory(base_basic)
add_subdirectory(base_media)
add_subdirectory(filepicker)

if (LINUX)
    add_subdirectory(gstreamer010)
endif (LINUX)

if (WINDOWS)
  if (LIBVLCPLUGIN)
    add_subdirectory(libvlc)
  endif (LIBVLCPLUGIN)
endif (WINDOWS)

add_subdirectory(cef)
add_subdirectory(example_basic)
add_subdirectory(example_media)
