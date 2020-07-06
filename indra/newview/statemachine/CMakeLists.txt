# -*- cmake -*-

project(statemachine)

include(00-Common)
include(LLCommon)
include(LLPlugin)
include(LLMessage)
include(LLMath)
include(LLVFS)
include(LLXML)
include(LLWindow)
include(LLUI)
include(LLRender)
include(LLImage)
include(LLCharacter)
include(LLInventory)
include(LLPrimitive)
include(LLAppearance)

include_directories(
	${CMAKE_SOURCE_DIR}/newview
	${LLCOMMON_INCLUDE_DIRS}
	${LLPLUGIN_INCLUDE_DIRS}
	${LLMESSAGE_INCLUDE_DIRS}
	${LLMATH_INCLUDE_DIRS}
	${LLVFS_INCLUDE_DIRS}
	${LLXML_INCLUDE_DIRS}
	${LLWINDOW_INCLUDE_DIRS}
	${LLUI_INCLUDE_DIRS}
	${LLRENDER_INCLUDE_DIRS}
	${LLIMAGE_INCLUDE_DIRS}
	${LLCHARACTER_INCLUDE_DIRS}
	${LLINVENTORY_INCLUDE_DIRS}
	${LLPRIMITIVE_INCLUDE_DIRS}
	${LLAPPEARANCE_INCLUDE_DIRS}
    )

set(statemachine_SOURCE_FILES
    aifilepicker.cpp
    aifetchinventoryfolder.cpp
    aievent.cpp
    )

set(statemachine_HEADER_FILES
    CMakeLists.txt
    aifilepicker.h
    aidirpicker.h
    aifetchinventoryfolder.h
    aievent.h
    )

set_source_files_properties(${statemachine_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND statemachine_SOURCE_FILES ${statemachine_HEADER_FILES})

add_library (statemachine ${statemachine_SOURCE_FILES})

target_link_libraries(
    statemachine
    PUBLIC
    llcommon
    )
