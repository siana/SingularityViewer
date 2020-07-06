# -*- cmake -*-

project(aistatemachine)

include(00-Common)
include(LLCommon)
include(LLMessage)
include(LLMath)
include(LLXML)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

include_directories(
    ${LLCOMMON_INCLUDE_DIRS}
    ${LLMESSAGE_INCLUDE_DIRS}
    ${LLMATH_INCLUDE_DIRS}
    ${LLXML_INCLUDE_DIRS}
    )

set(aistatemachine_SOURCE_FILES
    aistatemachine.cpp
    aistatemachinethread.cpp
    aitimer.cpp
    aicondition.cpp
    )

set(aistatemachine_HEADER_FILES
    CMakeLists.txt

    aistatemachine.h
    aistatemachinethread.h
    aitimer.h
    aicondition.h
    )

set_source_files_properties(${aistatemachine_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND aistatemachine_SOURCE_FILES ${aistatemachine_HEADER_FILES})

add_library (aistatemachine ${aistatemachine_SOURCE_FILES})

target_link_libraries(
    aistatemachine
    PUBLIC
    llcommon
    )
