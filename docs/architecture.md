# NextGit Architecture

## Overview

NextGit is a Glulx interpreter for the ZX Spectrum Next.

The project is based on the Git Glulx interpreter and provides a custom Glk implementation called NextGlk. The goal is to run modern Inform 7 Glulx games on real Spectrum Next hardware with support for inline images, SD card storage, save/restore functionality, and optional future acceleration using the Raspberry Pi co-processor.

The project prioritises:

- Correctness
- Simplicity
- Maintainability
- Low memory usage
- Spectrum Next compatibility

The project does not attempt to implement the complete Glk specification.

---

## Design Goals

### Required

- Run Inform 7 Glulx games
- Support save and restore
- Support inline images
- Run entirely on the Spectrum Next
- Use SD card storage
- Compile with z88dk

### Optional Future Features

- Raspberry Pi image decoding
- Audio support
- Additional Glk features

### Explicitly Out Of Scope

- Hyperlinks
- Timers
- Mouse support
- Networking
- Multiple graphics windows
- Full Glk compliance
- Running without a Glulx VM

---

## High Level Architecture

```text
+----------------+
|    Game        |
| (.ulx/.gblorb) |
+--------+-------+
         |
         v
+----------------+
|      Git       |
|  Glulx VM      |
+--------+-------+
         |
         v
+----------------+
|    NextGlk     |
+--------+-------+
         |
         +----------------+
         |                |
         v                v
+----------------+ +----------------+
|  Text/Input    | |     Images     |
+----------------+ +----------------+
         |                |
         +-------+--------+
                 |
                 v
+----------------+
| NextZXOS / SD  |
+----------------+
```

---

## Layer Responsibilities

### Git VM

Responsible for:

- Glulx opcode execution
- Glulx memory access
- Stack management
- Save state generation
- Resource requests

Not responsible for:

- Graphics
- Input devices
- File system details
- Memory banking implementation

Git communicates only through the Glk interface.

---

### NextGlk

Responsible for:

- Text output
- Keyboard input
- Save/restore UI
- File access wrappers
- Image display
- Event loop

Not responsible for:

- Glulx execution
- Game state
- Memory banking

NextGlk is intended to be the only platform-specific layer used by Git.

---

### Platform Layer

Responsible for:

- NextZXOS integration
- File operations
- Layer 2 access
- Keyboard access
- Future Raspberry Pi communication

This layer hides Spectrum Next hardware details from NextGlk.

---

## Memory Model

### Principle

Memory banking must not be implemented in NextGlk.

The VM owns VM memory.

NextGlk owns UI resources.

---

### VM Memory

Managed by a dedicated memory manager.

Responsibilities:

- Story memory
- Glulx heap
- Stack
- Save state memory
- Banking implementation

Example API:

```c
uint8_t vm_read8(uint32_t address);
uint16_t vm_read16(uint32_t address);

void vm_write8(uint32_t address, uint8_t value);
void vm_write16(uint32_t address, uint16_t value);
```

---

### UI Memory

Managed by NextGlk.

Examples:

- Text buffers
- Image cache
- Temporary file buffers

Bank switching should be hidden from NextGlk whenever possible.

---

## Graphical Model

### Layout

```text
+------------------------------------------+
|                                          |
|              IMAGE AREA                  |
|                                          |
+------------------------------------------+
| You are standing in a forest.            |
| A path leads north.                      |
|                                          |
| >                                        |
+------------------------------------------+
```

---

### Image Behaviour

Only one active image is displayed.

When a new image is requested:

1. Previous image is discarded
2. New image is loaded
3. Image is displayed
4. Text continues below image area

No floating windows.

No overlapping images.

No image scaling in the initial release.

---

## Image Loading Flow

```text
Game
 |
 | glk_image_draw()
 v
NextGlk
 |
 | Locate resource
 v
Blorb Loader
 |
 | Load from SD
 v
Image Decoder
 |
 | Produce bitmap
 v
Layer 2 Renderer
```

Future Pi acceleration may replace the image decoder step.

---

## Save / Restore Flow

```text
Game
 |
 | save
 v
Git VM
 |
 v
NextGlk
 |
 v
NextZXOS File API
 |
 v
SD Card
```

Restore follows the reverse path.

---

## Event Loop

```text
Git VM
 |
 | requests input
 v
glk_select()
 |
 v
NextGlk
 |
 v
Keyboard
 |
 v
Event returned to VM
```

Only keyboard events are required for the initial implementation.

---

## Required Glk Features

Initial target:

```text
glk_put_char
glk_put_string
glk_put_buffer

glk_request_line_event
glk_request_char_event

glk_select

glk_stream_*
glk_fileref_*

glk_image_draw
glk_image_get_info
```

---

## Unsupported Features

Initial implementation will return failure or stub behaviour for:

```text
glk_image_draw_scaled
sound channels
hyperlinks
timers
mouse events
graphics windows
text grid windows
style hints
```

---

## Optional Raspberry Pi Acceleration

The interpreter must run without a Raspberry Pi.

If present, the Pi may provide:

- PNG decoding
- JPEG decoding
- Image scaling
- Audio playback

The VM must remain unaware of Pi acceleration.

```text
Git
 |
NextGlk
 |
show_image()
 |
 +-- Z80 Decoder
 |
 +-- Pi Decoder
```

Both paths must produce identical results.

---

## Milestones

### M1

- Build Git
- Build CheapGlk
- Run test game

Status: Complete

### M2

- Identify actual Glk usage
- Document required APIs

### M3

- Text-only NextGlk
- Run game on Spectrum Next

### M4

- Image support
- Blorb resource loading

### M5

- Memory manager
- Large game support

### M6

- Optional Raspberry Pi acceleration