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

---

## Phase 3A.2 — Fileref Creation Functions

### mkstemp in glk_fileref_create_temp

- **Location:** `nextglk/nextglk_fileref.c`, `glk_fileref_create_temp()`
- **Concern:** `mkstemp()` is a POSIX function that creates a uniquely-named temporary file. It internally modifies the template string in place (replacing `XXXXXX` with random characters) and returns an open file descriptor.
- **Linux implementation:** Standard glibc `mkstemp()` with template `"nextgit-XXXXXX"`. The fd is immediately closed via `close()` — the file exists but is empty.
- **Likely Spectrum Next impact:** z88dk may not provide `mkstemp()`. NextZXOS file I/O APIs may not support atomic temporary file creation. The template string modification in place (required by POSIX `mkstemp`) requires the template to be in writable memory (not a string literal).
- **Possible future mitigation:** Implement a simple platform-specific temp file generator using a counter or random suffix, e.g. `sprintf(tempname, "nextgit-%04x.tmp", rand())`, and create the file with `fopen(..., "w")`. Alternatively, use a fixed temp filename if only one temp file is needed at a time.

### malloc in glk_fileref_create_by_name for suffix construction

- **Location:** `nextglk/nextglk_fileref.c`, `glk_fileref_create_by_name()`
- **Concern:** Allocates a temporary buffer (`fullname`) via `malloc()` or `strdup()` to hold the name+suffix combination, then passes it to `gli_new_fileref()` (which calls `strdup()` internally), then frees the temporary buffer. This means the filename string is allocated twice during creation (once for the temp name, once for the owned copy in the fileref).
- **Linux implementation:** `malloc()` for the combined name, then `strdup()` inside `gli_new_fileref()`, then `free()` of the temporary.
- **Likely Spectrum Next impact:** Double allocation is wasteful on a constrained heap but unlikely to be a real problem since fileref creation is infrequent. The temporary buffer is freed immediately, so no long-term memory is lost.
- **Possible future mitigation:** Combine into a single allocation by calling `gli_new_fileref()` with the temp name and removing the internal `strdup()` — but this would change the ownership contract of `gli_new_fileref()`. Prefer to keep the current clean ownership model unless profiling shows a real problem.

### strdup in glk_fileref_create_by_name for known-extension path

- **Location:** `nextglk/nextglk_fileref.c`, `glk_fileref_create_by_name()`
- **Concern:** When the game-supplied name already has a recognised extension, `strdup(name)` is called to create a temporary copy, which is then passed to `gli_new_fileref()` (which calls `strdup()` again). This is a double allocation for the same string.
- **Linux implementation:** `strdup()` for the temp copy.
- **Likely Spectrum Next impact:** Same as above — wasteful but infrequent.
- **Possible future mitigation:** Same as above — restructure ownership to avoid double allocation, or accept the minor overhead.

### mkstemp template string is string literal passed to strdup

- **Location:** `nextglk/nextglk_fileref.c`, `glk_fileref_create_temp()`
- **Concern:** The template `"nextgit-XXXXXX"` is passed to `strdup()` before being passed to `mkstemp()`. This is correct — `mkstemp()` requires a writable buffer and modifies it in place. The `strdup()` call ensures the writable copy is on the heap.
- **Linux implementation:** `strdup()` of the template literal, then `mkstemp()` modifies the heap copy.
- **Likely Spectrum Next impact:** No additional concern beyond existing `strdup`/`malloc`/`free` concerns already documented. The heap-allocated copy is freed after use.
- **Possible future mitigation:** None needed — the current pattern is correct and portable.

### Fixed filenames in glk_fileref_create_by_prompt

