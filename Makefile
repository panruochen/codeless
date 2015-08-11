EXTRA_CFLAGS    := -ggdb #-O3
EXTRA_CFLAGS    += -Wall -Wno-unused-result #-Wno-format
DEFINES         :=
SRCS            := ./ ./support/
INCS            := $(SRCS)
TARGET_TYPE     := EXE
TARGET          := ycpp.exe
LIBS            := -lpthread
OBJ_DIR         := .objs
SRC_EXTS        := cpp
TARGET_DEPENDS  := precedence-matrix.h

##VERBOSE_COMMAND := y

include common_Makefile

SCRIPTS_DIR := scripts

precedence-matrix.h: $(SCRIPTS_DIR)/c_opr.bnf $(SCRIPTS_DIR)/ssymid.cfg ./$(SCRIPTS_DIR)/bnf_parser.sh
	@echo GEN $@; if test ! -x "$(word 3,$^)" ; then chmod +x "$(word 3,$^)"; fi; \
	$(word 3,$^) -m g1_oprmx -s g1_oprset -c $(word 1,$^) $(word 2,$^) >$@ || { rm -rf $@; exit 1; }

.PHONY: run
run: all
	@chmod +x run-demo
	./run-demo
