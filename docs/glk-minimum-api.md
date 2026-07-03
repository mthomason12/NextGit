# Glk Minimum API — Implementation Phases

## Purpose

This document identifies the minimum set of Glk functions required to implement:

1. Display text
2. Accept keyboard input
3. Save a game
4. Restore a game
5. Display inline images

Functions are grouped by implementation phase and annotated with:

- **Signature** — from `nextglk/glk.h`
- **Category** — the functional group
- **Core types involved** — `winid_t`, `strid_t`, `frefid_t`, `event_t`, or none
- **Status** — one of:
  - **Mandatory** — must be implemented for this phase
  - **Deferred** — needed by spec but not required for these five capabilities
  - **Stub-able** — must be present (linked) but can return a safe default

Sources:
- `nextglk/glk.h` — function declarations
- `nextglk/gi_dispa.c` — dispatch table, prototypes, and call routing
- `docs/glk-core-types.md` — type analysis and subsystem mapping

---

## Phase 1 — Boot and Text Display

Required to boot a game and display text. Without these, a game cannot start.

### 1.1 Lifecycle

| Function | Signature | Category | Types | Status |
|----------|-----------|----------|-------|--------|
| `glk_main` | `extern void glk_main(void);` | Entry point | none | **Mandatory** (defined by game, called by library) |
| `glk_exit` | `extern void glk_exit(void) GLK_ATTRIBUTE_NORETURN;` | Termination | none | **Mandatory** |
| `glk_gestalt` | `extern glui32 glk_gestalt(glui32 sel, glui32 val);` | Query | none | **Mandatory** |
| `glk_gestalt_ext` | `extern glui32 glk_gestalt_ext(glui32 sel, glui32 val, glui32 *arr, glui32 arrlen);` | Query | none | **Mandatory** (required by Gi dispatch) |
| `glk_select` | `extern void glk_select(event_t *event);` | Event loop | `event_t` | **Mandatory** |

### 1.2 Window

| Function | Signature | Category | Types | Status |
|----------|-----------|----------|-------|--------|
| `glk_window_get_root` | `extern winid_t glk_window_get_root(void);` | Window | `winid_t` | **Mandatory** |
| `glk_window_get_type` | `extern glui32 glk_window_get_type(winid_t win);` | Window | `winid_t` | **Mandatory** |
| `glk_window_get_size` | `extern void glk_window_get_size(winid_t win, glui32 *widthptr, glui32 *heightptr);` | Window | `winid_t` | **Mandatory** |
| `glk_window_get_stream` | `extern strid_t glk_window_get_stream(winid_t win);` | Window | `winid_t`, `strid_t` | **Mandatory** |
| `glk_set_window` | `extern void glk_set_window(winid_t win);` | Window | `winid_t` | **Mandatory** |
| `glk_window_clear` | `extern void glk_window_clear(winid_t win);` | Window | `winid_t` | **Mandatory** |
| `glk_window_open` | `extern winid_t glk_window_open(winid_t split, glui32 method, glui32 size, glui32 wintype, glui32 rock);` | Window | `winid_t` | **Mandatory** |
| `glk_window_close` | `extern void glk_window_close(winid_t win, stream_result_t *result);` | Window | `winid_t`, `stream_result_t` | **Mandatory** (needed if game opens windows) |
| `glk_window_get_sibling` | `extern winid_t glk_window_get_sibling(winid_t win);` | Window | `winid_t` | **Mandatory** (used in window iteration by game) |
| `glk_window_iterate` | `extern winid_t glk_window_iterate(winid_t win, glui32 *rockptr);` | Window | `winid_t` | **Mandatory** (required by Gi dispatch object registry) |

### 1.3 Text Output

