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

---

## Phase 3A.5 — File Stream Output

### writecount is glui32 (32-bit unsigned) for file stream byte counting

- **Location:** `nextglk/next_stream.c`, `glk_put_char_stream()` and `glk_put_buffer_stream()` for `strtype_File`
- **Concern:** `st->writecount` is a `glui32` (32-bit unsigned integer). For file streams, the writecount tracks the cumulative number of bytes written to the file. On Linux, a 32-bit counter wraps at 4 GB, which is not a practical limit for Glulx save files (typically under 1 MB). On the Spectrum Next, the same 4 GB limit applies, but save files are expected to be tiny (under 100 KB).
- **Linux implementation:** `writecount` is incremented by the actual number of bytes written (returned by `nextglk_file_write()`). For single-character writes, this is 0 or 1. For buffer writes, this is the return value of `fwrite()`.
- **Likely Spectrum Next impact:** No additional concern. 32-bit arithmetic is cheap on Z80 via z88dk's long arithmetic support. The 4 GB limit is not practical on a machine with ~1.7 MB of RAM.
- **Possible future mitigation:** None needed.

### Per-byte writecount increment in glk_put_char_stream for strtype_File

- **Location:** `nextglk/next_stream.c`, `glk_put_char_stream()` `case strtype_File:`
- **Concern:** `writecount` is incremented by the number of bytes *actually* written (`nextglk_file_write` return value), not unconditionally by 1. This means if the platform write layer returns 0 (e.g., disk full), `writecount` will not be incremented, accurately reflecting that no bytes were written. The previous implementation unconditionally incremented `writecount` at the top of the function before the switch statement, which would have incorrectly counted bytes for file streams that failed to write. The fix moved `writecount++` into each case branch for `strtype_Window` and `strtype_Memory`, while `strtype_File` uses the platform layer return value.
- **Linux implementation:** `writecount += written` where `written` is from `nextglk_file_write()`.
- **Likely Spectrum Next impact:** On SD card filesystems, disk-full errors are possible and should result in a `writecount` that accurately reflects actual bytes written. The current implementation handles this correctly. z88dk's `fwrite` may have different error-reporting semantics, but the return value should still reflect actual bytes written.
- **Possible future mitigation:** None needed — the pattern of deferring to the platform layer's return value is correct on all platforms.

### glk_put_buffer_stream bulk-write optimisation for strtype_File

- **Location:** `nextglk/next_stream.c`, `glk_put_buffer_stream()`
- **Concern:** For file streams, `glk_put_buffer_stream()` now calls `nextglk_file_write()` directly with the entire buffer, rather than looping through `glk_put_char_stream()` for each byte. This is an optimisation: one `fwrite()` call instead of N `fwrite()` calls for an N-byte buffer. The `writecount` is incremented by the total bytes written in a single addition, not per-byte.
- **Linux implementation:** Single `fwrite()` call with the full buffer, significantly reducing system call overhead for large writes (e.g., save files with multi-kilobyte memory chunks).
- **Likely Spectrum Next impact:** On NextZXOS, reducing the number of individual write operations is beneficial because SD card writes are expensive. A single bulk write is much faster than N individual byte writes. However, if z88dk's `fwrite` has a smaller maximum transfer size (e.g., due to a small internal buffer), large buffers may need to be split into smaller chunks. This is not a concern for the Linux reference implementation but should be tested on real hardware.
- **Possible future mitigation:** If z88dk's `fwrite` has a practical transfer limit, add a chunking loop in `nextglk_file_write()` or in the platform layer. The Glk layer (`glk_put_buffer_stream`) should continue to pass the full buffer.

### glk_put_string_stream delegates to glk_put_char_stream for file streams

