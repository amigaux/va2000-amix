/*
 * va2000_mod.c - the MNT VA2000 driver as an Amix loadable kernel module
 *
 * Copyright (c) 2026 Jussi Alanärä
 * SPDX-License-Identifier: MIT
 *
 * The driver itself is ../src/va2000.c, compiled with -DVA2000_MODULE (see
 * module/Makefile).  This file is only what turns it into a module: the
 * character switch row for major 68, which the module core installs from the
 * module's .moddata (majors=c:68) instead of a cdevsw[] edit, and the load and
 * unload routines.  The module is of the demand class: the core loads it at
 * the first open of /dev/va2000.
 */

#include "sys/types.h"
#include "sys/errno.h"
#include "sys/conf.h"
#include "sys/moddefs.h"
#include "sys/amixddi.h"

extern int va2000open(), va2000close(), va2000read(), va2000write();
extern int va2000ioctl(), va2000mmap();
extern void va2000init();
extern void va2000_unmap_all();
extern long va2000_boards[];

static int va2000_load();
static int va2000_unload();

struct mod_drv_data va2000_drvdata[] = {
    { { 0 }, 0, 0,
      { va2000open, va2000close, va2000read, va2000write, va2000ioctl, va2000mmap },
      68, 1 }
};

/* No halt routine: the board neither interrupts nor writes memory. */
MOD_DRV_WRAPPER(va2000, va2000_load, va2000_unload, 0, "MNT VA2000 RTG framebuffer");

/*
 * The static kernel calls va2000init() from its init table at boot; the module
 * calls it here, at its first open.  It finds the first board, maps its
 * registers and checks the firmware.  No board, or one that does not answer:
 * ENXIO, with any mapping released, so the core does not keep the module.
 * The module takes no parameters.
 */
static int
va2000_load(params)
char *params;
{
    va2000init();
    if (va2000_boards[0])
        return 0;
    va2000_unmap_all();
    return ENXIO;
}

/*
 * unload=never: moduload refuses this module, because the driver keeps its
 * register mappings for as long as the kernel runs.  The core still calls this
 * when a load fails after va2000_load() returned 0; nothing has opened the
 * device then, so the mappings can go.
 */
static int
va2000_unload()
{
    va2000_unmap_all();
    return 0;
}
