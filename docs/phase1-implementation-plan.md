# Phase 1 Implementation Plan — Boot and Text Display

## Purpose

Define the exact implementation order for Phase 1 of the NextGlk Glk library.
Each numbered unit is a commit. Every commit must **compile** and **link**,
leaving the project in a valid state.

## References

- `docs/glk-core-types.md` — minimum data fields per type
- `docs/glk-minimum-api.md` — Phase 1 function list and priority
- `docs/glk-object-lifecycle.md` — allocation, destruction, ownership, dispatch wiring
- `nextglk/glk.h` — public Glk API declarations
- `nextglk/gi_dispa.c` — dispatch-layer function table and call routing
- `nextglk/gi_dispa.h` — dispatch-layer types, class IDs, object registry API
- `nextglk/nextglk.h` — existing NextGlk platform API
- `nextglk/nextglk.c` — existing lifecycle stubs
- `nextglk/Makefile` — existing build

## Architecture Summary

```
Game (via Git VM)
    ↓
gi_dispa.c  (dispatch layer)
    ↓
NextGlk API  (Phase 1: window + stream + text)
    ↓
nextglk_screen.c  (platform output)
```

The dispatch layer routes Glulx opcodes to Glk C functions. NextGlk implements
those functions. The platform layer (`nextglk_screen.c` etc.) provides the
actual hardware I/O.

## Global State

Defined in a new internal header. Four globals needed before any Phase 1 code
can function:

| Variable | Type | Purpose | Owner |
|---|---|---|---|
| `gli_streamlist` | `stream_t*` | Head of doubly-linked stream list | next_stream.c |
| `gli_currentstr` | `stream_t*` | Current output stream | next_stream.c |
| `gli_mainwin` | `window_t*` | Single root window | next_window.c |
| `gli_filereflist` | `fileref_t*` | Head of doubly-linked fileref list (deferred to Phase 3) | — |

`gli_fileref_list` is declared for completeness but not used until Phase 3.

---

## Commit 1 — Internal Header + Stream Struct + Stream Lifecycle

### Objective

Define the `stream_t` struct and implement stream creation, destruction, and
iteration. This is the foundational object type — windows depend on streams.

### Required source files

| File | Action |
|---|---|
| `nextglk/nextglk_internal.h` | **Create.** Internal header with stream_t, window_t, fileref_t structs, function declarations, and global variable declarations. |
| `nextglk/next_stream.c` | **Create.** Stream lifecycle: gli_new_stream(), gli_delete_stream(), glk_stream_iterate(), glk_stream_get_rock(). |
| `nextglk/nextglk.h` | **Keep.** No changes needed. |
| `nextglk/Makefile` | **Modify.** Add `next_stream.o` to `OBJS`. Add `-I.` for includes. |

### Required structs

```c
/* nextglk_internal.h */

typedef struct glk_stream_struct stream_t;
struct glk_stream_struct {
    gidispatch_rock_t disprock;   /* dispatch-layer bookkeeping */
    
    int type;                     /* strtype_Window / strtype_File / strtype_Memory */
    int unicode;                  /* 0 = 1-byte chars, 1 = 4-byte */
    int readable, writable;
    glui32 readcount, writecount; /* stream_result_t */
    
    /* strtype_Window */
    window_t *win;                /* back-pointer (NOT OWNED) */
    
    /* strtype_File */
    void *file;                   /* platform file handle (deferred) */
    
    /* strtype_Memory + strtype_Resource */
    int isbinary;
    unsigned char *buf, *bufptr, *bufend, *bufeof;
    glui32 *ubuf, *ubufptr, *ubufend, *ubufeof;
    glui32 buflen;
    
    /* Linked list */
    stream_t *next, *prev;
};
```

### Required global variables

| Name | Type | Initial value |
|---|---|---|
| `gli_streamlist` | `stream_t*` | `NULL` |
| `gli_currentstr` | `stream_t*` | `NULL` |

### Required functions

