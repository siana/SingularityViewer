# -*- cmake -*-

project(llinventory)

include(00-Common)
include(LLCommon)
include(LLMath)
include(LLMessage)
include(LLXML)

include_directories(
    ${LLCOMMON_INCLUDE_DIRS}
    ${LLMATH_INCLUDE_DIRS}
    ${LLMESSAGE_INCLUDE_DIRS}
    ${LLXML_INCLUDE_DIRS}
    )

set(llinventory_SOURCE_FILES
    llcategory.cpp
    llfoldertype.cpp
    llinventory.cpp
    llinventorydefines.cpp
    llinventorysettings.cpp
    llinventorytype.cpp
    lllandmark.cpp
    llnotecard.cpp
    llparcel.cpp
    llpermissions.cpp
    llsaleinfo.cpp
    llsettingsbase.cpp
    llsettingsdaycycle.cpp
    llsettingssky.cpp
    llsettingswater.cpp
    lltransactionflags.cpp
    lluserrelations.cpp
    )
    
set(llinventory_HEADER_FILES
    CMakeLists.txt

    llcategory.h
    llfoldertype.h
    llinventory.h
    llinventorydefines.h
    llinventorysettings.h
    llinventorytype.h
    llinvtranslationbrdg.h
    lllandmark.h
    llnotecard.h
    llparcel.h
    llparcelflags.h
    llpermissions.h
    llpermissionsflags.h
    llsaleinfo.h
    llsettingsbase.h
    llsettingsdaycycle.h
    llsettingssky.h
    llsettingswater.h
    lltransactionflags.h
    lltransactiontypes.h
    lluserrelations.h
    )

set_source_files_properties(${llinventory_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND llinventory_SOURCE_FILES ${llinventory_HEADER_FILES})

add_library (llinventory ${llinventory_SOURCE_FILES})

target_link_libraries(
    llinventory
    PUBLIC
    llcommon
    )
