WD              := server
EXTRA           := $(COMMON_CFLAGS) -ggdb -O3
EXTRA           += -Wall ##-Wno-unused-result #-Wno-format
DEFINES         := -DSANITY_CHECK
SRCS            := $(WD)/
INCS            := $(SRCS) ./common/
TARGET          := bin/cl-server.exe
LIBS            := -lpthread
OBJ_DIR         := .objs-server
SRC_EXTS        := cpp c
#MY_ASM_EXTS     := asm
EXCLUDE_FILES   := common/got_error.cpp
DEPENDENT_LIBS  := lib/libcommon.a
#OVERRIDE_CMD_AS := tools/nasm -f coff -o $$@ $$<
OVERRIDE_CMD_AS := tools/ml -nologo -c -Fo$$@ $$<
CFLAGS_server/cl-server.cpp := -DMOON

$(call BUILD_EXECUTABLE)
$(call CLEAN_VARS)
