# -*- cmake -*-

project(llrender)

include(00-Common)
include(FreeType)
include(LLCommon)
include(LLImage)
include(LLMath)
include(LLRender)
include(LLVFS)
include(LLWindow)
include(LLXML)
include(LLVFS)

include_directories(
    ${FREETYPE_INCLUDE_DIRS}
    ${LLCOMMON_INCLUDE_DIRS}
    ${LLIMAGE_INCLUDE_DIRS}
    ${LLMATH_INCLUDE_DIRS}
    ${LLRENDER_INCLUDE_DIRS}
    ${LLVFS_INCLUDE_DIRS}
    ${LLWINDOW_INCLUDE_DIRS}
    ${LLXML_INCLUDE_DIRS}
    ${LLVFS_INCLUDE_DIRS}
    )

set(llrender_SOURCE_FILES
    llcubemap.cpp
    llfontbitmapcache.cpp
    llfontfreetype.cpp
    llfontgl.cpp
    llfontregistry.cpp
    llgl.cpp
    llgldbg.cpp
    llglslshader.cpp
    llgltexture.cpp
    llimagegl.cpp
    llpostprocess.cpp
    llrender.cpp
    llrender2dutils.cpp
    llrendersphere.cpp
    llrendertarget.cpp
    llshadermgr.cpp
    lltexture.cpp
    lluiimage.cpp
    llvertexbuffer.cpp
    )
    
set(llrender_HEADER_FILES
    CMakeLists.txt

    llcubemap.h
    llfontbitmapcache.h
    llfontfreetype.h
    llfontgl.h
    llfontregistry.h
    llgl.h
    llgldbg.h
    llglheaders.h
    llglslshader.h
    llglstates.h
    llgltexture.h
    llgltypes.h
    llimagegl.h
    llpostprocess.h
    llrender.h
    llrender2dutils.h
    llrendersphere.h
    llshadermgr.h
    lltexture.h
    lluiimage.h
    llvertexbuffer.h
    )

set_source_files_properties(${llrender_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

# Workaround hack for clang bugs
if (DARWIN)
    set_property(SOURCE llgl.cpp PROPERTY COMPILE_FLAGS -O1)
endif (DARWIN)

list(APPEND llrender_SOURCE_FILES ${llrender_HEADER_FILES})

add_library (llrender ${llrender_SOURCE_FILES})

target_link_libraries(
    llrender
    PUBLIC
    llcommon
    )
