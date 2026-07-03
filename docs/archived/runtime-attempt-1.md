# Runtime Attempt 1 — Analysis

## Purpose

Analyse the exact startup sequence, expected Glk call chain, and first failure
point when running a real Glulx game file through the Git VM linked against
NextGlk. No Git source code is modified. No NextGlk implementation code is
written. This document records what will happen and where it will break.

---

## 1. Exact Startup Sequence

### 1.1 Entry point: `main()`

**File:** `nextglk/main.c` (lines 14–31)

```
main(argc, argv)
  → glkunix_startup_t startdata = { argc, argv }
  → glkunix_startup_code(&startdata)
  → glk_main()
  → glk_exit()
```

### 1.2 `glkunix_startup_code()`

**Two definitions exist — linker selects one:**

| Source | File | Behaviour |
|--------|------|-----------|
| **Git VM** | `thirdparty/git/git_unix.c` (lines 48–57 or 96–105) | Checks `data->argc <= 1`, prints usage, returns 0 |
| **NextGlk** | `nextglk/glkstart.c` (lines 21–30) | Calls `nextglk_init()`, returns TRUE |

**Also two definitions of `glkunix_arguments[]`:**

| Source | File | Contents |
|--------|------|----------|
| **Git VM** | `thirdparty/git/git_unix.c` (lines 19–23) | `{ "", glkunix_arg_ValueFollows, "filename:..." }, { NULL, glkunix_arg_End, NULL }` |
| **NextGlk** | `nextglk/glkstart.c` (lines 17–19) | `{ NULL, glkunix_arg_End, NULL }` (empty) |

**Linker resolution:** Object files (`git_unix.o`) take precedence over library
archives (`libnextglk.a`). Therefore `git_unix.c`'s definitions are used.

**Result:** `glkunix_startup_code()` checks `argc <= 1`. If no game file is
provided on the command line, it prints:

```
usage: git gamefile.ulx
```

and returns 0, causing `main()` to call `glk_exit()` immediately.

**If a game file IS provided** (argc >= 2):

- **MMAP path** (`#ifdef USE_MMAP`, lines 48–89): Stores `argv[1]` in
  `gFilename`, returns 1. `glk_main()` opens the file with `open()`, mmaps it,
  calls `git(ptr, size, CACHE_SIZE, UNDO_SIZE)`.
- **Non-MMAP path** (lines 96–113): Calls
  `glkunix_stream_open_pathname(argv[1], 0, 0)` which returns NULL (stub in
  `glkstart.c` line 50–58). `gStream` is NULL. `glk_main()` calls
  `fatalError("could not open game file")`.

### 1.3 `glk_main()` (Git VM)

**File:** `thirdparty/git/git_unix.c`

**MMAP path (lines 59–89):**
```
glk_main()
  → open(gFilename, O_RDONLY)
  → fstat(file, &info)
  → mmap(NULL, info.st_size, PROT_READ, MAP_PRIVATE, file, 0)
  → git(ptr, info.st_size, CACHE_SIZE, UNDO_SIZE)
  → munmap(ptr, info.st_size)
```

**Non-MMAP path (lines 107–113):**
```
glk_main()
  → if (gStream == NULL) fatalError("could not open game file")
  → gitWithStream(gStream, CACHE_SIZE, UNDO_SIZE)
```

### 1.4 `git()` / `gitWithStream()` (Git VM)

**File:** `thirdparty/git/git.c`

**`git()` (MMAP path, lines 153–173):**
```
git(game, gameSize, cacheSize, undoSize)
  → if (read32(game) == FORM) {  // Blorb detection
      stream = glk_stream_open_memory(game, gameSize, filemode_Read, 0)
      → returns NULL (stub in nextglk.c line 477–485)
      → fatalError("Can't open the Blorb file as a Glk memory stream.")
    }
  → gitMain(game, gameSize, cacheSize, undoSize)
```

