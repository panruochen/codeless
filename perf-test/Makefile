EXTRA_CFLAGS    := -ggdb -O3
EXTRA_CFLAGS    += -Wall -Wno-unused-result #-Wno-format
DEFINES         := -DHAVE_MD5
CL_DIR          := ../cl
SRCS            := ../common/ ./main.cpp
SRCS            += $(CL_DIR)/DsFileWriter.cpp $(CL_DIR)/FileWriter.cpp $(CL_DIR)/utils.cpp $(CL_DIR)/GlobalVars.cpp $(CL_DIR)/support/cc_string.cpp
EXCLUDE_FILES   := $(CL_DIR)/startup.cpp
INCS            := $(CL_DIR)/ $(CL_DIR)/support ../common
TARGET_TYPE     := EXE
TARGET          := ../cl-lite.exe
LIBS            := -lpthread
OBJ_DIR         := .objs
SRC_EXTS        := cpp c

VERBOSE_COMMANDS := y

include ../tools/common_Makefile


