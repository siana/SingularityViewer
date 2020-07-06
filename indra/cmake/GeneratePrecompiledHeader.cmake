# -*- cmake -*-

# Distributed under the MIT Software License
# Copyright (c) 2015-2017 Borislav Stanimirov
# Modifications Copyright (c) 2019 Cinder Roxley. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of 
# this software and associated documentation files (the "Software"), to deal in 
# the Software without restriction, including without limitation the rights to 
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
# of the Software, and to permit persons to whom the Software is furnished to do 
# so, subject to the following conditions:
# The above copyright notice and this permission notice shall be included in all 
# copies or substantial portions of the Software.
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
# SOFTWARE.

# target_precompiled_header
#
# Sets a precompiled header for a given target
# Args:
#   TARGET_NAME - Name of the target. Only valid after add_library or add_executable
#   PRECOMPILED_HEADER - Header file to precompile
#   PRECOMPILED_SOURCE - MSVC specific source to do the actual precompilation. Ignored on other platforms
#

macro(target_precompiled_header TARGET_NAME PRECOMPILED_HEADER PRECOMPILED_SOURCE)
    get_filename_component(PRECOMPILED_HEADER_NAME ${PRECOMPILED_HEADER} NAME)

    if(MSVC)
        get_filename_component(PRECOMPILED_SOURCE_NAME ${PRECOMPILED_SOURCE} NAME)
        get_filename_component(PRECOMPILED_HEADER_PATH ${PRECOMPILED_HEADER} DIRECTORY)
        target_include_directories(${TARGET_NAME} PRIVATE ${PRECOMPILED_HEADER_PATH}) # fixes occasional IntelliSense glitches

        get_filename_component(PRECOMPILED_HEADER_WE ${PRECOMPILED_HEADER} NAME_WE)
        if(GEN_IS_MULTI_CONFIG)
            set(PRECOMPILED_BINARY "$(IntDir)/${PRECOMPILED_HEADER_WE}.pch")
        else()
            set(PRECOMPILED_BINARY "${CMAKE_CURRENT_BINARY_DIR}/${PRECOMPILED_HEADER_WE}.pch")
        endif()
        
        set_source_files_properties(${PRECOMPILED_SOURCE} PROPERTIES 
             COMPILE_OPTIONS "/Yc${PRECOMPILED_HEADER_NAME};/Fp${PRECOMPILED_BINARY}"
             OBJECT_OUTPUTS "${PRECOMPILED_BINARY}")

        get_target_property(TARGET_SOURCES ${TARGET_NAME} SOURCES)
        foreach(src ${TARGET_SOURCES})
            if(${src} MATCHES \\.\(cpp|cxx|cc\)$)
                set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/${src}" PROPERTIES
                    COMPILE_OPTIONS "/Yu${PRECOMPILED_HEADER_NAME};/FI${PRECOMPILED_HEADER_NAME};/Fp${PRECOMPILED_BINARY}"
                    OBJECT_DEPENDS "${PRECOMPILED_BINARY}"
                    )
            endif()
        endforeach()
        #set_target_properties(${TARGET_NAME} PROPERTIES 
        #     COMPILE_OPTIONS "/Yu${PRECOMPILED_HEADER_NAME};/FI${PRECOMPILED_HEADER_NAME};/Fp${PRECOMPILED_BINARY}")

        target_sources(${TARGET_NAME} PRIVATE ${PRECOMPILED_SOURCE} ${PRECOMPILED_HEADER})
    elseif(CMAKE_GENERATOR STREQUAL Xcode)
        set_target_properties(
            ${TARGET_NAME}
            PROPERTIES
            XCODE_ATTRIBUTE_GCC_PREFIX_HEADER "${PRECOMPILED_HEADER}"
            XCODE_ATTRIBUTE_GCC_PRECOMPILE_PREFIX_HEADER "YES"
            )
    elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Create and set output directory.
        set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/${PRECOMPILED_HEADER_NAME}.gch")
        make_directory(${OUTPUT_DIR})
        set(OUTPUT_NAME "${OUTPUT_DIR}/${PRECOMPILED_HEADER_NAME}.gch")

    # Export compiler flags via a generator to a response file
        set(PCH_FLAGS_FILE "${OUTPUT_DIR}/${PRECOMPILED_HEADER_NAME}.rsp")
        set(_include_directories "$<TARGET_PROPERTY:${TARGET_NAME},INCLUDE_DIRECTORIES>")
        set(_compile_definitions "$<TARGET_PROPERTY:${TARGET_NAME},COMPILE_DEFINITIONS>")
        set(_compile_flags "$<TARGET_PROPERTY:${TARGET_NAME},COMPILE_FLAGS>")
        set(_compile_options "$<TARGET_PROPERTY:${TARGET_NAME},COMPILE_OPTIONS>")
        set(_include_directories "$<$<BOOL:${_include_directories}>:-I$<JOIN:${_include_directories},\n-I>\n>")
        set(_compile_definitions "$<$<BOOL:${_compile_definitions}>:-D$<JOIN:${_compile_definitions},\n-D>\n>")
        set(_compile_flags "$<$<BOOL:${_compile_flags}>:$<JOIN:${_compile_flags},\n>\n>")
        set(_compile_options "$<$<BOOL:${_compile_options}>:$<JOIN:${_compile_options},\n>\n>")
        file(GENERATE OUTPUT "${PCH_FLAGS_FILE}" CONTENT "${_compile_definitions}${_include_directories}${_compile_flags}${_compile_options}\n")

        # Gather global compiler options, definitions, etc.
        string(TOUPPER "CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}" CXX_FLAGS)
        set(COMPILER_FLAGS "${${CXX_FLAGS}} ${CMAKE_CXX_FLAGS}")
        separate_arguments(COMPILER_FLAGS)

        # Add a custom target for building the precompiled header.
        add_custom_command(
            OUTPUT ${OUTPUT_NAME}
            COMMAND ${CMAKE_CXX_COMPILER} @${PCH_FLAGS_FILE} ${COMPILER_FLAGS} -x c++-header -o ${OUTPUT_NAME} ${PRECOMPILED_HEADER}
            DEPENDS ${PRECOMPILED_HEADER})
        add_custom_target(${TARGET_NAME}_gch DEPENDS ${OUTPUT_NAME})
        add_dependencies(${TARGET_NAME} ${TARGET_NAME}_gch)

        # set_target_properties(${TARGET_NAME} PROPERTIES COMPILE_FLAGS "-include ${PRECOMPILED_HEADER_NAME} -Winvalid-pch")
        get_target_property(SOURCE_FILES ${TARGET_NAME} SOURCES)
        get_target_property(asdf ${TARGET_NAME} COMPILE_FLAGS)
        foreach(SOURCE_FILE ${SOURCE_FILES})
            if(SOURCE_FILE MATCHES \\.\(c|cc|cxx|cpp\)$)
                set_source_files_properties(${SOURCE_FILE} PROPERTIES
                   COMPILE_FLAGS "-include ${OUTPUT_DIR}/${PRECOMPILED_HEADER_NAME} -Winvalid-pch"
                )
            endif()
        endforeach()
    else()
        message(FATAL_ERROR "Unknown generator for target_precompiled_header. [${CMAKE_CXX_COMPILER_ID}]")
    endif()
endmacro(target_precompiled_header)

