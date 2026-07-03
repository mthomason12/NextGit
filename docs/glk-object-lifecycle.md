# Glk Object Lifecycle — Architecture for NextGlk

## Purpose

This document defines the minimum viable object lifecycle for the three core
Glk opaque types — `winid_t`, `strid_t`, and `frefid_t` — as they must be
implemented in NextGlk to support Phases 1–4 of the minimum API.

It is derived from:

- `docs/glk-core-types.md` — minimum data fields per type
- `docs/glk-minimum-api.md` — phase breakdown and function requirements
- `thirdparty/cheapglk/cheapglk.h` — CheapGlk struct definitions
- `thirdparty/cheapglk/cgwindow.c` — CheapGlk window lifecycle
- `thirdparty/cheapglk/cgstream.c` — CheapGlk stream lifecycle
- `thirdparty/cheapglk/cgfref.c` — CheapGlk fileref lifecycle
- `nextglk/gi_dispa.c` — dispatch-layer object registration patterns

---

## 1. Global State Requirements

NextGlk requires exactly three global variables for object tracking:

```c
/* nextglk/nextglk.h (or nextglk_internal.h) */

/* Window tracking — single window for minimum implementation */
static window_t *gli_mainwin = NULL;

/* Stream tracking — doubly-linked list */
static stream_t *gli_streamlist = NULL;

/* Fileref tracking — doubly-linked list */
static fileref_t *gli_filereflist = NULL;

/* Current output stream (not owned by any list) */
static stream_t *gli_currentstr = NULL;
```

### 1.1 Why no window linked list?

The minimum implementation supports a single `TextBuffer` window. The Glk spec
requires `glk_window_iterate()`, but with one window this is trivially
implemented (return `mainwin` on first call, `NULL` thereafter). A window
linked list is **deferred** until multi-window support (Pair windows) is needed.

### 1.2 Why stream and fileref linked lists?

- `glk_stream_iterate()` and `glk_fileref_iterate()` are required by the
  dispatch-layer object registry (`gi_dispa.c`). The Git VM dispatcher calls
  these to enumerate all objects of a class.
- The dispatch layer's `gidispatch_set_object_registry()` callbacks
  (`gli_register_obj` / `gli_unregister_obj`) need to iterate objects to
  convert between C pointers and Glulx `glui32` IDs.
- A doubly-linked list with `next`/`prev` pointers is the standard CheapGlk
  pattern and is the minimum viable approach.

---

## 2. Ownership Rules

### 2.1 Window ownership

| Relationship | Owner | Notes |
|---|---|---|
| `window_t.str` | **Window** | The window's output stream is created by `gli_new_window()` and destroyed by `gli_delete_window()`. |
| `window_t.echostr` | **Not owned** | The echo stream is set by the game via `glk_window_set_echo_stream()`. The window does NOT close the echo stream when closed. |
| `window_t.linebuf` | **Not owned** | Points into VM memory. The window does NOT free it. The dispatch layer unregisters the array on close. |

### 2.2 Stream ownership

| Relationship | Owner | Notes |
|---|---|---|
| `stream_t.file` (strtype_File) | **Stream** | The `FILE*` is opened by `glk_stream_open_file()` and closed by `gli_delete_stream()`. |
| `stream_t.buf`/`ubuf` (strtype_Memory) | **Not owned** | Points to caller-provided buffer. The stream does NOT free it. The dispatch layer unregisters the array on close. |
| `stream_t.buf` (strtype_Resource) | **Not owned** | Points into Blorb resource map. The stream does NOT free it. |
| `stream_t.win` (strtype_Window) | **Not owned** | Back-pointer to the owning window. The stream does NOT free the window. |

### 2.3 Fileref ownership

| Relationship | Owner | Notes |
|---|---|---|
| `fileref_t.filename` | **Fileref** | Allocated by `gli_new_fileref()` via `malloc`/`strdup`, freed by `gli_delete_fileref()`. |

### 2.4 Dispatch-layer registration

All three types register with the dispatch layer on creation and unregister on
destruction. The dispatch layer's `gidispatch_rock_t` is stored in the
`disprock` field of each struct. This is **mandatory** because the Git VM
dispatcher (`glkop.c`) uses the dispatch-layer object registry to convert
between Glulx `glui32` object IDs and C pointers.

---

## 3. winid_t — Window Lifecycle

### 3.1 Struct definition (minimum)