| Function | Signature | Defined in | Purpose |
|---|---|---|---|
| `gli_new_stream` | `stream_t* gli_new_stream(int type, int readable, int writable, glui32 rock)` | next_stream.c | Allocate stream, initialise fields, insert into `gli_streamlist`, register with dispatch layer. |
| `gli_delete_stream` | `void gli_delete_stream(stream_t *str, stream_result_t *result)` | next_stream.c | Fill result, unregister from dispatch layer, unlink from `gli_streamlist`, free. |
| `gli_stream_fill_result` | `void gli_stream_fill_result(stream_t *str, stream_result_t *result)` | next_stream.c | Copy readcount/writecount into result struct. |
| `glk_stream_iterate` | `strid_t glk_stream_iterate(strid_t str, glui32 *rockptr)` | next_stream.c | Walk `gli_streamlist`. Required by dispatch object registry. |
| `glk_stream_get_rock` | `glui32 glk_stream_get_rock(strid_t str)` | next_stream.c | Return 0 (stub). Required by dispatch function table. |

### Prerequisite functions

None. This is the foundational commit.

### Test strategy

1. Call `gli_new_stream(strtype_Window, 0, 1, 0)` → returns non-NULL pointer.
2. Verify `gli_streamlist == returned_ptr`.
3. Call `glk_stream_iterate(NULL, NULL)` → returns same pointer.
4. Call `glk_stream_iterate(returned_ptr, NULL)` → returns NULL (end of list).
5. Call `gli_delete_stream(returned_ptr, NULL)` → stream freed, `gli_streamlist` is NULL again.
6. Repeat with strtype_File and strtype_Memory to verify type field.

### Expected observable behaviour

No user-visible behaviour yet. Streams exist in memory and can be iterated.
The `stream_t` struct is the building block for everything that follows.

### Verification

```
make -C nextglk clean && make -C nextglk
```

Must produce `libnextglk.a` containing `next_stream.o` with no warnings.

---

## Commit 2 — Window Struct + Window Lifecycle

### Objective

Define the `window_t` struct and implement window creation, destruction,
iteration, and query functions. A window owns a window stream.

### Required source files

| File | Action |
|---|---|
| `nextglk/nextglk_internal.h` | **Modify.** Add `window_t` struct, window function declarations. |
| `nextglk/next_window.c` | **Create.** Window lifecycle: gli_new_window(), gli_delete_window(), glk_window_open(), glk_window_close(), glk_window_get_root(), glk_window_get_type(), glk_window_get_size(), glk_window_get_stream(), glk_window_get_sibling(), glk_window_iterate(), glk_window_clear(), glk_window_get_rock(), glk_window_get_parent(), glk_window_move_cursor(), glk_set_window(). |
| `nextglk/Makefile` | **Modify.** Add `next_window.o` to `OBJS`. |

### Required structs

```c
/* nextglk_internal.h — window_t */

typedef struct glk_window_struct window_t;
struct glk_window_struct {
    gidispatch_rock_t disprock;   /* dispatch-layer bookkeeping */
    
    stream_t *str;                /* window's output stream (OWNED) */
    stream_t *echostr;            /* echo stream, or NULL (NOT OWNED) */
    
    int line_request;             /* 1 if line input pending */
    int line_request_uni;
    int char_request;
    int char_request_uni;
    
    void *linebuf;                /* VM line buffer (NOT OWNED) */
    glui32 linebuflen;
};
```

### Required global variables

| Name | Type | Initial value |
|---|---|---|
| `gli_mainwin` | `window_t*` | `NULL` |

(already declared in internal header from Commit 1)

### Required functions

