# module/Makefile -- the VA2000 driver as an Amix loadable kernel module, one
# per kernel line (k30 = 68030, k46 = 68040/68060), built with the Amix ABI
# kit (GNU make).
#
#   make -C module            both lines: module/build/k30/mod.d/va2000 and
#                             module/build/k46/mod.d/va2000, each checked by
#                             the kit's modfix and modcheck.py
#   make -C module k30        one line (or k46)
#   make -C module clean
#
# DLM is the dlm directory of the amix repository (kernel/dlm; `make kit` there
# gives build/kit/k30 and build/kit/k46), by default an amix checkout beside
# this repository.  KIT_k30 / KIT_k46 name a kit directly.  The pinned cross
# tools (m68k-cbm-sysv4-*, gcc-cross-amix) and python3 must be on PATH.
#
# The driver source is the static one, src/va2000.c, compiled with
# -DVA2000_MODULE and the line's way of reaching the board:
#   k46  -DVA2000_KVA  registers mapped with dev_kvmap (noncacheable, serialised)
#   k30  -DVA2000_SPT  a Zorro III board (above 0x40000000) mapped with
#                      sptalloc; a Zorro II board reached directly
# The framebuffer's cache class is asked for in va2000_mod.c through the kit's
# DDI_FB_REGISTER (k46.1.6), on both lines and with no switch: a k46 kernel
# without it, the 68030 and a kit from before it all leave the default class.
#
# Copyright (c) 2026 Jussi Alanärä.  SPDX-License-Identifier: MIT

TOP := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))..)
DLM ?= $(abspath $(TOP)/../amix/kernel/dlm)
LINES = k30 k46
kit = $(or $(KIT_$(1)),$(DLM)/build/kit/$(1))

ifeq ($(KIT),)

all: $(LINES)

$(LINES):
	$(MAKE) KIT=$(call kit,$@)

clean: $(LINES:%=clean-%)

$(LINES:%=clean-%): clean-%:
	$(MAKE) KIT=$(call kit,$*) clean

.PHONY: all clean $(LINES) $(LINES:%=clean-%)

else	# one line, the kit's

MODULE = va2000
OBJS   = va2000.o va2000_mod.o
MODOUT = $(TOP)/module/build/$(LINE)
SRC    = $(TOP)/src
CFLAGS = -DVA2000_MODULE \
	 $(if $(filter k46,$(LINE)),-DVA2000_KVA,-DVA2000_SPT)
vpath %.c $(SRC)
include $(KIT)/mk/module.mk
$(MODOUT)/obj/va2000.o: $(SRC)/va2000.h

endif