| Function | Signature | Category | Types | Status |
|----------|-----------|----------|-------|--------|
| `glk_put_char` | `extern void glk_put_char(unsigned char ch);` | Output | none (uses current stream) | **Mandatory** |
| `glk_put_char_stream` | `extern void glk_put_char_stream(strid_t str, unsigned char ch);` | Output | `strid_t` | **Mandatory** (used by Git VM fast-path) |
| `glk_put_string` | `extern void glk_put_string(char *s);` | Output | none (uses current stream) | **Mandatory** |
| `glk_put_string_stream` | `extern void glk_put_string_stream(strid_t str, char *s);` | Output | `strid_t` | **Mandatory** |
| `glk_put_buffer` | `extern void glk_put_buffer(char *buf, glui32 len);` | Output | none (uses current stream) | **Mandatory** |
| `glk_put_buffer_stream` | `extern void glk_put_buffer_stream(strid_t str, char *buf, glui32 len);` | Output | `strid_t` | **Mandatory** |
| `glk_set_style` | `extern void glk_set_style(glui32 styl);` | Output | none (uses current stream) | **Mandatory** |
| `glk_set_style_stream` | `extern void glk_set_style_stream(strid_t str, glui32 styl);` | Output | `strid_t` | **Mandatory** |

### 1.4 Stream Management

| Function | Signature | Category | Types | Status |
|----------|-----------|----------|-------|--------|
| `glk_stream_get_current` | `extern strid_t glk_stream_get_current(void);` | Stream | `strid_t` | **Mandatory** |
| `glk_stream_set_current` | `extern void glk_stream_set_current(strid_t str);` | Stream | `strid_t` | **Mandatory** |
| `glk_stream_iterate` | `extern strid_t glk_stream_iterate(strid_t str, glui32 *rockptr);` | Stream | `strid_t` | **Mandatory** (required by Gi dispatch object registry) |

### 1.5 Style Hints

| Function | Signature | Category | Types | Status |
|----------|-----------|----------|-------|--------|
| `glk_stylehint_set` | `extern void glk_stylehint_set(glui32 wintype, glui32 styl, glui32 hint, glsi32 val);` | Style | none | **Mandatory** (called by most games at startup) |
| `glk_stylehint_clear` | `extern void glk_stylehint_clear(glui32 wintype, glui32 styl, glui32 hint);` | Style | none | **Mandatory** (paired with set) |

### 1.6 Utility

| Function | Signature | Category | Types | Status |
|----------|-----------|----------|-------|--------|
| `glk_char_to_lower` | `extern unsigned char glk_char_to_lower(unsigned char ch);` | Utility | none | **Mandatory** (used by Git VM) |
| `glk_char_to_upper` | `extern unsigned char glk_char_to_upper(unsigned char ch);` | Utility | none | **Mandatory** (used by Git VM) |

### 1.7 Stub-able in Phase 1

These must be linked (Gi dispatch references them) but can return safe defaults:

| Function | Stub behaviour |
|----------|----------------|
| `glk_window_get_rock` | Return 0 |
| `glk_window_get_parent` | Return NULL |
| `glk_window_move_cursor` | No-op (only relevant for TextGrid/Graphics windows) |
| `glk_window_set_arrangement` | No-op |
| `glk_window_get_arrangement` | Return size=0, method=0, keywin=NULL |
| `glk_stream_get_rock` | Return 0 |
| `glk_stream_open_memory` | Can stub if no game uses memory streams (rare at startup); see Phase 3 |
| `glk_style_distinguish` | Return 1 (styles indistinguishable) |
| `glk_style_measure` | Return 0 (hint not available) |
| `glk_tick` | No-op |
| `glk_set_interrupt_handler` | No-op (or store pointer, never call) |

---

## Phase 2 — Keyboard Input

Required to accept player input. Builds on Phase 1.

### 2.1 Line Input

| Function | Signature | Category | Types | Status |
|----------|-----------|----------|-------|--------|
| `glk_request_line_event` | `extern void glk_request_line_event(winid_t win, char *buf, glui32 maxlen, glui32 initlen);` | Input | `winid_t` | **Mandatory** |
| `glk_cancel_line_event` | `extern void glk_cancel_line_event(winid_t win, event_t *event);` | Input | `winid_t`, `event_t` | **Mandatory** |
| `glk_set_echo_line_event` | `extern void glk_set_echo_line_event(winid_t win, glui32 val);` | Input | `winid_t` | **Mandatory** (most games use this) |

### 2.2 Character Input

