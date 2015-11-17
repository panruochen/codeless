EXTRA_CFLAGS    := $(COMMON_CFLAGS) -ggdb -O3
EXTRA_CFLAGS    += -Wall ##-Wno-unused-result #-Wno-format
DEFINES         := -DSANITY_CHECK
SRCS            := ./ ../common/
INCS            := $(SRCS)
TARGET_TYPE     := EXE
TARGET          := ../cl-server.exe
LIBS            := -lpthread
OBJ_DIR         := .objs
SRC_EXTS        := cpp c
EXTERNAL_DEPENDS:= ../Makefile

include ../tools/common_Makefile

