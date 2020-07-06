# -*- cmake -*-

include(Variables)
include(Prebuilt)

if (NOT (STANDALONE OR DARWIN))
  use_prebuilt_binary(glext)
  set(GLEXT_INCLUDE_DIR
    ${LIBS_PREBUILT_DIR}/include
    ${LIBS_PREBUILT_LEGACY_DIR}/include
    )
endif (NOT (STANDALONE OR DARWIN))

include(FindOpenGL)