| Function | Signature | Category | Types | Status |
|----------|-----------|----------|-------|--------|
| `glk_request_char_event` | `extern void glk_request_char_event(winid_t win);` | Input | `winid_t` | **Mandatory** |
| `glk_cancel_char_event` | `extern void glk_cancel_char_event(winid_t win);` | Input | `winid_t` | **Mandatory** |

### 2.3 Echo Stream

| Function | Signature | Category | Types | Status |
|----------|-----------|----------|-------|--------|
| `glk_window_set_echo_stream` | `extern void glk_window_set_echo_stream(winid_t win, strid_t str);` | Window | `winid_t`, `strid_t` | **Mandatory** |
| `glk_window_get_echo_stream` | `extern strid_t glk_window_get_echo_stream(winid_t win);` | Window | `winid_t`, `strid_t` | **Mandatory** |

### 2.4 Stub-able in Phase 2

| Function | Stub behaviour |
|----------|----------------|
| `glk_request_mouse_event` | No-op (deferred) |
| `glk_cancel_mouse_event` | No-op (deferred) |
| `glk_set_terminators_line_event` | No-op (ignore custom terminators, use Enter only) |
| `glk_select_poll` | Return `evtype_None` event |

---

## Phase 3 — Save and Restore

Required for `savefile.c` to function. Builds on Phases 1 and 2.

### 3.1 File References

| Function | Signature | Category | Types | Status |
|----------|-----------|----------|-------|--------|
| `glk_fileref_create_by_name` | `extern frefid_t glk_fileref_create_by_name(glui32 usage, char *name, glui32 rock);` | Fileref | `frefid_t` | **Mandatory** |
| `glk_fileref_create_temp` | `extern frefid_t glk_fileref_create_temp(glui32 usage, glui32 rock);` | Fileref | `frefid_t` | **Mandatory** (some games use temp for autosave) |
| `glk_fileref_create_by_prompt` | `extern frefid_t glk_fileref_create_by_prompt(glui32 usage, glui32 fmode, glui32 rock);` | Fileref | `frefid_t` | **Stub-able** (return NULL; hardcoded filename for Next) |
| `glk_fileref_destroy` | `extern void glk_fileref_destroy(frefid_t fref);` | Fileref | `frefid_t` | **Mandatory** |
| `glk_fileref_iterate` | `extern frefid_t glk_fileref_iterate(frefid_t fref, glui32 *rockptr);` | Fileref | `frefid_t` | **Mandatory** (required by Gi dispatch object registry) |
| `glk_fileref_delete_file` | `extern void glk_fileref_delete_file(frefid_t fref);` | Fileref | `frefid_t` | **Mandatory** |
| `glk_fileref_does_file_exist` | `extern glui32 glk_fileref_does_file_exist(frefid_t fref);` | Fileref | `frefid_t` | **Mandatory** |

### 3.2 File Streams

| Function | Signature | Category | Types | Status |
|----------|-----------|----------|-------|--------|
| `glk_stream_open_file` | `extern strid_t glk_stream_open_file(frefid_t fileref, glui32 fmode, glui32 rock);` | Stream | `frefid_t`, `strid_t` | **Mandatory** |
| `glk_stream_close` | `extern void glk_stream_close(strid_t str, stream_result_t *result);` | Stream | `strid_t`, `stream_result_t` | **Mandatory** |
| `glk_get_buffer_stream` | `extern glui32 glk_get_buffer_stream(strid_t str, char *buf, glui32 len);` | Input | `strid_t` | **Mandatory** (used by savefile.c to read save data) |
| `glk_get_char_stream` | `extern glsi32 glk_get_char_stream(strid_t str);` | Input | `strid_t` | **Mandatory** (used by savefile.c to read save data) |
| `glk_get_line_stream` | `extern glui32 glk_get_line_stream(strid_t str, char *buf, glui32 len);` | Input | `strid_t` | **Mandatory** (used by savefile.c) |
| `glk_stream_set_position` | `extern void glk_stream_set_position(strid_t str, glsi32 pos, glui32 seekmode);` | Stream | `strid_t` | **Mandatory** (used by savefile.c for seeks) |
| `glk_stream_get_position` | `extern glui32 glk_stream_get_position(strid_t str);` | Stream | `strid_t` | **Mandatory** (used by savefile.c to track position) |

