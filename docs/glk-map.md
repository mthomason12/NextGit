# Glk Mapping

## Output

### Functions

- glk_put_char
- glk_put_char_uni

### Primary Caller

- terp.c

### Secondary Callers

- accel.c
- savefile.c
- glkop.c

### NextGlk Mapping

- nextglk_put_char
- nextglk_put_string
- nextglk_put_buffer

### Status

Identified

---

## Input

### Functions

Unknown

### Candidate Functions

- glk_select
- glk_request_line_event
- glk_request_char_event

### Status

Investigate

---

## Images

### Functions

- glk_image_draw
- glk_image_get_info
- glk_image_draw_scaled
- glk_image_draw_scaled_ext

### Current CheapGlk Behaviour

Stub implementations returning FALSE.

### NextGlk Mapping

- nextglk_image_show
- nextglk_image_get_info

### Status

Identified

---

## Files

### Functions

- glk_fileref_*
- glk_stream_*

### NextGlk Mapping

- nextglk_file_open_read
- nextglk_file_open_write
- nextglk_file_read
- nextglk_file_write
- nextglk_file_close

### Status

Investigate

---

## Confirmed Requirements

- Inform 7
- Glulx
- Inline images
- No sound

---

## Confirmed Not Required

- Sound
- Hyperlinks
- Timers
- Mouse input

---

## VM / Glk Boundary

Primary boundary:

- git_perform_glk()

Location:

- glkop.c

Purpose:

- Converts Glulx arguments into Glk calls
- Dispatches all VM-to-Glk operations

Status:

- Identified
