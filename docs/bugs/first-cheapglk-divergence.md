# First CheapGlk / NextGlk Behavioural Divergence

**Date**: 2026-07-04
**Scenario**: A (start, look, look, quit)

## 1. Behaviour in CheapGlk (Reference)

CheapGlk processes all piped commands successfully:

```
BENCH_TRACE: glk_window_open(split=(nil), method=0, size=0, wintype=3, rock=201)
BENCH_TRACE: glk_window_open -> 0x6160d6c02ba0
BENCH_TRACE: glk_window_open(split=0x6160d6c02ba0, method=18, size=1, wintype=4, rock=202)
BENCH_TRACE: glk_window_open -> (nil)
BENCH_TRACE: glk_set_window(win=0x6160d6c02ba0)
BENCH_TRACE: glk_request_line_event(win=0x6160d6c02ba0, maxlen=256)
BENCH_TRACE: glk_select() called
BENCH_TRACE: glk_select -> event.type=3 win=0x6160d6c02ba0 val1=4 val2=0    <- "look"
BENCH_TRACE: glk_request_line_event(win=0x6160d6c02ba0, maxlen=256)
BENCH_TRACE: glk_select() called
BENCH_TRACE: glk_select -> event.type=3 win=0x6160d6c02ba0 val1=4 val2=0    <- "look"
BENCH_TRACE: glk_request_line_event(win=0x6160d6c02ba0, maxlen=256)
BENCH_TRACE: glk_select() called
BENCH_TRACE: glk_select -> event.type=3 win=0x6160d6c02ba0 val1=4 val2=0    <- "quit"
BENCH_TRACE: glk_request_line_event(win=0x6160d6c02ba0, maxlen=256)
BENCH_TRACE: glk_select() called
... game exits cleanly. Total BENCH_TRACE lines: 16.
```

CheapGlk's `glk_select` gracefully handles EOF on stdin. The game
displays the quit confirmation prompt, and execution terminates.

## 2. Behaviour in NextGlk

NextGlk processes the first three commands identically to CheapGlk,
then enters an infinite loop on the fourth `glk_select` call (the
quit confirmation prompt) when stdin reaches EOF:

```
BENCH_TRACE: glk_window_open(split=(nil), method=0, size=0, wintype=3, rock=201)
BENCH_TRACE: glk_window_open -> 0x6070efb4c8b0
BENCH_TRACE: glk_window_open(split=0x6070efb4c8b0, method=18, size=1, wintype=4, rock=202)
BENCH_TRACE: glk_window_open -> (nil)
BENCH_TRACE: glk_set_window(win=0x6070efb4c8b0)
BENCH_TRACE: glk_request_line_event(win=0x6070efb4c8b0, maxlen=256)
BENCH_TRACE: glk_select() called
BENCH_TRACE: glk_select -> event.type=3 win=0x6070efb4c8b0 val1=4 val2=0    <- "look"
BENCH_TRACE: glk_request_line_event(win=0x6070efb4c8b0, maxlen=256)
BENCH_TRACE: glk_select() called
BENCH_TRACE: glk_select -> event.type=3 win=0x6070efb4c8b0 val1=4 val2=0    <- "look"
BENCH_TRACE: glk_request_line_event(win=0x6070efb4c8b0, maxlen=256)
BENCH_TRACE: glk_select() called
BENCH_TRACE: glk_select -> event.type=3 win=0x6070efb4c8b0 val1=0 val2=0    <- EOF on quit prompt
BENCH_TRACE: glk_request_line_event(win=0x6070efb4c8b0, maxlen=256)
BENCH_TRACE: glk_select() called
BENCH_TRACE: glk_select -> event.type=3 win=0x6070efb4c8b0 val1=0 val2=0    <- INFINITE LOOP START
... repeats forever. Total BENCH_TRACE lines: 7,130,141 (and counting).
```

## 3. First Point of Divergence

The first three `glk_select` calls produce **identical** results in both
implementations. The divergence occurs on the **fourth `glk_select` call**
— the quit confirmation prompt after "quit" has been entered.

| | CheapGlk | NextGlk |
|---|---|---|
| 1st select (look) | evtype_LineInput, val1=4 | evtype_LineInput, val1=4 |
| 2nd select (look) | evtype_LineInput, val1=4 | evtype_LineInput, val1=4 |
| 3rd select (quit) | evtype_LineInput, val1=4 | evtype_LineInput, val1=4 |
| 4th select (confirm) | *game exits cleanly* | evtype_LineInput, val1=0, infinite loop |

## 4. Exact Function Where Divergence Occurs

**`glk_select`** in `nextglk/nextglk.c` (line 273).

When `nextglk_read_line()` returns 0 (EOF on stdin), NextGlk still
populates and returns an `evtype_LineInput` event with `val1=0`.
CheapGlk does **not** return `evtype_LineInput` under the same
conditions.

## 5. Exact Inputs

All three commands are piped via stdin:
```
printf "look\nlook\nquit\n" | ./git[nc] output.ulx
```

The fourth `glk_select` occurs when the game re-prompts after "quit"
with the confirmation "Are you sure you want to quit?" — at this
point stdin is at EOF (no more piped input).

## 6. Exact Outputs

**CheapGlk `glk_select` at EOF**: Game terminates. No infinite loop.

**NextGlk `glk_select` at EOF**:
```
BENCH_TRACE: glk_select -> event.type=3 win=0x6070efb4c8b0 val1=0 val2=0
```
Repeated ~7 million times. Event type 3 = evtype_LineInput, val1=0
means a zero-length line was "read" from an exhausted stdin.

---

**Investigation complete. First divergence identified.**

NextGlk's `glk_select` returns `evtype_LineInput` with `val1=0` when
`nextglk_read_line` hits EOF. CheapGlk does not. The infinite loop
arises because the game receives an empty-line event and re-prompts,
NextGlk again returns the same empty-line event, and so on forever.