| Function | Signature | Defined in | Purpose |
|---|---|---|---|
| `gli_new_window` | `window_t* gli_new_window(glui32 rock)` | next_window.c | Alloc window, creates window stream via `gli_new_stream`, sets back-pointer, inserts into dispatch layer. |
| `gli_delete_window` | `void gli_delete_window(window_t *win, stream_result_t *result)` | next_window.c | Unregisters array + window from dispatch, deletes owned stream via `gli_delete_stream`, frees window, sets `gli_mainwin = NULL`. |
| `glk_window_open` | `winid_t glk_window_open(winid_t split, glui32 method, glui32 size, glui32 wintype, glui32 rock)` | next_window.c | Validate single-window constraint, call `gli_new_window`. |
| `glk_window_close` | `void glk_window_close(winid_t win, stream_result_t *result)` | next_window.c | Validate, call `gli_delete_window`. |
| `glk_window_get_root` | `winid_t glk_window_get_root(void)` | next_window.c | Return `gli_mainwin`. |
| `glk_window_get_type` | `glui32 glk_window_get_type(winid_t win)` | next_window.c | Return `wintype_TextBuffer`. |
| `glk_window_get_size` | `void glk_window_get_size(winid_t win, glui32 *widthptr, glui32 *heightptr)` | next_window.c | Return fixed size (e.g. 80×25) or platform query. |
| `glk_window_get_stream` | `strid_t glk_window_get_stream(winid_t win)` | next_window.c | Return `win->str`. |
| `glk_window_get_sibling` | `winid_t glk_window_get_sibling(winid_t win)` | next_window.c | Return NULL (single window). |
| `glk_window_iterate` | `winid_t glk_window_iterate(winid_t win, glui32 *rockptr)` | next_window.c | Return `gli_mainwin` on first call, NULL thereafter. |
| `glk_window_clear` | `void glk_window_clear(winid_t win)` | next_window.c | Call `nextglk_screen_clear()`. |
| `glk_set_window` | `void glk_set_window(winid_t win)` | next_window.c | Set `gli_currentstr = win->str`. |
| `glk_window_get_rock` | `glui32 glk_window_get_rock(winid_t win)` | next_window.c | Return 0 (stub). |
| `glk_window_get_parent` | `winid_t glk_window_get_parent(winid_t win)` | next_window.c | Return NULL (stub, no parent). |
| `glk_window_move_cursor` | `void glk_window_move_cursor(winid_t win, glui32 xpos, glui32 ypos)` | next_window.c | No-op (stub, only relevant for TextGrid/Graphics). |

### Prerequisite functions

From Commit 1:
- `gli_new_stream()` — called by `gli_new_window()` to create the window stream
- `gli_delete_stream()` — called by `gli_delete_window()` to destroy the window stream
- `glk_stream_iterate()` — required because the window stream is in `gli_streamlist`

### Test strategy

1. Call `glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0)` → returns non-NULL.
2. Verify `gli_mainwin == returned_ptr`.
3. Verify `glk_window_get_root() == returned_ptr`.
4. Verify `glk_window_get_type(win) == wintype_TextBuffer`.
5. Verify `glk_window_get_stream(win)` returns a non-NULL `strid_t` with `type == strtype_Window`.
6. Call `glk_set_window(win)` → verify `gli_currentstr == win->str`.
7. Call `glk_window_iterate(NULL, NULL)` → returns win.
8. Call `glk_window_iterate(win, NULL)` → returns NULL.
9. Call `glk_window_close(win, NULL)` → verify `gli_mainwin == NULL` and window stream removed from `gli_streamlist`.

### Expected observable behaviour

The game can open a window, query its type and size, get its output stream,
set it as the current window, and close it. No text is rendered yet.

### Verification

```
make -C nextglk clean && make -C nextglk
```

Must produce `libnextglk.a` containing both `next_stream.o` and `next_window.o`.

---

## Commit 3 — Dispatch Wiring + Compile gi_dispa.c

### Objective

Wire the Git VM's dispatch-layer callbacks into NextGlk's object lifecycle
and compile `gi_dispa.c` so that the dispatch layer can route function calls
from the VM.

### Required source files

| File | Action |
|---|---|
| `nextglk/gi_dispa.c` | **Compile.** No changes needed. Already contains the full dispatch layer. |
| `nextglk/nextglk.c` | **Modify.** Add `nextglk_init()` to call `gidispatch_set_object_registry()`, store callback pointers. |
| `nextglk/next_stream.c` | **Modify.** Call `gli_register_obj()` in `gli_new_stream()`, `gli_unregister_obj()` in `gli_delete_stream()`. |
| `nextglk/next_window.c` | **Modify.** Call `gli_register_obj()` in `gli_new_window()`, `gli_unregister_obj()` in `gli_delete_window()`. |
| `nextglk/nextglk_internal.h` | **Modify.** Declare dispatch callback function pointers: `extern gidispatch_rock_t (*gli_register_obj)(void *obj, glui32 objclass)` and `extern void (*gli_unregister_obj)(void *obj, glui32 objclass, gidispatch_rock_t objrock)`. |
| `nextglk/Makefile` | **Modify.** Add `gi_dispa.o` to `OBJS`. |

### Required function pointer globals

```c
/* In nextglk.c, declared extern in nextglk_internal.h */
gidispatch_rock_t (*gli_register_obj)(void *obj, glui32 objclass) = NULL;
void (*gli_unregister_obj)(void *obj, glui32 objclass, gidispatch_rock_t objrock) = NULL;
```

