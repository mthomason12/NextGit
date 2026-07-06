# 8-Bit Portability Notes

Observations on platform differences and memory constraints when
building NextGlk for ZX Spectrum Next.

---

## Buffer Reductions

**File:** `cheapglk.h`

On Spectrum Next, `GLI_BUF_LEN` is reduced from 256 to 128 bytes
and `GLI_TEMP_LEN` from 256 to 64 bytes. These are used for:

- `cgfref.c`: filename buffers, temp file path generation, prompt handling
- `cgmisc.c`: line input buffer, UTF-8 decode scratch buffer
- `main.c`: working directory storage

**Portability concern:** The Spectrum Next has ~1.7 MB total RAM.
Every static buffer costs real memory. 256-byte buffers are wasteful
for a machine whose entire screen is 256x192 pixels. 128 bytes is
more than sufficient for filenames and input on an 8-bit system.

**Suggested future mitigation:** Consider dynamically sizing buffers
based on actual hardware. Could use `#pragma bank` or similar z88dk
directives to move less-critical buffers to specific memory banks.

---

## File Operations Without POSIX

**File:** `cgfref.c`

The Spectrum Next build uses:

| Operation | Linux | Spectrum Next |
|-----------|-------|---------------|
| File exists | `stat()` | `fopen("rb")` then `fclose()` |
| File delete | `unlink()` | `remove()` |
| Temp file | `mkstemp()` | Counter-based name generator |

**Portability concern:** `unlink()` and `stat()` are POSIX, not C89.
z88dk may not provide them on all targets. `remove()` is standard C89
and available everywhere. The temp file counter is a pragmatic
alternative to `mkstemp()`.

**Suggested future mitigation:** When a real NextZXOS platform layer
exists, these wrappers can be replaced with native ESXDOS calls for
better performance.

---

## Date/Time Functions

**File:** `cgdate.c`

On Spectrum Next, we wrap all time functions through fallback
implementations:

- `timespec_get` → approximated from `time()` with nsec=0
- `gmtime_r` → falls back to non-reentrant `gmtime()` if unavailable
- `localtime_r` → falls back to non-reentrant `localtime()` if unavailable
- `timegm` → uses `mktime()` (no timezone on Spectrum Next)

If no hardware clock is present, `time()` returns -1 and we
default to the Unix epoch.

**Portability concern:** z88dk's C library may not provide the
full POSIX time API. Re-entrant `_r` variants and `timespec_get`
are particularly questionable.

**Suggested future mitigation:** If the Spectrum Next acquires a
real-time clock add-on, implement a proper clock driver.

---

## Debug Console Disabled

**Files:** `gi_debug.h`, `cheapglk.h`, `main.c`, `cgmisc.c`

The debug console (`-D` flag, `gidebug_output`, `gidebug_pause`,
`perform_debug_command`) is entirely compiled out on Spectrum Next
via `#ifndef __SPECTRUM_NEXT__`.

**Portability concern:** The Spectrum Next has no separate debug
console window concept. The debug code uses `fgets`/`printf` on
stdin/stdout which would interfere with game I/O. It also adds
unnecessary code size.

**Suggested future mitigation:** Could implement a simple in-band
debug command prefix (like CheapGlk's `/`) that works within the
existing line input path, without requiring separate debug infrastructure.

---

## Unicode Tables

**Files:** `cgunigen.c`, `cgunicod.c`

The full Unicode case-mapping and normalization tables (`cgunigen.c`)
are approximately 3000+ lines of static array data — roughly 100KB+
of compiled data. On the Linux build, these are included in full.

On Spectrum Next, `cgunicod.c` has the infrastructure to use a
trimmed-down table, but currently both platforms include the same
file. The conditional is in place:

```c
#ifdef __SPECTRUM_NEXT__
#include "cgunigen.c"  /* trimmed version */
#else
#include "cgunigen.c"  /* full version */
#endif
```

**Portability concern:** The full Unicode tables exceed the practical
memory budget for Spectrum Next. A Spectrum-specific `cgunigen.c`
should contain only the ASCII range (0x00-0x7F) plus any Latin-1
extensions needed for accented characters that can be mapped into
Spectrum character positions 128-255.

**Suggested future mitigation:** Generate a Spectrum-specific
`cgunigen.c` that:
1. Provides case tables for ASCII A-Z/a-z only
2. Maps accented Latin-1 characters (0xC0-0xFF) to Spectrum
   character positions 128-255 (replacing graphical characters)
3. Returns identity for all other code points
4. Provides empty decomposition/composition tables

---

## Memory Allocation

**Files:** All files using `malloc`/`free`

The code uses standard `malloc`/`free` throughout for:
- Window structures
- Stream structures
- Fileref structures and filename strings
- Unicode case-change temporary buffers
- Resource data loading

**Portability concern:** z88dk's `malloc` implementation may have
limitations. Heap fragmentation on an 8-bit system with only ~1.7 MB
is a real concern for long-running games. The code creates and
destroys objects during normal operation (streams, filerefs).

**Suggested future mitigation:**
- Consider a pool allocator for frequently-allocated structures
  (streams, filerefs)
- Pre-allocate window and main stream at startup
- Avoid malloc in the hot path (character output, input handling)

---

## Stack Usage

**Files:** Several files have moderate stack allocations

- `cgmisc.c`: `char buf[GLI_BUF_LEN]` on stack in `glk_select()`
- `cgfref.c`: `char buf[GLI_BUF_LEN]`, `char newbuf[2*GLI_BUF_LEN+10]`
- `cgstream.c`: various local buffers
- `cgdate.c`: `struct tm` on stack

**Portability concern:** z88dk's default stack size may be limited.
Large stack allocations could overflow into heap or other memory
regions.

**Suggested future mitigation:**
- Move large buffers to static/global storage
- Use `#pragma` to set stack size if z88dk supports it
- Audit all stack allocations before Spectrum Next deployment

---

## 32-Bit Integer Assumptions

**Files:** `cgdate.c`, `glk.h`

The code assumes `time_t` is at least 32 bits and uses `int64_t`
casts for handling >32-bit timestamps. `glui32` is defined as
`uint32_t`.

**Portability concern:** z88dk's `time_t` may be 32-bit and may
not support `int64_t` on all targets. The Z80 is an 8-bit CPU
with 16-bit address space; 64-bit arithmetic requires software
emulation.

**Suggested future mitigation:** The `int64_t` usage is guarded by
`sizeof(timestamp) > 4` checks so it should never execute on 32-bit
systems. Verify this holds on z88dk.

---

## File Seek Assumptions

**Files:** `cgstream.c`

The code uses `fseek` with `SEEK_SET`, `SEEK_CUR`, `SEEK_END` and
`ftell`. It also uses the trick of `fopen()` with `"ab"` followed
by `fclose()` to pre-create files for `ReadWrite` and `WriteAppend`
modes.

**Portability concern:** z88dk's stdio implementation on NextZXOS
may not support all seek modes on all file types. The `"ab"` +
`fclose()` trick may behave differently.

**Suggested future mitigation:** Test file I/O thoroughly on real
hardware. May need to replace some stdio patterns with direct
ESXDOS calls.

---

## Path Handling

**Files:** `cgfref.c`, `main.c`

The code assumes Unix-style path separators ("/"), a working
directory concept, and a `/tmp` directory for temporary files.

**Portability concern:** NextZXOS uses different path conventions.
There may be no `/tmp` directory.

**Suggested future mitigation:**
- Define platform-specific path separators
- Use current directory (".") for temp files if `/tmp` is unavailable
- Truncate paths to fit Spectrum Next filesystem limits