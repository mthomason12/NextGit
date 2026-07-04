# Save/Restore — Input State After Restore Investigation

## Summary

After a successful `save` then `restore`, the game resumes but never processes
subsequent line input. The user can type characters (terminal echo shows them),
but pressing Enter elicits no game response.

Static analysis of the complete save/restore flow and the NextGlk input
state management identifies several contributing factors. The primary root
cause is **missing Unicode line input support** combined with a
**dispatch-layer prototype bug** for `glk_request_line_event`.

---

## 1. Complete Save/Restore Flow (Static Trace)

### 1.1 Normal Save Flow

```
Player types "save"
  → glk_select() blocks on fgets(), reads "save"
  → gli_unregister_arr() writes buffer back to VM memory
  → win->line_request = 0
  → Game processes "save" action:
      1. glk_fileref_create_by_prompt(fileusage_SavedGame, ...) → "nextgit.sav"
      2. glk_stream_open_file(fref, filemode_Write, ...) → stream S
      3. @save(S)
         → finish_save_stub (terp.c:651-682)
         → saveToFile(base, sp, S) (savefile.c:212-315)
           - redirects current stream to S via glk_stream_set_current(S)
           - writes Quetzal chunks (IFhd, Stks, MAll, CMem)
           - restores current stream via glk_stream_set_current(oldFile)
         → returns 0, S1 = -1
         → goto do_pop_call_stub
      4. glk_stream_close(S, result)
      5. glk_fileref_destroy(fref)
      6. Output: "Save succeeded."
  → Main game loop:
      7. glk_request_line_event(win, buf, maxlen, initlen)
      8. glk_select(&event) → blocks for next command
```

### 1.2 Normal Restore Flow

```
Player types "restore"
  → glk_select() blocks on fgets(), reads "restore"
  → gli_unregister_arr() writes buffer back to VM memory
  → win->line_request = 0
  → Game processes "restore" action:
      1. glk_fileref_create_by_prompt(fileusage_SavedGame, ...) → "nextgit.sav"
      2. glk_stream_open_file(fref, filemode_Read, ...) → stream R
      3. @restore(R)
         → do_restore (terp.c:684-693)
         → restoreFromFile(base, R, protectPos, protectSize) (savefile.c:29-210)
           - reads Quetzal chunks
           - REPLACES ALL VM MEMORY via memcpy/resizeMemory
           - restores stack: gStackPointer = base; for(...) *gStackPointer++ = readWord()
           - restores heap via heap_apply_summary()
         → returns 0
         → sp = gStackPointer (restored stack pointer)
         → S1 = -1 (success)
         → goto do_pop_call_stub
```

**Critical observation**: After `do_restore`, the VM's memory, stack, heap,
and program counter are completely replaced with the state from the saved
game. The NextGlk library state (`gli_mainwin`, `gli_streamlist`, etc.) is
NOT restored — it persists across the `@restore` opcode.

### 1.3 Post-Restore Execution

After `@restore` returns, the VM continues at the saved program counter,
which is the instruction AFTER the `@save` opcode in the save action code:

```
  (saved PC points here)
      4. glk_stream_close(S, result)
      5. glk_fileref_destroy(fref)
      6. Output: "Save succeeded."
  → Main game loop:
      7. glk_request_line_event(win, buf, maxlen, initlen)
      8. glk_select(&event)
```

Step 4 closes the save stream. The save stream was already closed in the
original timeline (during the save that happened before restore). In
NextGlk, the stream has been removed from gli_streamlist and unregistered
from the dispatch hash table. So `git_find_stream_by_id(S_id)` returns NULL,
and `glk_stream_close(NULL, ...)` is a safe no-op.

Step 5 similarly calls `glk_fileref_destroy(NULL)` which is a safe no-op.

Step 6 prints output to the window stream — this should work correctly.

**Step 7 is the critical point**: `glk_request_line_event(win, buf, maxlen, initlen)`.
This function must set `win->line_request = 1` so that `glk_select()` in
step 8 blocks and reads input.

---

## 2. Glk Input State Machine

### 2.1 window_t Input Fields

