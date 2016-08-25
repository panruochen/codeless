WD              := server
EXTRA_CFLAGS    := $(COMMON_CFLAGS) -ggdb -O3
EXTRA_CFLAGS    += -Wall ##-Wno-unused-result #-Wno-format
DEFINES         := -DSANITY_CHECK
SRCS            := $(WD)/ ./common/
INCS            := $(SRCS)
TARGET_TYPE     := EXE
TARGET          := cl-server.exe
LIBS            := -lpthread
OBJ_DIR         := .objs-server
SRC_EXTS        := cpp c
EXCLUDE_FILES   := common/got_error.cpp

$(call BUILD_EXECUTABLE)
$(call CLEAN_VARS)
