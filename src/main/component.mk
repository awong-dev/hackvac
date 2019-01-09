#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)
CXXFLAGS += -std=gnu++17
COMPONENT_EMBED_TXTFILES := static/index.html static/resp404.html
COMPONENT_SRCDIRS := .

# TODO(awong): Incorrect
ifdef GTEST_MAIN
COMPONENT_SRCDIRS += ./test
endif
