# Post-Save Input Event Trace

## Summary

After `save`, `glk_request_line_event` is called with `win=(nil)` in NextGlk.
This causes an infinite `evtype_None` loop in `glk_select`, freezing
command processing. The same game works correctly with CheapGlk.

The window=NULL parameter originates BEFORE NextGlk — it comes from the
Git VM's dispatch layer (`glkop.c`), which resolves the VM window ID to
NULL. The NextGlk function `glk_request_line_event` silently returns on
NULL window, setting no line request flag.

## Trace Evidence

### Working Path (Fresh Game → "save")

Calls extracted from `/tmp/trace-next-save.log` (build confirmed at
2026-07-04, commit `be63e47`):

```
Line 86:  TRACE gi_dispa request_line_event win=0x6362b87998b0
Line 87:  DEBUG req_line_event(8-bit): win=0x6362b87998b0 buf=... maxlen=256 initlen=0
Line 88:  DEBUG req_line_event(8-bit): done, line_request=1
Line 91:  TRACE git_perform_glk CALL func=0x00C0
Line 92:  DEBUG glk_select: line_req=1 line_req_uni=0 mainwin=0x6362b87998b0
Line 93:  DEBUG glk_select: 8-bit input, buf=... buflen=256
Line 94:  DEBUG glk_select: read 4 chars
Line 95:  TRACE git_perform_glk RET  func=0x00C0 retval=0
```

- `glk_request_line_event` receives a valid window pointer
- Sets `line_request=1`
- `glk_select` reads "save" (4 chars)
- Returns `evtype_LineInput`
- Game processes save command

### Broken Path (After Save → "look")

```
Line 138: TRACE glkop Q-resolve: class=0 vm_id=0 -> opref=NULL
Line 140: TRACE gi_dispa request_line_event win=(nil)
Line 141: DEBUG req_line_event(8-bit): win=(nil) buf=... maxlen=256 initlen=0
Line 143: TRACE git_perform_glk CALL func=0x00C0
Line 144: DEBUG glk_select: line_req=0 line_req_uni=0 mainwin=0x6362b87998b0
Line 145: DEBUG glk_select: returning evtype_None
Line 146: TRACE git_perform_glk RET  func=0x00C0 retval=0
Line 148: TRACE git_perform_glk CALL func=0x00C0
Line 149: DEBUG glk_select: line_req=0 line_req_uni=0 mainwin=0x6362b87998b0
Line 150: DEBUG glk_select: returning evtype_None
... (repeats indefinitely)
```

- `glkop.c` resolves class=window, vm_id=0 → opref=NULL
- `gi_dispa.c` calls `glk_request_line_event(NULL, ...)`
- `glk_request_line_event` sees `winptr == NULL`, returns immediately
- `line_request` remains 0
- `glk_select` returns `evtype_None` in an infinite loop
- Characters still echo (terminal default echo), no game response

### Comparison with CheapGlk

CheapGlk processes "save" + "look" + "quit" without any issue. After
save, the game prompts "Frodo>" and processes "look" correctly, then
"quit" correctly. CheapGlk never receives a NULL window from the VM.

## First Point of Divergence

```
NextGlk, after save completes:
  glkop.c: classes_get(gidisp_Class_Window, 0) → NULL
   ↓
  gi_dispa.c line 1022-1023: glk_request_line_event(NULL, ...)
   ↓
  nextglk.c line 1070-1071: if (!winptr) return;  ← SILENT RETURN
   ↓
  line_request never set to 1
   ↓
  glk_select returns evtype_None forever

CheapGlk, after save completes:
  VM passes a valid window ID
  glk_request_line_event receives valid window
  line_request set to TRUE
  glk_select reads "look"
  Works correctly
```

## Exact Function Responsible

**NextGlk `glk_request_line_event`** (`nextglk.c`, lines 1062–1087):

```c
void glk_request_line_event(winid_t win, char *buf, glui32 maxlen,
    glui32 initlen)
{
    window_t *winptr = (window_t *)win;
    ...
    if (!winptr)
        return;   // ← Returns silently. No error. No fallback.
    ...
}
```

CheapGlk's equivalent (`cgwindow.c`):

```c
void glk_request_line_event(window_t *win, char *buf, glui32 maxlen,
    glui32 initlen)
{
    if (!win || win != mainwin) {
        gli_strict_warning("request_line_event: invalid id");
        return;
    }
    ...
    mainwin->line_request = TRUE;
    ...
}
```

## Evidence Summary

| Stage | Fresh Game | After Save |
|-------|-----------|------------|
| VM window ID | 38 (valid) | 0 (NULL) |
| glkop Q-resolve | opref=0x6362b87998b0 | opref=NULL |
| request_line_event win | 0x6362b87998b0 | (nil) |
| line_request | 1 | 0 (never set) |
| glk_select result | evtype_LineInput | evtype_None loop |

## Recommended Fix

`glk_request_line_event` must handle the NULL-window case by falling back
to `gli_mainwin`. This matches the behavior expected by the VM after save:

```c
void glk_request_line_event(winid_t win, char *buf, glui32 maxlen,
    glui32 initlen)
{
    window_t *winptr = (window_t *)win;

    if (!winptr)
        winptr = gli_mainwin;

    if (!winptr)
        return;

    winptr->linebuf = (void *)buf;
    winptr->linebuflen = maxlen;
    winptr->line_request = 1;
    ...
}
```

The same fallback should be applied to `glk_request_line_event_uni` in
`nextglk_stubs.c`.

## Trace Files

- NextGlk trace: `/tmp/trace-next-save.log` (51,158,587 lines)
- CheapGlk trace: `/tmp/trace-cheap-save.log`
- Builds confirmed at 2026-07-04, commit `be63e47`