These are set by the Git VM during startup by calling
`gidispatch_set_object_registry()`. NextGlk's `nextglk_init()` does not
set them — they are set externally. NextGlk merely *calls* them when objects
are created/destroyed.

### Required functions

| Function | Defined in | Purpose |
|---|---|---|
| `gidispatch_call` | gi_dispa.c | Route a Glulx dispatch call to the correct Glk function. |
| `gidispatch_prototype` | gi_dispa.c | Return the prototype string for marshaling arguments. |
| `gidispatch_count_classes` | gi_dispa.c | Number of object classes. |
| `gidispatch_get_class` | gi_dispa.c | Return class name and numeric ID. |
| `gidispatch_count_intconst` | gi_dispa.c | Number of integer constants. |
| `gidispatch_get_intconst` | gi_dispa.c | Return integer constant name and value. |
| `gidispatch_count_functions` | gi_dispa.c | Number of dispatchable functions. |
| `gidispatch_get_function` | gi_dispa.c | Return function by index. |
| `gidispatch_get_function_by_id` | gi_dispa.c | Return function by ID (used by Git VM). |

### Prerequisite functions

From Commit 1 + Commit 2:
- All stream and window lifecycle functions must be defined so the dispatch
  table entries in `gi_dispa.c` resolve at link time.

### Test strategy

1. Verify that `gi_dispa.o` compiles without errors.
2. Verify that all function pointers in the `function_table` resolve:
   - `glk_exit`, `glk_set_interrupt_handler`, `glk_tick`, `glk_gestalt`, `glk_gestalt_ext`
   - `glk_window_iterate`, `glk_window_get_rock`, `glk_window_get_root`, `glk_window_open`, `glk_window_close`, `glk_window_get_size`, `glk_window_set_arrangement`, `glk_window_get_arrangement`, `glk_window_get_type`, `glk_window_get_parent`, `glk_window_clear`, `glk_window_move_cursor`, `glk_window_get_stream`, `glk_window_set_echo_stream`, `glk_window_get_echo_stream`, `glk_set_window`, `glk_window_get_sibling`
   - `glk_stream_iterate`, `glk_stream_get_rock`, `glk_stream_open_file`, `glk_stream_open_memory`, `glk_stream_close`, `glk_stream_set_position`, `glk_stream_get_position`, `glk_stream_set_current`, `glk_stream_get_current`
   - `glk_put_char`, `glk_put_char_stream`, `glk_put_string`, `glk_put_string_stream`, `glk_put_buffer`, `glk_put_buffer_stream`, `glk_set_style`, `glk_set_style_stream`
   - `glk_get_char_stream`, `glk_get_line_stream`, `glk_get_buffer_stream`
   - `glk_char_to_lower`, `glk_char_to_upper`
   - `glk_stylehint_set`, `glk_stylehint_clear`, `glk_style_distinguish`, `glk_style_measure`
   - `glk_select`, `glk_select_poll`
   - `glk_request_line_event`, `glk_cancel_line_event`, `glk_request_char_event`, `glk_cancel_char_event`, `glk_request_mouse_event`, `glk_cancel_mouse_event`, `glk_request_timer_events`
   - Unicode, Image, Sound, Hyperlink, DateTime stubs (as guarded by `#ifdef`)
3. Verify that `gidispatch_call(0x0047, ...)` routes to `glk_stream_set_current()`.
4. Verify that `gidispatch_call(0x0023, ...)` routes to `glk_window_open()`.

### Expected observable behaviour

No user-visible change. The dispatch layer is now linked and wired.
Object creation automatically registers with the dispatch layer.
The Git VM can now call Glk functions through `gidispatch_call()`.

### Verification

```
make -C nextglk clean && make -C nextglk
```

Must produce `libnextglk.a` with no undefined symbols.

---

## Commit 4 — Stream Current + Stream Output Functions

### Objective

Implement the stream current-pointer functions and the stream-oriented output
routines. Characters written to a window stream are forwarded to the platform
screen layer.

### Required source files

| File | Action |
|---|---|
| `nextglk/next_stream.c` | **Modify.** Add stream I/O functions. |
| `nextglk/nextglk_internal.h` | **No change needed.** Declarations already present. |
| `nextglk/nextglk_screen.c` | **Modify.** Implement `nextglk_put_char()`, `nextglk_put_string()`, `nextglk_put_buffer()` with `putchar()`/`fputs()` for Linux validation. |

### Required functions

