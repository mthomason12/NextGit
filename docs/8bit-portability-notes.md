# 8-Bit Portability Notes

Observations about potential Spectrum Next / z88dk portability concerns.
Each entry is an observation, not a task item.
Do not redesign working code solely for 8-bit compatibility.

---

Portability Note #1

Location:
    Project-wide

Observation:
    Current Git executable is approximately 195 KB.

Concern:
    Code size does not currently appear to be a major Spectrum Next risk.

More important future measurements:

- output.ulx size
- Loaded VM memory size
- Dynamic heap usage
- Image cache size
- Save-file memory usage

Action:
    Measure memory consumption after Save/Restore is implemented.

Priority:
    Observation only.

---

## Phase 3A.1 — Fileref Lifecycle

### malloc in gli_new_fileref

- **Location:** `nextglk/nextglk_fileref.c`, `gli_new_fileref()`
- **Concern:** `malloc(sizeof(fileref_t))` — fileref_t is approximately 40 bytes (disprock + rock + usage + filename pointer + next/prev pointers). Each fileref created (typically one for save, one for transcript, one for restore) adds heap allocation.
- **Linux implementation:** Standard `malloc()` via glibc.
- **Likely Spectrum Next impact:** z88dk's `malloc` may have limited heap size and fragmentation risks. However, filerefs are small and few in number (typically 2–4 active at a time).
- **Possible future mitigation:** Pre-allocate a small pool of fileref_t structs at startup, or use a static array if the maximum number of concurrent filerefs is bounded (e.g., 4).

### strdup in gli_new_fileref

- **Location:** `nextglk/nextglk_fileref.c`, `gli_new_fileref()`
- **Concern:** `strdup(filename)` allocates heap memory for the filename copy. Filenames on the Spectrum Next are typically short (8.3 or similar), but still require dynamic allocation.
- **Linux implementation:** Standard `strdup()` via glibc.
- **Likely Spectrum Next impact:** z88dk's `strdup` may not be available or may have different behaviour. Additional heap allocation per fileref.
- **Possible future mitigation:** Store filenames in a fixed-size buffer within fileref_t (e.g., `char filename[64]`) instead of heap-allocating, or implement a custom `gli_strdup` that uses a pre-allocated pool.

### free in gli_delete_fileref

- **Location:** `nextglk/nextglk_fileref.c`, `gli_delete_fileref()`
- **Concern:** `free(fref->filename)` and `free(fref)` release heap memory.
- **Linux implementation:** Standard `free()` via glibc.
- **Likely Spectrum Next impact:** z88dk `free()` may not coalesce blocks efficiently, leading to heap fragmentation over many create/destroy cycles. This is unlikely to be a problem in practice since games typically create few filerefs.
- **Possible future mitigation:** Use a pool allocator or avoid freeing filerefs until shutdown.

### Doubly-linked list traversal for dispatch lookups

- **Location:** `nextglk/nextglk.c`, `gidispatch_get_objrock()` (fileref case)
- **Concern:** Linear search of `gli_filereflist` for each `gidispatch_get_objrock` call. The dispatch layer calls this function to map VM object IDs to C pointers during every Glk function call that passes a fileref.
- **Linux implementation:** O(n) linked-list traversal.
- **Likely Spectrum Next impact:** Negligible — fileref lists are very small.
- **Possible future mitigation:** None needed.

### strdup availability on z88dk

- **Location:** `nextglk/nextglk_fileref.c`, `gli_new_fileref()`
- **Concern:** `strdup` is a POSIX function, not part of C89/C99. z88dk may not provide it.
- **Linux implementation:** Standard glibc `strdup`.
- **Likely Spectrum Next impact:** May require a custom implementation: `malloc(strlen(s)+1)` followed by `strcpy`.
- **Possible future mitigation:** Implement `gli_strdup` in a platform layer that uses `malloc`/`strcpy` on z88dk.

### Pointer-size assumptions in fileref_t

- **Location:** `nextglk/nextglk_internal.h`, `struct glk_fileref_struct`
- **Concern:** `fileref_t` uses `next`/`prev` pointers and a `filename` pointer. On z88dk for Z80, pointers may be 2 or 3 bytes depending on memory model, not 8 bytes as on 64-bit Linux.
- **Linux implementation:** 64-bit pointers (8 bytes each).
- **Likely Spectrum Next impact:** Struct layout differs between platforms. This is expected and handled by the compiler — no action needed, but be aware when computing struct sizes for memory budgeting.
- **Possible future mitigation:** Use `sizeof(fileref_t)` for any size calculations; avoid hardcoding struct sizes.

### gidispatch_get_objrock linear search for filerefs

- **Location:** `nextglk/nextglk.c`, `gidispatch_get_objrock()`
- **Concern:** The previous implementation returned zeroed rock for all fileref lookups. The new implementation walks `gli_filereflist`. Both are O(n), but the new code actually works.
- **Linux implementation:** Linear search of doubly-linked list.
- **Likely Spectrum Next impact:** Same as stream list — negligible for small lists.
- **Possible future mitigation:** None needed.