**`gitWithStream()` (non-MMAP path, lines 104–151):**
```
gitWithStream(str, cacheSize, undoSize)
  → glk_stream_set_position(str, 0, seekmode_Start)  // stub, no-op
  → glk_get_buffer_stream(str, buffer, 4)             // stub, returns 0
  → fatalError("can't read from game file stream")
```

### 1.5 `gitMain()` (Git VM)

**File:** `thirdparty/git/git.c` (lines 10–64)

```
gitMain(game, gameSize, cacheSize, undoSize)
  → init_accel()
  → git_init_dispatch()
      → gidispatch_count_classes()       // returns 4
      → gidispatch_set_object_registry() // stores callbacks in nextglk.c
      → gidispatch_set_retained_registry() // stores callback (stub)
  → gPeephole = 1
  → gDebug = 0
  → initMemory(game, gameSize)           // loads game into VM memory
  → initUndo(undoSize)                   // initialises undo records
  → version = memRead32(4)               // reads Glulx version from game header
  → if version matches: set gIoMode
  → startProgram(cacheSize)              // begins VM execution
  → shutdownUndo()
  → shutdownMemory()
```

### 1.6 `startProgram()` (Git VM)

**File:** `thirdparty/git/terp.c` (lines 205–1755)

```
startProgram(cacheSize)
  → testDouble()
  → initCompiler(cacheSize)
  → git_seed_random(0)
  → malloc(stack)
  → L1 = startPos  (from memRead32(24) — initial function address)
  → L2 = 0         (no arguments)
  → goto do_enter_function_L1
```

The VM begins executing the game's initial function. This function will make
Glk calls via the `do_glk` opcode handler (line 1339–1348):

```
do_glk:
  → pop args from stack into args[]
  → gStackPointer = sp
  → S1 = git_perform_glk(L1, L2, (glui32*) args)
  → sp = gStackPointer
  → NEXT
```

---

## 2. Exact Call Chain to Load and Execute a .ulx File

### MMAP path (recommended, requires `-DUSE_MMAP`):

```
1. main(argc, argv)
2.   glkunix_startup_code(&startdata)
3.     git_unix.c: stores argv[1] in gFilename, returns 1
4.   glk_main()
5.     git_unix.c: open(gFilename, O_RDONLY)
6.     git_unix.c: fstat(file, &info)
7.     git_unix.c: mmap(NULL, info.st_size, PROT_READ, MAP_PRIVATE, file, 0)
8.     git(ptr, info.st_size, CACHE_SIZE, UNDO_SIZE)
9.       git.c: read32(game) — check for Blorb magic (FORM)
10.      if Blorb: glk_stream_open_memory() — STUB, returns NULL → FATAL
11.      gitMain(game, gameSize, cacheSize, undoSize)
12.        init_accel()
13.        git_init_dispatch()
14.          gidispatch_count_classes() → 4
15.          gidispatch_set_object_registry() → stores callbacks
16.          gidispatch_set_retained_registry() → stores callback (stub)
17.        initMemory(game, gameSize)
18.        initUndo(undoSize)
19.        memRead32(4) → Glulx version
20.        startProgram(cacheSize)
21.          testDouble()
22.          initCompiler(cacheSize)
23.          git_seed_random(0)
24.          malloc(stack)
25.          L1 = memRead32(24)  // start function address
26.          goto do_enter_function_L1
27.            → VM begins executing game bytecode
28.            → First Glk call via do_glk opcode
```

### Critical Blorb issue (step 10):

If the game file is a Blorb file (starts with `FORM`), `git()` calls
`glk_stream_open_memory()` which is a stub returning NULL. This causes a
**fatal error** before the game even starts.

**Workaround:** Use a raw `.ulx` file (not Blorb-wrapped). Raw `.ulx` files
do not start with `FORM`, so the Blorb branch is skipped and `gitMain()` is
called directly.

---

## 3. First Glk Functions Expected to Be Invoked by a Real Game

Based on analysis of Inform 7-generated Glulx games and the standard Glulx
startup sequence, the first Glk calls are:

### Phase A — Gestalt queries (before any window is opened)

The game queries the Glk library's capabilities to decide how to render output:

| Order | Function | Purpose |
|-------|----------|---------|
| 1 | `glk_gestalt(gestalt_Version, 0)` | Get Glk spec version |
| 2 | `glk_gestalt(gestalt_CharOutput, 0)` | Get character output mode |
| 3 | `glk_gestalt(gestalt_Unicode, 0)` | Check Unicode support |
| 4 | `glk_gestalt(gestalt_LineInput, 0)` | Check line input support |
| 5 | `glk_gestalt(gestalt_CharInput, 0)` | Check character input support |
| 6 | `glk_gestalt(gestalt_DrawImage, 0)` | Check image support |
| 7 | `glk_gestalt(gestalt_MouseInput, 0)` | Check mouse support |
| 8 | `glk_gestalt(gestalt_Timer, 0)` | Check timer support |
| 9 | `glk_gestalt(gestalt_Sound, 0)` | Check sound support |

### Phase B — Window creation

After gestalt queries, the game creates the main window:

| Order | Function | Purpose |
|-------|----------|---------|
| 10 | `glk_stylehint_set(wintype_TextBuffer, ...)` | Set style hints for text buffer |
| 11 | `glk_window_open(NULL, method, size, wintype_TextBuffer, rock)` | Create main window |
| 12 | `glk_window_get_stream(win)` | Get window's output stream |
| 13 | `glk_set_window(win)` | Set as current output window |
| 14 | `glk_window_clear(win)` | Clear the window |

### Phase C — Initial text output

The game prints its banner/introduction:

| Order | Function | Purpose |
|-------|----------|---------|
| 15 | `glk_put_char(ch)` | Print individual characters |
| 16 | `glk_put_string(s)` | Print strings |
| 17 | `glk_put_buffer(buf, len)` | Print buffers |

### Phase D — Input and event loop

The game enters its main loop, waiting for player input:

| Order | Function | Purpose |
|-------|----------|---------|
| 18 | `glk_select(event)` | Wait for an event |
| 19 | `glk_request_line_event(win, buf, maxlen, initlen)` | Request line input |
| 20 | `glk_request_char_event(win)` | Request character input |

---

## 4. Implementation Status of First 20 Expected Calls

### Phase A — Gestalt queries (all implemented)

| # | Function | Status | File | Line |
|---|----------|--------|------|------|
| 1 | `glk_gestalt(gestalt_Version, 0)` | **Implemented** | `nextglk.c` | 201–231 |
| 2 | `glk_gestalt(gestalt_CharOutput, 0)` | **Implemented** | `nextglk.c` | 201–231 |
| 3 | `glk_gestalt(gestalt_Unicode, 0)` | **Implemented** | `nextglk.c` | 201–231 |
| 4 | `glk_gestalt(gestalt_LineInput, 0)` | **Implemented** (returns 0) | `nextglk.c` | 201–231 |
| 5 | `glk_gestalt(gestalt_CharInput, 0)` | **Implemented** (returns 0) | `nextglk.c` | 201–231 |
| 6 | `glk_gestalt(gestalt_DrawImage, 0)` | **Implemented** (returns 0) | `nextglk.c` | 201–231 |
| 7 | `glk_gestalt(gestalt_MouseInput, 0)` | **Implemented** (returns 0) | `nextglk.c` | 201–231 |
| 8 | `glk_gestalt(gestalt_Timer, 0)` | **Implemented** (returns 0) | `nextglk.c` | 201–231 |
| 9 | `glk_gestalt(gestalt_Sound, 0)` | **Implemented** (returns 0) | `nextglk.c` | 201–231 |

### Phase B — Window creation (mostly implemented)

