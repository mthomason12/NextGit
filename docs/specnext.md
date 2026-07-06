### Console Output

On the ZX Spectrum Next, using z88dk, there are standard C runtime functions available such as:

```c
printf(...)
puts(...)
putchar(...)
```

These are perfectly reasonable for an initial implementation.

Longer-term, output may eventually be redirected to:

- Tilemap layers
- Layer 2 graphics
- ULA screen memory
- A custom text renderer

However, that is an implementation detail. From NextGlk's point of view, it should still expose the same Glk behaviour regardless of how characters ultimately reach the screen.

---

### Keyboard Input

z88dk provides standard input functions such as:

```c
getchar()
fgets()
```

as well as lower-level keyboard libraries.

Internally these ultimately scan the Spectrum keyboard matrix, but most applications do not need to access hardware ports directly.

For a text-adventure interpreter, a model similar to:

```c
fgets(linebuf, len, stdin);
```

is likely sufficient initially.

---

### File I/O

The Spectrum Next typically uses NextZXOS and ESXDOS-compatible filesystem services.

Through z88dk these are generally exposed using familiar C runtime file APIs:

```c
FILE *
fopen()

fclose()

fread()

fwrite()

fseek()

ftell()
```

Therefore the current NextGlk file API:

```c
nextglk_file_open_read()
nextglk_file_open_write()
nextglk_file_read()
nextglk_file_write()
nextglk_file_set_position()
nextglk_file_get_position()
```

can likely remain unchanged at the interface level.

The implementation underneath can later be swapped from Linux stdio to the Spectrum Next runtime.

---

### Existing Platform Layer

There is currently no dedicated Spectrum Next platform layer implemented.

Current architecture is effectively:

```text
NextGlk
    ->
stdio
    ->
Linux
```

Future architectures may become:

```text
NextGlk
    ->
stdio-compatible runtime
    ->
NextZXOS
```

or:

```text
NextGlk
    ->
direct NextZXOS APIs
```

depending on what proves most practical.

At present:

- There is no existing NextZXOS wrapper layer.
- There is no existing platform abstraction library.
- The Linux implementation is the current development implementation.

---

### Guidance for Current Development

For now:

```text
Use:

    fopen
    fread
    fwrite
    fseek
    ftell
    printf
    getchar
    fgets

through the existing NextGlk interfaces.
```

Do not create a new platform abstraction layer unless explicitly requested.

Do not introduce Spectrum-specific behaviour.

The immediate goal remains:

```text
CheapGlk behaviour
        ==
NextGlk behaviour
```

on Linux.

Once behavioural compatibility is achieved, the underlying implementation can be replaced with Spectrum Next specific code while preserving identical observable behaviour.