### 3.3 Memory Streams

| Function | Signature | Category | Types | Status |
|----------|-----------|----------|-------|--------|
| `glk_stream_open_memory` | `extern strid_t glk_stream_open_memory(char *buf, glui32 buflen, glui32 fmode, glui32 rock);` | Stream | `strid_t` | **Mandatory** (required by Gi dispatch for various operations) |

### 3.4 Stub-able in Phase 3

| Function | Stub behaviour |
|----------|----------------|
| `glk_fileref_create_from_fileref` | Return clone (or same pointer with refcount) |
| `glk_fileref_get_rock` | Return 0 |
| `glk_stream_open_file_uni` | Call `glk_stream_open_file` and ignore Unicode flag (if Unicode module not implemented yet) |

---

## Phase 4 — Inline Images

Required to display inline images from Blorb resources. Builds on Phases 1–3.

### 4.1 Image Drawing

| Function | Signature | Category | Types | Status |
|----------|-----------|----------|-------|--------|
| `glk_image_draw` | `extern glui32 glk_image_draw(winid_t win, glui32 image, glsi32 val1, glsi32 val2);` | Image | `winid_t` | **Mandatory** |
| `glk_image_draw_scaled` | `extern glui32 glk_image_draw_scaled(winid_t win, glui32 image, glsi32 val1, glsi32 val2, glui32 width, glui32 height);` | Image | `winid_t` | **Mandatory** |
| `glk_image_get_info` | `extern glui32 glk_image_get_info(glui32 image, glui32 *width, glui32 *height);` | Image | none | **Mandatory** |

### 4.2 Image Support

| Function | Signature | Category | Types | Status |
|----------|-----------|----------|-------|--------|
| `glk_window_flow_break` | `extern void glk_window_flow_break(winid_t win);` | Window | `winid_t` | **Mandatory** (image alignment control) |
| `glk_window_erase_rect` | `extern void glk_window_erase_rect(winid_t win, glsi32 left, glsi32 top, glui32 width, glui32 height);` | Window | `winid_t` | **Mandatory** (needed to clear image area) |
| `glk_window_fill_rect` | `extern void glk_window_fill_rect(winid_t win, glui32 color, glsi32 left, glsi32 top, glui32 width, glui32 height);` | Window | `winid_t` | **Mandatory** (needed for color fills) |
| `glk_window_set_background_color` | `extern void glk_window_set_background_color(winid_t win, glui32 color);` | Window | `winid_t` | **Mandatory** |
| `glk_window_move_cursor` | `extern void glk_window_move_cursor(winid_t win, glui32 xpos, glui32 ypos);` | Window | `winid_t` | **Mandatory** (needed for image positioning) |

### 4.3 Stub-able in Phase 4

| Function | Stub behaviour |
|----------|----------------|
| `glk_image_draw_scaled_ext` | Fall back to `glk_image_draw_scaled` (ignore imagerule/maxwidth) |

---

## Phase 5 — Everything Else

Functions that are part of the Glk ABI but are not required for the five core capabilities. These can be deferred until after the mandatory phases are complete.

### 5.1 Unicode

