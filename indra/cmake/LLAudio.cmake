# -*- cmake -*-

include(Audio)
include(OPENAL)

set(LLAUDIO_INCLUDE_DIRS
    ${LIBS_OPEN_DIR}/llaudio
    )

#add_definitions(-DOV_EXCLUDE_STATIC_CALLBACKS)
	
set(LLAUDIO_LIBRARIES llaudio ${OPENAL_LIBRARIES})