```c
// nextglk_internal.h, lines 81-90
struct glk_window_struct {
    ...
    int line_request;           // 1 if line input is pending (8-bit)
    int line_request_uni;       // 1 if line input is pending (Unicode)
    int char_request;           // 1 if char input is pending
    int char_request_uni;       // 1 if char input is pending (Unicode)

    void *linebuf;              // pointer to VM memory buffer (NOT OWNED)
    glui32 linebuflen;          // buffer capacity
    gidispatch_rock_t inarrayrock;  // retained array registration
};
```

### 2.2 glk_request_line_event (8-bit) — nextglk.c:1012-1031

```c
void glk_request_line_event(winid_t win, char *buf, glui32 maxlen, glui32 initlen)
{
    window_t *winptr = (window_t *)win;
    if (!winptr)
        return;
    winptr->linebuf = (void *)buf;
    winptr->linebuflen = maxlen;
    winptr->line_request = 1;
    (void)initlen;
    if (gli_register_arr) {
        winptr->inarrayrock = (*gli_register_arr)(buf, maxlen, "&+#!Cn");
    }
}
```

### 2.3 glk_request_line_event_uni (Unicode) — nextglk_stubs.c:214-223

```c
void glk_request_line_event_uni(winid_t win, glui32 *buf, glui32 maxlen, glui32 initlen)
{
    (void)win;
    (void)buf;
    (void)maxlen;
    (void)initlen;
    /* Stub — no-op. */
}
```

**THIS IS A COMPLETE NO-OP.** It does NOT set `line_request_uni` or any
other field.

### 2.4 glk_select — nextglk.c:292-331

```c
void glk_select(event_t *event)
{
    if (!event)
        return;

    if (gli_mainwin && gli_mainwin->line_request) {
        window_t *win = gli_mainwin;
        uint16_t len = nextglk_read_line(
            (char *)win->linebuf,
            (uint16_t)(win->linebuflen > 0 ? win->linebuflen : 0));

        event->type = evtype_LineInput;
        event->win = (winid_t)win;
        event->val1 = (glui32)len;
        event->val2 = 0;

        if (gli_unregister_arr) {
            (*gli_unregister_arr)(win->linebuf, win->linebuflen,
                "&+#!Cn", win->inarrayrock);
        }

        win->line_request = 0;
        win->linebuf = NULL;
        win->linebuflen = 0;
        return;
    }

    event->type = evtype_None;
    event->win = NULL;
    event->val1 = 0;
    event->val2 = 0;
}
```

**glk_select only checks `line_request`.** It does NOT check
`line_request_uni`. If the game calls `glk_request_line_event_uni`,
glk_select returns `evtype_None` immediately.

---

## 3. Root Cause Analysis

### 3.1 Primary Cause: Missing Unicode Line Input Support

Modern Inform 7 games compiled for Glulx 3.0+ use the Unicode Glk APIs.
The dispatch function table in `gi_dispa.c` registers BOTH:

| Dispatch ID | Function | Prototype | NextGlk Status |
|-------------|----------|-----------|----------------|
| 0x00D0 | `glk_request_line_event` | `3Qa&+#!CnIu:` | **REAL** |
| 0x0141 | `glk_request_line_event_uni` | `3Qa&+#!IuIu:` | **STUB (no-op)** |

Whether the game calls 0x00D0 or 0x0141 depends on the Inform 7 compiler
version and the Glulx target version:

- Glulx 2.x games → always use 0x00D0 (8-bit)
- Glulx 3.0+ games → typically use 0x0141 (Unicode)

**If the game uses `glk_request_line_event_uni`**, then:
1. `line_request_uni` is never set (stub is a no-op)
2. `glk_select` never checks `line_request_uni`
3. `glk_select` always returns `evtype_None`
4. The game spins in an event loop, never reading input
5. The user's keystrokes echo via terminal default echo, but no one reads them

**Verification needed**: Determine which dispatch ID the target game actually
calls. If it's 0x0141, the fix is to implement `glk_request_line_event_uni`
and update `glk_select` to handle `line_request_uni`.

