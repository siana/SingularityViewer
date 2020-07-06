# -*- cmake -*-
include(Prebuilt)

set(GLOD_FIND_QUIETLY OFF)
set(GLOD_FIND_REQUIRED ON)

if (STANDALONE)
  include(FindGLOD)
else (STANDALONE)
  use_prebuilt_binary(glod)

  set(GLOD_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include/glod)
  set(GLOD_LIBRARIES GLOD)
endif (STANDALONE)
