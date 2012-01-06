# -*- cmake -*-

project(openjpeg)

include(00-Common)


# OPENJPEG version number, useful for packaging and doxygen doc:
set(OPENJPEG_VERSION_MAJOR 1)
set(OPENJPEG_VERSION_MINOR 4)
set(OPENJPEG_VERSION_BUILD 0)
set(OPENJPEG_VERSION
  "${OPENJPEG_VERSION_MAJOR}.${OPENJPEG_VERSION_MINOR}.${OPENJPEG_VERSION_BUILD}")

set(openjpeg_SOURCE_FILES
    bio.c
    cio.c
    dwt.c
    event.c
    image.c
    j2k.c
    j2k_lib.c
    jp2.c
    jpt.c
    mct.c
    mqc.c
    openjpeg.c
    pi.c
    raw.c
    t1.c
    t2.c
    tcd.c
    tgt.c
)

set(openjpeg_HEADER_FILES
    bio.h
    cio.h
    dwt.h
    event.h 
    fix.h
    image.h
    int.h
    j2k.h
    j2k_lib.h
    jp2.h
    jpt.h
    mct.h
    mqc.h
    openjpeg.h
    opj_includes.h
    opj_malloc.h
    pi.h
    raw.h
    t1.h
    t1_luts.h
    t2.h
    tcd.h
    tgt.h
)

IF(WINDOWS)
   add_definitions(-D_CRT_SECURE_NO_WARNINGS)
   add_definitions(-DOPJ_STATIC)
ENDIF(WINDOWS)


set_source_files_properties(${openjpeg_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND openjpeg_SOURCE_FILES ${openjpeg_HEADER_FILES})

add_library (openjpeg ${openjpeg_SOURCE_FILES})

# This setting of SOVERSION assumes that any API change
# will increment either the minor or major version number of openjpeg
set(OPENJPEG_LIBRARY_PROPERTIES
  VERSION   "${OPENJPEG_VERSION_MAJOR}.${OPENJPEG_VERSION_MINOR}.${OPENJPEG_VERSION_BUILD}"
  SOVERSION "${OPENJPEG_VERSION_MAJOR}.${OPENJPEG_VERSION_MINOR}"
)

set_target_properties(openjpeg PROPERTIES
	${OPENJPEG_LIBRARY_PROPERTIES})

