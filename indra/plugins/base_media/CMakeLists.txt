# -*- cmake -*-

project(media_plugin_base)

# Fail at configure, not link time.
set(OpenGL_FIND_REQUIRED ON)

include(00-Common)
include(LLCommon)
include(LLImage)
include(LLPlugin)
include(LLMath)
include(LLRender)
include(LLWindow)
include(Linking)
include(PluginAPI)
include(FindOpenGL)
include(BasicPluginBase)

include_directories(
    ${LLPLUGIN_INCLUDE_DIRS}
    ${LLCOMMON_INCLUDE_DIRS}
    ${LLMATH_INCLUDE_DIRS}
    ${LLIMAGE_INCLUDE_DIRS}
    ${LLRENDER_INCLUDE_DIRS}
    ${LLWINDOW_INCLUDE_DIRS}
	${BASIC_PLUGIN_BASE_INCLUDE_DIRS}
)

### media_plugin_base

set(media_plugin_base_SOURCE_FILES
    media_plugin_base.cpp
)

set(media_plugin_base_HEADER_FILES
    CMakeLists.txt

    media_plugin_base.h
)

set_source_files_properties(${media_plugin_base_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND media_plugin_base_SOURCE_FILES ${media_plugin_base_HEADER_FILES})

add_library(media_plugin_base
    ${media_plugin_base_SOURCE_FILES}
)

set_target_properties(media_plugin_base PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_link_libraries(media_plugin_base
    PUBLIC
    llcommon
	${BASIC_PLUGIN_BASE_LIBRARIES}
)
