# -*- cmake -*-

project(libndhacd)

include(00-Common)

include_directories(${LIBS_OPEN_DIR}/libhacd)

set (libndhacd_SOURCE_FILES
    llconvexdecomposition.cpp
    nd_hacdConvexDecomposition.cpp
    nd_hacdStructs.cpp
    nd_hacdUtils.cpp
    nd_EnterExitTracer.cpp
    nd_StructTracer.cpp
)

set (libndhacd_HEADER_FILES
    llconvexdecomposition.h
    ndConvexDecomposition.h
    nd_hacdConvexDecomposition.h
    nd_hacdStructs.h
    nd_StructTracer.h
    llconvexdecompositionstubimpl.h
    nd_EnterExitTracer.h
    nd_hacdDefines.h
    nd_hacdUtils.h
    windowsincludes.h
)

set_source_files_properties(${libndhacd_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

add_library( nd_hacdConvexDecomposition STATIC ${libndhacd_SOURCE_FILES} ${libndhacd_HEADER_FILES})

target_link_libraries(
    nd_hacdConvexDecomposition
    PUBLIC
    llcommon
    )
