# Glk Core Types — Minimum Viable Implementation Strategy

## Introduction

This document analyses the three opaque Glk object types — `winid_t`,
`strid_t`, and `frefid_t` — as they appear in the imported CheapGlk sources
(`thirdparty/cheapglk/`) and the Git VM dispatcher (`thirdparty/git/glkop.c`).

The goal is to identify the *minimum viable* data each type must carry for the
four required subsystems:

- text output
- keyboard input
- save / restore
- inline image support

and to distinguish fields that are mandatory from those that can be deferred.

---

## 1. `winid_t` — Window Identifier

### 1.1 Declaration

File: `nextglk/glk.h`, line 49.

```c
typedef struct glk_window_struct *winid_t;
```

### 1.2 Representation

A pointer to a `struct glk_window_struct`. The struct is defined in the
private library header; the public header only sees the typedef'd pointer.

In CheapGlk the struct is `typedef struct glk_window_struct window_t` defined
in `thirdparty/cheapglk/cheapglk.h`, lines 75–91:

```c
struct glk_window_struct {
    glui32 magicnum;        /* MAGIC_WINDOW_NUM — sanity check */
    glui32 rock;            /* arbitrary game-supplied value */
    gidispatch_rock_t disprock;  /* dispatch-layer bookkeeping */

    stream_t *str;          /* the window's primary output stream */
    stream_t *echostr;      /* echo stream for line input, or NULL */

    int line_request;       /* 1 if line input is pending */
    int line_request_uni;   /* 1 if line input is Unicode */
    int char_request;       /* 1 if char input is pending */
    int char_request_uni;   /* 1 if char input is Unicode */

    void *linebuf;          /* pointer to VM-provided input buffer */
    glui32 linebuflen;      /* capacity of linebuf */
    gidispatch_rock_t inarrayrock; /* dispatch-layer array registration */
};
```

### 1.3 Functions that consume it

| Function                          | Notes                                    |
|-----------------------------------|------------------------------------------|
| `glk_window_open`                 | `split` parameter — parent window        |
| `glk_window_close`                |                                          |
| `glk_window_get_size`             |                                          |
| `glk_window_set_arrangement`      |                                          |
| `glk_window_get_arrangement`      |                                          |
| `glk_window_get_type`             |                                          |
| `glk_window_get_parent`           |                                          |
| `glk_window_get_sibling`          |                                          |
| `glk_window_clear`                |                                          |
| `glk_window_move_cursor`          |                                          |
| `glk_window_get_stream`           |                                          |
| `glk_window_set_echo_stream`      |                                          |
| `glk_window_get_echo_stream`      |                                          |
| `glk_set_window`                  | sets current-window for output           |
| `glk_request_line_event`          | keyboard input                           |
| `glk_request_char_event`          | keyboard input                           |
| `glk_cancel_line_event`           |                                          |
| `glk_cancel_char_event`           |                                          |
| `glk_request_mouse_event`         | stub only                                |
| `glk_cancel_mouse_event`          | stub only                                |
| `glk_set_echo_line_event`         |                                          |
| `glk_set_terminators_line_event`  |                                          |
| `glk_image_draw`                  | inline image                             |
| `glk_image_draw_scaled`           |                                          |
| `glk_image_draw_scaled_ext`       |                                          |
| `glk_window_flow_break`           |                                          |
| `glk_window_erase_rect`           |                                          |
| `glk_window_fill_rect`            |                                          |
| `glk_window_set_background_color` |                                          |
| `glk_style_distinguish`           |                                          |
| `glk_style_measure`               |                                          |
| `glk_request_hyperlink_event`     | stub only                                |
| `glk_cancel_hyperlink_event`      | stub only                                |
| `event_t.win`                     | returned by `glk_select`                 |

The dispatcher encodes `winid_t` as class `Qa` (gidisp_Class_Window = 0).
In the dispatch prototype string it appears as `Qa`.

### 1.4 Functions that create it

| Function                  | Notes                               |
|---------------------------|-------------------------------------|
| `glk_window_open`         | Returns a new window                |
| `glk_window_get_root`     | Returns the root window             |
| `glk_window_iterate`      | Returns the next window in sequence |

### 1.5 Functions that destroy it

| Function           | Notes           |
|--------------------|-----------------|
| `glk_window_close` | Destroys window |

### 1.6 Git VM code paths that depend on it

