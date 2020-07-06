# -*- cmake -*-

# these should be moved to their own cmake file
include(Prebuilt)
include(Boost)
include(Colladadom)


use_prebuilt_binary(libxml2)

set(LLPRIMITIVE_INCLUDE_DIRS
  ${LIBS_OPEN_DIR}/llprimitive
  ${COLLADADOM_INCLUDE_DIRS}
  )

if (WINDOWS)
    set(LLPRIMITIVE_LIBRARIES
        llprimitive
        ${COLLADADOM_LIBRARIES}
        libxml2_a
        ${BOOST_SYSTEM_LIBRARIES}
        )
else (WINDOWS)
    set(LLPRIMITIVE_LIBRARIES 
        llprimitive
        ${COLLADADOM_LIBRARIES}
        ${BOOST_SYSTEM_LIBRARIES}
        minizip
        xml2
        )
endif (WINDOWS)