- **Location:** `nextglk/next_stream.c`, `glk_put_string_stream()`
- **Concern:** `glk_put_string_stream()` always calls `glk_put_char_stream()` in a loop, regardless of stream type. For file streams, this means each character results in a separate `nextglk_file_write()` call (one byte at a time). This is less efficient than the bulk-write path in `glk_put_buffer_stream()`. However, `glk_put_string_stream()` is typically used for short strings (filenames, chunk IDs like "FORM", "IFZS") in the Glulx save path, not for large data blocks. Large data blocks use `glk_put_buffer_stream()`.
- **Linux implementation:** Per-byte `fwrite()` for each character in the string.
- **Likely Spectrum Next impact:** For short strings (4–20 bytes), the per-byte overhead is negligible. If a game writes very long strings via `glk_put_string_stream()`, performance could degrade on SD card. However, this is unlikely because the Glulx save format uses `glk_put_buffer_stream()` for bulk data and `glk_put_string_stream()` only for small metadata.
- **Possible future mitigation:** If profiling shows `glk_put_string_stream()` is a bottleneck on the Next, it could be optimised to call `glk_put_buffer_stream()` internally for file streams (using `strlen()` to get the length). This would trade a small `strlen()` scan for N individual `fwrite()` calls.

### NextGlkFile type cast in stream output functions

- **Location:** `nextglk/next_stream.c`, `glk_put_char_stream()` and `glk_put_buffer_stream()`
- **Concern:** The `st->file` pointer (declared as `void *` in `stream_t`) is cast directly to `NextGlkFile *` before being passed to `nextglk_file_write()`. There is no runtime type check ensuring that `st->file` actually points to a valid `NextGlkFile`. This is safe because only `strtype_File` streams reach this code path, and `glk_stream_open_file()` always sets `st->file` to a valid `NextGlkFile*` before returning.
- **Linux implementation:** Direct pointer cast from `void *` to `NextGlkFile *`.
- **Likely Spectrum Next impact:** Same casting pattern — no additional risk. If `NextGlkFile` changes to an inline struct or a different pointer type on the Next, the cast must be updated. This is a compile-time concern (the compiler will catch type mismatches).
- **Possible future mitigation:** Consider adding a `static_assert` or compile-time check that `NextGlkFile` is a pointer-compatible type if the definition changes.

### Binary data (null bytes, 0xFF) handled correctly in file stream output

- **Location:** `nextglk/next_stream.c`, `glk_put_char_stream()` and `glk_put_buffer_stream()` for `strtype_File`
- **Concern:** The previous (deferred) implementation silently ignored all writes to file streams. The new implementation passes data directly to `nextglk_file_write()`, which uses `fwrite()` in binary mode. Binary data including null bytes (`0x00`) and byte value `0xFF` are preserved correctly because the file is opened with `"wb"` (binary write) mode. `glk_put_char_stream()` casts `ch` to `unsigned char` before writing, avoiding any sign-extension issues.
- **Linux implementation:** Binary-safe `fwrite()` via `"wb"` mode.
- **Likely Spectrum Next impact:** On NextZXOS, binary file output must similarly preserve all byte values. If the platform layer uses a text-mode file API, null bytes or certain control characters could be mangled. Ensure the Next platform layer opens files in binary mode (if the concept exists on NextZXOS).
- **Possible future mitigation:** When implementing the Next platform layer, verify that file writes are binary-safe (all byte values 0x00–0xFF round-trip correctly).

### glk_put_char_stream no longer unconditionally increments writecount for all types

- **Location:** `nextglk/next_stream.c`, `glk_put_char_stream()`
- **Concern:** The previous implementation had `st->writecount++` at the top of `glk_put_char_stream()` before the `switch` statement, incrementing the counter even for `strtype_File` (which was a no-op). This meant that `writecount` would have reported N bytes written even though 0 bytes were actually written to a file stream. The fix moves the `writecount` increment into each `case` branch, ensuring accurate byte counts. For `strtype_File`, `writecount` is incremented by the actual number of bytes written as returned by `nextglk_file_write()`.
- **Linux implementation:** Per-case writecount management.
- **Likely Spectrum Next impact:** Accurate writecount is critical for the Glulx save path, which uses `glk_stream_get_position()` and `glk_stream_set_position()` to backpatch chunk sizes. If writecount were inflated, position calculations would be wrong and save files would be corrupted. The fix ensures correctness on all platforms.
- **Possible future mitigation:** None needed — this is a correctness fix, not a platform-specific concern.

### fwrite behaviour for file stream output