File: `thirdparty/git/glkop.c`.

- `git_perform_glk()` — the VM-side dispatcher converts a Glulx `glui32` object
  ID into a C pointer using `classes_get(classid, objid)` (line 715). The class
  ID for windows is `0` (`gidisp_Class_Window`). The hash table is managed by
  `git_classtable_register()` / `git_classtable_unregister()`.
- The fast-path functions in `git_perform_glk()` do not use windows directly;
  they operate on streams. But the full dispatcher path (`gidispatch_call`)
  handles all window functions.
- `savefile.c` does NOT reference `winid_t` at all. It operates entirely on
  `strid_t`.

### 1.7 Required subsystems

| Subsystem        | Required? | Rationale                                         |
|------------------|-----------|---------------------------------------------------|
| Text output      | **Yes**   | Window owns the output stream; `glk_set_window` sets current stream |
| Keyboard input   | **Yes**   | `glk_request_line_event` / `glk_request_char_event` and `glk_select` all use `winid_t` |
| Save / restore   | No        | `savefile.c` works through `strid_t` directly     |
| Inline images    | **Yes**   | `glk_image_draw` takes a `winid_t`                |

### 1.8 Minimum implementation

#### Linux validation

The CheapGlk model is the correct minimum:

```c
struct glk_window_struct {
    stream_t *str;              /* the one-and-only output stream */
    stream_t *echostr;          /* echo stream, or NULL */

    int line_request;           /* line input pending? */
    int line_request_uni;
    int char_request;           /* char input pending? */
    int char_request_uni;

    void *linebuf;              /* pointer into VM memory for line input */
    glui32 linebuflen;          /* buffer capacity */
};
```

- A single global `window_t *mainwin` (as CheapGlk uses) is sufficient for
  both Linux and Spectrum Next.
- `gidispatch_rock_t disprock` is needed only for the dispatch-layer object
  registry. On the Next with a custom dispatcher, this can be omitted.
- `magicnum` and `rock` are Glk spec requirements for introspection; they
  can be deferred if the dispatcher is custom.

#### Spectrum Next

Identical to Linux. A single `TextBuffer` window. The `str` pointer points
to the window's own stream, which routes characters to the Next's screen/
Layer 2 framebuffer via `nextglk_put_char()` etc.

### 1.9 Minimum data fields

| Field            | Required? | Reason                                              |
|------------------|-----------|-----------------------------------------------------|
| `str`            | **MANDATORY** | Window owns an output stream                   |
| `echostr`        | Optional  | Echo stream for line input; can be NULL initially  |
| `line_request`   | **MANDATORY** | Must track pending input requests              |
| `char_request`   | **MANDATORY** | Must track pending input requests              |
| `line_request_uni` | **MANDATORY** | Needed if Unicode input is requested             |
| `char_request_uni` | **MANDATORY** | Needed if Unicode input is requested             |
| `linebuf`        | **MANDATORY** | Holds pointer to VM's line buffer               |
| `linebuflen`     | **MANDATORY** | Capacity of the line buffer                      |
| `magicnum`       | Defer      | Only needed for debugging / spec compliance       |
| `rock`           | Defer      | Only needed for `glk_window_get_rock`             |
| `disprock`       | Defer      | Not needed with custom dispatcher                 |
| `inarrayrock`    | **MANDATORY** | Needed for dispatch-layer array registration  |

### 1.10 Data fields that can be deferred

- `magicnum` — not needed for correctness.
- `rock` — not needed unless the game calls `glk_window_get_rock`.
- `disprock` — not needed if the NextGlk dispatcher manages object identity
  differently (e.g. using a small integer ID instead of a C pointer rock).
- Input event tracking beyond `line_request`/`char_request` (mouse, hyperlink)
  can be deferred, as these are stubs.

---

## 2. `strid_t` — Stream Identifier

### 2.1 Declaration

File: `nextglk/glk.h`, line 50.

```c
typedef struct glk_stream_struct *strid_t;
```

### 2.2 Representation

A pointer to a `struct glk_stream_struct`. In CheapGlk the struct is
`typedef struct glk_stream_struct stream_t` defined in
`thirdparty/cheapglk/cheapglk.h`, lines 98–133.

The Glk spec defines four stream types:

| Stream type      | Constant          | Notes                         |
|------------------|-------------------|-------------------------------|
| Window stream    | `strtype_Window`  | Attached to a window          |
| File stream      | `strtype_File`    | Reads/writes a stdio FILE*    |
| Memory stream    | `strtype_Memory`  | Reads/writes a buffer         |
| Resource stream  | `strtype_Resource`| Reads from a loaded resource  |

### 2.3 Functions that consume it

| Function                          | Notes                          |
|-----------------------------------|--------------------------------|
| `glk_put_char_stream`            |                                |
| `glk_put_string_stream`          |                                |
| `glk_put_buffer_stream`          |                                |
| `glk_set_style_stream`           |                                |
| `glk_get_char_stream`            |                                |
| `glk_get_line_stream`            |                                |
| `glk_get_buffer_stream`          |                                |
| `glk_stream_close`               |                                |
| `glk_stream_set_position`        |                                |
| `glk_stream_get_position`        |                                |
| `glk_stream_set_current`         |                                |
| `glk_stream_get_current`         | returns `strid_t`              |
| `glk_window_get_stream`          | returns `strid_t`              |
| `glk_window_set_echo_stream`     |                                |
| `glk_window_get_echo_stream`     | returns `strid_t`              |
| `glk_stream_iterate`             | returns `strid_t`              |
| `glk_stream_get_rock`            |                                |
| `glk_set_hyperlink_stream`       |                                |
| `glk_put_char_stream_uni`        |                                |
| `glk_put_string_stream_uni`      |                                |
| `glk_put_buffer_stream_uni`      |                                |
| `glk_get_char_stream_uni`        |                                |
| `glk_get_line_stream_uni`        |                                |
| `glk_get_buffer_stream_uni`      |                                |

The dispatcher encodes `strid_t` as class `Qb` (gidisp_Class_Stream = 1).

### 2.4 Functions that create it

| Function                    | Notes                         |
|-----------------------------|-------------------------------|
| `glk_stream_open_file`      | Opens a file stream           |
| `glk_stream_open_memory`    | Opens a memory stream         |
| `glk_stream_open_resource`  | Opens a resource stream       |
| `glk_stream_open_file_uni`  | Unicode variant               |
| `glk_stream_open_memory_uni`| Unicode variant               |
| `glk_stream_open_resource_uni` | Unicode variant            |
| `glk_window_get_stream`     | Returns window's own stream   |
| `glk_stream_iterate`        | Iterates over active streams  |
| `glk_stream_get_current`    | Returns current output stream |

Note: `glk_window_get_stream` does not *create* a stream; it returns the
window's pre-existing stream.

### 2.5 Functions that destroy it

| Function           | Notes                                    |
|--------------------|------------------------------------------|
| `glk_stream_close` | Closes stream; window streams cannot be closed directly |

### 2.6 Git VM code paths that depend on it

File: `thirdparty/git/glkop.c`.

- Fast-path functions in `git_perform_glk()` directly call:
  - `glk_stream_set_current(git_find_stream_by_id(arglist[0]))` (0x0047)
  - `glk_stream_get_current()` → `git_find_id_for_stream()` (0x0048)
  - `glk_put_char_stream(git_find_stream_by_id(...), ...)` (0x0081, 0x012B)
- The full dispatcher handles all stream functions through `gidispatch_call()`.

File: `thirdparty/git/savefile.c`.

- Heavily stream-dependent:
  - `glk_get_buffer_stream(file, ...)` — reading save data
  - `glk_get_char_stream(file)` — reading save data
  - `glk_stream_get_position(file)` — tracking file position
  - `glk_stream_set_position(file, ...)` — seeking in save file
  - `glk_stream_set_current(file)` — making save stream the default
  - `glk_stream_get_current()` — saving/restoring current stream
  - `glk_put_buffer(...)` — writing save data (uses current stream)
  - `glk_put_string(...)` — writing IFF tags
  - `glk_put_char(...)` — writing save data bytes
- Git exports `git_find_stream_by_id(glui32 id)` and
  `git_find_id_for_stream(strid_t str)` for the VM-to-C-pointer conversion.

### 2.7 Required subsystems

| Subsystem        | Required? | Rationale                                                   |
|------------------|-----------|--------------------------------------------------------------|
| Text output      | **Yes**   | Window stream handles all output; file streams for transcripts |
| Keyboard input   | **Yes**   | Echo stream is a secondary stream for line-input echoing     |
| Save / restore   | **Yes**   | Save file is opened as a file stream, then read/written via `glk_get_buffer_stream` / `glk_put_buffer` |
| Inline images    | No        | Images are drawn through `winid_t`, not `strid_t`            |

