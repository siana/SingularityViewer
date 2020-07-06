# -*- cmake -*-
include(FreeType)

if (LINUX)
  include(FindPkgConfig)

    set(PKGCONFIG_PACKAGES
        atk
        cairo
        gdk-2.0
        gdk-pixbuf-2.0
        glib-2.0
        gmodule-2.0
        gtk+-2.0
        gthread-2.0
        libpng
        pango
        pangoft2
        pangox
        pangoxft
        sdl
        x11
        )

  foreach(pkg ${PKGCONFIG_PACKAGES})
    pkg_check_modules(${pkg} REQUIRED ${pkg})
    include_directories(${${pkg}_INCLUDE_DIRS})
    link_directories(${${pkg}_LIBRARY_DIRS})
    list(APPEND UI_LIBRARIES ${${pkg}_LIBRARIES})
    add_definitions(${${pkg}_CFLAGS_OTHERS})
  endforeach(pkg)

  add_definitions(-DLL_GTK=1 -DLL_X11=1)
endif (LINUX)