| # | Function | Status | File | Line |
|---|----------|--------|------|------|
| 10 | `glk_stylehint_set(...)` | **Stub** (no-op) | `nextglk.c` | 385–392 |
| 11 | `glk_window_open(...)` | **Implemented** | `next_window.c` | 154–171 |
| 12 | `glk_window_get_stream(win)` | **Implemented** | `next_window.c` | 240–248 |
| 13 | `glk_set_window(win)` | **Implemented** | `next_window.c` | 320–326 |
| 14 | `glk_window_clear(win)` | **Implemented** | `next_window.c` | 306–310 |

### Phase C — Text output (all implemented)

| # | Function | Status | File | Line |
|---|----------|--------|------|------|
| 15 | `glk_put_char(ch)` | **Implemented** | `next_stream.c` | 368–373 |
| 16 | `glk_put_string(s)` | **Implemented** | `next_stream.c` | 387–392 |
| 17 | `glk_put_buffer(buf, len)` | **Implemented** | `next_stream.c` | 407–412 |

### Phase D — Input and event loop (all stubs)

| # | Function | Status | File | Line |
|---|----------|--------|------|------|
| 18 | `glk_select(event)` | **Stub** (returns evtype_None) | `nextglk.c` | 275–283 |
| 19 | `glk_request_line_event(...)` | **Stub** (no-op) | `nextglk.c` | 698–706 |
| 20 | `glk_request_char_event(win)` | **Stub** (no-op) | `nextglk.c` | 734–738 |

---

## 5. Ranked Table of First 20 Runtime Glk Calls

| Rank | Function | Source Location | Status | Risk | Boot Blocker? | Why |
|------|----------|-----------------|--------|------|---------------|-----|
| 1 | `glk_gestalt(gestalt_Version, 0)` | `nextglk.c:201` | Implemented | None | No | Returns 0x00070600 |
| 2 | `glk_gestalt(gestalt_CharOutput, 0)` | `nextglk.c:201` | Implemented | None | No | Returns ExactPrint (2) |
| 3 | `glk_gestalt(gestalt_Unicode, 0)` | `nextglk.c:201` | Implemented | None | No | Returns 1 |
| 4 | `glk_gestalt(gestalt_LineInput, 0)` | `nextglk.c:201` | Implemented | Low | No | Returns 0 (unsupported) |
| 5 | `glk_gestalt(gestalt_CharInput, 0)` | `nextglk.c:201` | Implemented | Low | No | Returns 0 (unsupported) |
| 6 | `glk_gestalt(gestalt_DrawImage, 0)` | `nextglk.c:201` | Implemented | Low | No | Returns 0 (unsupported) |
| 7 | `glk_gestalt(gestalt_MouseInput, 0)` | `nextglk.c:201` | Implemented | None | No | Returns 0 (unsupported) |
| 8 | `glk_gestalt(gestalt_Timer, 0)` | `nextglk.c:201` | Implemented | None | No | Returns 0 (unsupported) |
| 9 | `glk_gestalt(gestalt_Sound, 0)` | `nextglk.c:201` | Implemented | None | No | Returns 0 (unsupported) |
| 10 | `glk_stylehint_set(...)` | `nextglk.c:385` | Stub | Low | No | No-op is safe |
| 11 | `glk_window_open(NULL, method, size, wintype_TextBuffer, rock)` | `next_window.c:154` | Implemented | **Medium** | **Yes** | Returns NULL if `gli_mainwin` already set; only one window allowed |
| 12 | `glk_window_get_stream(win)` | `next_window.c:240` | Implemented | None | No | Returns `win->str` |
| 13 | `glk_set_window(win)` | `next_window.c:320` | Implemented | None | No | Sets `gli_currentstr` |
| 14 | `glk_window_clear(win)` | `next_window.c:306` | Implemented | None | No | Calls `nextglk_screen_clear()` (no-op) |
| 15 | `glk_put_char(ch)` | `next_stream.c:368` | Implemented | None | No | Routes to `nextglk_put_char()` → stdout |
| 16 | `glk_put_string(s)` | `next_stream.c:387` | Implemented | None | No | Routes to `nextglk_put_char()` per char |
| 17 | `glk_put_buffer(buf, len)` | `next_stream.c:407` | Implemented | None | No | Routes to `nextglk_put_char()` per byte |
| 18 | `glk_select(event)` | `nextglk.c:275` | Stub | **High** | **Yes** | Returns `evtype_None` immediately — game expects real events |
| 19 | `glk_request_line_event(win, buf, maxlen, initlen)` | `nextglk.c:698` | Stub | **High** | **Yes** | No-op — game expects input buffer to be filled |
| 20 | `glk_request_char_event(win)` | `nextglk.c:734` | Stub | **High** | **Yes** | No-op — game expects character input |