```c
/* nextglk/nextglk.h */

typedef struct glk_window_struct window_t;

struct glk_window_struct {
    /* Dispatch-layer bookkeeping (MANDATORY) */
    gidispatch_rock_t disprock;

    /* Streams */
    stream_t *str;              /* window's output stream (OWNED) */
    stream_t *echostr;          /* echo stream, or NULL (NOT OWNED) */

    /* Input state */
    int line_request;           /* 1 if line input is pending */
    int line_request_uni;       /* 1 if line input is Unicode */
    int char_request;           /* 1 if char input is pending */
    int char_request_uni;       /* 1 if char input is Unicode */

    /* Line buffer (points to VM memory, NOT OWNED) */
    void *linebuf;
    glui32 linebuflen;
    gidispatch_rock_t inarrayrock;  /* dispatch-layer array registration */
};
```

### 3.2 Allocation

```
glk_window_open(split, method, size, wintype, rock)
  │
  ├── Validate: mainwin must be NULL (single window only)
  ├── Validate: wintype must be wintype_TextBuffer
  │
  └── gli_new_window(rock)
        │
        ├── malloc(sizeof(window_t))
        ├── Initialize fields to zero/NULL
        ├── gli_new_stream(strtype_Window, readable=FALSE, writable=TRUE, rock=0)
        │     └── Creates stream, inserts into gli_streamlist
        ├── str->win = win          (back-pointer)
        ├── gli_register_obj(win, gidisp_Class_Window)
        │     └── Stores disprock
        └── Return win
```

**Called from:** Phase 1 (Boot + Text)

**Required by:** `glk_window_open()` is mandatory in Phase 1.

### 3.3 Tracking

- Single global `window_t *gli_mainwin`
- No linked list for windows in minimum implementation
- The window's stream is tracked in `gli_streamlist`

### 3.4 Iteration

```
glk_window_iterate(win, rockptr)
  │
  ├── if win == NULL:
  │     └── Return gli_mainwin (or NULL if no window)
  ├── if win == gli_mainwin:
  │     └── Return NULL (no more windows)
  └── else:
        └── Return NULL (invalid window)
```

**Required by:** Phase 1 (dispatch-layer object registry)

### 3.5 Destruction

```
glk_window_close(win, result)
  │
  ├── Validate: win == gli_mainwin
  │
  └── gli_delete_window(win)
        │
        ├── gli_stream_fill_result(win->str, result)
        │
        ├── If linebuf != NULL:
        │     └── gli_unregister_arr(linebuf, linebuflen, "&+#!Cn", inarrayrock)
        │
        ├── gli_unregister_obj(win, gidisp_Class_Window, disprock)
        │
        ├── gli_delete_stream(win->str)
        │     └── Unlinks from gli_streamlist, frees stream
        │
        ├── win->echostr = NULL    (not owned, do not close)
        ├── free(win)
        └── gli_mainwin = NULL
```

**Required by:** Phase 1

### 3.6 Lifecycle transitions

```
NULL ──glk_window_open()──▶ ALLOCATED ──glk_window_close()──▶ NULL
                              │
                              ├── glk_set_window() → current stream set
                              ├── glk_request_line_event() → line_request=1
                              ├── glk_request_char_event() → char_request=1
                              ├── glk_cancel_line_event() → line_request=0
                              ├── glk_cancel_char_event() → char_request=0
                              └── glk_window_get_stream() → returns str
```

### 3.7 Phase requirements

| Phase | Requires winid_t? | Functions |
|---|---|---|
| Phase 1 (Boot + Text) | **Yes** | `glk_window_open`, `glk_window_close`, `glk_window_get_root`, `glk_window_get_type`, `glk_window_get_size`, `glk_window_get_stream`, `glk_window_get_sibling`, `glk_window_iterate`, `glk_set_window`, `glk_window_clear` |
| Phase 2 (Input) | **Yes** | `glk_request_line_event`, `glk_cancel_line_event`, `glk_request_char_event`, `glk_cancel_char_event`, `glk_window_set_echo_stream`, `glk_window_get_echo_stream` |
| Phase 3 (Save/Restore) | No | — |
| Phase 4 (Images) | **Yes** | `glk_image_draw`, `glk_image_draw_scaled`, `glk_window_flow_break`, `glk_window_erase_rect`, `glk_window_fill_rect`, `glk_window_set_background_color`, `glk_window_move_cursor` |

---

## 4. strid_t — Stream Lifecycle

### 4.1 Struct definition (minimum)

```c
/* nextglk/nextglk.h */

typedef struct glk_stream_struct stream_t;

struct glk_stream_struct {
    /* Dispatch-layer bookkeeping (MANDATORY) */
    gidispatch_rock_t disprock;

    /* Type and mode */
    int type;                   /* strtype_Window / strtype_File / strtype_Memory */
    int unicode;                /* 0 = 1-byte chars, 1 = 4-byte */
    int readable, writable;
    glui32 readcount, writecount;   /* for stream_result_t */

    /* For strtype_Window */
    window_t *win;              /* back-pointer to owning window (NOT OWNED) */

    /* For strtype_File */
    FILE *file;                 /* stdio FILE* (Linux); NextGlkFile* (Next) */
    glui32 lastop;              /* 0, filemode_Write, or filemode_Read */

    /* For strtype_Memory and strtype_Resource */
    int isbinary;
    unsigned char *buf, *bufptr, *bufend, *bufeof;
    glui32 *ubuf, *ubufptr, *ubufend, *ubufeof;
    glui32 buflen;
    gidispatch_rock_t arrayrock;    /* dispatch-layer array registration */

    /* Linked list (MANDATORY for iteration) */
    stream_t *next, *prev;
};
```

