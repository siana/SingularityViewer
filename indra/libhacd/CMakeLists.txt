# -*- cmake -*-

project(libhacd)
include(00-Common)

set(libhacd_SOURCE_FILES
    hacdGraph.cpp
    hacdHACD.cpp
    hacdICHull.cpp
    hacdManifoldMesh.cpp
    hacdMeshDecimator.cpp
    hacdMicroAllocator.cpp
    hacdRaycastMesh.cpp
)

set(libhacd_HEADER_FILES
    hacdCircularList.h
    hacdCircularList.inl
    hacdGraph.h
    hacdHACD.h
    hacdICHull.h
    hacdManifoldMesh.h
    hacdMeshDecimator.h
    hacdMicroAllocator.h
    hacdRaycastMesh.h
    hacdSArray.h
    hacdVector.h
    hacdVector.inl
    hacdVersion.h
)

set_source_files_properties(${libhacd_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

IF(WINDOWS)
   add_definitions(-D_CRT_SECURE_NO_WARNINGS)
ENDIF(WINDOWS)

add_library(hacd ${libhacd_SOURCE_FILES} ${libhacd_INCLUDE_FILES})
