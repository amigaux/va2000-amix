# /dev/va2000 as a loadable module

The same driver as the static form (`src/va2000.c`, linked into a kernel by
`install-va2000-driver.sh` or by the 68040/68060 port's relink script), built
as a loadable kernel module for the Amix module system: the ABI kit in
[amix](https://github.com/amigaux/amix) under `kernel/dlm`, contracts C3-1,
C3-2 and C3-6 in the kit's `docs/contracts/`. One module per kernel line:
`k30` (68030, 2 KiB pages) and `k46` (68040/68060, 4 KiB pages), both from the
one source.

The static form is unchanged: compiled without the defines below, `src/va2000.c`
gives the same object as before the module form existed.

**What the module does not do.** The static install also patches the console
(`KERNEL_CHANGES.md`: `scrdev.c`, `c0.c`, `screen.c`) so that the screen
manager knows the card. Those are changes to kernel source, and a module cannot
carry them. The module serves programs that open `/dev/va2000` directly and
drive the card through `ioctl` and `mmap`: the X server (`Xrtg`), the tools in
`tools/`, games that map the framebuffer.

## Layout

| Path | Content |
|---|---|
| `va2000_mod.c` | the module glue: the `cdevsw` row for character major 68, `_load` (what `va2000init()` does in the static kernel, then the framebuffer's cache class) and `_unload` |
| `Master` | the `.moddata` keys and the definition line, as the kit's `mkmod` reads them |
| `Makefile` | builds both lines on the kit's `mk/module.mk`, compiling `../src/va2000.c` with the line's defines |
| `man/va2000.7` | the manual page for the device |
| `VERSION` | the package version, `x.y.z-rev`, the same on both lines |

## Building

You need the pinned gcc-cross-amix (`m68k-cbm-sysv4-*`) on `PATH`, `python3`,
and the built kits of an amix checkout (`make kit` in its `kernel/dlm`).

```sh
make -C module            # both lines: module/build/k30/mod.d/va2000, module/build/k46/mod.d/va2000
make -C module k46        # one line
make -C module clean
```

`DLM` names `kernel/dlm` of the amix checkout (default `../amix/kernel/dlm`,
amix cloned beside this repository); `KIT_k30` and `KIT_k46` name kits
directly. Each build ends with the kit's `modfix` and `modcheck.py`; a module
the checker refuses is not left in `mod.d`.

Character major 68 must be registered to `va2000` in the kit's
`registry/majors`; a kit without that row refuses the module
(`major c:68 is not in the registry`), and so does a kernel whose module core
was built without it.

## The module's keys

| Key | Value | Why |
|---|---|---|
| `class` | `demand` | the core loads it at the first open of `/dev/va2000` |
| `majors` | `c:68` | the driver's major, the same as the static form's |
| `unload` | `never` | the driver keeps its register mappings for as long as the kernel runs (`va2000_map_regs` in `src/va2000.c`) |
| `cpu` | from the kit | 68030 on k30, 68040 on k46 |

There are no parameters and no halt routine: the board neither interrupts nor
writes memory.

`_load` runs `va2000init()`: it finds the first board with `autocon()`, maps
its register window, and checks that the firmware answers. With no board, or
one that does not answer, it releases what it mapped and returns `ENXIO`, and
the core does not keep the module. Otherwise it asks for the framebuffer's
cache class (below). A second board (minor 1) is found at its first open, as
in the static form. `_unload` releases the register mappings;
because of `unload=never` the core calls it only after a load that failed
once `_load` had returned 0.

## How each line reaches the board

The defines are the ones `src/va2000.c` documents in its header.

| Line | Defines | Registers | Framebuffer |
|---|---|---|---|
| k46 | `VA2000_MODULE VA2000_KVA` | `dev_kvmap`, noncacheable serialised, Zorro II and Zorro III alike | user `mmap` of its physical pages; `read`/`write` through a temporary `dev_kvmap` window |
| k30, board below `0x40000000` (Zorro II) | `VA2000_MODULE VA2000_SPT` | the AutoConfig address, as the static form | user `mmap`; `read`/`write` directly |
| k30, board at or above `0x40000000` (Zorro III) | the same | `sptalloc`, two 2 KiB pages | user `mmap`; `read`/`write` through a temporary `sptalloc` window |

On k30 the choice is made at run time from the board's address, so one object
serves both bus modes. `mmap` returns page frames with the kernel's own page
shift (`PNUMSHFT`: 11 on k30, 12 on k46).

## The framebuffer cache class

On a 68040/68060 kernel the pages a driver's `mmap` hands out are mapped
noncacheable and serialised, so every store to the framebuffer waits for the
one before. A framebuffer needs no serialisation: in the noncacheable, not
serialised class its stores drain through the 68060's store buffer, which is
faster. The static k46 form registers the framebuffer with the port's
`hat_cm_fb_add` (`va2000_map_regs` in `src/va2000.c`, `VA2000_FB_CM`). A
module may not import that function; it asks the module core instead, with
`fb_register` (DDI k46.1.6, contract C3-6 section 5), through the kit's
`DDI_FB_REGISTER` macro.

`_load` asks for the framebuffer window only, from the board's base +
`0x10000` to the end of the aperture; the register page stays serialised.
The kernel accepts the call only from `_load`, which runs before anything can
open or map the device. One module binary per line serves every kernel of it:

| Kernel | Framebuffer class | Console line at load |
|---|---|---|
| k46 that offers `fb_register` (k46.1.6 and later) | fast | `va2000: framebuffer cache class fast (noncacheable, not serialised)` |
| k46 before k46.1.6, such as the 1.1 kernels | default | `va2000: framebuffer cache class default (this kernel has no fb_register)` |
| k46, window refused (`EBUSY`: held by another driver; `ENOSPC`: both kernel slots taken; `EINVAL`) | default | `va2000: framebuffer cache class default (refused, error N)`, after the kernel's own line naming the reason |
| k30 | default, the only one: the 68030 never caches a device page and buffers no store | `va2000: framebuffer cache class default (the 68030 has no other class)` |

A refusal is never fatal: the framebuffer keeps the default class, which is
correct, only slower, and the driver works on. A second board is found at its
first open, after `_load`, so it keeps the default class. The class is kept
for the rest of the boot; nothing is released.

On k46 the macro makes `fb_register` a weak reference, so the module keeps the
kabi tag its other names give it (`k46.1.2`) and loads on the 1.1 kernels; on
k30 it is no reference at all. Built with a kit from before k46.1.6 (no
`DDI_FB_REGISTER` in its `<sys/amixddi.h>`), the module builds without the
call and says `default (module built without fb_register)`.

## Testing

The card cannot be emulated, so a host build and the kit's checker are the only
checks this tree can make. None of the module form has run on hardware: not
the k46 module, not the k30 module, not the fast framebuffer class through
`fb_register`, and in particular not the k30 Zorro III path through
`sptalloc`. The static form's hardware record is in the
top-level README.