- **Location:** `nextglk/next_stream.c` → `nextglk_file_write()` in `nextglk/nextglk_file.c`
- **Concern:** All file stream output ultimately calls `nextglk_file_write()`, which calls `fwrite(buffer, 1, length, fp)`. The return value from `fwrite()` is the number of items (bytes) successfully written. On a full disk, this may be less than `length`. The stream layer uses this return value to increment `writecount`, so the byte count accurately reflects reality. However, if a partial write occurs mid-stream, the game may not detect the error (Glk does not have an error-reporting mechanism for individual writes — the game only sees the aggregate counts via `glk_stream_close()`).
- **Linux implementation:** Standard glibc `fwrite()` with return value used for writecount.
- **Likely Spectrum Next impact:** On SD card, disk-full errors are a real concern. If `fwrite()` returns fewer bytes than requested, the game's save file will be truncated and invalid. The game will detect this when it tries to restore and finds a corrupt file. This is the expected failure mode — Glk does not require individual write error reporting.
- **Possible future mitigation:** Consider adding a flag or counter for write errors so the game can detect them. This is a Glk API design consideration, not specific to the Spectrum Next.

### Large file considerations for save files

- **Location:** `nextglk/next_stream.c`, all file stream output paths
- **Concern:** The Glulx save file format (Quetzal) chunks the game state into sections (IFhd, Stks, MAll, CMem). Large Inform 7 games may have multi-kilobyte memory chunks (e.g., `CMem` chunk containing the entire game heap, potentially hundreds of kilobytes). `glk_put_buffer_stream()` writes these chunks in a single `fwrite()` call via `nextglk_file_write()`. On Linux, `fwrite()` can handle large buffers efficiently via buffered I/O. On the Spectrum Next, a single large `fwrite()` may require a correspondingly large buffer in the z88dk stdio implementation, or may need to be split.
- **Linux implementation:** Single `fwrite()` call for the entire buffer, buffered by glibc's stdio (typically 4 KB or 8 KB buffer).
- **Likely Spectrum Next impact:** On NextZXOS, the stdio implementation may have a smaller internal buffer or may write directly to SD card without buffering. Large `fwrite()` calls could be slow or could fail if the underlying filesystem has transfer size limits. The z88dk stdio may split large writes internally, but this should be verified on real hardware.
- **Possible future mitigation:** Add a platform-specific write loop in `nextglk_file_write()` that splits large writes into smaller chunks (e.g., 512 bytes or 4 KB at a time) for SD card compatibility.

---

## Phase 3B.2 — glk_stream_open_file() Read Mode

### Stream ownership of file handles

- **Location:** `nextglk/nextglk.c`, `glk_stream_open_file()`, and `nextglk/next_stream.c`, `gli_delete_stream()` / `glk_stream_close()`
- **Concern:** The stream object owns the `NextGlkFile*` handle. When `glk_stream_open_file()` successfully opens a file, it stores the handle in `str->file`. When the stream is closed via `glk_stream_close()`, the handle is closed via `nextglk_file_close()` and set to NULL before calling `gli_delete_stream()`. This ensures the file is always closed when the stream is destroyed. The separation between the Glk stream layer and the NextGlk platform file layer is clean: the stream layer only knows about `NextGlkFile*` as an opaque pointer, and delegates all actual I/O to the platform layer.
- **Linux implementation:** The stream holds a `NextGlkFile*` pointer (which wraps a `FILE*`). `glk_stream_close()` calls `nextglk_file_close()` which calls `fclose()` and `free()`.
- **Likely Spectrum Next impact:** On NextZXOS, closing a file may require different semantics (e.g., flushing buffers to SD card). The separation of layers means only `nextglk_file_close()` needs to change, not the stream layer.
- **Possible future mitigation:** None required — the layer separation is already correct for portability.

### File handle lifetime

