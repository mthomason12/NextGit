# Save/Restore/Transcript Investigation

## Introduction

This document traces the exact Glk call paths used by Git and Inform 7 when
the player types `save`, `restore`, `script` or `transcript`.  It identifies
the first failing function in each path, documents every NextGlk function
that must be real for the feature to succeed, and ranks the minimum set of
functions required by priority.

> **Methodology:**  All observations are based on static analysis of the
> source files.  No runtime trace was performed.  The conclusions are drawn
> from:
>
> - `thirdparty/git/terp.c`        — opcode dispatch for save/restore
> - `thirdparty/git/savefile.c`    — Quetzal save/restore serialisation
> - `thirdparty/git/glkop.c`       — Glulx→Glk dispatch bridge
> - `nextglk/gi_dispa.c`           — function table and dispatch layer
> - `nextglk/glk.h`               — API signatures
> - `nextglk/nextglk_fileref.c`   — fileref stubs
> - `nextglk/nextglk_file.c`      — platform file I/O stubs
> - `nextglk/nextglk.c`           — stream open / read stubs
> - `nextglk/next_stream.c`       — stream output functions
> - `nextglk/next_window.c`       — window lifecycle

---

## 1. SAVE — Exact Call Sequence

### 1.1  High-level flow

The player types `save`.  Inform 7's typed command parser matches the
`save` action, which triggers the I7 template code (compiled into the game
file's Glulx code).  The template calls:

```
glk_fileref_create_by_prompt(fileusage_SavedGame, filemode_Write, rock)
glk_stream_open_file(fref, filemode_Write, rock)
  [Glulx @save opcode with stream-id]
glk_stream_close(str, result)
glk_fileref_destroy(fref)
```

(The I7 template may use `glk_fileref_create_by_name` if the game has a
fixed filename.  Most I7 games use `_create_by_prompt` because the Glk
spec says saved games should prompt the user for a filename.)

### 1.2  Git VM side

For the `@save` Glulx opcode:

**File:**  `thirdparty/git/terp.c`
**Labels:** `do_save_stub_discard`, `do_save_stub_addr`, `do_save_stub_local`,
  `do_save_stub_stack`, `finish_save_stub` (lines 651–682)

```
finish_save_stub:
    PUSH (READ_PC);                        // PC
    PUSH ((frame - base) * 4);             // FramePtr
    if (gIoMode == IO_GLK)
        S1 = saveToFile (base, sp, L1);   // L1 = stream ID from game
    else
        S1 = 1;                            // fail in non-GLK mode
    goto do_pop_call_stub;
```

`saveToFile()` (in `savefile.c`, line 212) receives the **stream ID** (`L1`),
not a fileref.  The I7 game code must have already called
`glk_fileref_create_by_prompt()` + `glk_stream_open_file()` to obtain
that stream ID before executing the `@save` opcode.

Inside `saveToFile()` (savefile.c lines 212–315):

```
saveToFile(base, sp, id):
  file = git_find_stream_by_id(id);      // lookup stream in dispatch table
  if (file == 0) return 1;               // FAILS if stream never opened

  oldFile = glk_stream_get_current();    // save current stream
  glk_stream_set_current(file);          // redirect output to save file

  glk_put_string("FORM");               // write IFF magic
  fileSizePos = glk_stream_get_position(file);
  writeWord(0);                          // placeholder for file size
  glk_put_string("IFZS");

  // write IFhd (ident) chunk:
  glk_put_string("IFhd");
  writeWord(128);
  glk_put_buffer((char *)gInitMem, 128);

  // write Stks (stack) chunk:
  glk_put_string("Stks");
  writeWord(...);  for (...) writeWord(base[n]);

  // write MAll (heap) chunk:
  glk_put_string("MAll");
  writeWord(...);  for (...) writeWord(heap[n]);

  // write CMem (memory) chunk:
  glk_put_string("CMem");
  writeWord(0);  writeWord(gEndMem);
  for (...) { glk_put_char(0); glk_put_char(...); glk_put_char(c); }

  // backpatch sizes:
  fileSize = glk_stream_get_position(file) - fileSizePos - 4;
  glk_stream_set_position(file, fileSizePos, seekmode_Start);
  writeWord(fileSize);                            // patch FORM size
  glk_stream_set_position(file, memSizePos, seekmode_Start);
  writeWord(memSize);                             // patch CMem size

  glk_stream_set_current(oldFile);               // restore current stream
  return 0;
```

### 1.3  Complete Glk function call chain for SAVE

| Step | Glk Function | Dispatch ID | Role | NextGlk Status |
|------|-------------|-------------|------|----------------|
| 1 | `glk_fileref_create_by_prompt` | 0x0062 | Prompt user for save filename → returns `frefid_t` | **STUB** — returns NULL |
| 2 | `glk_stream_open_file` | 0x0042 | Open file for writing → returns `strid_t` | **STUB** — returns NULL |
| 3 | `glk_fileref_destroy` | 0x0063 | Clean up fileref | **STUB** — no-op |
| 4 | `glk_stream_set_current` | 0x0047 | Redirect output to save stream | **REAL** |
| 5 | `glk_stream_get_current` | 0x0048 | Save current stream before redirect | **REAL** |
| 6 | `glk_put_string` | 0x0082 | Write "FORM", "IFZS", chunk IDs | **REAL** (routes to current stream) |
| 7 | `glk_stream_get_position` | 0x0046 | Get current write position | **STUB** — returns 0 |
| 8 | `glk_put_buffer` | 0x0084 | Write 128-byte IFhd ident | **REAL** |
| 9 | `glk_put_char` | 0x0080 | Write individual bytes | **REAL** |
| 10 | `glk_stream_set_position` | 0x0045 | Seek back to patch file/memory sizes | **STUB** — no-op |
| 11 | `glk_stream_close` | 0x0044 | Close the save file stream | **REAL** (for strtype_Window; untested for strtype_File) |

---

## 2. RESTORE — Exact Call Sequence

### 2.1  High-level flow

The player types `restore`.  I7 template code calls:

```
glk_fileref_create_by_prompt(fileusage_SavedGame, filemode_Read, rock)
glk_stream_open_file(fref, filemode_Read, rock)
  [Glulx @restore opcode with stream-id]
glk_stream_close(str, result)
glk_fileref_destroy(fref)
```

### 2.2  Git VM side

**File:** `thirdparty/git/terp.c`, label `do_restore` (lines 684–693)

```
do_restore:
    if (gIoMode == IO_GLK
     && restoreFromFile (base, L1, protectPos, protectSize) == 0)
    {
        sp = gStackPointer;
        S1 = -1;
        goto do_pop_call_stub;
    }
    S1 = 1;
    NEXT;
```

`restoreFromFile()` (savefile.c lines 29–210) reads the Quetzal save file:

```
restoreFromFile(base, id, protectPos, protectSize):
  file = git_find_stream_by_id(id);      // lookup stream
  if (file == 0) return 1;

  // read IFF header:
  if (readWord(file) != "FORM") return 1;
  fileSize = readWord(file);                     // uses glk_get_buffer_stream
  fileStart = glk_stream_get_position(file);
  if (readWord(file) != "IFZS") return 1;

  // read chunks:
  while (glk_stream_get_position(file) < fileStart + fileSize) {
    chunkType = readWord(file);
    chunkSize = readWord(file);

    if (chunkType == "IFhd") {
      for (i=0; i<128; i++)
        c = glk_get_char_stream(file);          // compare ident bytes
    }
    else if (chunkType == "Stks") {
      for (; chunkSize>0; chunkSize -= 4)
        *gStackPointer++ = readWord(file);      // uses glk_get_buffer_stream
    }
    else if (chunkType == "CMem") {
      // ... RLE-decompress memory from glk_get_char_stream()
      // ... skip padding with glk_get_char_stream()
    }
    else if (chunkType == "MAll") {
      for (...) heap[i] = readWord(file);
    }
    else {
      glk_stream_set_position(file, ..., seekmode_Current);  // skip unknown
    }
  }
```

### 2.3  Complete Glk function call chain for RESTORE

| Step | Glk Function | Dispatch ID | Role | NextGlk Status |
|------|-------------|-------------|------|----------------|
| 1 | `glk_fileref_create_by_prompt` | 0x0062 | Prompt for restore filename → returns `frefid_t` | **STUB** — returns NULL |
| 2 | `glk_stream_open_file` | 0x0042 | Open file for reading → returns `strid_t` | **STUB** — returns NULL |
| 3 | `glk_fileref_destroy` | 0x0063 | Clean up fileref | **STUB** — no-op |
| 4 | `glk_get_buffer_stream` | 0x0092 | Read IFF header words and chunk data | **STUB** — returns 0 |
| 5 | `glk_stream_get_position` | 0x0046 | Track read position (file bounds) | **STUB** — returns 0 |
| 6 | `glk_get_char_stream` | 0x0090 | Read individual bytes (ident, RLE) | **STUB** — returns -1 |
| 7 | `glk_stream_set_position` | 0x0045 | Skip unknown chunks | **STUB** — no-op |
| 8 | `glk_stream_close` | 0x0044 | Close the restore file stream | **REAL** (for strtype_Window) |

---

## 3. SCRIPT / TRANSCRIPT — Exact Call Sequence

### 3.1  High-level flow

The player types `script` or `transcript`.  I7 template code calls:

```
glk_fileref_create_by_prompt(fileusage_Transcript, filemode_Write, rock)
glk_stream_open_file(fref, filemode_Write, rock)
glk_window_set_echo_stream(mainwin, transcript_stream)
```

Once the echo stream is set, every character written to the main window is
also written to the echo stream.  The Git VM is **not involved** in
transcript output beyond the normal `glk_put_char_stream()` /
`glk_put_buffer_stream()` calls for window output — the echo stream
duplication happens inside the Glk library.

### 3.2  Git VM side

There is no Git-side code for transcript.  It is purely a Glk library
feature.

### 3.3  Complete Glk function call chain for TRANSCRIPT

| Step | Glk Function | Dispatch ID | Role | NextGlk Status |
|------|-------------|-------------|------|----------------|
| 1 | `glk_fileref_create_by_prompt` | 0x0062 | Prompt for transcript filename | **STUB** — returns NULL |
| 2 | `glk_stream_open_file` | 0x0042 | Open transcript file for writing | **STUB** — returns NULL |
| 3 | `glk_window_set_echo_stream` | 0x002D | Register echo stream on main window | **MISSING** — not implemented |
| 4 | `glk_put_char_stream` (→strtype_File) | 0x0081 | Write echoed output to file | **STUB** — (deferred to Phase 3) |
| 5 | `glk_put_buffer_stream` (→strtype_File) | 0x0085 | Write buffers to transcript file | **STUB** — (deferred to Phase 3) |
| 6 | `glk_put_string_stream` (→strtype_File) | 0x0083 | Write strings to transcript file | **STUB** — (deferred to Phase 3) |
| 7 | `glk_stream_close` | 0x0044 | Close transcript stream | **REAL** |
| 8 | `glk_fileref_destroy` | 0x0063 | Clean up fileref | **STUB** — no-op |

---

## 4. Root Cause Analysis

### 4.1  What fails first?

All three features fail at the **same first step**:

```
glk_fileref_create_by_prompt(usage, fmode, rock) → NULL
```

**File:** `nextglk/nextglk_fileref.c`, lines 41–49
**Status:** compileable stub

```
frefid_t glk_fileref_create_by_prompt(glui32 usage, glui32 fmode, glui32 rock)
{
    (void)usage;
    (void)fmode;
    (void)rock;

    /* Stub — returns NULL. */
    return NULL;
}
```

When this returns NULL, the I7 game code receives a `frefid_t` of NULL.
It then tries `glk_stream_open_file(NULL, ...)` which also returns NULL
(the stub in `nextglk.c` line 665).  At that point the game prints
"Save failed." / "Restore failed." / "Attempt to begin transcript failed."

### 4.2  Even if fileref were real — the next blocker

If `glk_fileref_create_by_prompt()` were made real, the next blocker is:

```
glk_stream_open_file(fref, fmode, rock) → NULL
```

**File:** `nextglk/nextglk.c`, lines 665–671
**Status:** compileable stub

```
strid_t glk_stream_open_file(frefid_t fileref, glui32 fmode, glui32 rock)
{
    (void)fileref;
    (void)fmode;
    (void)rock;
    return NULL;
}
```

### 4.3  Behind the stream functions

The platform file I/O layer that `glk_stream_open_file()` would call is
also entirely stub:

**File:** `nextglk/nextglk_file.c`, lines 8–51
**Status:** compileable stubs

- `nextglk_file_open_read()` → returns 0
- `nextglk_file_open_write()` → returns 0
- `nextglk_file_read()` → returns 0
- `nextglk_file_write()` → returns 0
- `nextglk_file_close()` → no-op

---

## 5. Function Status Summary

| Function | File | Ownership | Real? | Stub? | Missing? | Boot Blocker? | Save Blocker? | Transcript Blocker? |
|----------|------|-----------|:-----:|:-----:|:---------:|:-------------:|:-------------:|:-------------------:|
| `glk_fileref_create_by_prompt` | `nextglk_fileref.c` | NextGlk | | ✓ | | | **#1** | **#1** |
| `glk_fileref_create_by_name` | `nextglk_fileref.c` | NextGlk | | ✓ | | | fallback | fallback |
| `glk_fileref_create_temp` | `nextglk_fileref.c` | NextGlk | | ✓ | | | | |
| `glk_fileref_create_from_fileref` | `nextglk_fileref.c` | NextGlk | | ✓ | | | | |
| `glk_fileref_destroy` | `nextglk_fileref.c` | NextGlk | | ✓ | | | ✓ | ✓ |
| `glk_fileref_delete_file` | `nextglk_fileref.c` | NextGlk | | ✓ | | | | |
| `glk_fileref_does_file_exist` | `nextglk_fileref.c` | NextGlk | | ✓ | | | | |
| `glk_fileref_iterate` | `nextglk_fileref.c` | NextGlk | | ✓ | | | | |
| `glk_fileref_get_rock` | `nextglk_fileref.c` | NextGlk | | ✓ | | | | |
| `glk_stream_open_file` | `nextglk.c` | NextGlk | | ✓ | | | **#2** | **#2** |
| `glk_stream_set_position` | `nextglk.c` | NextGlk | | ✓ | | | **#3** | |
| `glk_stream_get_position` | `nextglk.c` | NextGlk | | ✓ | | | **#3** | |
| `glk_get_char_stream` | `nextglk.c` | NextGlk | | ✓ | | | | rest. #2 |
| `glk_get_buffer_stream` | `nextglk.c` | NextGlk | | ✓ | | | | rest. #2 |
| `glk_get_line_stream` | `nextglk.c` | NextGlk | | ✓ | | | | |
| `glk_put_char_stream` (File) | `next_stream.c` | NextGlk | | ✓ | | | **#4** | **#3** |
| `glk_put_buffer_stream` (File) | `next_stream.c` | NextGlk | | ✓ | | | **#4** | **#3** |
| `glk_put_string_stream` (File) | `next_stream.c` | NextGlk | | ✓ | | | **#4** | **#3** |
| `glk_window_set_echo_stream` | — | NextGlk | | | **✓** | | | **#4** |
| `nextglk_file_open_read` | `nextglk_file.c` | NextGlk | | ✓ | | | | |
| `nextglk_file_open_write` | `nextglk_file.c` | NextGlk | | ✓ | | | | |
| `nextglk_file_read` | `nextglk_file.c` | NextGlk | | ✓ | | | | |
| `nextglk_file_write` | `nextglk_file.c` | NextGlk | | ✓ | | | | |
| `nextglk_file_close` | `nextglk_file.c` | NextGlk | | ✓ | | | | |

---

## 6. Architecture: Where Each Function Lives

### 6.1  Fileref lifecycle (all in `nextglk/nextglk_fileref.c`)

NextGlk owns fileref objects.  They represent a reference to a file on
the file system.  The current implementation defines a `fileref_t` struct
(via typedef) but no actual instances are ever created because all
creation functions return NULL.

### 6.2  File stream I/O (in `nextglk/nextglk.c` and `nextglk/next_stream.c`)

`glk_stream_open_file()` creates a `stream_t` with `type = strtype_File`
and an associated platform file handle.  The platform handle is managed
by functions in `nextglk/nextglk_file.c`.

When writing to a file stream:
- `glk_put_char_stream()` → `next_stream.c` line 254: `case strtype_File: break;` (no-op)
- `glk_put_buffer_stream()` → same, calls `glk_put_char_stream()` for each byte → no-op

When reading from a file stream:
- `glk_get_char_stream()` → `nextglk.c` line 601: returns -1 (stub)
- `glk_get_buffer_stream()` → `nextglk.c` line 643: returns 0 (stub)

File positions:
- `glk_stream_get_position()` → `nextglk.c` line 583: returns 0 (stub)
- `glk_stream_set_position()` → `nextglk.c` line 574: no-op (stub)

### 6.3  Echo stream (transcript)

`glk_window_set_echo_stream()` — **is not implemented anywhere**.
The `window_t` struct in `nextglk_internal.h` has an `echostr` field
(line 78, `stream_t *echostr`), and `gli_new_window()` initialises it
to NULL (line 53 of `next_window.c`).  But no setter/getter function
is implemented.

The transcript output path works as follows:
1. Game writes output to window → `glk_put_char_stream(window_str, ch)`
2. The window stream's `put_char_stream` handler must also write to
   `window->echostr` if it is non-NULL
3. Currently the window output code in `next_stream.c` does not check
   `echostr` at all — it only calls `nextglk_put_char()`

---

## 7. Ranked Priority — Minimum Functions Required

### 7.1  To make SAVE succeed

```
Priority | Function                      | File                  | What it unlocks
---------+-------------------------------+-----------------------+---------------------------
   P0    | glk_fileref_create_by_prompt   | nextglk_fileref.c     | Game can obtain a fileref
   P0    | glk_fileref_destroy            | nextglk_fileref.c     | Game can free the fileref
   P1    | glk_stream_open_file (Write)   | nextglk.c             | Game can obtain a write stream
   P1    | glk_stream_close (File)        | nextglk.c/next_window.c| Game can close the file
   P2    | nextglk_file_open_write        | nextglk_file.c        | Platform opens a file for writing
   P2    | nextglk_file_write             | nextglk_file.c        | Platform writes bytes to file
   P2    | nextglk_file_close             | nextglk_file.c        | Platform closes the file
   P3    | glk_stream_set_position        | nextglk.c             | saveToFile seeks back to patch sizes
   P3    | glk_stream_get_position        | nextglk.c             | saveToFile calculates chunk sizes
   P3    | glk_put_char_stream (File)     | next_stream.c         | saveToFile writes bytes to file
   P3    | glk_put_buffer_stream (File)   | next_stream.c         | saveToFile writes ident chunk
   P3    | glk_put_string_stream (File)   | next_stream.c         | saveToFile writes chunk IDs
```

**Minimum viable shortcut:**  Make `glk_fileref_create_by_prompt()` return
a valid `frefid_t` that internally uses a fixed filename (e.g.
`"nextgit.sav"`), and implement `glk_stream_open_file()` plus the
platform file I/O layer for writing.  The position functions and file
output functions in next_stream.c must also be made real.  Estimated:
**5–6 functions must become real** for a minimal working save.

### 7.2  To make RESTORE succeed

```
Priority | Function                      | File                  | What it unlocks
---------+-------------------------------+-----------------------+---------------------------
   P0    | glk_fileref_create_by_prompt   | nextglk_fileref.c     | Game can obtain a fileref
   P0    | glk_fileref_destroy            | nextglk_fileref.c     | Game can free the fileref
   P1    | glk_stream_open_file (Read)    | nextglk.c             | Game can obtain a read stream
   P1    | glk_stream_close (File)        | nextglk.c/next_window.c| Game can close the file
   P2    | nextglk_file_open_read         | nextglk_file.c        | Platform opens a file for reading
   P2    | nextglk_file_read              | nextglk_file.c        | Platform reads bytes from file
   P2    | nextglk_file_close             | nextglk_file.c        | Platform closes the file
   P3    | glk_get_char_stream (File)     | nextglk.c             | restoreFromFile reads ident bytes
   P3    | glk_get_buffer_stream (File)   | nextglk.c             | readWord() reads 4-byte words
   P3    | glk_stream_get_position (File) | nextglk.c             | restoreFromFile tracks read bounds
   P3    | glk_stream_set_position (File) | nextglk.c             | restoreFromFile skips unknown chunks
```

**Minimum viable shortcut:**  Same fileref shortcut as save, plus
implement file reading on the platform layer and the four read/position
functions.  Estimated: **6–7 functions must become real** for a minimal
working restore.

### 7.3  To make TRANSCRIPT succeed

```
Priority | Function                      | File                  | What it unlocks
---------+-------------------------------+-----------------------+---------------------------
   P0    | glk_fileref_create_by_prompt   | nextglk_fileref.c     | Game can obtain a fileref
   P0    | glk_fileref_destroy            | nextglk_fileref.c     | Game can free the fileref
   P1    | glk_stream_open_file (Write)   | nextglk.c             | Game can obtain a write stream
   P1    | glk_window_set_echo_stream     | next_window.c         | Transcript stream is registered
   P2    | nextglk_file_open_write         | nextglk_file.c        | Platform opens file for writing
   P2    | nextglk_file_write              | nextglk_file.c        | Platform writes bytes to file
   P2    | nextglk_file_close              | nextglk_file.c        | Platform closes the file
   P3    | glk_put_char_stream (File)     | next_stream.c         | Echo stream writes to file
   P3    | glk_put_buffer_stream (File)   | next_stream.c         | Echo stream writes buffers
   P4    | glk_stream_close (File)        | nextglk.c/next_window.c| Game closes transcript stream
   P4    | Modify put_char_stream handler  | next_stream.c         | Must also write to win->echostr
```

**Minimum viable shortcut:**  Same fileref and file-write infrastructure
as save, plus implement `glk_window_set_echo_stream()` (a ~5-line function
that sets `win->echostr`), and modify the `strtype_Window` handler in
`next_stream.c` to echo to `win->echostr` when non-NULL.  Estimated:
**6–7 functions must be real, plus 2 modifications** to existing code.

---

## 8. Shared Infrastructure Dependencies

All three features share these dependencies:

```
            ┌─────────────────────────────────┐
            │    glk_fileref_create_by_prompt   │  P0 — return valid frefid_t
            └────────────┬─────────────────────┘
                         │
            ┌────────────▼─────────────────────┐
            │    glk_fileref_destroy            │  P0 — free allocated fref
            └────────────┬─────────────────────┘
                         │
            ┌────────────▼─────────────────────┐
            │    glk_stream_open_file           │  P1 — return valid strid_t
            └────────────┬─────────────────────┘
                         │
            ┌────────────▼─────────────────────┐
            │    nextglk_file layer             │  P2 — real file open/read/write/close
            └────────────┬─────────────────────┘
                         │
            ┌────────────▼─────────────────────┐
            │    glk_stream_close (File)        │  P1 — close file stream properly
            └──────────────────────────────────┘
```

After that, each feature diverges:

- **SAVE** adds: position tracking, file output
- **RESTORE** adds: position tracking, file input
- **TRANSCRIPT** adds: echo stream support, file output

---

## 9. Appendix: Key File References

| File | Lines | What's There |
|------|-------|-------------|
| `nextglk/nextglk_fileref.c` | 41–49 | `glk_fileref_create_by_prompt` stub |
| `nextglk/nextglk_fileref.c` | 68–73 | `glk_fileref_destroy` stub |
| `nextglk/nextglk.c` | 665–671 | `glk_stream_open_file` stub |
| `nextglk/nextglk.c` | 568–587 | `glk_stream_set_position` / `glk_stream_get_position` stubs |
| `nextglk/nextglk.c` | 601–649 | `glk_get_char_stream` / `glk_get_buffer_stream` stubs |
| `nextglk/next_stream.c` | 254–256 | `strtype_File` case in `glk_put_char_stream` — no-op |
| `nextglk/nextglk_file.c` | 8–51 | Entire platform file I/O layer — all stubs |
| `nextglk/next_window.c` | — | `glk_window_set_echo_stream` not implemented |
| `nextglk/nextglk_internal.h` | 78 | `window_t.echostr` declared but no setter exists |
| `thirdparty/git/savefile.c` | 29–210 | `restoreFromFile` — reads Quetzal format |
| `thirdparty/git/savefile.c` | 212–315 | `saveToFile` — writes Quetzal format |
| `thirdparty/git/terp.c` | 651–693 | `do_save_stub` / `finish_save_stub` / `do_restore` dispatch |
| `nextglk/gi_dispa.c` | 186–337 | Function table — all save/restore/transcript functions are registered |