### 2.8 Minimum implementation

#### Linux validation

Follow the CheapGlk model:

```c
struct glk_stream_struct {
    int type;                   /* strtype_Window / strtype_File / strtype_Memory */
    int unicode;                /* 0 = 1-byte chars, 1 = 4-byte */
    int readable, writable;
    glui32 readcount, writecount;  /* for stream_result_t */

    /* for strtype_Window */
    window_t *win;

    /* for strtype_File */
    FILE *file;                 /* stdio FILE* (Linux only) */

    /* for strtype_Memory */
    unsigned char *buf, *bufptr, *bufend, *bufeof;
    glui32 *ubuf, *ubufptr, *ubufend, *ubufeof;
    glui32 buflen;

    stream_t *next, *prev;      /* linked list of all streams */
};
```

- The linked list (`gli_streamlist`) is critical for `glk_stream_iterate()`
  and for the dispatch-layer object registry.

#### Spectrum Next

The same struct, but:

- `FILE *file` is replaced by a `NextGlkFile *file` that wraps the
  NextZXOS/ESXDOS file handle.
- The `nextglk_file_*` API in `nextglk.h` mirrors the operations needed:
  open_read, open_write, read, write, close.
- Memory streams are identical (no platform dependency).
- Window stream output routes through `nextglk_put_char()` etc.

### 2.9 Minimum data fields

| Field          | Required? | Reason                                            |
|----------------|-----------|---------------------------------------------------|
| `type`         | **MANDATORY** | Determines dispatch of all I/O operations      |
| `unicode`      | **MANDATORY** | Determines buffer element size                 |
| `readable`     | **MANDATORY** | Guards read operations                         |
| `writable`     | **MANDATORY** | Guards write operations                        |
| `readcount`    | **MANDATORY** | Required for `stream_result_t`                 |
| `writecount`   | **MANDATORY** | Required for `stream_result_t`                 |
| `win`          | **MANDATORY** | Must exist for window streams                  |
| `file`         | **MANDATORY** | Must exist for file streams; type differs per platform |
| `buf`/`ubuf`   | **MANDATORY** | Must exist for memory/resource streams         |
| `bufptr`/`ubufptr` | **MANDATORY** | Current read/write position               |
| `bufend`/`ubufend` | **MANDATORY** | End of allocated buffer                  |
| `bufeof`/`ubufeof` | **MANDATORY** | Logical end of data (may be < bufend)    |
| `buflen`       | **MANDATORY** | Total buffer capacity                          |
| `next`/`prev`  | **MANDATORY** | Required for stream iteration                  |
| `lastop`       | Optional  | Only needed for ReadWrite file streams          |
| `isbinary`     | **MANDATORY** | Determines byte ordering for file/resource streams |
| `magicnum`     | Defer     | Only needed for debugging/spec compliance       |
| `rock`         | Defer     | Only needed for `glk_stream_get_rock`           |
| `disprock`     | Defer     | Not needed with custom dispatcher               |
| `arrayrock`    | Defer     | Only needed with retained-array dispatch        |

### 2.10 Data fields that can be deferred

- `magicnum`
- `rock`
- `disprock`
- `arrayrock`
- `lastop` — can be deferred until ReadWrite file streams are needed

---

## 3. `frefid_t` — File Reference Identifier

### 3.1 Declaration

File: `nextglk/glk.h`, line 51.

```c
typedef struct glk_fileref_struct *frefid_t;
```

### 3.2 Representation

A pointer to a `struct glk_fileref_struct`. In CheapGlk the struct is
`typedef struct glk_fileref_struct fileref_t` defined in
`thirdparty/cheapglk/cheapglk.h`, lines 135–145:

```c
struct glk_fileref_struct {
    glui32 magicnum;        /* MAGIC_FILEREF_NUM */
    glui32 rock;

    char *filename;         /* resolved filesystem path */
    int filetype;           /* fileusage_Data, SavedGame, Transcript, InputRecord */
    int textmode;           /* 0 = binary, 1 = text */

    gidispatch_rock_t disprock;
    fileref_t *next, *prev; /* linked list of all filerefs */
};
```

### 3.3 Functions that consume it

