# -*- cmake -*-
if (WINDOWS)
   #message(WARNING, ${CMAKE_CURRENT_BINARY_DIR}/newview/viewerRes.rc.in)
   configure_file(
       ${CMAKE_SOURCE_DIR}/newview/res/viewerRes.rc.in
       ${CMAKE_CURRENT_BINARY_DIR}/viewerRes.rc
   )
endif (WINDOWS)

if (DARWIN)
   configure_file(
       ${CMAKE_SOURCE_DIR}/newview/English.lproj/InfoPlist.strings.in
       ${CMAKE_CURRENT_BINARY_DIR}/English.lproj/InfoPlist.strings
   )
endif (DARWIN)

if (LINUX)
   configure_file(
       ${CMAKE_SOURCE_DIR}/newview/linux_tools/wrapper.sh.in
       ${CMAKE_CURRENT_BINARY_DIR}/linux_tools/wrapper.sh
       @ONLY
   )
   configure_file(
       ${CMAKE_SOURCE_DIR}/newview/linux_tools/handle_secondlifeprotocol.sh.in
       ${CMAKE_CURRENT_BINARY_DIR}/linux_tools/handle_secondlifeprotocol.sh
       @ONLY
   )
   configure_file(
       ${CMAKE_SOURCE_DIR}/newview/linux_tools/install.sh.in
       ${CMAKE_CURRENT_BINARY_DIR}/linux_tools/install.sh
       @ONLY
   )
   configure_file(
       ${CMAKE_SOURCE_DIR}/newview/linux_tools/refresh_desktop_app_entry.sh.in
       ${CMAKE_CURRENT_BINARY_DIR}/linux_tools/refresh_desktop_app_entry.sh
       @ONLY
   )
endif (LINUX)