- **Location:** `nextglk/nextglk_fileref.c`, `glk_fileref_create_by_prompt()`
- **Concern:** Uses hardcoded fixed filenames (`"nextgit.sav"`, `"transcript.txt"`, `"input.rec"`, `"data.glkdata"`) as a substitute for interactive prompting (which is not possible on the ZX Spectrum Next). These filenames are short and deterministic.
- **Linux implementation:** Returns a string literal pointer cast to `char*` to `gli_new_fileref()`, which calls `strdup()` internally to make an owned copy.
- **Likely Spectrum Next impact:** Filenames are short (max 15 characters) and use only ASCII characters — well within 8.3 filename constraints if needed. No path separators are included, so files will be created in the current working directory. On the Next, this will be the SD card directory from which the game was launched. A future enhancement could iterate through numbered save slots.
- **Possible future mitigation:** When save/restore is fully working, consider implementing a save-slot system (e.g., `save00.glksave` through `save99.glksave`) to allow multiple save games.

### close() after mkstemp in glk_fileref_create_temp

- **Location:** `nextglk/nextglk_fileref.c`, `glk_fileref_create_temp()`
- **Concern:** `close(fd)` is called immediately after `mkstemp()` to close the file descriptor. The file remains on disk (empty). This is intentional — the spec says temp files should exist as empty placeholders.
- **Linux implementation:** Standard POSIX `close()`.
- **Likely Spectrum Next impact:** `close()` is a POSIX function. z88dk may provide it for file descriptors, but the NextZXOS file API may use a different mechanism. The fd-based approach (`mkstemp` returning an fd) may not map cleanly to NextZXOS.
- **Possible future mitigation:** When porting to NextZXOS, `mkstemp` + `close` can be replaced with a platform-specific equivalent that creates an empty file and returns its path. This is a platform-layer concern, not a design concern.

### has_known_extension static string array

- **Location:** `nextglk/nextglk_fileref.c`, `has_known_extension()`
- **Concern:** Uses a stack-allocated array of `const char*` string literals (`".glkdata"`, `".glksave"`, `".txt"`, `".glkrec"`). Each iteration calls `strlen()` on the candidate extension.
- **Linux implementation:** Stack array of 5 pointers (4 strings + NULL sentinel), approximately 40 bytes on 64-bit.
- **Likely Spectrum Next impact:** Minimal — the array is small and stack-allocated. `strlen()` on string literals is likely inlined or cheap. Not a concern for the Next.
- **Possible future mitigation:** None needed.

## Phase 3A.3 — Platform File Write I/O

### malloc/free in nextglk_file_open_write and nextglk_file_append

- **Location:** `nextglk/nextglk_file.c`, `nextglk_file_open_write()` and `nextglk_file_append()`
- **Concern:** Each call to `nextglk_file_open_write()` or `nextglk_file_append()` allocates a `NextGlkFile` wrapper struct via `malloc(sizeof(NextGlkFile))`. On 64-bit Linux this is 8 bytes (a single `FILE*` pointer). The wrapper is freed in `nextglk_file_close()`. On failure, the wrapper is freed and NULL is returned (no leak).
- **Linux implementation:** Standard `malloc()`/`free()` via glibc.
- **Likely Spectrum Next impact:** z88dk's `malloc` may have limited heap and fragmentation risks. However, the struct is very small (likely 2–4 bytes on Z80 depending on pointer size) and the number of concurrently open files is tiny (typically 0–2: one save file or one transcript file).
- **Possible future mitigation:** Embed the `NextGlkFile` wrapper inside `stream_t.file` as a direct `FILE*` instead of heap-allocating a separate wrapper. This would eliminate the malloc/free pair entirely for every file open/close.

### FILE* assumptions in NextGlkFile

