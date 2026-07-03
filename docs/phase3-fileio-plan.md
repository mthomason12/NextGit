# Phase 3 — File I/O Implementation Plan

## Overview

Implement save, restore, and transcript (script) for NextGlk.  The work is
split into two sub-phases:

- **Phase 3A** — Filerefs, file output, transcript, save
- **Phase 3B** — File input, restore

Every milestone must:
- Compile
- Link
- Leave the game playable

Acceptable intermediate states:
- Save/restore/transcript may fail gracefully (game prints "failed" message)
- The game must continue to play normally

---

## Source Files

| File | Purpose | Ownership |
|------|---------|-----------|
| `nextglk/nextglk_fileref.c` | Fileref lifecycle, create/destroy/iterate/query | NextGlk |
| `nextglk/nextglk_file.c` | Platform file I/O (read, write, open, close) | NextGlk |
| `nextglk/nextglk.c` | `glk_stream_open_file`, position functions, `glk_get_*_stream` | NextGlk |
| `nextglk/next_stream.c` | `glk_put_*_stream` for `strtype_File`, echo-stream wiring | NextGlk |
| `nextglk/next_window.c` | `glk_window_set_echo_stream`, `glk_window_get_echo_stream` | NextGlk |
| `nextglk/nextglk_internal.h` | `fileref_t` struct, `gli_filereflist` declarations | NextGlk |
| `nextglk/gi_dispa.c` | Dispatch-table entries (already registered, no changes needed) | NextGlk |
| `tests/stream_tests.c` | Stream output/position tests | Tests |
| `tests/fileio_tests.c` | New: fileref lifecycle + platform I/O tests | Tests |

---

## File Ownership Rules

From `docs/glk-object-lifecycle.md`:

- `fileref_t.filename` — **Owned** by fileref (malloc'd by `gli_new_fileref`, freed by `gli_delete_fileref`)
- `stream_t.file` (strtype_File) — **Owned** by stream (fclose'd by `gli_delete_stream`)
- `window_t.echostr` — **Not owned** (not closed on window close)
- `stream_t.win` (strtype_Window) — **Not owned** (back-pointer, not freed)

All filerefs are tracked in `gli_filereflist` (doubly-linked list, head insertion).
All file streams are tracked in `gli_streamlist` (already exists).

Dispatch-layer registration must happen in both creation and destruction paths
for filerefs, as the Git VM dispatcher uses the object registry to convert
between Glulx `glui32` IDs and C pointers.

---

## Phase 3A — Filerefs, File Output, Transcript, Save

### Shared Infrastructure (prerequisite for all 3A milestones)

#### Dependency chain

```
fileref_t struct + gli_filereflist
      │
      ▼
gli_new_fileref / gli_delete_fileref / glk_fileref_iterate
      │
      ▼
glk_fileref_create_by_name / glk_fileref_create_temp / glk_fileref_create_by_prompt
      │
      ▼
glk_fileref_destroy / glk_fileref_get_rock / glk_fileref_delete_file / glk_fileref_does_file_exist
      │
      ▼
nextglk_file_open_write / nextglk_file_write / nextglk_file_close
      │
      ▼
glk_stream_open_file (Write) / glk_stream_close (File)
      │
      ▼
glk_put_char_stream (File) / glk_put_buffer_stream (File) / glk_put_string_stream (File)
      │
      ▼
glk_stream_set_position (File) / glk_stream_get_position (File)
      │
      ▼
glk_window_set_echo_stream / echo in window output path
```

---

### Milestone 3A.1 — Fileref Struct and Lifecycle

**Objective:** Define `fileref_t`, implement `gli_new_fileref`, `gli_delete_fileref`,
`gli_filereflist`, and the fileref iteration function.

**Functions implemented:**
- `gli_new_fileref(filename, usage, rock)` — allocate, strdup filename, insert into list, register with dispatch
- `gli_delete_fileref(fref)` — unregister, free filename, unlink from list, free struct
- `glk_fileref_iterate(fref, rockptr)` — walk `gli_filereflist`

**Files modified:**
- `nextglk/nextglk_internal.h`
  - Add `fileref_t` struct definition (after existing forward declaration)
  - Add `extern fileref_t *gli_filereflist`
  - Add `gli_new_fileref` / `gli_delete_fileref` declarations
- `nextglk/nextglk_fileref.c`
  - Implement `gli_new_fileref`, `gli_delete_fileref`
  - Replace `glk_fileref_iterate` stub with real implementation
  - Add `gli_filereflist` global variable
- `nextglk/next_window.c` or `nextglk/nextglk.c`
  - Initialize `gli_filereflist = NULL` in existing init path

**Dependencies:** None (new code only, uses existing `malloc`/`free`/`strdup` and
existing dispatch callbacks `gli_register_obj`/`gli_unregister_obj`).

**Ownership rules enforced:**
- `fileref_t.filename` is allocated by `strdup` in `gli_new_fileref`, freed by `free` in `gli_delete_fileref`
- `fileref_t.disprock` is set by `gli_register_obj` callback

**Tests:**
```c
// tests/fileio_tests.c — Milestone 3A.1 tests
void test_fileref_create_destroy() {
    frefid_t fref = glk_fileref_create_by_name(fileusage_Data, "test", 42);
    assert(fref != NULL);
    assert(glk_fileref_get_rock(fref) == 42);  // if rock is stored
    
    frefid_t iter = glk_fileref_iterate(NULL, NULL);
    assert(iter == fref);
    
    iter = glk_fileref_iterate(fref, NULL);
    assert(iter == NULL);  // only one fileref
    
    glk_fileref_destroy(fref);
    iter = glk_fileref_iterate(NULL, NULL);
    assert(iter == NULL);  // list empty after destroy
}

void test_fileref_create_temp() {
    frefid_t fref = glk_fileref_create_temp(fileusage_Data, 0);
    assert(fref != NULL);
    assert(glk_fileref_does_file_exist(fref) == 1);  // temp creates the file
    glk_fileref_delete_file(fref);
    assert(glk_fileref_does_file_exist(fref) == 0);
    glk_fileref_destroy(fref);
}
```

**Runtime validation:**
1. Build and run test binary
2. Load a game, observe that save/restore still fail gracefully
3. Game must remain playable

---

### Milestone 3A.2 — Fileref Creation Functions

**Objective:** Replace stubs with real implementations for all `glk_fileref_create_*`
functions.

**Functions implemented:**
- `glk_fileref_create_by_name(usage, name, rock)` — sanitise name, append suffix,
  prepend working directory, call `gli_new_fileref`
- `glk_fileref_create_temp(usage, rock)` — generate temp filename via `mkstemp()`
  or equivalent, call `gli_new_fileref`
- `glk_fileref_create_by_prompt(usage, fmode, rock)` — use fixed filename based on
  `fileusage_*` type (see below), call `gli_new_fileref`
- `glk_fileref_create_from_fileref(usage, fref, rock)` — clone existing fileref
  with new usage, call `gli_new_fileref`

**Files modified:**
- `nextglk/nextglk_fileref.c`
  - Replace all `glk_fileref_create_*` stubs with real implementations

**Filenaming convention:**
```
fileusage_Data        → "<name>.glkdata"
fileusage_SavedGame   → "<name>.glksave"
fileusage_Transcript  → "<name>.txt"
fileusage_InputRecord → "<name>.glkrec"
```

**Create-by-prompt shortcut (ZX Spectrum Next constraint):**
Since the Next has no stdin prompt, `glk_fileref_create_by_prompt` uses a
fixed filename determined by `usage`:

| usage | Fixed filename |
|---|---|
| `fileusage_SavedGame` | `"nextgit.sav"` |
| `fileusage_Transcript` | `"transcript.txt"` |
| `fileusage_InputRecord` | `"input.rec"` |
| `fileusage_Data` | `"data.glkdata"` |

This is the **minimum viable implementation** for the Next platform.
Games that call `glk_fileref_create_by_prompt` will get a deterministic
filename.  A future enhancement could iterate through numbered filenames
(e.g., `"save00.glksave"`, `"save01.glksave"`, ...).

**Working directory:**
Assume files are created in the current working directory.  On the Next,
this is the directory from which the game was launched (SD card path).

**Suffix handling in `glk_fileref_create_by_name`:**
If the game-provided name already has a recognised extension (`.glksave`,
`.glkdata`, `.txt`), do not append a second suffix.  Otherwise append the
suffix matching `usage & fileusage_TypeMask`.

**Ownership rules enforced:**
- `glk_fileref_create_temp` creates the file immediately (calls `mkstemp`),
  then closes the fd.  The file exists but is empty.
- All other creation functions do NOT create the file — only the fileref.
  The file is created when `glk_stream_open_file` is called with
  `filemode_Write`.

**Dependencies:** Milestone 3A.1 (fileref lifecycle).

**Tests:**
```c
void test_fileref_create_by_name_suffix() {
    frefid_t fref;
    
    // Should append .glkdata
    fref = glk_fileref_create_by_name(fileusage_Data, "mydata", 0);
    assert(strstr(((fileref_t*)fref)->filename, "mydata.glkdata") != NULL);
    glk_fileref_destroy(fref);
    
    // Should NOT append .glkdata (already has extension)
    fref = glk_fileref_create_by_name(fileusage_Data, "config.glkdata", 0);
    assert(strstr(((fileref_t*)fref)->filename, "config.glkdata") != NULL);
    assert(strstr(((fileref_t*)fref)->filename, "config.glkdata.glkdata") == NULL);
    glk_fileref_destroy(fref);
}

void test_fileref_create_by_prompt_fixed() {
    frefid_t fref = glk_fileref_create_by_prompt(
        fileusage_SavedGame, filemode_Write, 0);
    assert(fref != NULL);
    // filename should end with ".glksave"
    glk_fileref_destroy(fref);
}

void test_fileref_create_temp_exists() {
    frefid_t fref = glk_fileref_create_temp(fileusage_Data, 0);
    assert(fref != NULL);
    assert(glk_fileref_does_file_exist(fref) == 1);
    glk_fileref_delete_file(fref);
    assert(glk_fileref_does_file_exist(fref) == 0);
    glk_fileref_destroy(fref);
}

void test_fileref_create_from_fileref() {
    frefid_t orig = glk_fileref_create_by_name(fileusage_Data, "source", 0);
    frefid_t clone = glk_fileref_create_from_fileref(fileusage_Transcript, orig, 0);
    assert(clone != NULL);
    assert(clone != orig);
    // clone's usage mask should be fileusage_Transcript, not fileusage_Data
    glk_fileref_destroy(clone);
    glk_fileref_destroy(orig);
}
```

**Runtime validation:**
1. Run tests — all create/destroy/iterate tests pass
2. Load a game, type `save` — game still says "Save failed" (because
   `glk_stream_open_file` still returns NULL, which is fine for this milestone)
3. Game remains playable

---

### Milestone 3A.3 — Platform File Write I/O

**Objective:** Implement file write/append on the platform layer.

**Functions implemented:**
- `nextglk_file_open_write(path)` — open a `NextGlkFile*` for writing (create/truncate)
- `nextglk_file_append(path)` — open a `NextGlkFile*` for appending
- `nextglk_file_write(file, buffer, length)` — write `length` bytes to file
- `nextglk_file_close(file)` — close and free the file handle

**Files modified:**
- `nextglk/nextglk_file.c`
  - Replace all stubs with real implementations
- `nextglk/nextglk_file.h` (create if needed, or use existing `nextglk.h`)
  - Declare `nextglk_file_append`

**Implementation strategy (dual-platform):**

For **Linux** (development/test):
```c
struct NextGlkFile {
    FILE *fp;
};

NextGlkFile* nextglk_file_open_write(const char *path) {
    struct NextGlkFile *nf = malloc(sizeof(*nf));
    if (!nf) return NULL;
    nf->fp = fopen(path, "wb");  // binary write, creates/truncates
    if (!nf->fp) { free(nf); return NULL; }
    return nf;
}

NextGlkFile* nextglk_file_append(const char *path) {
    struct NextGlkFile *nf = malloc(sizeof(*nf));
    if (!nf) return NULL;
    nf->fp = fopen(path, "ab");
    if (!nf->fp) { free(nf); return NULL; }
    return nf;
}

uint32_t nextglk_file_write(NextGlkFile *file, const void *buf, uint32_t len) {
    size_t written = fwrite(buf, 1, len, file->fp);
    return (uint32_t)written;
}

void nextglk_file_close(NextGlkFile *file) {
    if (file) {
        fclose(file->fp);
        free(file);
    }
}
```

For **ZX Spectrum Next** (stub in this milestone; real implementation deferred
to platform porting):
```c
// All functions return error/0 for now
NextGlkFile* nextglk_file_open_write(const char *path) { return NULL; }
```

The Linux implementation is what we build and test now.  The Next platform
layer gets filled in during platform-specific porting.

**Dependencies:** Milestone 3A.2 (fileref creation — needed only so we have
a filename; the platform layer is independent).

**Ownership rules:**
- `NextGlkFile` is **owned** by the caller (caller must call `nextglk_file_close`)
- The `FILE*` inside `NextGlkFile` is **owned** by the struct

**Tests:**
```c
void test_file_write_and_close() {
    NextGlkFile *f = nextglk_file_open_write("/tmp/nextgit-test-write.bin");
    assert(f != NULL);
    
    const char *data = "Hello, NextGit!";
    uint32_t written = nextglk_file_write(f, data, strlen(data));
    assert(written == strlen(data));
    
    nextglk_file_close(f);
    
    // Verify file contents
    FILE *fp = fopen("/tmp/nextgit-test-write.bin", "rb");
    assert(fp != NULL);
    char buf[64] = {0};
    size_t n = fread(buf, 1, sizeof(buf)-1, fp);
    assert(n == strlen(data));
    assert(strcmp(buf, data) == 0);
    fclose(fp);
    
    // Cleanup
    unlink("/tmp/nextgit-test-write.bin");
}

void test_file_append() {
    NextGlkFile *f = nextglk_file_open_write("/tmp/nextgit-test-append.bin");
    assert(f != NULL);
    nextglk_file_write(f, "first\n", 6);
    nextglk_file_close(f);
    
    f = nextglk_file_append("/tmp/nextgit-test-append.bin");
    assert(f != NULL);
    nextglk_file_write(f, "second\n", 7);
    nextglk_file_close(f);
    
    // Verify both lines present
    FILE *fp = fopen("/tmp/nextgit-test-append.bin", "rb");
    assert(fp != NULL);
    char buf[32] = {0};
    size_t n = fread(buf, 1, sizeof(buf)-1, fp);
    assert(n == 13);
    assert(strcmp(buf, "first\nsecond\n") == 0);
    fclose(fp);
    
    unlink("/tmp/nextgit-test-append.bin");
}
```

**Runtime validation:**
1. Run tests — write/append/close tests pass
2. Load a game, type `save` — still fails at `glk_stream_open_file` (fine)
3. Game remains playable

---

### Milestone 3A.4 — File Stream Open and Close

**Objective:** Implement `glk_stream_open_file` for write mode and
`glk_stream_close` for file streams.

**Functions implemented:**
- `glk_stream_open_file(fref, fmode, rock)`:
  - Validate `fref != NULL`
  - For `filemode_Write`: call `nextglk_file_open_write(fref->filename)`
  - For `filemode_ReadWrite`: call `nextglk_file_open_write` (simplified;
    true read-write deferred)
  - For `filemode_WriteAppend`: call `nextglk_file_append(fref->filename)`
  - For `filemode_Read`: return NULL (deferred to Phase 3B)
  - Call `gli_new_stream(strtype_File, readable, writable, rock)`
  - Set `str->file` to the `NextGlkFile*`
  - Set `str->readable`/`str->writable` based on fmode
- `glk_stream_close(str, result)`:
  - Validate `str != NULL`
  - For `strtype_File`: call `nextglk_file_close(str->file)`, then
    `gli_delete_stream` (the existing internal path)

**Files modified:**
- `nextglk/nextglk.c`
  - Replace `glk_stream_open_file` stub with real implementation
  - Update `glk_stream_close` (in `nextglk.c` or wherever it lives) to
    handle `strtype_File` cleanup
- `nextglk/nextglk_internal.h`
  - Update `stream_t.file` type: currently `void *file`, ensure it's
    compatible with `NextGlkFile*`

**Dependencies:** Milestones 3A.2 (fileref creation), 3A.3 (platform write I/O).

**Ownership rules enforced:**
- `stream_t.file` is set from the `NextGlkFile*` returned by `nextglk_file_open_write`
- On `gli_delete_stream(str)`, call `nextglk_file_close(str->file)` for
  `strtype_File` streams
- If `str == gli_currentstr` when closing, set `gli_currentstr = NULL`
- If `gli_mainwin->echostr == str` when closing, set `gli_mainwin->echostr = NULL`

**Tests:**
```c
void test_stream_open_file_write() {
    frefid_t fref = glk_fileref_create_by_name(fileusage_Data, "streambin", 0);
    assert(fref != NULL);
    
    strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
    assert(str != NULL);
    
    // Stream should be writable, not readable
    stream_t *s = (stream_t *)str;
    assert(s->type == strtype_File);
    assert(s->writable == 1);
    assert(s->readable == 0);
    assert(s->file != NULL);
    
    glk_stream_close(str, NULL);
    glk_fileref_destroy(fref);
    glk_fileref_delete_file(fref);  // cleanup temp file
}

void test_stream_open_file_write_append() {
    frefid_t fref = glk_fileref_create_by_name(fileusage_Transcript, "log", 0);
    strid_t str = glk_stream_open_file(fref, filemode_WriteAppend, 0);
    assert(str != NULL);
    glk_stream_close(str, NULL);
    
    // Append mode: file should still exist after close
    assert(glk_fileref_does_file_exist(fref));
    
    glk_fileref_destroy(fref);
    unlink("log.txt");  // cleanup
}

void test_stream_open_file_invalid_fref() {
    strid_t str = glk_stream_open_file(NULL, filemode_Write, 0);
    assert(str == NULL);
}

void test_stream_open_file_read_mode_returns_null() {
    frefid_t fref = glk_fileref_create_by_name(fileusage_Data, "test", 0);
    strid_t str = glk_stream_open_file(fref, filemode_Read, 0);
    assert(str == NULL);  // read mode deferred to Phase 3B
    glk_fileref_destroy(fref);
}
```

**Runtime validation:**
1. Run all tests
2. Load a game, type `save` — game should now prompt for save (or silently
   save to `nextgit.sav`).  Save may not produce a valid save file yet
   (stream output may be no-op), but the game should not crash.
3. Game remains playable

---

### Milestone 3A.5 — File Stream Output

**Objective:** Implement `glk_put_char_stream`, `glk_put_buffer_stream`, and
`glk_put_string_stream` for `strtype_File`.

**Functions modified:**
- `glk_put_char_stream(str, ch)` in `next_stream.c`:
  - `case strtype_File:` — call `nextglk_file_write(str->file, &ch, 1)`
- `glk_put_buffer_stream(str, buf, len)` in `next_stream.c`:
  - `case strtype_File:` — call `nextglk_file_write(str->file, buf, len)`
- `glk_put_string_stream(str, s)` in `next_stream.c`:
  - Already calls `glk_put_char_stream` in a loop, so it works automatically

**Files modified:**
- `nextglk/next_stream.c`
  - Replace `case strtype_File: break;` with actual write calls in
    `glk_put_char_stream`
  - Update `glk_put_buffer_stream` to handle `strtype_File` directly
    (call `nextglk_file_write` instead of looping `glk_put_char_stream`)

**Dependencies:** Milestone 3A.4 (file stream open/close).

**Write path logic for `glk_put_char_stream`:**
```c
case strtype_File:
{
    unsigned char c = (unsigned char)ch;
    nextglk_file_write((NextGlkFile *)st->file, &c, 1);
    break;
}
```

**Write path logic for `glk_put_buffer_stream`:**
```c
case strtype_File:
{
    nextglk_file_write((NextGlkFile *)st->file, buf, len);
    break;
}
```

**Updating writecount:**
Both paths must increment `st->writecount` by the number of bytes written.

**Ownership rules:**
- The `NextGlkFile*` is owned by the stream; output functions do NOT free it
- Writecount is incremented per byte written

**Tests:**
```c
void test_file_stream_write_char() {
    frefid_t fref = glk_fileref_create_by_name(fileusage_Data, "charout", 0);
    strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
    assert(str != NULL);
    
    glk_put_char_stream(str, 'A');
    glk_put_char_stream(str, 'B');
    glk_put_char_stream(str, 'C');
    
    glk_stream_close(str, NULL);
    glk_fileref_destroy(fref);
    
    // Verify contents
    FILE *fp = fopen("charout.glkdata", "rb");
    assert(fp != NULL);
    assert(fgetc(fp) == 'A');
    assert(fgetc(fp) == 'B');
    assert(fgetc(fp) == 'C');
    fclose(fp);
    unlink("charout.glkdata");
}

void test_file_stream_write_buffer() {
    frefid_t fref = glk_fileref_create_by_name(fileusage_Data, "bufout", 0);
    strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
    assert(str != NULL);
    
    const char *data = "Hello, World!";
    glk_put_buffer_stream(str, (char *)data, strlen(data));
    
    glk_stream_close(str, NULL);
    glk_fileref_destroy(fref);
    
    // Verify contents
    FILE *fp = fopen("bufout.glkdata", "rb");
    assert(fp != NULL);
    char readbuf[32] = {0};
    size_t n = fread(readbuf, 1, sizeof(readbuf)-1, fp);
    assert(n == strlen(data));
    assert(strcmp(readbuf, data) == 0);
    fclose(fp);
    unlink("bufout.glkdata");
}

void test_file_stream_writecount() {
    frefid_t fref = glk_fileref_create_by_name(fileusage_Data, "wcount", 0);
    strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
    assert(str != NULL);
    
    stream_result_t result;
    memset(&result, 0, sizeof(result));
    
    glk_put_string_stream(str, "ABCD");
    
    glk_stream_close(str, &result);
    assert(result.writecount >= 4);  // may include stream metadata
    
    glk_fileref_destroy(fref);
    unlink("wcount.glkdata");
}
```

**Runtime validation:**
1. Run all stream output tests — file contents match expected
2. Load a game, type `save` — save file is now created on disk.
   The file may be truncated/incorrect (because position functions are still
   stubs and the Quetzal format needs size backpatching), but the file
   should have data in it.
3. Game remains playable

---

### Milestone 3A.6 — Stream Position Functions (File)

**Objective:** Implement `glk_stream_get_position` and `glk_stream_set_position`
for `strtype_File` streams.

**Functions modified:**
- `glk_stream_get_position(str)` in `nextglk.c`:
  - `case strtype_File:` — use `ftell()` or platform equivalent on
    `((NextGlkFile *)str->file)`
- `glk_stream_set_position(str, pos, seekmode)` in `nextglk.c`:
  - `case strtype_File:` — use `fseek()` or platform equivalent
  - Map `seekmode_Start` → SEEK_SET, `seekmode_Current` → SEEK_CUR,
    `seekmode_End` → SEEK_END

**Files modified:**
- `nextglk/nextglk.c`
  - Update `glk_stream_get_position` and `glk_stream_set_position` to
    handle `strtype_File`

**Dependencies:** Milestone 3A.5 (file stream output).

**Implementation:**
```c
glui32 glk_stream_get_position(strid_t str)
{
    stream_t *st = (stream_t *)str;
    if (!st) return 0;
    
    switch (st->type) {
        case strtype_File: {
            NextGlkFile *nf = (NextGlkFile *)st->file;
            return (glui32)nextglk_file_get_position(nf);
        }
        case strtype_Window:
            // Windows don't have meaningful positions; return writecount
            return st->writecount;
        case strtype_Memory: {
            return (glui32)(st->bufptr - st->buf);
        }
        default:
            return 0;
    }
}

void glk_stream_set_position(strid_t str, glsi32 pos, glui32 seekmode)
{
    stream_t *st = (stream_t *)str;
    if (!st) return;
    
    switch (st->type) {
        case strtype_File: {
            int whence;
            switch (seekmode) {
                case seekmode_Start:   whence = SEEK_SET; break;
                case seekmode_Current: whence = SEEK_CUR; break;
                case seekmode_End:     whence = SEEK_END; break;
                default: return;
            }
            nextglk_file_set_position((NextGlkFile *)st->file, pos, whence);
            break;
        }
        case strtype_Memory: {
            unsigned char *newpos;
            switch (seekmode) {
                case seekmode_Start:
                    newpos = st->buf + pos;
                    break;
                case seekmode_Current:
                    newpos = st->bufptr + pos;
                    break;
                case seekmode_End:
                    newpos = st->bufeof + pos;  // pos is typically negative
                    break;
                default: return;
            }
            if (newpos >= st->buf && newpos <= st->bufend)
                st->bufptr = newpos;
            break;
        }
        case strtype_Window:
        default:
            break;  // no-op for windows
    }
}
```

Need to add `nextglk_file_get_position` and `nextglk_file_set_position` to
`nextglk/nextglk_file.c`:

```c
uint32_t nextglk_file_get_position(NextGlkFile *file) {
    long pos = ftell(file->fp);
    return (pos >= 0) ? (uint32_t)pos : 0;
}

void nextglk_file_set_position(NextGlkFile *file, int32_t pos, int whence) {
    fseek(file->fp, pos, whence);
}
```

**Tests:**
```c
void test_file_stream_position() {
    frefid_t fref = glk_fileref_create_by_name(fileusage_Data, "pos", 0);
    strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
    assert(str != NULL);
    
    glui32 pos = glk_stream_get_position(str);
    assert(pos == 0);  // empty file
    
    glk_put_string_stream(str, "Hello");
    pos = glk_stream_get_position(str);
    assert(pos == 5);
    
    // Seek back to start
    glk_stream_set_position(str, 0, seekmode_Start);
    pos = glk_stream_get_position(str);
    assert(pos == 0);
    
    // Overwrite
    glk_put_string_stream(str, "XYZ");
    pos = glk_stream_get_position(str);
    assert(pos == 3);  // XYZ overwrote first 3 bytes
    
    glk_stream_close(str, NULL);
    glk_fileref_destroy(fref);
    
    // Verify: "XYZlo"
    FILE *fp = fopen("pos.glkdata", "rb");
    assert(fp != NULL);
    char buf[16] = {0};
    fread(buf, 1, 5, fp);
    assert(strcmp(buf, "XYZlo") == 0);
    fclose(fp);
    unlink("pos.glkdata");
}
```

**Runtime validation:**
1. Run tests — position functions work for file streams
2. Load a game, type `save` — save should now produce a valid (or nearly
   valid) Quetzal save file.  The file should be non-empty with correct
   chunk sizes.
3. Game remains playable

---

### Milestone 3A.7 — Fileref Query Functions

**Objective:** Implement `glk_fileref_delete_file` and `glk_fileref_does_file_exist`
with real file system access.

**Functions modified:**
- `glk_fileref_delete_file(fref)` in `nextglk_fileref.c`:
  - Call `unlink(fref->filename)`
- `glk_fileref_does_file_exist(fref)` in `nextglk_fileref.c`:
  - Call `access(fref->filename, F_OK)` or `stat()`

**Files modified:**
- `nextglk/nextglk_fileref.c`
  - Replace stubs with real implementations

**Dependencies:** Milestone 3A.2 (fileref creation — need real `filename`).

**Implementation:**
```c
void glk_fileref_delete_file(frefid_t fref)
{
    fileref_t *f = (fileref_t *)fref;
    if (!f || !f->filename) return;
    unlink(f->filename);
}

glui32 glk_fileref_does_file_exist(frefid_t fref)
{
    fileref_t *f = (fileref_t *)fref;
    if (!f || !f->filename) return 0;
    
#ifdef _WIN32
    return (_access(f->filename, 0) == 0) ? 1 : 0;
#else
    return (access(f->filename, F_OK) == 0) ? 1 : 0;
#endif
}
```

**Tests:**
```c
void test_fileref_delete_file() {
    // Create a real file first
    frefid_t fref = glk_fileref_create_by_name(fileusage_Data, "del", 0);
    strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
    glk_put_string_stream(str, "delete me");
    glk_stream_close(str, NULL);
    
    // File should exist
    assert(glk_fileref_does_file_exist(fref) == 1);
    
    // Delete it
    glk_fileref_delete_file(fref);
    
    // File should no longer exist
    assert(glk_fileref_does_file_exist(fref) == 0);
    
    glk_fileref_destroy(fref);
}
```

**Runtime validation:**
1. Run tests
2. Game remains playable
3. (No visible change in game behaviour — these are supporting functions)

---

### Milestone 3A.8 — Transcript (Echo Stream)

**Objective:** Implement `glk_window_set_echo_stream`, `glk_window_get_echo_stream`,
and modify the window output path to echo to the echo stream.

**Functions implemented:**
- `glk_window_set_echo_stream(win, str)` — set `win->echostr = str`
- `glk_window_get_echo_stream(win)` — return `win->echostr`

**Functions modified:**
- Window output handler in `next_stream.c` (`glk_put_char_stream` for
  `strtype_Window`):
  - After writing to the platform screen, if `st->win && st->win->echostr`,
    also write the character to the echo stream via
    `glk_put_char_stream(st->win->echostr, ch)`

**Files modified:**
- `nextglk/next_window.c`
  - Add `glk_window_set_echo_stream` (at the end of the file)
  - Add `glk_window_get_echo_stream` (at the end of the file)
- `nextglk/next_stream.c`
  - In `glk_put_char_stream`, `case strtype_Window:` — add echo path

**Dependencies:** Milestone 3A.5 (file stream output — echo stream writes
to a file).

**Echo path implementation in `glk_put_char_stream`:**
```c
case strtype_Window:
    /* Route to platform screen layer */
    if (st->win) {
        nextglk_put_char((char)ch);
        
        /* Echo to transcript stream if set */
        if (st->win->echostr) {
            glk_put_char_stream(st->win->echostr, ch);
        }
    }
    break;
```

**Important:** This recursive call to `glk_put_char_stream` is safe because
`st->win->echostr` must be of type `strtype_File` (not `strtype_Window`),
so it cannot recurse.  However, we add a guard:
```c
if (st->win->echostr && st->win->echostr->type != strtype_Window) {
    glk_put_char_stream(st->win->echostr, ch);
}
```

**`glk_put_buffer_stream` for window streams:**
The existing `glk_put_buffer_stream` loops through `glk_put_char_stream` for
window streams, so echo support is automatically inherited.  No change needed
to `glk_put_buffer_stream`.

**Tests:**
```c
void test_echo_stream() {
    // Create transcript fileref + stream
    frefid_t fref = glk_fileref_create_by_name(fileusage_Transcript, "echo", 0);
    strid_t estr = glk_stream_open_file(fref, filemode_Write, 0);
    assert(estr != NULL);
    
    // Get main window
    winid_t win = glk_window_get_root();
    assert(win != NULL);
    
    // Set echo stream
    glk_window_set_echo_stream(win, estr);
    assert(glk_window_get_echo_stream(win) == estr);
    
    // Write to window
    glk_set_window(win);
    glk_put_char('X');
    glk_put_string("Y");
    glk_put_buffer("Z", 1);
    
    // Close echo stream
    glk_window_set_echo_stream(win, NULL);
    glk_stream_close(estr, NULL);
    glk_fileref_destroy(fref);
    
    // Verify transcript file contains "XYZ"
    FILE *fp = fopen("echo.txt", "rb");
    assert(fp != NULL);
    char buf[8] = {0};
    size_t n = fread(buf, 1, sizeof(buf)-1, fp);
    assert(n == 3);
    assert(strcmp(buf, "XYZ") == 0);
    fclose(fp);
    unlink("echo.txt");
}

void test_echo_stream_null() {
    // Setting NULL echo stream should not crash
    winid_t win = glk_window_get_root();
    glk_window_set_echo_stream(win, NULL);
    assert(glk_window_get_echo_stream(win) == NULL);
    
    // Writing to window with NULL echo should not crash
    glk_put_char('A');
}
```

**Runtime validation:**
1. Run all echo stream tests
2. Load a game, type `script` or `transcript` — the game should create a
   transcript file.  Output to the main window should be duplicated to
   the transcript file.
3. Type `script` again to stop transcript — echo stream should be unset
4. Game remains playable

---

### Milestone 3A.9 — Save End-to-End

**Objective:** Ensure the full save path works.  At this point, all required
functions should be real.  This milestone is validation-only (no new functions
unless debugging reveals issues).

**Required functions (all should be real at this point):**

| Function | Milestone |
|---|---|
| `glk_fileref_create_by_prompt` (or `_by_name`) | 3A.2 |
| `glk_fileref_destroy` | 3A.1 |
| `glk_stream_open_file` (Write) | 3A.4 |
| `glk_stream_close` (File) | 3A.4 |
| `glk_stream_set_current` | **Already real (Phase 1)** |
| `glk_stream_get_current` | **Already real (Phase 1)** |
| `glk_put_string_stream` (File) | 3A.5 |
| `glk_put_buffer_stream` (File) | 3A.5 |
| `glk_put_char_stream` (File) | 3A.5 |
| `glk_stream_get_position` (File) | 3A.6 |
| `glk_stream_set_position` (File) | 3A.6 |

**The Git VM's `saveToFile` call sequence** (from `savefile.c` lines 212–315):
1. `glk_stream_get_current()` → save current stream — works
2. `glk_stream_set_current(file)` → redirect to save file — works
3. `glk_put_string("FORM")` → goes to current stream (save file) — works if
   `glk_put_string` routes through `gli_currentstr` (it does in Phase 1)
4. `glk_stream_get_position(file)` → get FORM size position — works (3A.6)
5. `glk_put_string("IFZS")` → writes chunk ID — works
6. `glk_put_string`, `glk_put_buffer`, `glk_put_char` for IFhd, Stks, MAll, CMem
   — all work if they route through current stream
7. `glk_stream_get_position(file)` + `glk_stream_set_position(file)` for
   backpatching sizes — works (3A.6)
8. `glk_stream_set_current(oldFile)` → restore previous stream — works

**Critical check:** Does `glk_put_string`/`glk_put_char`/`glk_put_buffer` in
`nextglk.c` route through `gli_currentstr`?  If they hardcode the window
stream, save will write to the screen instead of the file.

Check `nextglk.c`:
```c
void glk_put_char(unsigned char ch)
{
    if (gli_currentstr) {
        glk_put_char_stream(gli_currentstr, ch);
    }
}
```

If this pattern is used, save works.  If not, this milestone requires fixing
the global output functions to route through `gli_currentstr`.

**Verify `glk_put_string`:**
```c
void glk_put_string(char *s)
{
    if (gli_currentstr) {
        glk_put_string_stream(gli_currentstr, s);
    }
}
```

**No new code needed if these already route through `gli_currentstr`.**
If they don't, this is a Phase 1 fix that must be made before this milestone.

**Tests:**
```c
// Full integration test — write a Quetzal-formatted save file manually
// then verify its structure
void test_integration_save_file_format() {
    frefid_t fref = glk_fileref_create_by_name(fileusage_SavedGame, "testsave", 0);
    strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
    assert(str != NULL);
    
    // Write a minimal Quetzal save file (just FORM/IFZS header)
    glk_stream_set_current(str);
    
    glk_put_string("FORM");
    glui32 sizePos = glk_stream_get_position(str);
    glk_put_buffer((char *)"\x00\x00\x00\x00", 4);  // placeholder size
    glk_put_string("IFZS");
    
    // Write IFhd chunk
    glk_put_string("IFhd");
    glk_put_buffer((char *)"\x00\x00\x00\x80", 4);  // chunk length 128
    // 128 bytes of zero ident
    char zeros[128] = {0};
    glk_put_buffer(zeros, 128);
    
    // Backpatch FORM size
    glui32 endPos = glk_stream_get_position(str);
    glui32 fileSize = endPos - sizePos - 4;
    glk_stream_set_position(str, sizePos, seekmode_Start);
    glk_put_buffer((char *)&fileSize, 4);
    
    glk_stream_close(str, NULL);
    
    // Verify file is valid Quetzal
    FILE *fp = fopen("testsave.glksave", "rb");
    assert(fp != NULL);
    char magic[4];
    fread(magic, 1, 4, fp);
    assert(memcmp(magic, "FORM", 4) == 0);
    
    glui32 fsize;
    fread(&fsize, 4, 1, fp);
    // file size should match expected
    fread(magic, 1, 4, fp);
    assert(memcmp(magic, "IFZS", 4) == 0);
    
    fclose(fp);
    
    glk_fileref_destroy(fref);
    unlink("testsave.glksave");
}
```

**Runtime validation:**
1. Run all tests
2. Load a game, type `save` — the game should now successfully save a
   Quetzal-format file to disk
3. Verify: `od -c nextgit.sav` should show `F O R M` header followed by
   `I F Z S`
4. Type `save` again — file should be overwritten without error
5. Game remains playable after save

---

## Phase 3B — File Input, Restore

### Milestone 3B.1 — Platform File Read I/O

**Objective:** Implement file reading on the platform layer.

**Functions implemented:**
- `nextglk_file_open_read(path)` — open a `NextGlkFile*` for reading
- `nextglk_file_read(file, buffer, length)` — read `length` bytes from file
  into buffer, return number of bytes actually read
- `nextglk_file_get_position(file)` — already implemented in 3A.6 (shared)
- `nextglk_file_set_position(file, pos, whence)` — already implemented in 3A.6 (shared)
- `nextglk_file_close(file)` — already implemented in 3A.3 (shared)

**Files modified:**
- `nextglk/nextglk_file.c`
  - Replace `nextglk_file_open_read` stub with real implementation
  - Replace `nextglk_file_read` stub with real implementation

**Dependencies:** Milestone 3A.6 (position functions already exist).

**Implementation:**
```c
NextGlkFile* nextglk_file_open_read(const char *path) {
    struct NextGlkFile *nf = malloc(sizeof(*nf));
    if (!nf) return NULL;
    nf->fp = fopen(path, "rb");
    if (!nf->fp) { free(nf); return NULL; }
    return nf;
}

uint32_t nextglk_file_read(NextGlkFile *file, void *buffer, uint32_t length) {
    size_t n = fread(buffer, 1, length, file->fp);
    return (uint32_t)n;
}
```

**Tests:**
```c
void test_file_open_read_exists() {
    // Create a file first
    NextGlkFile *wf = nextglk_file_open_write("/tmp/nextgit-test-read.bin");
    assert(wf != NULL);
    nextglk_file_write(wf, "read test data", 14);
    nextglk_file_close(wf);
    
    // Read it back
    NextGlkFile *rf = nextglk_file_open_read("/tmp/nextgit-test-read.bin");
    assert(rf != NULL);
    
    char buf[32] = {0};
    uint32_t n = nextglk_file_read(rf, buf, sizeof(buf)-1);
    assert(n == 14);
    assert(strcmp(buf, "read test data") == 0);
    
    nextglk_file_close(rf);
    unlink("/tmp/nextgit-test-read.bin");
}

void test_file_open_read_not_exists() {
    NextGlkFile *f = nextglk_file_open_read("/tmp/nextgit-nonexistent-file.bin");
    assert(f == NULL);
}
```

**Runtime validation:**
1. Run tests
2. Game remains playable (no visible change yet)
3. Type `restore` — still fails (no file stream open for read)

---

### Milestone 3B.2 — File Stream Open for Read

**Objective:** Extend `glk_stream_open_file` to support `filemode_Read`.

**Functions modified:**
- `glk_stream_open_file(fref, fmode, rock)` in `nextglk.c`:
  - Add `case filemode_Read:` — call `nextglk_file_open_read(fref->filename)`

**Files modified:**
- `nextglk/nextglk.c`
  - Update `glk_stream_open_file` to handle read mode

**Dependencies:** Milestones 3A.4 (stream open infrastructure), 3B.1 (platform
file read).

**Implementation addition to `glk_stream_open_file`:**
```c
if (fmode == filemode_Read) {
    fl = nextglk_file_open_read(fref->filename);
    if (!fl) return NULL;
    readable = 1;
    writable = 0;
}
```

**Tests:**
```c
void test_stream_open_file_read() {
    // Create a file first via write mode
    frefid_t fref = glk_fileref_create_by_name(fileusage_Data, "readtest", 0);
    strid_t wstr = glk_stream_open_file(fref, filemode_Write, 0);
    glk_put_string_stream(wstr, "Hello Reader!");
    glk_stream_close(wstr, NULL);
    
    // Now open for read
    strid_t rstr = glk_stream_open_file(fref, filemode_Read, 0);
    assert(rstr != NULL);
    
    stream_t *s = (stream_t *)rstr;
    assert(s->type == strtype_File);
    assert(s->readable == 1);
    assert(s->writable == 0);
    
    glk_stream_close(rstr, NULL);
    glk_fileref_destroy(fref);
    unlink("readtest.glkdata");
}

void test_stream_open_file_read_nonexistent() {
    frefid_t fref = glk_fileref_create_by_name(fileusage_Data, "nope", 0);
    strid_t str = glk_stream_open_file(fref, filemode_Read, 0);
    assert(str == NULL);  // file doesn't exist
    glk_fileref_destroy(fref);
}
```

**Runtime validation:**
1. Run tests
2. Load a game, type `restore` — game may still fail because read functions
   (get_char_stream etc.) are still stubs
3. Game remains playable

---

### Milestone 3B.3 — File Stream Input Functions

**Objective:** Implement `glk_get_char_stream`, `glk_get_buffer_stream`, and
`glk_get_line_stream` for `strtype_File`.

**Functions modified:**
- `glk_get_char_stream(str)` in `nextglk.c`:
  - `case strtype_File:` — read one byte via `nextglk_file_read`, return it
    (or -1 on EOF/error)
- `glk_get_buffer_stream(str, buf, len)` in `nextglk.c`:
  - `case strtype_File:` — read up to `len` bytes via `nextglk_file_read`,
    return number read
- `glk_get_line_stream(str, buf, len)` in `nextglk.c`:
  - `case strtype_File:` — read until `\n` or `len-1` bytes, null-terminate,
    return number of characters read

**Files modified:**
- `nextglk/nextglk.c`
  - Replace `glk_get_char_stream` stub with real implementation
  - Replace `glk_get_buffer_stream` stub with real implementation
  - Replace `glk_get_line_stream` stub with real implementation

**Dependencies:** Milestones 3B.1 (platform file read), 3B.2 (stream open for read).

**Implementation:**

```c
glsi32 glk_get_char_stream(strid_t str)
{
    stream_t *st = (stream_t *)str;
    if (!st || !st->readable) return -1;
    
    switch (st->type) {
        case strtype_File: {
            unsigned char c;
            uint32_t n = nextglk_file_read((NextGlkFile *)st->file, &c, 1);
            if (n == 0) return -1;  // EOF
            st->readcount++;
            return (glsi32)c;
        }
        case strtype_Memory: {
            if (st->bufptr && st->bufptr < st->bufend) {
                st->readcount++;
                return *(st->bufptr++);
            }
            return -1;
        }
        default:
            return -1;
    }
}

glui32 glk_get_buffer_stream(strid_t str, char *buf, glui32 len)
{
    stream_t *st = (stream_t *)str;
    if (!st || !st->readable || !buf || len == 0) return 0;
    
    switch (st->type) {
        case strtype_File: {
            uint32_t n = nextglk_file_read((NextGlkFile *)st->file, buf, len);
            st->readcount += n;
            return n;
        }
        case strtype_Memory: {
            glui32 remaining = (glui32)(st->bufend - st->bufptr);
            glui32 toread = (len < remaining) ? len : remaining;
            memcpy(buf, st->bufptr, toread);
            st->bufptr += toread;
            st->readcount += toread;
            return toread;
        }
        default:
            return 0;
    }
}

glui32 glk_get_line_stream(strid_t str, char *buf, glui32 len)
{
    stream_t *st = (stream_t *)str;
    if (!st || !st->readable || !buf || len == 0) return 0;
    
    switch (st->type) {
        case strtype_File: {
            glui32 count = 0;
            while (count < len - 1) {
                unsigned char c;
                uint32_t n = nextglk_file_read((NextGlkFile *)st->file, &c, 1);
                if (n == 0) break;  // EOF
                buf[count++] = (char)c;
                st->readcount++;
                if (c == '\n') break;
            }
            buf[count] = '\0';
            return count;
        }
        case strtype_Memory: {
            glui32 count = 0;
            while (count < len - 1 && st->bufptr < st->bufend) {
                char c = (char)*(st->bufptr++);
                buf[count++] = c;
                st->readcount++;
                if (c == '\n') break;
            }
            buf[count] = '\0';
            return count;
        }
        default:
            return 0;
    }
}
```

**Tests:**
```c
void test_file_stream_get_char() {
    frefid_t fref = glk_fileref_create_by_name(fileusage_Data, "getchar", 0);
    strid_t wstr = glk_stream_open_file(fref, filemode_Write, 0);
    glk_put_string_stream(wstr, "ABC");
    glk_stream_close(wstr, NULL);
    
    strid_t rstr = glk_stream_open_file(fref, filemode_Read, 0);
    assert(rstr != NULL);
    
    assert(glk_get_char_stream(rstr) == 'A');
    assert(glk_get_char_stream(rstr) == 'B');
    assert(glk_get_char_stream(rstr) == 'C');
    assert(glk_get_char_stream(rstr) == -1);  // EOF
    
    glk_stream_close(rstr, NULL);
    glk_fileref_destroy(fref);
    unlink("getchar.glkdata");
}

void test_file_stream_get_buffer() {
    frefid_t fref = glk_fileref_create_by_name(fileusage_Data, "getbuf", 0);
    strid_t wstr = glk_stream_open_file(fref, filemode_Write, 0);
    const char *data = "Buffer read test data.";
    glk_put_string_stream(wstr, data);
    glk_stream_close(wstr, NULL);
    
    strid_t rstr = glk_stream_open_file(fref, filemode_Read, 0);
    assert(rstr != NULL);
    
    char buf[32] = {0};
    glui32 n = glk_get_buffer_stream(rstr, buf, sizeof(buf)-1);
    assert(n == strlen(data));
    assert(strcmp(buf, data) == 0);
    
    // Second read should return 0 (EOF)
    n = glk_get_buffer_stream(rstr, buf, 16);
    assert(n == 0);
    
    glk_stream_close(rstr, NULL);
    glk_fileref_destroy(fref);
    unlink("getbuf.glkdata");
}

void test_file_stream_get_line() {
    frefid_t fref = glk_fileref_create_by_name(fileusage_Data, "getline", 0);
    strid_t wstr = glk_stream_open_file(fref, filemode_Write, 0);
    glk_put_string_stream(wstr, "line one\nline two\nline three\n");
    glk_stream_close(wstr, NULL);
    
    strid_t rstr = glk_stream_open_file(fref, filemode_Read, 0);
    assert(rstr != NULL);
    
    char buf[32];
    glui32 n;
    
    n = glk_get_line_stream(rstr, buf, sizeof(buf));
    assert(n == 9);  // "line one\n"
    assert(strncmp(buf, "line one", 8) == 0);
    
    n = glk_get_line_stream(rstr, buf, sizeof(buf));
    assert(n == 9);  // "line two\n"
    
    n = glk_get_line_stream(rstr, buf, sizeof(buf));
    assert(n == 11);  // "line three\n"
    
    n = glk_get_line_stream(rstr, buf, sizeof(buf));
    assert(n == 0);  // EOF
    
    glk_stream_close(rstr, NULL);
    glk_fileref_destroy(fref);
    unlink("getline.glkdata");
}

void test_file_stream_readcount() {
    frefid_t fref = glk_fileref_create_by_name(fileusage_Data, "rcount", 0);
    strid_t wstr = glk_stream_open_file(fref, filemode_Write, 0);
    glk_put_string_stream(wstr, "HELLO");
    glk_stream_close(wstr, NULL);
    
    strid_t rstr = glk_stream_open_file(fref, filemode_Read, 0);
    stream_result_t result;
    memset(&result, 0, sizeof(result));
    
    char buf[16] = {0};
    glk_get_buffer_stream(rstr, buf, 5);
    
    glk_stream_close(rstr, &result);
    assert(result.readcount >= 5);
    
    glk_fileref_destroy(fref);
    unlink("rcount.glkdata");
}
```

**Runtime validation:**
1. Run all input tests
2. Load a game, type `restore` — game should now be able to read a save file.
   If a save file exists, restoration should proceed (may still fail due to
   format issues in the save file from Phase 3A, but the read functions work)
3. Game remains playable

---

### Milestone 3B.4 — Restore End-to-End

**Objective:** Ensure the full restore path works.  Validation-only milestone.

**The Git VM's `restoreFromFile` call sequence** (from `savefile.c` lines 29–210):
1. `glk_stream_get_position(file)` → get file start — works (3A.6)
2. `glk_get_buffer_stream` or `glk_get_char_stream` via `readWord()` — works (3B.3)
3. `glk_stream_get_position(file)` → track read bounds — works (3A.6)
4. `glk_get_char_stream(file)` → read ident bytes — works (3B.3)
5. `glk_get_buffer_stream` for chunk data — works (3B.3)
6. `glk_stream_set_position(file, ..., seekmode_Current)` → skip unknown chunks
   — works (3A.6)

**Windows/non-file stream output:** The restore process writes to VM memory
directly (via the `savefile.c` functions), NOT through Glk output functions.
So the existing window/output infrastructure is not needed for restore.

**Potential issues:**
1. The Glulx `@restore` opcode calls `git_find_stream_by_id()` which looks
   up the stream ID in the dispatch-layer object registry.  If the file stream
   was not properly registered by `gli_new_stream`, the lookup fails.  This
   is handled by the existing stream lifecycle code.
2. The game must call `glk_fileref_create_by_prompt` + `glk_stream_open_file`
   before the `@restore` opcode.  If either returns NULL, the game's template
   code prints "Restore failed."  Both are real now.

**Tests:**
```c
// Full save+restore cycle at the Glk level
void test_integration_save_restore_cycle() {
    // Save: write known data
    frefid_t sfref = glk_fileref_create_by_name(fileusage_SavedGame, "cycle", 0);
    strid_t sstr = glk_stream_open_file(sfref, filemode_Write, 0);
    
    glk_stream_set_current(sstr);
    glk_put_string("FORM");
    glui32 sizePos = glk_stream_get_position(sstr);
    glk_put_buffer((char *)"\x00\x00\x00\x00", 4);
    glk_put_string("IFZS");
    // Write IFhd
    glk_put_string("IFhd");
    glk_put_buffer((char *)"\x00\x00\x00\x80", 4);
    char ident[128] = {0};
    glk_put_buffer(ident, 128);
    // Backpatch
    glui32 endPos = glk_stream_get_position(sstr);
    glui32 fsize = endPos - sizePos - 4;
    glk_stream_set_position(sstr, sizePos, seekmode_Start);
    glk_put_buffer((char *)&fsize, 4);
    glk_stream_set_position(sstr, endPos, seekmode_Start);
    
    glk_stream_close(sstr, NULL);
    glk_fileref_destroy(sfref);
    
    // Restore: read it back
    frefid_t rfref = glk_fileref_create_by_name(fileusage_SavedGame, "cycle", 0);
    strid_t rstr = glk_stream_open_file(rfref, filemode_Read, 0);
    assert(rstr != NULL);
    
    // Read FORM magic
    char magic[4];
    assert(glk_get_buffer_stream(rstr, magic, 4) == 4);
    assert(memcmp(magic, "FORM", 4) == 0);
    
    // Read file size
    char sizes[4];
    assert(glk_get_buffer_stream(rstr, sizes, 4) == 4);
    
    // Read IFZS magic
    assert(glk_get_buffer_stream(rstr, magic, 4) == 4);
    assert(memcmp(magic, "IFZS", 4) == 0);
    
    // Read IFhd chunk
    assert(glk_get_buffer_stream(rstr, magic, 4) == 4);
    assert(memcmp(magic, "IFhd", 4) == 0);
    
    char chksize[4];
    assert(glk_get_buffer_stream(rstr, chksize, 4) == 4);
    
    glui32 chunkLen;
    memcpy(&chunkLen, chksize, 4);
    assert(chunkLen == 128);
    
    char chunkData[128];
    assert(glk_get_buffer_stream(rstr, chunkData, 128) == 128);
    
    glk_stream_close(rstr, NULL);
    glk_fileref_destroy(rfref);
    unlink("cycle.glksave");
}
```

**Runtime validation:**
1. Run all integration tests
2. Load a game, type `save` — creates `nextgit.sav`
3. Type `restore` — should load the saved game state from `nextgit.sav`
4. Verify: game state after restore matches game state before save
5. Game remains playable after restore

---

## Minimum Implementation Summary

### Minimum for Transcript

| # | Function | File | Why essential |
|---|----------|------|-------------|
| 1 | `gli_new_fileref` / `gli_delete_fileref` | `nextglk_fileref.c` | Fileref lifecycle |
| 2 | `glk_fileref_create_by_name` | `nextglk_fileref.c` | Create transcript fileref |
| 3 | `glk_fileref_create_by_prompt` | `nextglk_fileref.c` | Game calls this for transcript |
| 4 | `glk_fileref_destroy` | `nextglk_fileref.c` | Cleanup |
| 5 | `nextglk_file_open_write` | `nextglk_file.c` | Open transcript file |
| 6 | `nextglk_file_write` | `nextglk_file.c` | Write transcript output |
| 7 | `nextglk_file_close` | `nextglk_file.c` | Close transcript file |
| 8 | `glk_stream_open_file` (Write) | `nextglk.c` | Open file stream |
| 9 | `glk_put_char_stream` (File) | `next_stream.c` | Write characters to file |
| 10 | `glk_window_set_echo_stream` | `next_window.c` | Register echo stream |
| 11 | Echo path in window output handler | `next_stream.c` | Duplicate output to echo stream |

**Total: 11 functions (6 new, 5 modifications to existing)**

### Minimum for Save

| # | Function | File | Why essential |
|---|----------|------|-------------|
| 1 | `gli_new_fileref` / `gli_delete_fileref` | `nextglk_fileref.c` | Fileref lifecycle |
| 2 | `glk_fileref_create_by_name` | `nextglk_fileref.c` | Create save fileref (or by_prompt) |
| 3 | `glk_fileref_create_by_prompt` | `nextglk_fileref.c` | Game calls this for save |
| 4 | `glk_fileref_destroy` | `nextglk_fileref.c` | Cleanup |
| 5 | `nextglk_file_open_write` | `nextglk_file.c` | Open save file |
| 6 | `nextglk_file_write` | `nextglk_file.c` | Write save data |
| 7 | `nextglk_file_close` | `nextglk_file.c` | Close save file |
| 8 | `glk_stream_open_file` (Write) | `nextglk.c` | Open file stream |
| 9 | `glk_stream_close` (File) | `nextglk.c` | Close file stream |
| 10 | `glk_put_char_stream` (File) | `next_stream.c` | Write bytes to save file |
| 11 | `glk_put_buffer_stream` (File) | `next_stream.c` | Write buffers to save file |
| 12 | `glk_put_string_stream` (File) | `next_stream.c` | Write chunk IDs to save file |
| 13 | `glk_stream_get_position` (File) | `nextglk.c` | Calculate chunk sizes |
| 14 | `glk_stream_set_position` (File) | `nextglk.c` | Backpatch chunk sizes |

**Total: 14 functions (6 new, 8 modifications to existing stubs)**

### Minimum for Restore

Everything required for Save, **plus**:

| # | Function | File | Why essential |
|---|----------|------|-------------|
| 15 | `nextglk_file_open_read` | `nextglk_file.c` | Open save file for reading |
| 16 | `nextglk_file_read` | `nextglk_file.c` | Read save data |
| 17 | `glk_stream_open_file` (Read) | `nextglk.c` | Open file stream for reading |
| 18 | `glk_get_char_stream` (File) | `nextglk.c` | Read individual bytes |
| 19 | `glk_get_buffer_stream` (File) | `nextglk.c` | Read chunk data |

**Total: 19 functions (9 modifications to existing stubs + positions from Save)**

---

## Recommended Implementation Order

```
Phase 3A:

  3A.1  ──  Fileref struct + lifecycle (gli_new_fileref, gli_delete_fileref,
            gli_filereflist, glk_fileref_iterate)
     │
     ▼
  3A.2  ──  Fileref creation (glk_fileref_create_by_name, _by_prompt, _temp,
            _from_fileref, _get_rock)
     │
     ▼
  3A.3  ──  Platform file write I/O (nextglk_file_open_write, _write, _close,
            _append)
     │
     ▼
  3A.4  ──  File stream open/close (glk_stream_open_file Write,
            glk_stream_close File)
     │
     ▼
  3A.5  ──  File stream output (glk_put_char_stream File,
            glk_put_buffer_stream File)
     │
     ▼
  3A.6  ──  File stream position (glk_stream_get_position File,
            glk_stream_set_position File)
     │
     ▼
  3A.7  ──  Fileref query (glk_fileref_delete_file, _does_file_exist)
     │
     ▼
  3A.8  ──  Transcript (glk_window_set_echo_stream, glk_window_get_echo_stream,
            echo path in window output)
     │
     ▼
  3A.9  ──  Save end-to-end validation

Phase 3B:

  3B.1  ──  Platform file read I/O (nextglk_file_open_read, _read)
     │
     ▼
  3B.2  ──  File stream open for read (glk_stream_open_file Read)
     │
     ▼
  3B.3  ──  File stream input (glk_get_char_stream File,
            glk_get_buffer_stream File, glk_get_line_stream File)
     │
     ▼
  3B.4  ──  Restore end-to-end validation
```

Ordering rationale:
1. Fileref lifecycle first — everything depends on being able to create
   and destroy filerefs.
2. Fileref creation next — games need filerefs before they can open streams.
3. Platform write I/O — needed before file streams can do anything useful.
4. Stream open/close for write — the bridge between filerefs and file I/O.
5. Stream output — the data path for save and transcript.
6. Stream position — required for Quetzal format backpatching in save.
7. Fileref query — nice-to-have but needed by some games.
8. Transcript — builds on write infrastructure, adds echo stream wiring.
9. Save validation — verify the complete save path works.
10. Platform read I/O — needed before restore can work.
11. Stream open for read — bridge between filerefs and file read I/O.
12. Stream input — the data path for restore.
13. Restore validation — verify the complete restore path works.

---

## Key Risk Areas

### 1. Stream ID lookup in Git VM

The Git VM's `savefile.c` calls `git_find_stream_by_id()` to convert a Glulx
`glui32` stream ID to a `strid_t`.  This function uses the dispatch-layer
object registry.  If the file stream is not properly registered by
`gli_new_stream`, the lookup returns 0 and save/restore immediately fails.

**Mitigation:** Ensure `gli_new_stream` calls `gli_register_obj` as documented
in `docs/glk-object-lifecycle.md`.  The existing stream lifecycle code already
does this for window streams; verify it also works for file streams.

### 2. Global output function routing

`glk_put_string`, `glk_put_char`, `glk_put_buffer` must route through
`gli_currentstr`.  If they hardcode the window stream, save writes to
the screen instead of the file.

**Mitigation:** Verify in `nextglk.c` that the global output functions use
`gli_currentstr`.  If not, fix before Milestone 3A.9.

### 3. Binary mode on the ZX Spectrum Next

The Next filesystem may need explicit binary mode.  The Linux implementation
uses `fopen(..., "wb")` / `fopen(..., "rb")`.  The Next platform layer
must handle binary vs text mode appropriately.

**Mitigation:** Abstract `nextglk_file_open_read` / `nextglk_file_open_write`
behind the platform layer.  The Linux implementations handle binary mode
correctly; the Next implementations will be filled in during platform porting.

### 4. File paths on the ZX Spectrum Next

The Next uses a different filesystem (NextZXOS on SD card).  File paths will
need to be converted from POSIX-style to Next-style paths.  The Glk spec
recommends short filenames (8.3 format) for portability.

**Mitigation:** The fileref creation functions already sanitise filenames.
The platform layer (`nextglk_file_open_read` / `nextglk_file_open_write`)
will be the place where path translation happens for the Next.

### 5. Save file integrity

A partially-written save file (if the game crashes during save) could leave
a corrupted file.  This is a quality-of-life issue, not a correctness issue.

**Mitigation:** Deferred.  The minimum implementation writes directly to the
target file.  A future enhancement could write to a temp file then rename.

---

## Appendix A: Full Function Manifest

Every function that must exist (real, not stub) for save/restore/transcript
to work in the minimum configuration.  Strikethrough = already real.

| Function | Real in Phase? | Notes |
|----------|---------------|-------|
| `gli_new_fileref` | **3A.1** | New internal function |
| `gli_delete_fileref` | **3A.1** | New internal function |
| `glk_fileref_iterate` | **3A.1** | Was stub |
| ~~`glk_fileref_get_rock`~~ | 3A.2 | Real via struct field |
| `glk_fileref_create_by_name` | **3A.2** | Was stub |
| `glk_fileref_create_by_prompt` | **3A.2** | Was stub |
| `glk_fileref_create_temp` | **3A.2** | Was stub |
| `glk_fileref_create_from_fileref` | **3A.2** | Was stub |
| `glk_fileref_destroy` | **3A.2** | Was stub |
| `nextglk_file_open_write` | **3A.3** | Was stub |
| `nextglk_file_append` | **3A.3** | New function |
| `nextglk_file_write` | **3A.3** | Was stub |
| `nextglk_file_close` | **3A.3** | Was stub |
| `glk_stream_open_file` | **3A.4** | Was stub |
| `glk_stream_close` (File) | **3A.4** | Was real only for strtype_Window |
| `glk_put_char_stream` (File) | **3A.5** | Was no-op for File |
| `glk_put_buffer_stream` (File) | **3A.5** | Was no-op for File |
| `glk_put_string_stream` (File) | **3A.5** | Was no-op for File |
| `nextglk_file_get_position` | **3A.6** | New function |
| `nextglk_file_set_position` | **3A.6** | New function |
| `glk_stream_get_position` | **3A.6** | Was stub |
| `glk_stream_set_position` | **3A.6** | Was stub |
| `glk_fileref_delete_file` | **3A.7** | Was stub |
| `glk_fileref_does_file_exist` | **3A.7** | Was stub |
| `glk_window_set_echo_stream` | **3A.8** | Was missing entirely |
| `glk_window_get_echo_stream` | **3A.8** | Was missing entirely |
| `nextglk_file_open_read` | **3B.1** | Was stub |
| `nextglk_file_read` | **3B.1** | Was stub |
| `glk_get_char_stream` (File) | **3B.3** | Was stub |
| `glk_get_buffer_stream` (File) | **3B.3** | Was stub |
| `glk_get_line_stream` (File) | **3B.3** | Was stub |

---

## Appendix B: Existing Real Functions (Phase 1 & 2)

These functions are already real and do not need modification:

- `glk_stream_get_current` — returns `gli_currentstr`
- `glk_stream_set_current` — sets `gli_currentstr`
- `glk_put_char` — routes through `gli_currentstr`
- `glk_put_string` — routes through `gli_currentstr`
- `glk_put_buffer` — routes through `gli_currentstr`
- `glk_put_char_stream` (Window) — writes to platform screen
- `glk_put_buffer_stream` (Window) — loops `glk_put_char_stream`
- `glk_put_string_stream` (Window) — loops `glk_put_char_stream`
- `gli_new_stream` — allocates, inserts into list, registers with dispatch
- `gli_delete_stream` — unregisters, unlinks, frees
- `glk_stream_iterate` — walks `gli_streamlist`