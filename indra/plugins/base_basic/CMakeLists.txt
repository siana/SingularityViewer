# -*- cmake -*-

project(basic_plugin_base)

include(00-Common)
include(LLCommon)
include(LLPlugin)
include(Linking)
include(PluginAPI)

include_directories(
    ${LLPLUGIN_INCLUDE_DIRS}
    ${LLCOMMON_INCLUDE_DIRS}
)

### basic_plugin_base

set(basic_plugin_base_SOURCE_FILES
    basic_plugin_base.cpp
)

set(basic_plugin_base_HEADER_FILES
    CMakeLists.txt

    basic_plugin_base.h
)

set_source_files_properties(${basic_plugin_base_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND basic_plugin_base_SOURCE_FILES ${basic_plugin_base_HEADER_FILES})

add_library(basic_plugin_base
    ${basic_plugin_base_SOURCE_FILES}
)

set_target_properties(basic_plugin_base PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_link_libraries(basic_plugin_base
    PUBLIC
    llcommon
)