### 3.2 Contributing Factor: Dispatch Prototype Bug

The dispatch implementation for `glk_request_line_event` (0x00D0) in
`gi_dispa.c:1016-1022` accesses `arglist[4].uint`:

```c
glk_request_line_event(arglist[0].opaqueref, arglist[2].array,
    arglist[3].uint, arglist[4].uint);
//                       ^^^^^^^^^^^^^^
//                       arglist index 4!
```

But the prototype `3Qa&+#!CnIu:` only defines 3 formal arguments, which
expand to 4 garglist entries (indices 0-3):
- arglist[0]: Qa → opaqueref (window)
- arglist[1]: ptrflag for the array reference
- arglist[2]: #Cn → the captured char array
- arglist[3]: Iu → the array length (maxlen)

`arglist[4]` is out of bounds. The `initlen` parameter of
`glk_request_line_event` has no corresponding slot in the arglist.

The `garglist` is allocated with `maxargs + 16` entries, so `arglist[4]`
reads from padding/leftover memory. If it happens to contain 0, the call
appears to work with `initlen=0`. On some platforms or after specific
call sequences, it might contain garbage.

**The correct prototype should include `initlen`**:

```
Current (incorrect):  "3Qa&+#!CnIu:"
Should be:            "4Qa&+#!CnIuIu:"
```

This would add a 5th arglist entry (index 4) containing the `initlen` value.
The game already passes 4 VM arguments: win, buf_addr, maxlen, initlen.

**Note**: This bug exists regardless of save/restore but may be exposed
more consistently after the restore path due to different call sequences
affecting the garbage value at `arglist[4]`.

### 3.3 Contributing Factor: Retained Array Lifecycle Across Restore

The Glk dispatch layer maintains a global linked list of array references
(`arrays` in `glkop.c:209`). When `glk_request_line_event` is called, a
temporary buffer is allocated (`grab_temp_c_array`), an `arrayref_t` is
added to the list, and `gli_register_arr` marks it as retained.

After `@restore` replaces VM memory:
- The `arrays` linked list in Git's dispatch layer is NOT cleared
- The `window_t.inarrayrock` in NextGlk is NOT reset
- The `window_t.linebuf` still points to the pre-restore VM buffer

When the game calls `glk_request_line_event` (step 7), it passes a new
buffer from the restored VM. This triggers:
1. `grab_temp_c_array(new_addr, maxlen, ...)` — allocates new temp buffer
2. `gli_register_arr(new_buf, maxlen, ...)` — marks the NEW array as retained

The old retained array (from before restore) is lost — it's never
unregistered. This is a memory leak but shouldn't cause incorrect behavior
because `glk_select` will unregister the NEW array.