- **Location:** `nextglk/nextglk_file.c`, `struct NextGlkFile`
- **Concern:** The `NextGlkFile` struct contains a single `FILE*` pointer. On Linux this is a standard `FILE*` from `<stdio.h>`. On the Spectrum Next / z88dk, the `FILE*` abstraction may not exist or may behave differently (e.g., NextZXOS uses file handles, not `stdio` streams).
- **Linux implementation:** Standard glibc `fopen()`/`fclose()`/`fwrite()`.
- **Likely Spectrum Next impact:** z88dk may provide a subset of `stdio` with different semantics for fopen modes, especially binary mode (`"wb"`/`"ab"`). The `fwrite()` return value (bytes written) may differ from POSIX expectations on a partial write. `fclose()` on NextZXOS may behave differently.
- **Possible future mitigation:** Replace the `FILE*` with a platform-specific file descriptor or handle type when porting to the Next. The `NextGlkFile` struct is opaque to callers (only `nextglk.h` users see the typedef), so the internals can be changed without affecting the rest of the codebase.

### fopen "wb" mode (binary write, create/truncate)

- **Location:** `nextglk/nextglk_file.c`, `nextglk_file_open_write()`
- **Concern:** Uses `fopen(path, "wb")` — creates the file if it doesn't exist, truncates to zero length if it does. The `"b"` flag ensures binary mode (no newline translation). On Linux this is standard.
- **Linux implementation:** Standard glibc `fopen("wb")`.
- **Likely Spectrum Next impact:** z88dk's `fopen` may not support the `"b"` flag or may treat it as a no-op (since the Spectrum Next doesn't do CR/LF translation). Truncation behaviour (`"w"` mode) may differ if the underlying filesystem (e.g., FAT on SD card) doesn't support truncation in the same way. If truncation fails silently, files could accumulate stale data after the new content.
- **Possible future mitigation:** Explicitly delete the file before creating it via platform-specific delete, then open with `"wb"` to ensure a clean file. Or test truncation behaviour on the actual Next hardware and adjust.

### fopen "ab" mode (binary append, create if missing)

- **Location:** `nextglk/nextglk_file.c`, `nextglk_file_append()`
- **Concern:** Uses `fopen(path, "ab")` — creates the file if it doesn't exist, appends to the end if it does. The append semantic (always write at end, atomically) is critical for transcript files that accumulate output over time.
- **Linux implementation:** Standard glibc `fopen("ab")`.
- **Likely Spectrum Next impact:** z88dk's `fopen` may not support append mode `"a"` on all targets. If append is not supported, the transcript file could be overwritten each time it's opened. NextZXOS file APIs may require explicit seek-to-end before each write.
- **Possible future mitigation:** If `"ab"` is not available, implement append by opening with `"r+b"` or `"wb"`, seeking to end with `fseek(fp, 0, SEEK_END)`, and then writing. This loses the atomic append guarantee of `"ab"` mode but is functionally equivalent for our single-threaded use case.

### fwrite return value and partial writes

- **Location:** `nextglk/nextglk_file.c`, `nextglk_file_write()`
- **Concern:** Uses `fwrite(buffer, 1, length, fp)` and returns the result directly cast to `uint32_t`. On Linux, `fwrite` returns the number of items written — either `length` (success) or a smaller number (partial write or error). Partial writes are not retried in this implementation.
- **Linux implementation:** Standard glibc `fwrite()`. Partial writes are rare on local filesystems but can happen on full disks.
- **Likely Spectrum Next impact:** On SD card filesystems, `fwrite` may return fewer bytes than requested on a full disk or filesystem error. The caller (`glk_put_buffer_stream`) receives the actual byte count and can handle it. z88dk's `fwrite` may have different behaviour for zero-length writes or NULL buffers.
- **Possible future mitigation:** Add a retry loop for partial writes, or at minimum ensure `glk_put_buffer_stream` correctly reports the byte count. The current single-call approach is acceptable for the Linux reference implementation.

### Path handling assumptions

