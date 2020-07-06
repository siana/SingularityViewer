# -*- cmake -*-
include(Prebuilt)

if (STANDALONE)
	include(FindHunSpell)
else (STANDALONE)
  use_prebuilt_binary(libhunspell)

  if (LINUX OR DARWIN)
    set(HUNSPELL_LIBRARY hunspell-1.3)
  else (LINUX OR DARWIN)
    set(HUNSPELL_LIBRARY libhunspell)
  endif (LINUX OR DARWIN)
  set(HUNSPELL_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include/hunspell)
  use_prebuilt_binary(dictionaries)
endif (STANDALONE)