| Function | Status | Notes |
|----------|--------|-------|
| `glk_put_char_uni` | **Stub-able** | Fall back to Latin-1 approximation |
| `glk_put_string_uni` | **Stub-able** | Fall back to Latin-1 approximation |
| `glk_put_buffer_uni` | **Stub-able** | Fall back to Latin-1 approximation |
| `glk_put_char_stream_uni` | **Stub-able** | Fall back to Latin-1 approximation |
| `glk_put_string_stream_uni` | **Stub-able** | Fall back to Latin-1 approximation |
| `glk_put_buffer_stream_uni` | **Stub-able** | Fall back to Latin-1 approximation |
| `glk_get_char_stream_uni` | **Stub-able** | Return Latin-1 or error |
| `glk_get_buffer_stream_uni` | **Stub-able** | Return Latin-1 or error |
| `glk_get_line_stream_uni` | **Stub-able** | Return Latin-1 or error |
| `glk_stream_open_file_uni` | **Stub-able** | Fall back to `glk_stream_open_file` |
| `glk_stream_open_memory_uni` | **Stub-able** | Fall back to `glk_stream_open_memory` |
| `glk_request_char_event_uni` | **Stub-able** | Fall back to `glk_request_char_event` |
| `glk_request_line_event_uni` | **Stub-able** | Fall back to `glk_request_line_event` |
| `glk_buffer_to_lower_case_uni` | **Stub-able** | Simple ASCII fallback |
| `glk_buffer_to_upper_case_uni` | **Stub-able** | Simple ASCII fallback |
| `glk_buffer_to_title_case_uni` | **Stub-able** | Simple ASCII fallback |

### 5.2 Unicode Normalization

| Function | Status | Notes |
|----------|--------|-------|
| `glk_buffer_canon_decompose_uni` | **Stub-able** | Return 0 (no decomposition needed) |
| `glk_buffer_canon_normalize_uni` | **Stub-able** | Return 0 (already normalized) |

### 5.3 Sound

| Function | Status | Notes |
|----------|--------|-------|
| `glk_schannel_create` | **Stub-able** | Return NULL |
| `glk_schannel_destroy` | **Stub-able** | No-op |
| `glk_schannel_iterate` | **Stub-able** | Return NULL |
| `glk_schannel_get_rock` | **Stub-able** | Return 0 |
| `glk_schannel_play` | **Stub-able** | Return 0 (failure) |
| `glk_schannel_play_ext` | **Stub-able** | Return 0 (failure) |
| `glk_schannel_stop` | **Stub-able** | No-op |
| `glk_schannel_set_volume` | **Stub-able** | No-op |
| `glk_sound_load_hint` | **Stub-able** | No-op |

### 5.4 Sound2

| Function | Status | Notes |
|----------|--------|-------|
| `glk_schannel_create_ext` | **Stub-able** | Return NULL |
| `glk_schannel_play_multi` | **Stub-able** | Return 0 (failure) |
| `glk_schannel_set_volume_ext` | **Stub-able** | No-op |
| `glk_schannel_pause` | **Stub-able** | No-op |
| `glk_schannel_unpause` | **Stub-able** | No-op |

### 5.5 Hyperlinks

| Function | Status | Notes |
|----------|--------|-------|
| `glk_set_hyperlink` | **Stub-able** | No-op |
| `glk_set_hyperlink_stream` | **Stub-able** | No-op |
| `glk_request_hyperlink_event` | **Stub-able** | No-op |
| `glk_cancel_hyperlink_event` | **Stub-able** | No-op |

### 5.6 DateTime

| Function | Status | Notes |
|----------|--------|-------|
| `glk_current_time` | **Stub-able** | Return zeroed struct |
| `glk_current_simple_time` | **Stub-able** | Return 0 |
| `glk_time_to_date_utc` | **Stub-able** | Return epoch date |
| `glk_time_to_date_local` | **Stub-able** | Return epoch date |
| `glk_simple_time_to_date_utc` | **Stub-able** | Return epoch date |
| `glk_simple_time_to_date_local` | **Stub-able** | Return epoch date |
| `glk_date_to_time_utc` | **Stub-able** | Return zeroed time |
| `glk_date_to_time_local` | **Stub-able** | Return zeroed time |
| `glk_date_to_simple_time_utc` | **Stub-able** | Return 0 |
| `glk_date_to_simple_time_local` | **Stub-able** | Return 0 |

### 5.7 Resource Streams

| Function | Status | Notes |
|----------|--------|-------|
| `glk_stream_open_resource` | **Stub-able** | Return NULL (images handled via Blorb, not resource streams) |
| `glk_stream_open_resource_uni` | **Stub-able** | Return NULL |

### 5.8 Remaining Window Operations

