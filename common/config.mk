CFLAGS          := $(COMMON_CFLAGS) -ggdb #-O3
CFLAGS          += -Wall -Wno-unused-result #-Wno-format
DEFINES         := -DSANITY_CHECK
SRCS            := ./common/
INCS            := ./common/
TARGET          := lib/libcommon.a
OBJ_DIR         := .objs-common
SRC_EXTS        := cpp c cc
EXCLUDE_FILES   := common/got_error.cpp

$(call BUILD_STATIC_LIBRARY)
$(call CLEAN_VARS)