### 4.2 Allocation

```
gli_new_stream(type, readable, writable, rock)
  │
  ├── malloc(sizeof(stream_t))
  ├── Initialize all fields to zero/NULL
  ├── Set type, readable, writable, rock
  ├── unicode = FALSE
  ├── isbinary = FALSE
  │
  ├── Insert into gli_streamlist (head insertion):
  │     ├── prev = NULL
  │     ├── next = gli_streamlist
  │     ├── gli_streamlist = str
  │     └── if next != NULL: next->prev = str
  │
  ├── gli_register_obj(str, gidisp_Class_Stream)
  │     └── Stores disprock
  │
  └── Return str
```

**Specialized allocation paths:**

```
glk_stream_open_file(fref, fmode, rock)
  │
  ├── Validate fref != NULL
  ├── Open FILE* from fref->filename
  ├── gli_new_stream(strtype_File, readable, writable, rock)
  ├── str->file = fl
  ├── str->isbinary = !fref->textmode
  └── Return str

glk_stream_open_memory(buf, buflen, fmode, rock)
  │
  ├── Validate fmode
  ├── gli_new_stream(strtype_Memory, readable, writable, rock)
  ├── Set buf/bufptr/bufend/bufeof
  ├── gli_register_arr(buf, buflen, "&+#!Cn")
  │     └── Stores arrayrock
  └── Return str
```

**Required by:** Phase 1 (window stream), Phase 3 (file/memory streams)

### 4.3 Tracking

- Global `static stream_t *gli_streamlist` — doubly-linked list
- Global `static stream_t *gli_currentstr` — current output stream (not in list ownership sense)
- Window streams are also reachable via `window_t.str`

### 4.4 Iteration

```
glk_stream_iterate(str, rockptr)
  │
  ├── if str == NULL:
  │     └── str = gli_streamlist
  ├── else:
  │     └── str = str->next
  │
  ├── if str != NULL:
  │     ├── if rockptr: *rockptr = str->rock
  │     └── Return str
  └── else:
        ├── if rockptr: *rockptr = 0
        └── Return NULL
```

**Required by:** Phase 1 (dispatch-layer object registry)

### 4.5 Destruction

```
glk_stream_close(str, result)
  │
  ├── Validate str != NULL
  ├── Reject if str->type == strtype_Window (cannot close window stream directly)
  │
  └── gli_delete_stream(str)
        │
        ├── gli_stream_fill_result(str, result)
        │
        ├── If str == gli_currentstr:
        │     └── gli_currentstr = NULL
        │
        ├── If gli_mainwin && gli_mainwin->echostr == str:
        │     └── gli_mainwin->echostr = NULL
        │
        ├── Type-specific cleanup:
        │     ├── strtype_File:   fclose(str->file)
        │     ├── strtype_Memory: gli_unregister_arr(buf, buflen, typedesc, arrayrock)
        │     └── strtype_Resource: (nothing — Blorb owns the buffer)
        │
        ├── gli_unregister_obj(str, gidisp_Class_Stream, disprock)
        │
        ├── Unlink from gli_streamlist:
        │     ├── if prev: prev->next = next
        │     ├── else:    gli_streamlist = next
        │     └── if next: next->prev = prev
        │
        └── free(str)
```

**Required by:** Phase 1 (window close → stream delete), Phase 3 (file stream close)

### 4.6 Lifecycle transitions

```
NULL ──gli_new_stream()──▶ ALLOCATED (in list)
  │                          │
  │                          ├── strtype_Window:
  │                          │     └── Destroyed by gli_delete_window() → gli_delete_stream()
  │                          │
  │                          ├── strtype_File:
  │                          │     ├── glk_stream_close() → gli_delete_stream()
  │                          │     └── file is fclose()'d
  │                          │
  │                          ├── strtype_Memory:
  │                          │     ├── glk_stream_close() → gli_delete_stream()
  │                          │     └── array is unregistered (not freed)
  │                          │
  │                          └── strtype_Resource:
  │                                └── glk_stream_close() → gli_delete_stream()
  │                                      (Blorb buffer not freed)
  │
  └── ALLOCATED ──gli_delete_stream()──▶ NULL
```

### 4.7 Phase requirements

