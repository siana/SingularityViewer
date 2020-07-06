# -*- cmake -*-

include(Prebuilt)
include(Boost)

set(COLLADADOM_FIND_QUIETLY OFF)
set(COLLADADOM_FIND_REQUIRED ON)

if (STANDALONE)
  include (FindColladadom)
else (STANDALONE)
  use_prebuilt_binary(colladadom)
  set(COLLADADOM_INCLUDE_DIRS
      ${LIBS_PREBUILT_DIR}/include/collada
      ${LIBS_PREBUILT_DIR}/include/collada/1.4
      )

  if (WINDOWS)
    set(COLLADADOM_LIBRARIES
        debug libcollada14dom23-sd
        optimized libcollada14dom23-s
        )
  else(WINDOWS)
    set(COLLADADOM_LIBRARIES
        debug collada14dom-d
        optimized collada14dom
        minizip
        )
  endif (WINDOWS)
endif (STANDALONE)