| Function | Defined in | Purpose |
|---|---|---|
| `glk_stream_get_current` | `strid_t glk_stream_get_current(void)` | Return `gli_currentstr`. |
| `glk_stream_set_current` | `void glk_stream_set_current(strid_t str)` | Set `gli_currentstr = str`. |
| `glk_put_char_stream` | `void glk_put_char_stream(strid_t str, unsigned char ch)` | Dispatch by `str->type`: window → `nextglk_put_char(ch)`; file → (deferred); memory → buffer write. |
| `glk_put_string_stream` | `void glk_put_string_stream(strid_t str, char *s)` | Loop over `glk_put_char_stream(str, *s++)`. |
| `glk_put_buffer_stream` | `void glk_put_buffer_stream(strid_t str, char *buf, glui32 len)` | Loop over `glk_put_char_stream(str, buf[i])`. |
| `glk_set_style_stream` | `void glk_set_style_stream(strid_t str, glui32 styl)` | Record current style in stream (or ignore — stub acceptable). |

### Prerequisite functions

From Commit 2:
- `glk_set_window()` — sets `gli_currentstr` to window's stream
- `glk_window_get_stream()` — retrieves window's stream

### Stream output routing

```
glk_put_char_stream(str, ch)
    ↓
str->type == strtype_Window  →  str->win exists  →  nextglk_put_char(ch)
str->type == strtype_File    →  (Phase 3 — write to file handle)
str->type == strtype_Memory  →  write to buf[bufptr++]
```

### Test strategy

1. Create window, get its stream via `glk_window_get_stream()`.
2. Call `glk_set_window(win)`.
3. Call `glk_put_char_stream(win_str, 'A')` → Verify `nextglk_put_char('A')` is called.
4. Call `glk_put_string_stream(win_str, "Hello")` → Verify `nextglk_put_char` called 5 times.
5. Call `glk_put_buffer_stream(win_str, "AB", 2)` → Verify `nextglk_put_char` called 2 times.
6. Call `glk_stream_get_current()` → Returns `win_str`.
7. Call `glk_stream_set_current(NULL)` → `glk_stream_get_current()` returns NULL.

### Expected observable behaviour

Characters written to a window stream via the `_stream` variants are forwarded
to the platform screen layer. On Linux, `nextglk_put_char()` writes to stdout.
No game-visible output yet (the `glk_put_char/string/buffer` convenience
functions come next).

### Verification

```
make -C nextglk clean && make -C nextglk
```

Write a small test program that links against `libnextglk.a` and calls
the stream output functions, verifying they produce expected stdout output.

---

## Commit 5 — Convenience Output Functions + Remaining Phase 1 Functions

### Objective

Implement the current-stream convenience output functions and all remaining
Phase 1 mandatory functions: lifecycle, gestalt, stylehints, utility, event
loop stub, and the remaining stubs.

### Required source files

| File | Action |
|---|---|
| `nextglk/next_stream.c` | **Modify.** Add `glk_put_char()`, `glk_put_string()`, `glk_put_buffer()`, `glk_set_style()`. |
| `nextglk/nextglk.c` | **Modify.** Add lifecycle, gestalt, utility, event stub functions. |
| `nextglk/nextglk.h` | **No change needed.** |

### Required functions

#### Lifecycle + Query

| Function | Implementation | Purpose |
|---|---|---|
| `glk_exit` | Call `exit(0)`. | Terminate process. |
| `glk_gestalt` | Switch on `sel`; return supported capabilities. | Report library features. |
| `glk_gestalt_ext` | Same as `glk_gestalt` for simple queries, or fill `arr` for extended. | Extended feature query. |
| `glk_select` | Block until an event occurs (initial stub: return `evtype_None` in event). | Event loop. |
| `glk_select_poll` | Return immediately with `evtype_None`. | Non-blocking event check. |

#### Convenience Output

| Function | Implementation |
|---|---|
| `glk_put_char(unsigned char ch)` | `if (gli_currentstr) glk_put_char_stream(gli_currentstr, ch);` |
| `glk_put_string(char *s)` | `if (gli_currentstr) glk_put_string_stream(gli_currentstr, s);` |
| `glk_put_buffer(char *buf, glui32 len)` | `if (gli_currentstr) glk_put_buffer_stream(gli_currentstr, buf, len);` |
| `glk_set_style(glui32 styl)` | `if (gli_currentstr) glk_set_style_stream(gli_currentstr, styl);` |