| Phase | Requires strid_t? | Functions |
|---|---|---|
| Phase 1 (Boot + Text) | **Yes** | `glk_put_char_stream`, `glk_put_string_stream`, `glk_put_buffer_stream`, `glk_set_style_stream`, `glk_stream_get_current`, `glk_stream_set_current`, `glk_stream_iterate`, `glk_window_get_stream` |
| Phase 2 (Input) | **Yes** | `glk_window_set_echo_stream`, `glk_window_get_echo_stream` |
| Phase 3 (Save/Restore) | **Yes** | `glk_stream_open_file`, `glk_stream_open_memory`, `glk_stream_close`, `glk_get_buffer_stream`, `glk_get_char_stream`, `glk_get_line_stream`, `glk_stream_set_position`, `glk_stream_get_position` |
| Phase 4 (Images) | No | — |

---

## 5. frefid_t — File Reference Lifecycle

### 5.1 Struct definition (minimum)

```c
/* nextglk/nextglk.h */

typedef struct glk_fileref_struct fileref_t;

struct glk_fileref_struct {
    /* Dispatch-layer bookkeeping (MANDATORY) */
    gidispatch_rock_t disprock;

    /* File identity */
    char *filename;             /* resolved filesystem path (OWNED) */
    int filetype;               /* fileusage_Data / SavedGame / Transcript / InputRecord */
    int textmode;               /* 0 = binary, 1 = text */

    /* Linked list (MANDATORY for iteration) */
    fileref_t *next, *prev;
};
```

### 5.2 Allocation

```
gli_new_fileref(filename, usage, rock)
  │
  ├── malloc(sizeof(fileref_t))
  ├── Initialize fields to zero
  ├── rock = rock
  ├── filename = malloc(strlen(filename) + 1); strcpy(filename, src)
  ├── textmode = (usage & fileusage_TextMode) != 0
  ├── filetype = (usage & fileusage_TypeMask)
  │
  ├── Insert into gli_filereflist (head insertion):
  │     ├── prev = NULL
  │     ├── next = gli_filereflist
  │     ├── gli_filereflist = fref
  │     └── if next != NULL: next->prev = fref
  │
  ├── gli_register_obj(fref, gidisp_Class_Fileref)
  │     └── Stores disprock
  │
  └── Return fref
```

**Specialized allocation paths:**

```
glk_fileref_create_by_name(usage, name, rock)
  │
  ├── Sanitise name (strip illegal chars, truncate at '.')
  ├── Append suffix based on usage (.glkdata / .glksave / .txt)
  ├── Prepend working directory path
  └── gli_new_fileref(fullpath, usage, rock)

glk_fileref_create_temp(usage, rock)
  │
  ├── Generate temp filename (e.g. "/tmp/glktempfref-XXXXXX")
  ├── Create file with mkstemp()
  └── gli_new_fileref(filename, usage, rock)

glk_fileref_create_by_prompt(usage, fmode, rock)
  │
  ├── [Stub on Next] Return NULL or hardcoded filename
  └── [Linux] Prompt stdin, sanitise, gli_new_fileref()
```

**Required by:** Phase 3 (Save/Restore)

### 5.3 Tracking

- Global `static fileref_t *gli_filereflist` — doubly-linked list

### 5.4 Iteration

```
glk_fileref_iterate(fref, rockptr)
  │
  ├── if fref == NULL:
  │     └── fref = gli_filereflist
  ├── else:
  │     └── fref = fref->next
  │
  ├── if fref != NULL:
  │     ├── if rockptr: *rockptr = fref->rock
  │     └── Return fref
  └── else:
        ├── if rockptr: *rockptr = 0
        └── Return NULL
```

**Required by:** Phase 3 (dispatch-layer object registry)

### 5.5 Destruction

```
glk_fileref_destroy(fref)
  │
  ├── Validate fref != NULL
  │
  └── gli_delete_fileref(fref)
        │
        ├── gli_unregister_obj(fref, gidisp_Class_Fileref, disprock)
        │
        ├── free(fref->filename)
        │
        ├── Unlink from gli_filereflist:
        │     ├── if prev: prev->next = next
        │     ├── else:    gli_filereflist = next
        │     └── if next: next->prev = prev
        │
        └── free(fref)
```

**Required by:** Phase 3

### 5.6 Lifecycle transitions

```
NULL ──glk_fileref_create_*()──▶ ALLOCATED (in list)
  │                                │
  │                                ├── glk_stream_open_file(fref) → creates strid_t
  │                                │     (fref continues to exist independently)
  │                                │
  │                                ├── glk_fileref_delete_file(fref)
  │                                │     └── unlink(fref->filename)
  │                                │
  │                                ├── glk_fileref_does_file_exist(fref)
  │                                │     └── stat(fref->filename)
  │                                │
  │                                └── glk_fileref_destroy(fref)
  │                                      └── gli_delete_fileref(fref)
  │
  └── ALLOCATED ──gli_delete_fileref()──▶ NULL
```