| Function | Status | Notes |
|----------|--------|-------|
| `glk_request_timer_events` | **Stub-able** | No-op |
| `glk_select_poll` | **Stub-able** | Return `evtype_None` |
| `glk_request_mouse_event` | **Stub-able** | No-op |
| `glk_cancel_mouse_event` | **Stub-able** | No-op |

### 5.9 Remaining Fileref Operations

| Function | Status | Notes |
|----------|--------|-------|
| `glk_fileref_create_from_fileref` | **Stub-able** | Return clone pointer with copied filename |

---

## Summary Table

| Phase | Capability | Mandatory functions | Stub-able functions | Total required |
|-------|------------|--------------------|--------------------|---------------|
| 1 | Boot + text | ~30 | ~10 | 30 |
| 2 | Keyboard input | 7 | 4 | 37 |
| 3 | Save/restore | 14 | 3 | 51 |
| 4 | Inline images | 8 | 1 | 59 |
| 5 | Everything else | 0 | ~55 | 59 |

**Total mandatory functions for minimum viable implementation: ~59**

---

## Dependency Chain

```
Phase 1 (Boot + Text)
│
├── glk_main, glk_exit, glk_gestalt, glk_gestalt_ext
├── glk_select (event loop)
├── glk_window_get_root, get_type, get_size, get_stream
├── glk_set_window
├── glk_put_char/string/buffer (+_stream variants)
├── glk_set_style (+_stream)
├── glk_stream_get_current, set_current, iterate
├── glk_stylehint_set, clear
├── glk_char_to_lower, to_upper
├── glk_window_open, close, iterate, get_sibling, clear
│
├── [stubs] get_rock, get_parent, move_cursor, arrangement,
│            stream_rock, open_memory, style_*measure, tick
│
▼
Phase 2 (Input)
│
├── glk_request_line_event, cancel_line_event
├── glk_request_char_event, cancel_char_event
├── glk_set_echo_line_event
├── glk_window_set/get_echo_stream
│
├── [stubs] mouse_event, terminators_line_event, select_poll
│
▼
Phase 3 (Save/Restore)
│
├── glk_fileref_create_by_name, create_temp, create_by_prompt
├── glk_fileref_destroy, iterate, delete_file, does_file_exist
├── glk_stream_open_file, close
├── glk_get_buffer_stream, get_char_stream, get_line_stream
├── glk_stream_set_position, get_position
├── glk_stream_open_memory
│
├── [stubs] fileref_create_from_fileref, get_rock, open_file_uni
│
▼
Phase 4 (Images)
│
├── glk_image_draw, draw_scaled
├── glk_image_get_info
├── glk_window_flow_break, erase_rect, fill_rect
├── glk_window_set_background_color, move_cursor
│
├── [stubs] image_draw_scaled_ext
│
▼
Phase 5 (Everything else — stub as needed)
│
├── All remaining glk.h functions


# Recommended Implementation Order

## Step 1 - Core Object Types

Implement:

- winid_t
- strid_t
- frefid_t

Files:

- next_window.c
- next_stream.c
- next_fileref.c

Goal:

- Objects can be created
- Objects can be destroyed
- Objects can be iterated

No rendering required.

---

## Step 2 - Stream Routing

Implement:

- glk_stream_get_current
- glk_stream_set_current
- glk_put_char_stream
- glk_put_string_stream
- glk_put_buffer_stream

Goal:

Characters written to a stream arrive somewhere visible.

Linux target:

stdout

Next target:

screen abstraction

---

## Step 3 - Window Routing

Implement:

- glk_window_open
- glk_window_get_stream
- glk_set_window

Goal:

Current window selects current output stream.

---

## Step 4 - Text Output

Implement:

- glk_put_char
- glk_put_string
- glk_put_buffer

Goal:

Game text appears.

---

## Step 5 - Event Loop

Implement:

- glk_select
- glk_request_line_event
- glk_request_char_event

Goal:

Player can type commands.

---

## Step 6 - Save / Restore

Implement Phase 3 APIs.

---

## Step 7 - Images

Implement:

- glk_image_draw
- glk_image_get_info

Initially:

- Placeholder rendering

Later:

- Blorb resource lookup
- Layer 2 rendering