### Risk level rationale

| Risk | Meaning |
|------|---------|
| **None** | Function is fully implemented and returns correct values |
| **Low** | Function is implemented or stubbed; incorrect return may cause degraded behaviour but not crash |
| **Medium** | Function is implemented but has constraints (single-window) that may cause issues with some games |
| **High** | Function is stubbed and the game will hang or crash without a real implementation |

---

## 6. Earliest Point Where a Real Game Is Expected to Fail

### Failure Point 1 — Startup (before any Glk call)

**If no game file is provided:**
```
$ ./git
usage: git gamefile.ulx
```
The program exits immediately. This is expected behaviour.

**If a game file IS provided but the build does NOT define USE_MMAP:**
```
$ ./git game.ulx
*** fatal error: could not open game file ***
```
Because `glkunix_stream_open_pathname()` returns NULL (stub).

**If a game file IS provided and USE_MMAP IS defined, but the file is a Blorb:**
```
$ ./git game.blorb
*** fatal error: Can't open the Blorb file as a Glk memory stream. ***
```
Because `glk_stream_open_memory()` returns NULL (stub).

### Failure Point 2 — First Glk call (if startup succeeds)

**If a raw .ulx file is used with USE_MMAP:**
The VM boots, enters `startProgram()`, and begins executing game bytecode.
The first Glk call (`glk_gestalt`) succeeds. All gestalt queries succeed.
Window creation succeeds. Text output works.

**The first REAL failure is at the event loop:**

After printing the banner, the game calls `glk_select()` to wait for input.
`glk_select()` is a stub that returns `evtype_None` immediately. The game
will either:

1. **Spin in an infinite loop** — calling `glk_select()` repeatedly, getting
   `evtype_None` each time, with no way to make progress.
2. **Crash** — if the game doesn't handle `evtype_None` gracefully.
3. **Exit** — if the game interprets `evtype_None` as a signal to quit.

### Earliest definitive failure

**The earliest point where a real game is guaranteed to fail is the first
`glk_select()` call.** This occurs after:
- All gestalt queries (9 calls)
- Window creation (1 call)
- Stream retrieval (1 call)
- Window selection (1 call)
- Window clear (1 call)
- Banner text output (multiple put_char/put_string calls)

The game will have printed its banner to stdout successfully, then hang or
crash when it tries to wait for input.

---

## 7. Minimum Code Change Required to Get Past That Failure

### Change 1 — Fix the startup sequence

**Problem:** `glkstart.c` defines `glkunix_arguments[]` and
`glkunix_startup_code()` which conflict with `git_unix.c`'s definitions.

**Fix:** Remove `glkunix_arguments[]` and `glkunix_startup_code()` from
`nextglk/glkstart.c`. Let `git_unix.c` provide these symbols. The
`glkstart.c` file should only provide `glkunix_stream_open_pathname*()`
and `glkunix_set_base_file()` stubs.

**Alternatively:** Keep `glkunix_startup_code()` in `glkstart.c` but have it
call `git_unix.c`'s version, or vice versa. The simplest approach is to
remove the conflicting definitions from `glkstart.c`.

### Change 2 — Enable MMAP path

**Problem:** The non-MMAP path requires `glkunix_stream_open_pathname()` to
return a valid stream, which is a stub.

