/*
 * va2000_mod.c - the MNT VA2000 driver as an Amix loadable kernel module
 *
 * Copyright (c) 2026 Jussi Alanärä
 * SPDX-License-Identifier: MIT
 *
 * The driver itself is ../src/va2000.c, compiled with -DVA2000_MODULE (see
 * module/Makefile).  This file is only what turns it into a module: the
 * character switch row for major 68, which the module core installs from the
 * module's .moddata (majors=c:68) instead of a cdevsw[] edit, the load and
 * unload routines, and the framebuffer's cache class.  The module is of the
 * demand class: the core loads it at the first open of /dev/va2000.
 */

#include "sys/types.h"
#include "sys/errno.h"
#include "sys/conf.h"
#include "sys/moddefs.h"
#include "sys/amixddi.h"

/*
 * The framebuffer's cache class comes from DDI_FB_REGISTER in the kit's
 * <sys/amixddi.h> (k46.1.6; contract C3-6 section 5): on k46 a weak
 * reference to fb_register, so the module loads on a kernel without it and
 * gets ENOSYS there; on k30 ENOSYS with no reference.  A kit from before
 * k46.1.6 has no such macro: the module then builds without it, and the
 * framebuffer keeps the kernel's default class.
 */
#ifndef DDI_FB_REGISTER
#define VA2000_NO_FBREG
#define DDI_FB_REGISTER_DECL        extern int va2000_fbnone
#define DDI_FB_REGISTER(pa, len)    ENOSYS
#endif
DDI_FB_REGISTER_DECL;

/* src/va2000.c: the framebuffer starts 64 KiB into the board's aperture, in
 * both bus modes; the register page below it stays serialised. */
#define VA2000_FB_OFFSET 0x00010000UL

extern int va2000open(), va2000close(), va2000read(), va2000write();
extern int va2000ioctl(), va2000mmap();
extern void va2000init();
extern void va2000_unmap_all();
extern long va2000_boards[];
extern long va2000_size[];

static int va2000_load();
static int va2000_unload();
static void va2000_fb_class();

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
    if (!va2000_boards[0]) {
        va2000_unmap_all();
        return ENXIO;
    }
    va2000_fb_class(0);
    return 0;
}

/*
 * va2000_fb_class() -- ask for the framebuffer's fast cache class, from
 * _load only, which is before anything can open or map the device (the
 * kernel refuses the call anywhere else).  This is where the static k46
 * form calls hat_cm_fb_add (va2000_map_regs in src/va2000.c), and for the
 * same window: the framebuffer, from VA2000_FB_OFFSET to the end of the
 * aperture, never the register page.  A board's AutoConfig address and size
 * are multiples of 64 KiB, so both ends are page-aligned on either line.
 *
 * Granted, the framebuffer's pages are mapped noncacheable but not
 * serialised.  Refused, they keep the kernel's default class, noncacheable
 * serialised: correct, only slower, and the driver works on.  A second
 * board is found at its first open, after _load, so it keeps the default
 * class.  One console line from the driver either way; the kernel prints its
 * own for a window it installs and for each refusal.
 */
static void
va2000_fb_class(dev)
int dev;
{
    int e;

    e = DDI_FB_REGISTER((unsigned long)va2000_boards[dev] + VA2000_FB_OFFSET,
                        (unsigned long)va2000_size[dev] - VA2000_FB_OFFSET);
    if (e == 0)
        printf("va2000: framebuffer cache class fast (noncacheable, not serialised)\n");
    else if (e == ENOSYS)
        printf("va2000: framebuffer cache class default (%s)\n",
#ifdef VA2000_NO_FBREG
               "module built without fb_register");
#else
#ifdef VA2000_KVA
               "this kernel has no fb_register");
#else
               "the 68030 has no other class");
#endif
#endif
    else    /* the kernel's own line before this one names the reason */
        printf("va2000: framebuffer cache class default (refused, error %d)\n", e);
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