- **Location:** `nextglk/nextglk.c`, `glk_stream_open_file()` and `glk_stream_close()`
- **Concern:** The file handle lifetime is tied to the stream lifetime. The stream is created by `gli_new_stream()` (which allocates + registers with dispatch layer + inserts into linked list) and destroyed by `gli_delete_stream()` (which unregisters + unlinks + frees). The platform file handle is opened after stream creation and closed before stream destruction. This order is correct: if stream allocation fails, the platform file handle is closed immediately (no leak). If the platform open fails, no stream is created (NULL returned). This pattern avoids the common bug where a file handle leaks on stream allocation failure.
- **Linux implementation:** `glk_stream_open_file()`: allocate stream → if fail, close file → return NULL. On success, attach file to stream. `glk_stream_close()`: close file → clear file → delete stream.
- **Likely Spectrum Next impact:** Same pattern should work on NextZXOS. The key requirement is that `nextglk_file_close()` also frees the `NextGlkFile` struct itself (it calls `free(file)` after `fclose()`).
- **Possible future mitigation:** None required — the pattern is sound.

### fopen/fclose portability

- **Location:** `nextglk/nextglk_file.c`, `nextglk_file_open_read()`, `nextglk_file_close()`
- **Concern:** The platform file layer uses POSIX `fopen(path, "rb")` for reading. On NextZXOS, the standard library may not support `fopen()` with the same mode strings, or may require different path handling (e.g., drive letters, forward-slash vs backslash, 8.3 filenames). Additionally, `fclose()` may behave differently on a filesystem without proper buffering (e.g., SD card raw access).
- **Linux implementation:** Standard glibc `fopen()` / `fclose()` with `"rb"` mode.
- **Likely Spectrum Next impact:** NextZXOS uses a FAT filesystem with POSIX-like file operations via z88dk's stdio. The `"rb"` and `"wb"` mode strings are likely supported, but should be verified. Path handling may differ (e.g., NextZXOS may use `/` but may require explicit device prefix `sd0:/` or similar).
- **Possible future mitigation:** Add a platform abstraction layer for path handling (`nextglk_resolve_path()`) that normalizes paths for the target platform. The `fopen()` calls themselves are likely fine if z88dk provides a conforming stdio.

### Separation of stream and file layers

- **Location:** `nextglk/next_stream.c` and `nextglk/nextglk_file.c`
- **Concern:** The Glk stream layer (`next_stream.c`) and the platform file layer (`nextglk_file.c`) are cleanly separated. The stream layer only sees `NextGlkFile*` as an opaque pointer (via a forward declaration in `nextglk.h`). The platform file layer is entirely independent and can be replaced for different platforms without touching the stream layer. This is a good architecture for portability.
- **Linux implementation:** The separation is enforced by the header structure: `struct NextGlkFile` is defined only in `nextglk_file.c`, not in any header. All access goes through the function API (`nextglk_file_read()`, `nextglk_file_write()`, etc.).
- **Likely Spectrum Next impact:** This architecture means the Spectrum Next port can replace `nextglk_file.c` entirely with a version that uses NextZXOS native file APIs, while keeping the stream layer unchanged. This is a significant portability win.
- **Possible future mitigation:** None required — the architecture is already designed for this.

### Implications for upcoming save/restore support

- **Location:** `nextglk/nextglk.c`, `glk_stream_open_file()`
- **Concern:** Save and restore will use `filemode_Read` and `filemode_Write` respectively through this same `glk_stream_open_file()` path. The stream layer now fully supports opening files for both reading and writing. The save implementation will need to:
  1. Create a fileref via `glk_fileref_create_by_prompt(fileusage_SavedGame, ...)`.
  2. Open a write-mode stream via `glk_stream_open_file(fref, filemode_Write, rock)`.
  3. Write the Quetzal save data via `glk_put_buffer_stream()`.
  4. Close the stream.
  The restore implementation will follow the same pattern with `filemode_Read` and `glk_get_buffer_stream()` (still a stub). The stream layer is ready for save/restore from an architectural perspective — the only missing piece is the `glk_get_buffer_stream()` implementation for reading save data back.