**Fix:** Build with `-DUSE_MMAP` in `OPTIONS`. This causes `git_unix.c` to
use the MMAP code path which opens the file directly with `open()`/`mmap()`
and bypasses `glkunix_stream_open_pathname()` entirely.

### Change 3 — Handle raw .ulx files (not Blorb)

**Problem:** `git()` checks for Blorb magic and calls
`glk_stream_open_memory()` which returns NULL.

**Fix:** Use a raw `.ulx` game file (not Blorb-wrapped). Raw `.ulx` files
skip the Blorb branch and call `gitMain()` directly.

### Change 4 — Implement `glk_select()` minimally

**Problem:** `glk_select()` returns `evtype_None` immediately, causing the
game to spin or hang.

**Fix:** The minimum viable implementation of `glk_select()` needs to:
1. Call `glk_request_line_event()` on the main window (simulating that
   input was requested).
2. Read a line from stdin.
3. Return an `evtype_LineInput` event with the input.

This is the **smallest possible change** that would allow a game to make
progress past the event loop. A full implementation would also need
`glk_request_line_event()` to actually store the buffer pointer and
`glk_cancel_line_event()` to work correctly.

### Summary of minimum changes

| # | Change | File | Lines affected |
|---|--------|------|----------------|
| 1 | Remove `glkunix_arguments[]` and `glkunix_startup_code()` from glkstart.c | `nextglk/glkstart.c` | 17–30 |
| 2 | Add `-DUSE_MMAP` to Git Makefile OPTIONS | `thirdparty/git/Makefile` | Line 21 |
| 3 | Use raw `.ulx` file (not Blorb) | N/A (test setup) | N/A |
| 4 | Implement `glk_select()` to read from stdin and return LineInput | `nextglk/nextglk.c` | 275–283 |

---

## 8. Recommended First Real Runtime Test

### Required command line

```bash
cd thirdparty/git
make clean && make GLK=nextglk OPTIONS="-DUSE_MMAP"
./git /path/to/test-game.ulx
```

### Required game file

A minimal Glulx test game that:
- Is a raw `.ulx` file (not Blorb-wrapped)
- Prints a short banner message
- Requests a single line of input
- Prints the input back
- Exits

**Recommended:** Use a hand-crafted minimal `.ulx` binary (or extract the
Glulx chunk from a known-working Blorb file). The game should be no more
than a few hundred bytes.

### Expected output

```
<game banner text>
>
```

Where `<game banner text>` is the game's introductory text, and `>` is the
command prompt. The game should then wait for input.

### Success criteria

1. Git binary links without errors.
2. `./git test.ulx` starts without crashing.
3. Game banner text is printed to stdout.
4. Game reaches the input prompt and waits for input.
5. After typing a line and pressing Enter, the game echoes the input and
   exits cleanly.

### Failure criteria

1. **Link error:** Duplicate symbol `glkunix_arguments[]` or
   `glkunix_startup_code()` — fix by removing from `glkstart.c`.
2. **"usage: git gamefile.ulx"** — `glkunix_startup_code()` from
   `git_unix.c` is being used but `glkunix_arguments[]` from `glkstart.c`
   is empty — fix by removing from `glkstart.c`.
3. **"could not open game file"** — non-MMAP path used — fix by adding
   `-DUSE_MMAP`.
4. **"Can't open the Blorb file as a Glk memory stream"** — Blorb file
   used — fix by using raw `.ulx`.
5. **No output / immediate exit** — `glk_select()` returns `evtype_None`
   and game exits — expected with current stubs.
6. **Infinite loop (no output after banner)** — `glk_select()` returns
   `evtype_None` and game spins — expected with current stubs.

### Current expected outcome (without any code changes)

```
$ ./git test.ulx
usage: git gamefile.ulx
```

Because `glkstart.c`'s empty `glkunix_arguments[]` is used (linker picks
object file over library, but the empty array from glkstart.c may be picked
depending on link order), or the startup code from `git_unix.c` sees
`argc <= 1` because the argument wasn't parsed.