**However**, if the game does NOT call `glk_request_line_event` after
restore (e.g., because it uses a Unicode API that's a no-op), then
`line_request` remains 0 and `glk_select` returns `evtype_None`.

### 3.4 Symptom Explanation

The user reports: "User can type. Characters echo. Pressing Enter causes no
game response."

This matches the behavior when `glk_select` returns `evtype_None`:

1. Game calls `glk_select(&event)`
2. `gli_mainwin->line_request` is 0 (either because the Unicode stub didn't
   set it, or because something cleared it)
3. `glk_select` returns immediately with `evtype_None`
4. Game sees `evtype_None`, loops, calls `glk_select` again
5. The user types characters — terminal echoes them (Linux default behavior)
6. But `fgets()` is never called, so the game never sees the input
7. Pressing Enter just adds a newline to the terminal but doesn't trigger
   any game processing

---

## 4. Diagnostic Commands

To verify which dispatch ID the game uses, add diagnostic output:

### 4.1 In glk_request_line_event (nextglk.c ~line 1012):

```c
fprintf(stderr, "DEBUG: glk_request_line_event win=%p buf=%p maxlen=%u\n",
    (void*)win, (void*)buf, maxlen);
```

### 4.2 In glk_request_line_event_uni (nextglk_stubs.c ~line 214):

```c
fprintf(stderr, "DEBUG: glk_request_line_event_UNI win=%p buf=%p maxlen=%u\n",
    (void*)win, (void*)buf, maxlen);
```

### 4.3 In glk_select (nextglk.c ~line 292):

```c
fprintf(stderr, "DEBUG: glk_select line_request=%d line_request_uni=%d\n",
    gli_mainwin ? gli_mainwin->line_request : -1,
    gli_mainwin ? gli_mainwin->line_request_uni : -1);
```

Run the game, perform save/restore, and observe which functions are called.

---

## 5. Fix Plan

### Priority 1: Implement glk_request_line_event_uni

Convert the stub in `nextglk_stubs.c` to a real implementation that mirrors
`glk_request_line_event` but for Unicode (glui32) buffers:

```c
void glk_request_line_event_uni(winid_t win, glui32 *buf, glui32 maxlen,
    glui32 initlen)
{
    window_t *winptr = (window_t *)win;
    if (!winptr)
        return;
    winptr->linebuf = (void *)buf;
    winptr->linebuflen = maxlen * sizeof(glui32);
    winptr->line_request_uni = 1;
    (void)initlen;
    if (gli_register_arr) {
        winptr->inarrayrock = (*gli_register_arr)(buf, maxlen, "&+#!Iu");
    }
}
```

### Priority 2: Update glk_select for Unicode

Add a check for `line_request_uni` alongside `line_request`:

```c
if (gli_mainwin && gli_mainwin->line_request_uni) {
    // Same flow but read Unicode characters
}
```

### Priority 3: Fix dispatch prototype

Change the prototype for `glk_request_line_event` from `3Qa&+#!CnIu:` to
`4Qa&+#!CnIuIu:` to properly pass `initlen`.

### Priority 4: Consider glk_cancel_line_event on restore path

After `@restore`, the game's VM state is from the save point. Any pending
line request in NextGlk's window_t predates the restore. Consider adding
logic to clear pending input state when a restore is detected, or document
that the game is responsible for managing input state transitions.

---

## 6. Files Examined

| File | Lines | Contents |
|------|-------|----------|
| `nextglk/nextglk.c:1012-1031` | `glk_request_line_event` | Real implementation |
| `nextglk/nextglk.c:292-331` | `glk_select` | Only checks `line_request` |
| `nextglk/nextglk_stubs.c:214-223` | `glk_request_line_event_uni` | **Complete no-op stub** |
| `nextglk/nextglk_internal.h:73-91` | `window_t` struct | Has both `line_request` and `line_request_uni` |
| `nextglk/nextglk_input.c:1-26` | `nextglk_read_line` | Reads ASCII only |
| `nextglk/next_window.c:40-85` | `gli_new_window` | Initializes all input fields to 0 |
| `nextglk/gi_dispa.c:990-1022` | select/request_line dispatch | Prototype bug at arglist[4] |
| `nextglk/gi_dispa.c:814` | function table | Both 0x00D0 and 0x0141 registered |
| `thirdparty/git/terp.c:684-693` | `do_restore` | Calls `restoreFromFile` |
| `thirdparty/git/savefile.c:29-210` | `restoreFromFile` | Replaces VM memory entirely |
| `thirdparty/git/glkop.c:1403-1489` | retained array registry | Manages array lifecycle |
| `thirdparty/git/glkop.c:1196-1259` | `grab_temp_c_array` | Allocates temp buffers |

---

## 7. Conclusion

The most likely root cause is that the target game uses
`glk_request_line_event_uni` (dispatch ID 0x0141), which is a complete no-op
stub in NextGlk. This is consistent with:

1. Modern Inform 7 / Glulx 3.0+ games using Unicode APIs
2. The symptom of characters echoing but no game response
   (`glk_select` returns `evtype_None` because `line_request` is 0)
3. The fact that the game works before save/restore (if the game uses
   `glk_request_line_event` in some code paths and the Unicode variant in
   others)

The fix is to implement `glk_request_line_event_uni` and update `glk_select`
to handle `line_request_uni`.

A secondary concern is the dispatch prototype bug for `glk_request_line_event`
(0x00D0) which accesses `arglist[4]` out of bounds. This should be fixed
regardless.