#### Style Hints

| Function | Implementation |
|---|---|
| `glk_stylehint_set` | No-op (store and ignore; stub acceptable). |
| `glk_stylehint_clear` | No-op (stub). |

#### Char Conversion

| Function | Implementation |
|---|---|
| `glk_char_to_lower` | `return (ch >= 'A' && ch <= 'Z') ? ch + 32 : ch;` |
| `glk_char_to_upper` | `return (ch >= 'a' && ch <= 'z') ? ch - 32 : ch;` |

#### Stubs (compileable, return safe defaults)

```
glk_tick()                           →  no-op
glk_set_interrupt_handler(func)      →  no-op
glk_window_get_rock(win)             →  return 0
glk_window_get_parent(win)           →  return NULL
glk_window_set_arrangement(...)       →  no-op
glk_window_get_arrangement(...)       →  set method=0, size=0, keywin=NULL
glk_stream_get_rock(str)             →  return 0
glk_stream_open_memory(buf, ...)      →  return NULL (stub)
glk_stream_close(str, result)         →  reject if strtype_Window, else gli_delete_stream
glk_stream_set_position(...)          →  no-op (stub)
glk_stream_get_position(str)          →  return 0 (stub)
glk_get_char_stream(str)             →  return -1 (stub)
glk_get_line_stream(str, ...)         →  return 0 (stub)
glk_get_buffer_stream(str, ...)       →  return 0 (stub)
glk_stream_open_file(...)             →  return NULL (stub, Phase 3)
glk_style_distinguish(win, s1, s2)   →  return 1 (indistinguishable)
glk_style_measure(win, styl, h, r)    →  return 0 (not available)
glk_request_line_event(...)           →  no-op (Phase 2)
glk_cancel_line_event(...)            →  no-op (Phase 2)
glk_request_char_event(win)           →  no-op (Phase 2)
glk_cancel_char_event(win)            →  no-op (Phase 2)
glk_request_mouse_event(win)          →  no-op (Phase 2)
glk_cancel_mouse_event(win)           →  no-op (Phase 2)
glk_request_timer_events(ms)          →  no-op (Phase 5)
glk_window_set_echo_stream(...)       →  no-op (Phase 2)
glk_window_get_echo_stream(win)       →  return NULL (Phase 2)
glk_set_echo_line_event(...)          →  no-op (Phase 2)
glk_set_terminators_line_event(...)   →  no-op (Phase 2)
```

(All Unicode APIs, Image APIs, Sound APIs, Hyperlink APIs, DateTime APIs,
and Resource Stream APIs are guarded by `#ifdef` in `glk.h` and must have
stub implementations that compile.)

### Prerequisite functions

From Commit 4:
- `glk_stream_get_current()` — used by `glk_put_char`/`glk_put_string`/`glk_put_buffer`
- `glk_put_char_stream()` — called by convenience output functions

### Test strategy

1. Open window, set as current.
2. Call `glk_put_char('A')` → character appears via `nextglk_put_char()`.
3. Call `glk_put_string("Hello")` → string appears.
4. Call `glk_put_buffer("AB", 2)` → buffer appears.
5. Call `glk_gestalt(gestalt_Version, 0)` → returns 0x00070600 (0.7.6).
6. Call `glk_gestalt(gestalt_CharOutput, 0)` → returns `gestalt_CharOutput_ExactPrint`.
7. Call `glk_gestalt(gestalt_Unicode, 0)` → returns 1 (yes, but rendering is ASCII-only).
8. Call `glk_exit()` → process terminates.
9. Call `glk_char_to_lower('A')` → returns `'a'`.
10. Call `glk_char_to_upper('a')` → returns `'A'`.

### Expected observable behaviour

The game can display text. A typical startup sequence works:
1. `glk_gestalt` queries → return sensible defaults.
2. `glk_stylehint_set` → no-op, no crash.
3. `glk_window_open` → creates window and stream.
4. `glk_set_window` → sets current stream.
5. `glk_put_string("Hello")` → "Hello" appears on screen.

### Verification

```
make -C nextglk clean && make -C nextglk
```

Write a minimal test program that:
1. Opens a window.
2. Sets it as current.
3. Calls `glk_put_string("Hello, world!\n")`.
4. Verifies output appears on stdout.

---

## Commit 6 — Stylehint Storage + Gestalt Tuning