**With `-DUSE_MMAP` and glkstart.c fixes:**

```
$ ./git test.ulx
<game banner text>
<program hangs or exits immediately>
```

The banner text will be printed successfully. The program will then either
hang (spinning on `glk_select()` returning `evtype_None`) or exit
(depending on how the game handles `evtype_None`).

---

## Appendix A: Dispatch Function IDs for the First 20 Calls

| Rank | Function | Dispatch ID | Prototype |
|------|----------|-------------|-----------|
| 1 | `glk_gestalt` | 0x0004 | `2IuIu:Iu` |
| 2 | `glk_gestalt` | 0x0004 | `2IuIu:Iu` |
| 3 | `glk_gestalt` | 0x0004 | `2IuIu:Iu` |
| 4 | `glk_gestalt` | 0x0004 | `2IuIu:Iu` |
| 5 | `glk_gestalt` | 0x0004 | `2IuIu:Iu` |
| 6 | `glk_gestalt` | 0x0004 | `2IuIu:Iu` |
| 7 | `glk_gestalt` | 0x0004 | `2IuIu:Iu` |
| 8 | `glk_gestalt` | 0x0004 | `2IuIu:Iu` |
| 9 | `glk_gestalt` | 0x0004 | `2IuIu:Iu` |
| 10 | `glk_stylehint_set` | 0x00B0 | `4IuIuIuIs` |
| 11 | `glk_window_open` | 0x0023 | `5IuIuIuIuIu:Qa` |
| 12 | `glk_window_get_stream` | 0x002C | `1Qa:Qb` |
| 13 | `glk_set_window` | 0x002F | `1Qa` |
| 14 | `glk_window_clear` | 0x002A | `1Qa` |
| 15 | `glk_put_char` | 0x0080 | `1Cu` |
| 16 | `glk_put_string` | 0x0082 | `1S` |
| 17 | `glk_put_buffer` | 0x0084 | `2#+!CnIu` |
| 18 | `glk_select` | 0x00C0 | `1&+!Iu:[]` |
| 19 | `glk_request_line_event` | 0x00D0 | `3Qa+!CnIuIu` |
| 20 | `glk_request_char_event` | 0x00D2 | `1Qa` |

## Appendix B: Key Source Locations

| Component | File | Key Lines |
|-----------|------|-----------|
| Entry point | `nextglk/main.c` | 14–31 |
| Git startup (MMAP) | `thirdparty/git/git_unix.c` | 48–89 |
| Git startup (non-MMAP) | `thirdparty/git/git_unix.c` | 96–113 |
| Git main | `thirdparty/git/git.c` | 10–64, 104–151, 153–173 |
| Glk dispatch init | `thirdparty/git/glkop.c` | 268–305 |
| Glk dispatch call | `thirdparty/git/glkop.c` | 311–425 |
| VM interpreter | `thirdparty/git/terp.c` | 205–1755 |
| Glk opcode handler | `thirdparty/git/terp.c` | 1339–1348 |
| NextGlk init | `nextglk/glkstart.c` | 21–30 |
| NextGlk lifecycle | `nextglk/nextglk.c` | 148–180 |
| Gestalt queries | `nextglk/nextglk.c` | 201–231 |
| Event loop (stub) | `nextglk/nextglk.c` | 275–303 |
| Window creation | `nextglk/next_window.c` | 40–85, 154–171 |
| Stream output | `nextglk/next_stream.c` | 236–327 |
| Screen output | `nextglk/nextglk_screen.c` | 46–50 |
| Dispatch table | `nextglk/gi_dispa.c` | 186–337 |
| Dispatch call router | `nextglk/gi_dispa.c` | 695–1568 |
| Git gestalt | `thirdparty/git/gestalt.c` | 3–57 |
| Git Makefile | `thirdparty/git/Makefile` | 1–99 |
| NextGlk Makefile | `nextglk/Makefile` | 1–52 |