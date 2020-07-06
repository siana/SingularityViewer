# -*- cmake -*-
# Construct the viewer version number based on the indra/VIEWER_VERSION file

if (NOT DEFINED VIEWER_SHORT_VERSION) # will be true in indra/, false in indra/newview/
    set(VIEWER_VERSION_BASE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/newview/VIEWER_VERSION.txt")

    if ( EXISTS ${VIEWER_VERSION_BASE_FILE} )
        file(STRINGS ${VIEWER_VERSION_BASE_FILE} VIEWER_SHORT_VERSION REGEX "^[0-9]+\\.[0-9]+\\.[0-9]+")
        string(REGEX REPLACE "^([0-9]+)\\.[0-9]+\\.[0-9]+" "\\1" VIEWER_VERSION_MAJOR ${VIEWER_SHORT_VERSION})
        string(REGEX REPLACE "^[0-9]+\\.([0-9]+)\\.[0-9]+" "\\1" VIEWER_VERSION_MINOR ${VIEWER_SHORT_VERSION})
        string(REGEX REPLACE "^[0-9]+\\.[0-9]+\\.([0-9]+)" "\\1" VIEWER_VERSION_PATCH ${VIEWER_SHORT_VERSION})

        if (DEFINED ENV{revision})
           set(VIEWER_VERSION_REVISION $ENV{revision})
           message("Revision (from environment): ${VIEWER_VERSION_REVISION}")

        else (DEFINED ENV{revision})
          execute_process(
                       COMMAND git rev-list HEAD
                       OUTPUT_VARIABLE GIT_REV_LIST_STR
                       WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                       OUTPUT_STRIP_TRAILING_WHITESPACE
                       )

            if(GIT_REV_LIST_STR)
              string(REPLACE "\n" ";" GIT_REV_LIST ${GIT_REV_LIST_STR})
            else()
              string(REPLACE "\n" ";" GIT_REV_LIST "")
            endif()

            if(GIT_REV_LIST)
              list(LENGTH GIT_REV_LIST VIEWER_VERSION_REVISION)
            else(GIT_REV_LIST)
              set(VIEWER_VERSION_REVISION 99)
            endif(GIT_REV_LIST)
        endif (DEFINED ENV{revision})
        message("Building '${VIEWER_CHANNEL}' Version ${VIEWER_SHORT_VERSION}.${VIEWER_VERSION_REVISION}")
    else ( EXISTS ${VIEWER_VERSION_BASE_FILE} )
        message(SEND_ERROR "Cannot get viewer version from '${VIEWER_VERSION_BASE_FILE}'") 
    endif ( EXISTS ${VIEWER_VERSION_BASE_FILE} )

    if ("${VIEWER_VERSION_REVISION}" STREQUAL "")
      message("Ultimate fallback, revision was blank or not set: will use 0")
      set(VIEWER_VERSION_REVISION 0)
    endif ("${VIEWER_VERSION_REVISION}" STREQUAL "")

    set(VIEWER_CHANNEL_VERSION_DEFINES
        "LL_VIEWER_CHANNEL=\"${VIEWER_CHANNEL}\""
        "LL_VIEWER_CHANNEL_GRK=L\"${VIEWER_CHANNEL_GRK}\""
        "LL_VIEWER_VERSION_MAJOR=${VIEWER_VERSION_MAJOR}"
        "LL_VIEWER_VERSION_MINOR=${VIEWER_VERSION_MINOR}"
        "LL_VIEWER_VERSION_PATCH=${VIEWER_VERSION_PATCH}"
        "LL_VIEWER_VERSION_BUILD=${VIEWER_VERSION_REVISION}"
        )
endif (NOT DEFINED VIEWER_SHORT_VERSION)