### 5.7 Phase requirements

| Phase | Requires frefid_t? | Functions |
|---|---|---|
| Phase 1 (Boot + Text) | No | — |
| Phase 2 (Input) | No | — |
| Phase 3 (Save/Restore) | **Yes** | `glk_fileref_create_by_name`, `glk_fileref_create_temp`, `glk_fileref_create_by_prompt`, `glk_fileref_destroy`, `glk_fileref_iterate`, `glk_fileref_delete_file`, `glk_fileref_does_file_exist` |
| Phase 4 (Images) | No | — |

---

## 6. Sequence Diagrams

### 6.1 Sequence A: Window Creation

```
Game                    NextGlk                     Dispatch Layer          gli_streamlist
 │                        │                            │                       │
 │ glk_window_open()      │                            │                       │
 │──────────────────────▶│                            │                       │
 │                        │                            │                       │
 │                        │ gli_new_window(rock)       │                       │
 │                        │ ─────────────────────────▶│                       │
 │                        │                            │                       │
 │                        │   gli_new_stream(          │                       │
 │                        │     strtype_Window,        │                       │
 │                        │     FALSE, TRUE, 0)        │                       │
 │                        │   ────────────────────────▶│                       │
 │                        │                            │                       │
 │                        │     malloc(stream_t)       │                       │
 │                        │     insert into list       │──────────────────────▶│
 │                        │                            │                       │
 │                        │     gli_register_obj(      │                       │
 │                        │       str,                 │                       │
 │                        │       gidisp_Class_Stream) │                       │
 │                        │     ◀──────────────────────│                       │
 │                        │                            │                       │
 │                        │   ◀────────────────────────│                       │
 │                        │   return str               │                       │
 │                        │                            │                       │
 │                        │ str->win = win             │                       │
 │                        │ win->str = str             │                       │
 │                        │                            │                       │
 │                        │ gli_register_obj(          │                       │
 │                        │   win,                     │                       │
 │                        │   gidisp_Class_Window)     │                       │
 │                        │ ◀─────────────────────────│                       │
 │                        │                            │                       │
 │                        │ ◀─────────────────────────│                       │
 │                        │ return win                 │                       │
 │                        │                            │                       │
 │ ◀──────────────────────│                            │                       │
 │ return winid_t         │                            │                       │
 │                        │                            │                       │
 │ glk_set_window(win)    │                            │                       │
 │──────────────────────▶│                            │                       │
 │                        │ gli_stream_set_current(str)│                       │
 │                        │ ──────────────────────────│                       │
 │                        │ ◀─────────────────────────│                       │
 │ ◀──────────────────────│                            │                       │
 │                        │                            │                       │
 │ glk_window_get_stream( │                            │                       │
 │   win)                 │                            │                       │
 │──────────────────────▶│                            │                       │
 │ ◀──────────────────────│                            │                       │
 │ return strid_t         │                            │                       │
 │                        │                            │                       │
```

**Key observations:**

1. Window creation atomically creates a window stream.
2. The window stream is inserted into `gli_streamlist` immediately.
3. The window is registered with the dispatch layer after stream creation.
4. `glk_set_window()` sets the current output stream to the window's stream.
5. The window stream's lifetime is tied to the window's lifetime — it is
   destroyed by `gli_delete_window()`, not by `glk_stream_close()`.

### 6.2 Sequence B: Save File Flow