- **Linux implementation:** All modes (Write, WriteAppend, Read) are now functional in `glk_stream_open_file()`. `glk_put_buffer_stream()` works end-to-end for writing. `glk_get_buffer_stream()` remains a stub (returns 0).
- **Likely Spectrum Next impact:** Save/restore will work on the Spectrum Next once the same path is ported. The key concern is that save files may be large (hundreds of kilobytes for large Inform 7 games), which could stress SD card write performance and heap memory. The Quetzal format is chunked, so the VM writes the save in sections — each section is a `glk_put_buffer_stream()` call, which the stream layer passes to `nextglk_file_write()` as a single `fwrite()`. Large chunks could be problematic on the Spectrum Next.
- **Possible future mitigation:** Consider adding a chunked write loop in `nextglk_file_write()` for the Spectrum Next port (similar to the note above about large fwrite calls). Also consider memory budgeting: if the save file requires a temporary heap buffer for compression or chunking, this could consume significant memory on the Spectrum Next.

### Stream rock value now fully functional

- **Location:** `nextglk/nextglk_internal.h` (stream_t.rock field), `nextglk/next_stream.c` (`gli_new_stream()`, `glk_stream_get_rock()`, `glk_stream_iterate()`)
- **Concern:** The stream rock value was previously a stub (always returned 0). It is now stored and retrievable via `glk_stream_get_rock()` and visible via `glk_stream_iterate()`. The rock value is a `glui32` (4 bytes) stored per-stream. This is a minimal memory cost (4 bytes per stream) but should be noted.
- **Linux implementation:** Stored as `glui32 rock` in the `stream_t` struct, set by `gli_new_stream()` from the caller-supplied rock parameter.
- **Likely Spectrum Next impact:** 4 bytes per stream is negligible (typically < 10 streams active at once when counting window streams). No significant impact.
- **Possible future mitigation:** None required.

---

## Phase 3B.3 — Stream Reading APIs

### `glk_get_char_stream()` EOF behaviour

- **Location:** `nextglk/nextglk.c` (`glk_get_char_stream()`)
- **Concern:** EOF is signalled by returning `-1` as a `glsi32` (signed 32-bit integer), while valid byte values are returned as `0..255`. This is the standard Glk convention and is portable. On the Spectrum Next, `glsi32` is the same size — no concern.
- **Linux implementation:** Reads a single `unsigned char` via `nextglk_file_read()`, converts to `glsi32` for return. Returns `-1` when `nextglk_file_read()` returns 0.
- **Likely Spectrum Next impact:** None — `glsi32` is always 32-bit signed per the Glk spec.
- **Possible future mitigation:** None required.

### Signed vs unsigned char considerations

- **Location:** `nextglk/nextglk.c` (`glk_get_char_stream()`, `glk_get_line_stream()`)
- **Concern:** File data is read into an `unsigned char` before being stored in the user buffer (which is `char *`). On platforms where `char` is signed (most), byte values 128–255 will have implementation-defined behaviour when cast to `char`. However, the Glk spec defines `glk_get_char_stream()` to return values 0–255, and `glk_get_buffer_stream()` / `glk_get_line_stream()` store raw bytes — the caller is expected to interpret them as unsigned.
- **Linux implementation:** Uses `unsigned char` for the internal read, then assigns to `char` (signed) in the buffer. This works correctly on Linux where `char` is typically signed and two's complement, but the stored byte values for 128–255 will appear as negative `char` values if interpreted as signed. This matches Glk behaviour — the game must treat buffer bytes as `unsigned char`.
- **Likely Spectrum Next impact:** z88dk's `char` is unsigned by default, which means byte values 128–255 will appear as positive `char` values. This is actually *better* behaviour than on Linux, as the raw bytes are preserved without sign interpretation.
- **Possible future mitigation:** If portability issues arise, the user buffer type could be changed to `unsigned char *` at the NextGlk layer, but this would change the Glk ABI. Not recommended.

### Newline handling in `glk_get_line_stream()`

