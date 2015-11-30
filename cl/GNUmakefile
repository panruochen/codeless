EXTRA_CFLAGS    := $(COMMON_CFLAGS) -ggdb #-O3
EXTRA_CFLAGS    += -Wall -Wno-unused-result #-Wno-format
DEFINES         := -DSANITY_CHECK
SRCS            := ./ ./support/ ../common/
INCS            := $(SRCS)
TARGET_TYPE     := EXE
TARGET          := ../cl.exe
LIBS            := -lpthread
OBJ_DIR         := .objs
SRC_EXTS        := cpp c
TARGET_DEPENDS  := precedence-matrix.h help.h
EXTERNAL_DEPENDS:= ../Makefile

#VERBOSE := y

include ../tools/common_Makefile

SCRIPTS_DIR := scripts

precedence-matrix.h: $(SCRIPTS_DIR)/c_opr.bnf $(SCRIPTS_DIR)/ssymid.cfg ./$(SCRIPTS_DIR)/bnf_parser.sh
	@echo GEN $@; if test ! -x "$(word 3,$^)" ; then chmod +x "$(word 3,$^)"; fi; \
	$(word 3,$^) -m g1_oprmx -s g1_oprset -c $(word 1,$^) $(word 2,$^) >$@ || { rm -rf $@; exit 1; }

help.h: help.txt
	@ { echo '#ifndef __HELP_H'; \
		echo '#define __HELP_H'; \
		echo 'static const char help_msg[] ='; \
		sed -e 's/^/"/' -e 's/$$/\\n"/' $<; \
		echo ';\n#endif'; } > $@