```
Game                    NextGlk                     Dispatch Layer          gli_filereflist     gli_streamlist
 │                        │                            │                       │                   │
 │ glk_fileref_create_by_ │                            │                       │                   │
 │   name(usage, "save",  │                            │                       │                   │
 │   rock)                │                            │                       │                   │
 │──────────────────────▶│                            │                       │                   │
 │                        │                            │                       │                   │
 │                        │ Sanitise name, append      │                       │                   │
 │                        │ suffix ".glksave"          │                       │                   │
 │                        │                            │                       │                   │
 │                        │ gli_new_fileref(           │                       │                   │
 │                        │   fullpath, usage, rock)   │                       │                   │
 │                        │ ─────────────────────────▶│                       │                   │
 │                        │                            │                       │                   │
 │                        │   malloc(fileref_t)        │                       │                   │
 │                        │   strdup(filename)         │                       │                   │
 │                        │   insert into list         │──────────────────────▶│                   │
 │                        │                            │                       │                   │
 │                        │   gli_register_obj(        │                       │                   │
 │                        │     fref,                  │                       │                   │
 │                        │     gidisp_Class_Fileref)  │                       │                   │
 │                        │   ◀────────────────────────│                       │                   │
 │                        │                            │                       │                   │
 │                        │ ◀─────────────────────────│                       │                   │
 │                        │ return fref                │                       │                   │
 │                        │                            │                       │                   │
 │ ◀──────────────────────│                            │                       │                   │
 │ return frefid_t        │                            │                       │                   │
 │                        │                            │                       │                   │
 │ glk_stream_open_file(  │                            │                       │                   │
 │   fref, filemode_Read, │                            │                       │                   │
 │   rock)                │                            │                       │                   │
 │──────────────────────▶│                            │                       │                   │
 │                        │                            │                       │                   │
 │                        │ fopen(fref->filename, "r") │                       │                   │
 │                        │                            │                       │                   │
 │                        │ gli_new_stream(            │                       │                   │
 │                        │   strtype_File,            │                       │                   │
 │                        │   TRUE, FALSE, rock)       │                       │                   │
 │                        │ ─────────────────────────▶│                       │                   │
 │                        │                            │                       │                   │
 │                        │   malloc(stream_t)         │                       │                   │
 │                        │   insert into list         │──────────────────────────────────────────▶│
 │                        │                            │                       │                   │
 │                        │   gli_register_obj(        │                       │                   │
 │                        │     str,                   │                       │                   │
 │                        │     gidisp_Class_Stream)   │                       │                   │
 │                        │   ◀────────────────────────│                       │                   │
 │                        │                            │                       │                   │
 │                        │ ◀─────────────────────────│                       │                   │
 │                        │ return str                 │                       │                   │
 │                        │                            │                       │                   │
 │ ◀──────────────────────│                            │                       │                   │
 │ return strid_t         │                            │                       │                   │
 │                        │                            │                       │                   │
 │ glk_get_buffer_stream( │                            │                       │                   │
 │   str, buf, len)       │                            │                       │                   │
 │──────────────────────▶│                            │                       │                   │
 │                        │ fread(buf, 1, len, file)   │                       │                   │
 │ ◀──────────────────────│                            │                       │                   │
 │ return bytes_read      │                            │                       │                   │
 │                        │                            │                       │                   │
 │ glk_stream_close(str,  │                            │                       │                   │
 │   result)              │                            │                       │                   │
 │──────────────────────▶│                            │                       │                   │
 │                        │                            │                       │                   │
 │                        │ gli_delete_stream(str)     │                       │                   │
 │                        │ ─────────────────────────▶│                       │                   │
 │                        │                            │                       │                   │
 │                        │   fclose(str->file)        │                       │                   │
 │                        │   gli_unregister_obj()     │                       │                   │
 │                        │   unlink from list         │──────────────────────────────────────────▶│
 │                        │   free(str)                │                       │                   │
 │                        │                            │                       │                   │
 │                        │ ◀─────────────────────────│                       │                   │
 │ ◀──────────────────────│                            │                       │                   │
 │                        │                            │                       │                   │
 │ glk_fileref_destroy(   │                            │                       │                   │
 │   fref)                │                            │                       │                   │
 │──────────────────────▶│                            │                       │                   │
 │                        │                            │                       │                   │
 │                        │ gli_delete_fileref(fref)   │                       │                   │
 │                        │ ─────────────────────────▶│                       │                   │
 │                        │                            │                       │                   │
 │                        │   gli_unregister_obj()     │                       │                   │
 │                        │   free(fref->filename)     │                       │                   │
 │                        │   unlink from list         │──────────────────────▶│                   │
 │                        │   free(fref)               │                       │                   │
 │                        │                            │                       │                   │
 │                        │ ◀─────────────────────────│                       │                   │
 │ ◀──────────────────────│                            │                       │                   │
 │                        │                            │                       │                   │
```

**Key observations:**

1. `frefid_t` and `strid_t` are independent objects with independent lifetimes.
2. Creating a file stream from a fileref does NOT transfer ownership.
3. The fileref continues to exist after the stream is opened.
4. Both must be explicitly destroyed/closed by the game.
5. The fileref's `filename` is used to open the `FILE*`, then the fileref is no
   longer needed for I/O.

### 6.3 Sequence C: Shutdown Flow