| Function                          | Notes                          |
|-----------------------------------|--------------------------------|
| `glk_stream_open_file`            | Opens a stream from this ref   |
| `glk_stream_open_file_uni`        | Unicode variant                |
| `glk_fileref_destroy`             | Destroys the ref               |
| `glk_fileref_delete_file`         | Deletes the underlying file    |
| `glk_fileref_does_file_exist`     | Checks file existence          |
| `glk_fileref_get_rock`            |                                  |
| `glk_fileref_create_from_fileref` | Duplicates a fileref           |

The dispatcher encodes `frefid_t` as class `Qc` (gidisp_Class_Fileref = 2).

### 3.4 Functions that create it

| Function                           | Notes                        |
|------------------------------------|------------------------------|
| `glk_fileref_create_temp`          | Temporary file (usually in /tmp) |
| `glk_fileref_create_by_name`       | Named file in working directory |
| `glk_fileref_create_by_prompt`     | Prompt user for filename     |
| `glk_fileref_create_from_fileref`  | Clone an existing fileref    |
| `glk_fileref_iterate`              | Iterate over existing refs   |

### 3.5 Functions that destroy it

| Function              | Notes            |
|-----------------------|------------------|
| `glk_fileref_destroy` | Destroys the ref |

### 3.6 Git VM code paths that depend on it

File: `thirdparty/git/glkop.c`.

- `glk_fileref_create_by_prompt` (0x0062) has a fast-path hook for
  `library_select_hook()`, but falls through to the full dispatcher.
- All other fileref operations go through `gidispatch_call()`.
- The dispatcher prototypes show `Qc` for all fileref arguments.

File: `thirdparty/git/savefile.c`.

- **Does NOT use `frefid_t`** at all. Instead, it calls
  `git_find_stream_by_id(id)` where `id` is a Glulx `glui32` stream ID
  passed by the game. The game itself creates the fileref and opens the
  stream before calling save/restore.

### 3.7 Required subsystems

| Subsystem        | Required? | Rationale                                          |
|------------------|-----------|----------------------------------------------------|
| Text output      | No        | Transcript output uses frefs, but can be deferred  |
| Keyboard input   | No        |                                                    |
| Save / restore   | **Yes**   | Game calls `glk_fileref_create_by_name` then `glk_stream_open_file` before save/restore |
| Inline images    | No        | Images come from Blorb resource map, not file refs |

### 3.8 Minimum implementation

#### Linux validation

```c
struct glk_fileref_struct {
    char *filename;             /* resolved filesystem path */
    int filetype;               /* fileusage_* mask */
    int textmode;               /* 0 = binary, 1 = text */

    fileref_t *next, *prev;    /* linked list for iteration */
};
```

- The linked list (`gli_filereflist`) is required for `glk_fileref_iterate()`.
- The `filetype` determines the filename suffix (`.glkdata`, `.glksave`, `.txt`).

#### Spectrum Next

The same struct, but:

- `filename` is a path relative to the SD card root (e.g. `"save.glksave"`).
- `glk_fileref_create_by_name` must sanitise the filename for the
  NextZXOS 8.3 or ESXDOS filesystem (truncate, replace illegal chars).
- `glk_fileref_does_file_exist` and `glk_fileref_delete_file` map to
  `nextglk_file_*` API calls.
- `glk_fileref_create_by_prompt` is problematic on the Next (no stdin).
  It can initially be a stub that always returns NULL or uses a hardcoded
  filename.

### 3.9 Minimum data fields

| Field       | Required? | Reason                                    |
|-------------|-----------|-------------------------------------------|
| `filename`  | **MANDATORY** | Required to open the underlying file   |
| `filetype`  | **MANDATORY** | Determines usage class and suffix       |
| `textmode`  | **MANDATORY** | Determines whether to open in text or binary mode |
| `next`/`prev` | **MANDATORY** | Required for `glk_fileref_iterate()`  |
| `magicnum`  | Defer     | Only needed for debugging                |
| `rock`      | Defer     | Only needed for `glk_fileref_get_rock`   |
| `disprock`  | Defer     | Not needed with custom dispatcher        |

### 3.10 Data fields that can be deferred

- `magicnum`
- `rock`
- `disprock`

---

## 4. Relationships Between Types and Glk Subsystems

### 4.1 Type relationship diagram

