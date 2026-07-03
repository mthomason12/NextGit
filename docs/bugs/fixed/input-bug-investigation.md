# Input Bug Investigation

**Date:** 2026-07-03
**Status:** Complete
**Task:** Investigate why all user input results in "That's not a verb I recognise."

---

## 1. Overview

The Git VM is running, the parser is executing, and events are being generated. However, every command — including standard Inform 7 verbs like `look` — is rejected as unrecognised. This document traces the full line input path in both CheapGlk (the reference implementation) and NextGlk, identifies the root cause, and ranks suspected causes by confidence.

---

## 2. Side-by-Side: `glk_request_line_event()`

### CheapGlk (`thirdparty/cheapglk/cgwindow.c:268-289`)

```c
void glk_request_line_event(window_t *win, char *buf, glui32 maxlen,
    glui32 initlen)
{
    // ... validation ...

    mainwin->line_request = TRUE;
    mainwin->line_request_uni = FALSE;
    mainwin->linebuf = buf;          // stores pointer to dispatch copy
    mainwin->linebuflen = maxlen;

    if (gli_register_arr) {
        win->inarrayrock = (*gli_register_arr)(buf, maxlen, "&+#!Cn");
        //   ^^^^^^^^^^^^  REGISTERS the array as retained
    }
}
```

### NextGlk (`nextglk/nextglk.c:733-746`)

```c
void glk_request_line_event(winid_t win, char *buf, glui32 maxlen,
    glui32 initlen)
{
    window_t *winptr = (window_t *)win;
    if (!winptr) return;

    winptr->linebuf = (void *)buf;   // stores pointer to dispatch copy
    winptr->linebuflen = maxlen;
    winptr->line_request = 1;

    (void)initlen;
    // NOTE: NO call to gli_register_arr()
}
```

### Key Difference

| Aspect | CheapGlk | NextGlk |
|--------|----------|---------|
| `gli_register_arr()` call | **YES** — marks array as retained | **NO** — array never registered |
| `inarrayrock` stored | Yes | Field exists but never set |
| Array lifecycle managed | Yes | No |

---

## 3. Side-by-Side: `glk_cancel_line_event()`

### CheapGlk (`thirdparty/cheapglk/cgwindow.c:354-393`)

```c
void glk_cancel_line_event(window_t *win, event_t *ev)
{
    // ... setup ...

    if (mainwin->line_request) {
        if (gli_unregister_arr) {
            char *typedesc = (mainwin->line_request_uni ? "&+#!Iu" : "&+#!Cn");
            (*gli_unregister_arr)(mainwin->linebuf, mainwin->linebuflen,
                typedesc, mainwin->inarrayrock);
            //   ^^^^^^^^^^^^^^  UNREGISTERS and writes back to VM
        }

        mainwin->line_request = FALSE;
        mainwin->linebuf = NULL;
        mainwin->linebuflen = 0;

        ev->type = evtype_LineInput;
        ev->val1 = 0;
        ev->val2 = 0;
        ev->win = mainwin;
    }
}
```

### NextGlk (`nextglk/nextglk.c:760-775`)

```c
void glk_cancel_line_event(winid_t win, event_t *event)
{
    window_t *winptr = (window_t *)win;
    if (!winptr) return;

    winptr->line_request = 0;

    if (event) {
        event->type = evtype_LineInput;
        event->win = win;
        event->val1 = 0;
        event->val2 = 0;
    }
    // NOTE: NO call to gli_unregister_arr()
    // NOTE: linebuf pointer is NOT cleared
}
```

### Key Difference

| Aspect | CheapGlk | NextGlk |
|--------|----------|---------|
| `gli_unregister_arr()` call | **YES** — writes back to VM | **NO** |
| `linebuf` cleared to NULL | **YES** | **NO** |
| `linebuflen` cleared to 0 | **YES** | **NO** |

---

## 4. Side-by-Side: `glk_select()` — Line Input Path