### Objective

Implement actual stylehint storage (so `glk_stylehint_set` has an effect on
`glk_style_measure`) and tune `glk_gestalt` responses for Phase 1 accuracy.

### Required source files

| File | Action |
|---|---|
| `nextglk/next_window.c` | **Modify.** Add stylehint table storage and `glk_style_distinguish()`, `glk_style_measure()`. |
| `nextglk/nextglk_internal.h` | **Modify.** Add stylehint storage struct/array. |

### Required new declarations

```c
/* In nextglk_internal.h */
#define STYLEHINT_TABLE_SIZE (wintype_Graphics + 1) * style_NUMSTYLES * stylehint_NUMHINTS
/* Store hints as glsi32; a sentinel value (0x7FFFFFFF) means "not set" */
extern glsi32 gli_stylehints[wintype_Graphics + 1][style_NUMSTYLES][stylehint_NUMHINTS];
```

### Required functions

| Function | Implementation |
|---|---|
| `glk_stylehint_set` | Store `val` in `gli_stylehints[wintype][styl][hint]`. |
| `glk_stylehint_clear` | Set `gli_stylehints[wintype][styl][hint]` to sentinel. |
| `glk_style_distinguish` | Return 0 (styles are distinguishable) if any hint differs between the two styles for the given window type; else return 1. |
| `glk_style_measure` | Return `gli_stylehints[win->type][styl][hint]` if set; else return 0 with result = 0. |

### Prerequisite functions

From Commit 5:
- `glk_stylehint_set()` and `glk_stylehint_clear()` now have backing storage.

### Test strategy

1. Call `glk_stylehint_set(wintype_TextBuffer, style_Normal, stylehint_Size, 12)`.
2. Call `glk_style_measure(mainwin, style_Normal, stylehint_Size, &result)` → returns 1, result == 12.
3. Call `glk_stylehint_clear(wintype_TextBuffer, style_Normal, stylehint_Size)`.
4. Call `glk_style_measure(mainwin, style_Normal, stylehint_Size, &result)` → returns 0.

### Expected observable behaviour

Stylehints are stored and can be queried. Games that call `glk_stylehint_set`
followed by `glk_style_measure` get consistent results. No visual difference
in text output (style rendering is Phase 5).

### Verification

```
make -C nextglk clean && make -C nextglk
```

---

## Phase 1 Completion Checklist

### Deliverables

| Artifact | Status |
|---|---|
| `nextglk/nextglk_internal.h` | **Created** — internal struct definitions, globals, function declarations |
| `nextglk/next_stream.c` | **Created** — stream lifecycle, current stream, stream output |
| `nextglk/next_window.c` | **Created** — window lifecycle, window queries, stylehints |
| `nextglk/nextglk.c` | **Modified** — lifecycle, gestalt, utility, stubs |
| `nextglk/nextglk_screen.c` | **Modified** — `nextglk_put_char/string/buffer` implemented |
| `nextglk/Makefile` | **Modified** — `OBJS` includes `next_stream.o`, `next_window.o`, `gi_dispa.o` |
| `nextglk/gi_dispa.c` | **Compiled** — no changes needed to this file |

### Required functions implemented

34 mandatory functions, ~20 stub functions, all `#ifdef`-guarded modules
have stub implementations that compile.

### Key invariants at Phase 1 completion

1. A single `TextBuffer` window can be opened.
2. The window owns a window stream (`strtype_Window`).
3. Characters written to the window stream appear on screen.
4. The dispatch layer is linked and routes Glulx calls to Glk functions.
5. Stylehints are stored (but not rendered).
6. All Phase 1 `glk_gestalt` queries return correct values.
7. The event loop (`glk_select`) returns immediately with `evtype_None`.
8. Input functions are stubs — no keyboard input works yet.

### What Phase 1 enables

The Git VM can:
1. Start and call `glk_main()`.
2. Query capabilities via `glk_gestalt`.
3. Create a window.
4. Display text via `glk_put_char/string/buffer`.
5. Read and apply stylehints.
6. Call `glk_exit()` to terminate.

### What Phase 1 does NOT enable

- Keyboard input (Phase 2)
- Save/restore (Phase 3)
- Inline images (Phase 4)
- Unicode rendering (Phase 5)
- Sound, hyperlinks, datetime (Phase 5)
- Multiple windows (deferred)
- TextGrid or Graphics window types (deferred)