```
Game                    NextGlk                     Dispatch Layer          gli_streamlist      gli_filereflist
 │                        │                            │                       │                   │
 │ glk_stream_close(      │                            │                       │                   │
 │   file_str, result)    │                            │                       │                   │
 │──────────────────────▶│                            │                       │                   │
 │                        │ gli_delete_stream(str)     │                       │                   │
 │                        │ ─────────────────────────▶│                       │                   │
 │                        │                            │                       │                   │
 │                        │   fclose(str->file)        │                       │                   │
 │                        │   gli_unregister_obj()     │                       │                   │
 │                        │   unlink from list         │──────────────────────▶│                   │
 │                        │   free(str)                │                       │                   │
 │                        │                            │                       │                   │
 │                        │ ◀─────────────────────────│                       │                   │
 │ ◀──────────────────────│                            │                       │                   │
 │                        │                            │                       │                   │
 │ glk_window_close(      │                            │                       │                   │
 │   mainwin, result)     │                            │                       │                   │
 │──────────────────────▶│                            │                       │                   │
 │                        │                            │                       │                   │
 │                        │ gli_delete_window(win)     │                       │                   │
 │                        │ ─────────────────────────▶│                       │                   │
 │                        │                            │                       │                   │
 │                        │   gli_unregister_arr(      │                       │                   │
 │                        │     linebuf, ...)          │                       │                   │
 │                        │                            │                       │                   │
 │                        │   gli_unregister_obj(      │                       │                   │
 │                        │     win,                   │                       │                   │
 │                        │     gidisp_Class_Window)   │                       │                   │
 │                        │                            │                       │                   │
 │                        │   gli_delete_stream(       │                       │                   │
 │                        │     win->str)              │                       │                   │
 │                        │   ───────────────────────▶│                       │                   │
 │                        │                            │                       │                   │
 │                        │     gli_unregister_obj()   │                       │                   │
 │                        │     unlink from list       │──────────────────────▶│                   │
 │                        │     free(str)              │                       │                   │
 │                        │                            │                       │                   │
 │                        │   ◀────────────────────────│                       │                   │
 │                        │                            │                       │                   │
 │                        │   free(win)                │                       │                   │
 │                        │   gli_mainwin = NULL       │                       │                   │
 │                        │                            │                       │                   │
 │                        │ ◀─────────────────────────│                       │                   │
 │ ◀──────────────────────│                            │                       │                   │
 │                        │                            │                       │                   │
 │ glk_fileref_destroy(   │                            │                       │                   │
 │   fref)                │                            │                       │                   │
 │──────────────────────▶│                            │                       │                   │
 │                        │                            │                       │                   │
 │                        │ gli_delete_fileref(fref)   │                       │                   │
 │                        │ ─────────────────────────▶│                       │                   │
 │                        │                            │                       │                   │
 │                        │   gli_unregister_obj()     │                       │                   │
 │                        │   free(fref->filename)     │                       │                   │
 │                        │   unlink from list         │──────────────────────────────────────────▶│
 │                        │   free(fref)               │                       │                   │
 │                        │                            │                       │                   │
 │                        │ ◀─────────────────────────│                       │                   │
 │ ◀──────────────────────│                            │                       │                   │
 │                        │                            │                       │                   │
```

**Key observations:**

1. Shutdown order matters: streams should be closed before windows, because
   `gli_delete_window()` destroys the window's stream.
2. If a fileref's stream is still open when the fileref is destroyed, the
   stream continues to function (it has its own `FILE*` copy). The fileref
   is just a filename wrapper.
3. The echo stream is NOT closed when the window is closed — it is not owned.
4. After shutdown, `gli_streamlist`, `gli_filereflist` should be empty, and
   `gli_mainwin` should be NULL.

---

## 7. Minimum Object Model for Phases 1–4

### 7.1 Required objects

| Phase | winid_t | strid_t | frefid_t |
|---|---|---|---|
| Phase 1 | 1 (TextBuffer) | 1 (window stream) | 0 |
| Phase 2 | 1 (same) | 1–2 (window + optional echo) | 0 |
| Phase 3 | 1 (same) | 1–3 (window + file + optional memory) | 0–N (save/restore filerefs) |
| Phase 4 | 1 (same) | 1 (window stream only) | 0 |

### 7.2 Maximum concurrent objects (worst case)

```
Phase 3 worst case:
  - 1 winid_t (main window)
  - 1 strid_t (window stream)
  - 1 strid_t (echo stream, optional)
  - 1 strid_t (save file stream)
  - 1 strid_t (memory stream, for save data processing)
  - 1 frefid_t (save file reference)
  - 1 frefid_t (temp file reference, for autosave)
  ─────────────────────────────────
  Total: 7 objects
```

### 7.3 Memory budget

Each object is a small struct (~40–80 bytes on a 32-bit system):

| Type | Size (approx) | Count | Total |
|---|---|---|---|
| `window_t` | 40 bytes | 1 | 40 bytes |
| `stream_t` | 80 bytes | 4 | 320 bytes |
| `fileref_t` | 28 bytes | 2 | 56 bytes |
| **Total** | | **7** | **~416 bytes** |

This is well within the ZX Spectrum Next's memory budget.

### 7.4 Dispatch-layer registration

All three types must be registered with the dispatch layer via
`gidispatch_set_object_registry()`. The callbacks are:

```c
/* Registration callbacks (set by dispatch layer) */
extern gidispatch_rock_t (*gli_register_obj)(void *obj, glui32 objclass);
extern void (*gli_unregister_obj)(void *obj, glui32 objclass,
    gidispatch_rock_t objrock);
extern gidispatch_rock_t (*gli_register_arr)(void *array, glui32 len,
    char *typecode);
extern void (*gli_unregister_arr)(void *array, glui32 len, char *typecode,
    gidispatch_rock_t objrock);
```

These are set by the Git VM's `git_classtable_register()` /
`git_classtable_unregister()` callbacks, which are wired through
`gidispatch_set_object_registry()` in `gi_dispa.c`.

---

## 8. Features That Can Be Safely Deferred

### 8.1 Window features

| Feature | Defer reason |
|---|---|
| Multi-window support (Pair windows) | Not needed for single TextBuffer window |
| `glk_window_get_parent` | Returns NULL for single window |
| `glk_window_get_sibling` | Returns NULL for single window |
| `glk_window_set_arrangement` | Only relevant for Pair windows |
| `glk_window_get_arrangement` | Only relevant for Pair windows |
| `glk_window_get_rock` | Not called by minimum games; stub returns 0 |
| `magicnum` field | Debugging only; not needed for correctness |
| `rock` field | Only needed if game calls `glk_window_get_rock` |
| Window linked list | Single global pointer is sufficient |

### 8.2 Stream features

| Feature | Defer reason |
|---|---|
| `glk_stream_get_rock` | Not called by minimum games; stub returns 0 |
| `magicnum` field | Debugging only |
| `rock` field | Only needed if game calls `glk_stream_get_rock` |
| `lastop` field | Only needed for ReadWrite file streams (rare) |
| Resource streams (`strtype_Resource`) | Images handled via Blorb, not resource streams |
| Unicode streams (`unicode=1`) | Phase 5; stub with Latin-1 fallback |

### 8.3 Fileref features

| Feature | Defer reason |
|---|---|
| `glk_fileref_get_rock` | Not called by minimum games; stub returns 0 |
| `glk_fileref_create_from_fileref` | Rarely used; stub returns clone |
| `glk_fileref_create_by_prompt` | Problematic on Next (no stdin); stub returns NULL |
| `magicnum` field | Debugging only |
| `rock` field | Only needed if game calls `glk_fileref_get_rock` |

### 8.4 Dispatch-layer features

| Feature | Defer reason |
|---|---|
| `gidispatch_rock_t` for windows | Can use small integer ID instead of C pointer rock |
| Array registration for non-memory streams | Only needed for dispatch-layer array tracking |

### 8.5 Sound, Hyperlinks, DateTime, Unicode

All of Phase 5 can be deferred. These are not required for text output,
keyboard input, save/restore, or inline images.

---

## 9. Summary

### 9.1 Lifecycle patterns

| Type | Allocation | Tracking | Iteration | Destruction |
|---|---|---|---|---|
| `winid_t` | `gli_new_window()` → malloc | Single global `gli_mainwin` | Trivial NULL/mainwin check | `gli_delete_window()` → free |
| `strid_t` | `gli_new_stream()` → malloc | `gli_streamlist` (doubly-linked) | Walk `next` pointers | `gli_delete_stream()` → unlink + free |
| `frefid_t` | `gli_new_fileref()` → malloc | `gli_filereflist` (doubly-linked) | Walk `next` pointers | `gli_delete_fileref()` → unlink + free |

### 9.2 Key invariants

1. A window's stream is destroyed WITH the window, not by `glk_stream_close()`.
2. An echo stream is NOT owned by the window and is NOT destroyed on window close.
3. A fileref and its file stream are independent — destroying one does NOT
   destroy the other.
4. All objects are registered with the dispatch layer on creation and
   unregistered on destruction.
5. The current output stream (`gli_currentstr`) is set to NULL when its stream
   is destroyed.
6. The window's `echostr` pointer is set to NULL when its stream is destroyed.

### 9.3 Implementation order

1. **Stream lifecycle first** — `gli_new_stream()`, `gli_delete_stream()`,
   `gli_streamlist`, `glk_stream_iterate()`. Everything else depends on streams.

2. **Window lifecycle second** — `gli_new_window()`, `gli_delete_window()`,
   `gli_mainwin`, `glk_window_iterate()`. Depends on stream lifecycle.

3. **Fileref lifecycle third** — `gli_new_fileref()`, `gli_delete_fileref()`,
   `gli_filereflist`, `glk_fileref_iterate()`. Only needed for Phase 3.

4. **Dispatch wiring** — Set `gli_register_obj`/`gli_unregister_obj` callbacks
   before any objects are created. This is done by the Git VM's
   `gidispatch_set_object_registry()` during initialization.