- **Location:** `nextglk/nextglk_file.c`, all file functions
- **Concern:** File paths are passed as `const char*` and forwarded directly to `fopen()`. No path sanitisation or validation is performed. The path may be a relative path (current working directory), an absolute path, or contain directory separators.
- **Linux implementation:** Standard POSIX path handling via glibc `fopen()`.
- **Likely Spectrum Next impact:** NextZXOS uses different path conventions (e.g., drive letters or volume names). The maximum path length on SD card FAT filesystems may be limited (e.g., 8.3 filenames or 255-character long filenames). Forward slashes `/` vs backslashes `\` may matter. The current working directory concept may differ from POSIX.
- **Possible future mitigation:** The path is generated by `nextglk_fileref.c` (fileref creation), not by the game. Fileref creation already produces short, ASCII-only filenames without path separators. This limits the path handling concern to the working directory prefix, which can be handled by a platform-specific init step that sets the NextZXOS current directory.

### Temporary file cleanup in tests

- **Location:** `tests/fileio_tests.c`, Phase 3A.3 tests
- **Concern:** Tests create files in `/tmp/nextgit-test-3a3-*.bin` and clean them up with `unlink()`. If a test crashes mid-execution, temporary files may be left behind.
- **Linux implementation:** Standard POSIX `unlink()` via glibc.
- **Likely Spectrum Next impact:** Not applicable — tests run on Linux only.
- **Possible future mitigation:** Test harness could use a temporary directory that is cleaned on startup, or use `atexit()` handlers. Acceptable as-is for reference implementation.

### fseek/ftell in tests for file content verification

- **Location:** `tests/fileio_tests.c`, Phase 3A.3 tests
- **Concern:** Tests use `fseek(fp, 0, SEEK_END)` and `ftell(fp)` to determine file size for verification. These are POSIX stdio functions.
- **Linux implementation:** Standard glibc `fseek()`/`ftell()`.
- **Likely Spectrum Next impact:** Not applicable — tests run on Linux only. However, `fseek`/`ftell` will be needed in Phase 3A.6 for `glk_stream_get_position`/`glk_stream_set_position`, and those functions will need platform-specific implementations on the Next.
- **Possible future mitigation:** When implementing position functions for the Next, prefer explicit byte counters maintained in `NextGlkFile` rather than relying on `ftell()`/`fseek()` if those are unreliable on NextZXOS.

### assert-based test macros vs TEST_ASSERT

- **Location:** `tests/fileio_tests.c`, Phase 3A.3 tests
- **Concern:** Phase 3A.3 tests use `TEST_ASSERT` consistently (matching existing test style). No raw `assert()` calls from `<assert.h>`. Test failure returns 1 from `fileio_tests_run()`, which is handled by `test_main.c`.
- **Linux implementation:** Custom `TEST_ASSERT` macro from `test_common.h` using `printf` and `return`.
- **Likely Spectrum Next impact:** Not applicable — tests run on Linux only.
- **Possible future mitigation:** None needed.

### unlink in tests/fileio_tests.c for temp file cleanup

- **Location:** `tests/fileio_tests.c`, Phase 3A.2 temp file tests
- **Concern:** Tests call `unlink()` directly on temp files created by `glk_fileref_create_temp()` because `glk_fileref_delete_file()` is still a stub (deferred to Phase 3A.7). This is a test-only concern.
- **Linux implementation:** Standard POSIX `unlink()`.
- **Likely Spectrum Next impact:** Not applicable — tests run on Linux only. The Spectrum Next build will not compile the test suite.

## Phase 3A.4 — File Stream Open and Close

### Stream/file coupling (stream_t.file owns NextGlkFile*)

- **Location:** `nextglk/nextglk.c`, `glk_stream_open_file()` and `glk_stream_close()`
- **Concern:** `stream_t.file` stores a `NextGlkFile*` pointer. The stream owns this pointer and is responsible for calling `nextglk_file_close()` on it during stream destruction. The coupling is 1:1; each file stream wraps exactly one platform file handle. If the platform layer changes the `NextGlkFile` to embed data directly rather than heap-allocate a wrapper, the `void *file` field in `stream_t` can still hold it (pointer or opaque handle).
- **Linux implementation:** `NextGlkFile*` is a heap-allocated wrapper around a `FILE*`. Two allocations per file stream open: one for `stream_t` (in `gli_new_stream`), one for `NextGlkFile` (in `nextglk_file_open_write`/`nextglk_file_append`).
- **Likely Spectrum Next impact:** Double allocation per file open is wasteful on a constrained heap. The `NextGlkFile` wrapper could be eliminated by storing the platform handle directly inside `stream_t` or using a fixed pool. For now, the number of concurrently open file streams is tiny (0–2), so this is not a critical concern.
- **Possible future mitigation:** Eliminate the `NextGlkFile` wrapper heap allocation. Either embed a fixed-size platform file handle in `stream_t`, or pre-allocate a small array of `NextGlkFile` structs at startup.

### Cleanup ordering in glk_stream_close for strtype_File

- **Location:** `nextglk/nextglk.c`, `glk_stream_close()`
- **Concern:** The platform file handle must be closed *before* calling `gli_delete_stream()` because `gli_delete_stream()` frees the `stream_t` structure. The cleanup sequence is: close platform handle → NULL the `file` pointer → clear any window `echostr` cross-reference → call `gli_delete_stream()` to unlink and free the stream. The `gli_currentstr` clearing is handled inside `gli_delete_stream()` (existing behaviour). If the order were reversed (free first, close second), the `NextGlkFile*` pointer would be accessed after the stream struct is freed, causing a use-after-free.
- **Linux implementation:** Sequential cleanup with explicit NULL-ing of `file` before `gli_delete_stream()`.
- **Likely Spectrum Next impact:** Same ordering constraints apply on all platforms. No additional Next-specific concern — this is a correctness requirement regardless of target.
- **Possible future mitigation:** None needed — the ordering is correct and well-documented.

### Window echostr cross-reference clearing

- **Location:** `nextglk/nextglk.c`, `glk_stream_close()`
- **Concern:** When closing a file stream, if any window's `echostr` references this stream, the reference must be cleared to avoid a dangling pointer. Currently only `gli_mainwin` is checked (single-window implementation). If multi-window support is added, this would need to iterate all windows.
- **Linux implementation:** Single check: `if (gli_mainwin && gli_mainwin->echostr == st) gli_mainwin->echostr = NULL;`
- **Likely Spectrum Next impact:** If multi-window support is added for the Next, the single-window assumption breaks. The fix would be to iterate all windows in a list (similar to `gli_streamlist`). For the current single-window design, this is sufficient.
- **Possible future mitigation:** When multi-window support is added, replace the single check with a loop over `gli_windowlist` (if implemented) or equivalent.

### malloc in gli_new_stream for stream_t

- **Location:** `nextglk/next_stream.c`, `gli_new_stream()`
- **Concern:** `gli_new_stream()` allocates a `stream_t` struct via `malloc(sizeof(stream_t))`. On 64-bit Linux, `stream_t` is approximately 120 bytes (disprock, type/mode fields, pointers, buffer fields, linked-list pointers). Each file stream open causes this allocation plus the `NextGlkFile` wrapper allocation from the platform layer.
- **Linux implementation:** Standard `malloc()` via glibc.
- **Likely Spectrum Next impact:** `stream_t` on Z80 will be smaller (likely 40–60 bytes depending on pointer sizes). Combined with the `NextGlkFile` wrapper, each file stream open is two heap allocations. With only 0–2 file streams open concurrently (save + transcript), this is not a memory crisis, but the heap fragmentation risk from repeated open/close cycles should be noted.
- **Possible future mitigation:** Consider a pre-allocated pool of `stream_t` structs or a static array for the small number of concurrently open file streams.

### Stream rock is not stored by gli_new_stream

- **Location:** `nextglk/next_stream.c`, `gli_new_stream()`, and `nextglk/nextglk.c`, `glk_stream_get_rock()`, `glk_stream_iterate()`
- **Concern:** `gli_new_stream()` accepts a `rock` parameter but discards it with `(void)rock`. The stream's rock field is not currently stored in `stream_t` (there is no `rock` field in the struct). `glk_stream_get_rock()` returns 0 unconditionally. `glk_stream_iterate()` writes 0 to `rockptr`. This is a known gap — the Glk ABI requires rock values on all opaque objects. Filerefs already store their rock correctly; streams and windows do not yet.
- **Linux implementation:** Rock parameter accepted but discarded.
- **Likely Spectrum Next impact:** Rock values are used by the game to associate opaque objects with game-level data. If a game relies on stream rock values (uncommon but allowed by the spec), it would receive incorrect data. This should be fixed before declaring Glk API compliance.
- **Possible future mitigation:** Add a `glui32 rock` field to `stream_t` in `nextglk_internal.h`, store the rock value in `gli_new_stream()`, and return it from `glk_stream_get_rock()` and `glk_stream_iterate()`. This is a Phase 3B or later task.

### glk_stream_open_file error cleanup (platform open succeeds but gli_new_stream fails)

- **Location:** `nextglk/nextglk.c`, `glk_stream_open_file()`
- **Concern:** If `nextglk_file_open_write()` succeeds but `gli_new_stream()` returns NULL (malloc failure), the platform file handle must be closed before returning NULL to avoid a resource leak. The current implementation does this: `if (!str) { nextglk_file_close(nf); return NULL; }`. This is correct but worth documenting because the dual-allocation pattern (platform malloc + stream malloc) creates this cleanup requirement.
- **Linux implementation:** Guarded close in the error path.
- **Likely Spectrum Next impact:** Same resource-leak concern on all platforms. On the Next, malloc failure is more likely due to constrained heap, so this error path may be exercised more frequently.
- **Possible future mitigation:** None needed — the leak is already prevented.

### glk_fileref_does_file_exist used in test for append mode

- **Location:** `tests/fileio_tests.c`, Phase 3A.4 append stream test
- **Concern:** The test calls `glk_fileref_does_file_exist()` to verify that an appended file exists on disk after closing the stream. This function was implemented in Phase 3A.2 and uses `access()` on Linux. This is the first test that couples file stream close with fileref existence checking.
- **Linux implementation:** Standard POSIX `access(path, F_OK)`.
- **Likely Spectrum Next impact:** Not applicable — tests run on Linux only.
- **Possible future mitigation:** None needed for tests. For the Next platform, `glk_fileref_does_file_exist` will need a platform-specific implementation (e.g., NextZXOS file query).

### unlink in Phase 3A.4 tests for file cleanup

- **Location:** `tests/fileio_tests.c`, Phase 3A.4 tests
- **Concern:** Each test creates a file on disk via `glk_fileref_create_by_name()` + `glk_stream_open_file()` and cleans up with `unlink()`. File paths are relative (e.g., `"test3a4write.glkdata"`), so they land in the current working directory. If a test crashes mid-execution, stale files may be left behind.
- **Linux implementation:** Standard POSIX `unlink()` on relative paths.
- **Likely Spectrum Next impact:** Not applicable — tests run on Linux only.
- **Possible future mitigation:** Consider a test helper that registers test files for cleanup via `atexit()`, or use a temporary subdirectory.

### Read mode deferred to Phase 3B

- **Location:** `nextglk/nextglk.c`, `glk_stream_open_file()`
- **Concern:** `filemode_Read` returns NULL because file reading is not yet implemented. The game's restore path calls `glk_stream_open_file(fref, filemode_Read, 0)`, and a NULL return causes the game to print "Restore failed" and continue. This is the documented graceful failure behaviour for Phase 3A.
- **Linux implementation:** Immediate return of NULL for `filemode_Read`.
- **Likely Spectrum Next impact:** No additional concern — the path is not yet implemented on any platform.
- **Possible future mitigation:** Implement in Phase 3B with `nextglk_file_open_read()`.
