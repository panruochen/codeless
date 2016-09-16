WD              := server
EXTRA           := $(COMMON_CFLAGS) -ggdb -O3
EXTRA           += -Wall ##-Wno-unused-result #-Wno-format
DEFINES         := -DSANITY_CHECK
SRCS            := $(WD)/
INCS            := $(SRCS) ./common/
TARGET          := cl-server.exe
LIBS            := -lpthread
OBJ_DIR         := .objs-server
SRC_EXTS        := cpp c
#MY_ASM_EXTS     := asm
EXCLUDE_FILES   := common/got_error.cpp
DEPENDENT_LIBS  := lib/libcommon.a

$(call BUILD_EXECUTABLE)
$(call CLEAN_VARS)