- **Location:** `nextglk/nextglk.c` (`glk_get_line_stream()`)
- **Concern:** The function reads one byte at a time via `nextglk_file_read()` to detect newlines. This is inefficient on the Spectrum Next where each `fread()` call may involve SD card latency. A buffered approach (reading a chunk into an internal buffer and then scanning for newlines) would reduce SD card interactions.
- **Linux implementation:** Byte-by-byte loop calling `nextglk_file_read(nf, &ch, 1)` — each call goes through `fread(buffer, 1, 1, fp)`. Newline (`\n`) is included in the returned buffer. NUL-termination is provided when `count < len`.
- **Likely Spectrum Next impact:** Significant performance concern for line-oriented reads on SD card-backed files. Each byte may require a separate read operation at the hardware level if unbuffered. However, `fread()` on z88dk typically uses an internal `FILE` buffer (BUFSIZ), which mitigates this somewhat.
- **Possible future mitigation:** Add a small read-ahead buffer at the NextGlk file layer, or use `fgets()`-style buffered reading. Not urgent for the Linux reference implementation.

### NUL termination requirements for `glk_get_line_stream()`

- **Location:** `nextglk/nextglk.c` (`glk_get_line_stream()`)
- **Concern:** The Glk spec requires NUL-termination only when the returned count is less than the buffer length. When the buffer is exactly filled (including the newline at the last position), the buffer is not NUL-terminated. The ZX Spectrum Next implementation must preserve this exact behaviour — adding unconditional NUL-termination would violate the spec.
- **Linux implementation:** NUL-terminates when `count < len`. When `len == 1`, the buffer is fully used and never NUL-terminated (the game must handle this per the Glk spec). When `len == 0`, the function returns 0 immediately without writing to the buffer.
- **Likely Spectrum Next impact:** None if the same logic is preserved. The edge case `len == 1` with a single newline byte means the game sees a 1-byte buffer containing `\n` with no NUL. This is correct Glk behaviour.
- **Possible future mitigation:** None required — behaviour matches the Glk spec.

### Binary-file safety for stream reads

- **Location:** `nextglk/nextglk.c` (`glk_get_buffer_stream()`, `glk_get_char_stream()`, `glk_get_line_stream()`)
- **Concern:** All three functions read via `nextglk_file_read()`, which uses `fopen(path, "rb")` — binary mode. This ensures no newline translation or encoding issues on platforms that distinguish text/binary modes (Windows). On the Spectrum Next, all file I/O is inherently binary.
- **Linux implementation:** Files are always opened with `"rb"` in `nextglk_file_open_read()`. Buffer reads return raw bytes without any transformation. Line reads detect only `\n` (not `\r\n` or other platform-specific sequences). Char reads return the exact byte value 0–255.
- **Likely Spectrum Next impact:** z88dk typically opens all files in binary mode. No concern.
- **Possible future mitigation:** None required.

### Implications for save/restore

- **Location:** `nextglk/nextglk.c` (read APIs), `nextglk/nextglk_file.c` (file I/O)
- **Concern:** The stream read APIs are now fully functional for file-backed streams. This means restore (reading a Quetzal save file) can now be implemented at the Glk layer — all the raw I/O primitives exist. Save (writing) was already functional from Phase 3A. The remaining work for save/restore end-to-end is:
  1. `glk_gestalt()` should report that save/restore is supported (currently returns 0 for `gestalt_Sound` etc., but save/restore-specific gestalt selectors may need updating).
  2. The Git VM's `saveToFile()` and `restoreFromFile()` functions in `savefile.c` will now receive real data from `glk_get_buffer_stream()` when reading save files, and `glk_put_buffer_stream()` will write real data.
  3. No changes are needed at the NextGlk layer for basic save/restore to work — the primitives are all implemented.
- **Linux implementation:** `glk_get_char_stream()`, `glk_get_buffer_stream()`, and `glk_get_line_stream()` all delegate to `nextglk_file_read()`, which uses `fread()`. Reads correctly advance file position, which `glk_stream_get_position()` reports via `ftell()`.
- **Likely Spectrum Next impact:** Save file sizes for large Inform 7 games may be hundreds of KB. Reading a full save file sequentially via `glk_get_buffer_stream()` will require SD card throughput. The Quetzal format is chunked, so the VM reads the file in sections — each section corresponds to a `glk_get_buffer_stream()` call. The same chunked-read concern applies as noted for chunked writes in Phase 3A notes.
- **Possible future mitigation:** Test restore with large save files early in the Spectrum Next port. If SD card latency causes issues, consider adding a read-ahead buffer at the `NextGlkFile` layer.
