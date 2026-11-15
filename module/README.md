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
| `va2000_mod.c` | the module glue: the `cdevsw` row for character major 68, `_load` (what `va2000init()` does in the static kernel) and `_unload` |
| `Master` | the `.moddata` keys and the definition line, as the kit's `mkmod` reads them |
| `Makefile` | builds both lines on the kit's `mk/module.mk`, compiling `../src/va2000.c` with the line's defines |
| `man/va2000.7` | the manual page for the device |

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
the core does not keep the module. A second board (minor 1) is found at its
first open, as in the static form. `_unload` releases the register mappings;
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

**The framebuffer cache class.** The port's static kernel registers the
framebuffer with `hat_cm_fb_add`, so that its pages are noncacheable but not
serialised, which is faster. That function is not an interface a module may
import, so the module is built without `VA2000_FB_CM` and the framebuffer
keeps the kernel's default class: correct, slower. When the module interface
exists, the one call in `va2000_map_regs` changes to use it and the k46 module
is built with `make FB_CM=1`; until then the checker refuses such a build
(`modfix: undefined: hat_cm_fb_add`).

## Testing

The card cannot be emulated, so a host build and the kit's checker are the only
checks this tree can make. None of the module form has run on hardware: not
the k46 module, not the k30 module, and in particular not the k30 Zorro III
path through `sptalloc`. The static form's hardware record is in the
top-level README.
