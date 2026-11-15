# Changelog

Changes in the amigaux fork, newest first. The upstream history is in git.

## 2026-11-15 — loadable module form for both kernel lines

A loadable module form of the driver in `module/`, for the 68030 and the 68040/060 kernel lines: character major 68, loaded on demand, never unloaded, with a va2000(7) man page. In `src/va2000.c` the mmap page shift now comes from the kernel's `PNUMSHFT`; with `VA2000_SPT` a Zorro III board on a 68030 is reached through a kernel mapping while a Zorro II board keeps the direct path; the framebuffer cache-class call sits behind `VA2000_FB_CM`, on by default in the static `VA2000_KVA` build and off in the module. The static object is unchanged. Not yet run on hardware.