```
                ┌─────────────┐
                │   event_t   │
                │  .win field │
                └──────┬──────┘
                       │ references
                       v
  ┌────────────────────────────────────────────────────┐
  │                    winid_t                         │
  │  (struct glk_window_struct)                        │
  │                                                     │
  │  .str ────────────┐  .echostr ────────────┐        │
  │                    │                       │        │
  │  .line_request     │                       │        │
  │  .char_request     │                       │        │
  │  .linebuf          │                       │        │
  └────────────────────┼───────────────────────┼────────┘
                       │                       │
                       v                       v
  ┌─────────────────────────────┐   ┌─────────────────────────────┐
  │         strid_t (1)        │   │       strid_t (2)           │
  │  (window stream)           │   │  (echo stream, optional)    │
  │                             │   │                             │
  │  .type = strtype_Window    │   │  .type = any                │
  │  .win -> winid_t           │   │  (usually file or memory)   │
  └─────────────────────────────┘   └─────────────────────────────┘
         │
         │ (for strtype_Memory / strtype_File / strtype_Resource)
         v
  ┌─────────────────────────────┐   ┌─────────────────────────────┐
  │    strid_t (file/mem/res)  │   │      frefid_t               │
  │                             │   │                             │
  │  .file (FILE* / NextGlkFile*) │  .filename                   │
  │  .buf / .ubuf               │   │  .filetype / .textmode      │
  │  .bufptr / .ubufptr         │   │                             │
  └─────────────────────────────┘   └─────────────────────────────┘
                                             │
                                    (created by game, then
                                     passed to stream_open_file)
                                             │
                                             v
                                     ┌───────────────┐
                                     │   strid_t     │
                                     │ (file stream) │
                                     └───────────────┘
```

### 4.2 Subsystem relationships

```
Text Output:
  Game ──put_char──▶ winid_t.str (window stream) ──▶ nextglk_put_char() ──▶ screen

Keyboard Input:
  Game ──request_line_event──▶ winid_t ──▶ sets line_request + linebuf
  Game ──glk_select()───────▶  fills event_t.win = winid_t, .val1 = line length
                              optionally echoes to winid_t.echostr (strid_t)

Save / Restore:
  Game ──fileref_create_by_name──▶ frefid_t
  Game ──stream_open_file(fref)──▶ strid_t (file stream)
  Game ──save/restore────────────▶ glk_get_buffer_stream(strid_t) etc.
                                  (strid_t is the file stream, no winid_t or frefid_t needed beyond open)

Inline Images:
  Game ──image_draw(winid_t, ...)──▶ NextGlk──▶ Blorb resource lookup──▶ Layer 2
```

### 4.3 Lifecycle summary

```
            Create              Consumed by              Destroy
winid_t   glk_window_open()    ~15 functions            glk_window_close()
          glk_window_get_root()
          glk_window_iterate()

strid_t   glk_stream_open_*()  ~20 functions            glk_stream_close()
          glk_window_get_stream()
          glk_stream_iterate()
          glk_stream_get_current()

frefid_t  glk_fileref_create_*()  glk_stream_open_file()   glk_fileref_destroy()
          glk_fileref_iterate()   glk_fileref_delete_file()
                                  glk_fileref_does_file_exist()
                                  glk_fileref_get_rock()
                                  glk_fileref_create_from_fileref()
```

### 4.4 Dispatch-layer class mapping (from gi_dispa.h and glkop.c)

```
Class letter  Class name           Numeric ID   Type       Managed by
    Qa        gidisp_Class_Window      0        winid_t    classes_get(0, id)
    Qb        gidisp_Class_Stream      1        strid_t    classes_get(1, id)
    Qc        gidisp_Class_Fileref     2        frefid_t   classes_get(2, id)
    Qd        gidisp_Class_Schannel    3        schanid_t  (deferred)
```

The Git VM dispatcher (`glkop.c`) maintains a `classtable_t` hash table for
each class. When the VM passes a `glui32` object ID, the dispatcher converts
it to a C pointer via `classes_get(classid, objid)`. When the Glk library
returns a new C pointer, the dispatcher registers it with
`classes_put(classid, obj, origid)` and returns the assigned `glui32` ID
to the VM.

For NextGlk, the `gi_dispa.c` dispatch layer is already imported. The
`git_classtable_register` / `git_classtable_unregister` callbacks must be
wired to `gidispatch_set_object_registry()`.