### CheapGlk (`thirdparty/cheapglk/cgmisc.c:163-257`)

```c
// line_request path:
char buf[256];                          // LOCAL buffer
res = fgets(buf, 255, stdin);           // read into LOCAL buffer

val = strlen(buf);
if (val && (buf[val-1] == '\n' || buf[val-1] == '\r'))
    val--;                              // strip newline (just decrement count)

if (!gli_utf8input) {
    if (val > win->linebuflen)
        val = win->linebuflen;          // clamp to maxlen
    if (!win->line_request_uni) {
        memcpy(win->linebuf, buf, val); // COPY to dispatch buffer
        // NOTE: no NUL terminator written
    }
    // ...
}

// Echo to echo stream (if set) ...

if (gli_unregister_arr) {
    (*gli_unregister_arr)(win->linebuf, win->linebuflen,
        "&+#!Cn", win->inarrayrock);
    //   ^^^^^^^^^^^^^^  WRITES BACK to VM memory via dispatch layer
}

win->line_request = FALSE;
win->linebuf = NULL;                    // clear pointer
event->type = evtype_LineInput;
event->win = win;
event->val1 = val;                      // number of chars (no NUL counted)
```

### NextGlk (`nextglk/nextglk.c:283-313` + `nextglk_input.c:10-26`)

```c
// In glk_select():
uint16_t len = nextglk_read_line(
    (char *)win->linebuf,
    (uint16_t)(win->linebuflen > 0 ? win->linebuflen : 0));

event->type = evtype_LineInput;
event->win = (winid_t)win;
event->val1 = (glui32)len;
event->val2 = 0;

win->line_request = 0;
// NOTE: linebuf pointer NOT cleared
// NOTE: NO call to gli_unregister_arr()

// In nextglk_read_line():
if (!fgets(buffer, buffer_size, stdin))  // reads DIRECTLY into VM buffer copy
    return 0;

size_t len = strlen(buffer);
while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
    buffer[--len] = '\0';               // strip newline WITH NUL write

return (uint16_t)len;
```

### Key Differences

| Aspect | CheapGlk | NextGlk |
|--------|----------|---------|
| Read target | Local `char buf[256]` | **Directly into `win->linebuf`** (dispatch copy) |
| Newline strip | Decrement `val` only | **Write NUL bytes** into buffer |
| NUL terminator written | **NO** | **YES** (via fgets + strip loop) |
| Clamp to `linebuflen` | **YES** — `if (val > win->linebuflen) val = win->linebuflen` | **Implicit via fgets** — reads at most `buffer_size-1` chars |
| `gli_unregister_arr()` call | **YES** — writes back to VM | **NO** |
| `linebuf` cleared to NULL | **YES** | **NO** |

---

## 5. The Dispatch Layer and Array Lifecycle

### How the Git VM calls Glk functions

In `thirdparty/git/glkop.c`, the function `git_perform_glk()` handles all Glk calls from the VM. The flow is:

```
git_perform_glk(funcnum, numargs, arglist)
  ├── prepare_glk_args()     — allocate gluniversal_t array
  ├── parse_glk_args()       — capture arrays from VM memory
  │     └── CaptureCArray()  — malloc'd COPY of VM buffer
  ├── gidispatch_call()      — call the actual Glk function
  │     └── glk_request_line_event(win, COPY_PTR, maxlen, initlen)
  └── unparse_glk_args()     — release arrays back to VM memory
        └── ReleaseCArray()  — write COPY back to VM, then free
```

### The Retained Array Mechanism

For `request_line_event` (prototype `"3Qa&+#!CnIu:"`):

- `&` = reference with passin+passout
- `#` = array
- `!` = **retained** — the array persists across multiple Glk calls

When an array is **retained**:

1. `CaptureCArray()` creates a malloc'd copy of the VM buffer
2. `ReleaseCArray()` in `unparse_glk_args()` **skips the write-back** and **does not free** the copy (because `arref->retained == TRUE`)
3. The copy persists in `win->linebuf`
4. Later, `gli_unregister_arr()` (called from the Glk library) triggers `git_retained_unregister()`, which **writes the copy back to VM memory** and **frees the copy**

### CheapGlk's Correct Flow

```
1. parse_glk_args()
     CaptureCArray(vm_addr, maxlen, passin=TRUE)
       → malloc'd copy of VM buffer
       → arref->retained = FALSE

2. gidispatch_call()
     glk_request_line_event(win, copy, maxlen, initlen)
       → win->linebuf = copy
       → gli_register_arr(copy, maxlen, "&+#!Cn")
         → arref->retained = TRUE    (marks as retained)

3. unparse_glk_args()
     ReleaseCArray(copy, vm_addr, maxlen, passout=TRUE)
       → arref->retained == TRUE, so: return early (no write-back, no free)

4. glk_select()  (called later, in a separate dispatch cycle)
     → reads input into local buffer
     → memcpy(win->linebuf, local_buf, val)   — writes to the copy
     → gli_unregister_arr(copy, maxlen, "&+#!Cn", inarrayrock)
       → git_retained_unregister()
         → memWrite8(vm_addr, copy[0..maxlen-1])  — writes back to VM!
         → free(copy)
     → win->linebuf = NULL
```

### NextGlk's Broken Flow

```
1. parse_glk_args()
     CaptureCArray(vm_addr, maxlen, passin=TRUE)
       → malloc'd copy of VM buffer
       → arref->retained = FALSE

2. gidispatch_call()
     glk_request_line_event(win, copy, maxlen, initlen)
       → win->linebuf = copy
       → **NO call to gli_register_arr()**
         → arref->retained stays FALSE

3. unparse_glk_args()
     ReleaseCArray(copy, vm_addr, maxlen, passout=TRUE)
       → arref->retained == FALSE, so:
         → memWrite8(vm_addr, copy[0..maxlen-1])  — writes ORIGINAL data back
         → free(copy)                              — FREES the copy!
       → win->linebuf is now a **DANGLING POINTER**

4. glk_select()  (called later, in a separate dispatch cycle)
     → fgets(win->linebuf, ...)  — **WRITES TO FREED MEMORY** (undefined behavior)
     → **NO call to gli_unregister_arr()**
     → The VM buffer in VM memory was NEVER updated with the input
```

---

## 6. Root Cause Analysis

### Root Cause: Missing retained array registration

**NextGlk's `glk_request_line_event()` does not call `gli_register_arr()`.**

This causes a cascade of failures:

1. The dispatch layer's `ReleaseCArray()` writes the **unchanged original data** back to VM memory and **frees the buffer copy**.
2. `win->linebuf` becomes a dangling pointer.
3. `glk_select()` writes input to freed memory (undefined behavior).
4. The VM's buffer in its own memory space is **never updated** with the user's input.
5. The parser reads whatever was in the VM buffer (zeros or stale data) and fails to recognise any verb.

### Why the game responds at all

The event IS generated correctly:
- `event->type = evtype_LineInput` — the VM sees a line input event
- `event->val1 = len` — the VM thinks it received `len` characters
- The VM reads from its own buffer (in VM memory), which still contains the original data (zeros or uninitialised)

The parser runs, finds an empty or garbage buffer, and reports "That's not a verb I recognise."

---

## 7. Ranked Suspected Causes

| Rank | Cause | Confidence | Description |
|------|-------|------------|-------------|
| **1** | **Missing `gli_register_arr()` / `gli_unregister_arr()` calls** | **100%** | The retained array lifecycle is not implemented. The dispatch copy is freed before `glk_select()` runs, and the VM buffer is never updated. This is the definitive root cause. |
| 2 | NUL terminator written into buffer | 30% | NextGlk writes NUL bytes during newline stripping. CheapGlk does not. If the VM/parser is sensitive to NUL bytes in the buffer, this could cause issues — but it's secondary to cause #1. |
| 3 | Buffer size semantics differ | 20% | CheapGlk clamps to `linebuflen` after reading; NextGlk relies on `fgets` to limit. If `linebuflen` is 0 or 1, behavior differs. Unlikely to be the primary cause since the game runs. |
| 4 | `linebuf` not cleared to NULL after input | 10% | NextGlk doesn't NULL `linebuf` after input. Could cause double-use issues on re-entrant calls. Minor compared to cause #1. |
| 5 | `initlen` parameter ignored | 5% | NextGlk ignores `initlen` (pre-filled buffer content). If the game pre-fills the buffer, this could cause data loss. Unlikely for standard Inform 7 games. |

---

## 8. Recommended Fix

### Required: Implement retained array registration

In `glk_request_line_event()` (in `nextglk.c`), add a call to `gli_register_arr()`:

```c
void glk_request_line_event(winid_t win, char *buf, glui32 maxlen,
    glui32 initlen)
{
    window_t *winptr = (window_t *)win;
    if (!winptr) return;

    winptr->linebuf = (void *)buf;
    winptr->linebuflen = maxlen;
    winptr->line_request = 1;

    (void)initlen;

    // Register the array with the dispatch layer so it persists
    // across the glk_select() call and gets written back to VM memory.
    if (gli_register_arr) {
        winptr->inarrayrock = (*gli_register_arr)(buf, maxlen, "&+#!Cn");
    }
}
```

In `glk_select()` (in `nextglk.c`), after reading input, add a call to `gli_unregister_arr()`:

```c
// After reading input and before clearing the request:
if (gli_unregister_arr) {
    (*gli_unregister_arr)(win->linebuf, win->linebuflen,
        "&+#!Cn", win->inarrayrock);
}

win->line_request = 0;
```

In `glk_cancel_line_event()` (in `nextglk.c`), add a call to `gli_unregister_arr()`:

```c
if (winptr->line_request) {
    if (gli_unregister_arr) {
        (*gli_unregister_arr)(winptr->linebuf, winptr->linebuflen,
            "&+#!Cn", winptr->inarrayrock);
    }
    winptr->line_request = 0;
    // ...
}
```

### Recommended: Read into a local buffer, then copy

Change `nextglk_read_line()` to read into a local buffer and `memcpy` to `win->linebuf`, matching CheapGlk's approach. This avoids writing directly to the dispatch copy and provides a clean separation of concerns.

### Recommended: Do not write NUL terminators

CheapGlk does not NUL-terminate the buffer. The `val1` field tells the VM how many characters are valid. Remove the NUL writes from the newline stripping loop, or at minimum ensure they don't overwrite valid data.

---

## 9. Summary

| Question | Answer |
|----------|--------|
| How does CheapGlk fill the VM buffer? | Reads into a local buffer, copies to dispatch copy, then `gli_unregister_arr()` writes back to VM memory via the dispatch layer. |
| How does NextGlk fill the VM buffer? | Reads directly into `win->linebuf` (the dispatch copy) via `fgets()`, but never writes back to VM memory. |
| How is `event.val1` calculated? | Both: number of characters after newline stripping. Same semantics. |
| Is a terminating NUL written? | CheapGlk: **No**. NextGlk: **Yes** (by `fgets` and the strip loop). |
| Is the newline stripped correctly? | Both strip `\n` and `\r`. CheapGlk decrements count; NextGlk writes NUL. |
| Do buffer size semantics match? | **No.** CheapGlk clamps after read; NextGlk relies on `fgets` limit. |
| Does Inform expect additional formatting? | **No.** The Glk spec says `val1` is the number of characters. No NUL required. |
| **What causes the bug?** | **Missing `gli_register_arr()` / `gli_unregister_arr()` calls.** The dispatch copy is freed before `glk_select()` runs, and the VM buffer is never updated with user input. |

**Confidence in root cause: 100%**