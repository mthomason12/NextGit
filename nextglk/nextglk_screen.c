#include "nextglk.h"
#include <stdio.h>

/* -------------------------------------------------------------------------
 * Test counters — incremented by each output function so tests can verify
 * that window stream output is correctly routed to the platform layer.
 *
 * These are declared extern in nextglk.h and defined here.
 * ------------------------------------------------------------------------- */

int nextglk_put_char_count = 0;
int nextglk_put_string_count = 0;
int nextglk_put_buffer_count = 0;

/* -------------------------------------------------------------------------
 * nextglk_screen_clear — Clear the screen
 *
 * Currently a no-op stub. Will be implemented when the platform screen
 * layer is fully integrated.
 * ------------------------------------------------------------------------- */

void nextglk_screen_clear(void)
{
}

/* -------------------------------------------------------------------------
 * nextglk_screen_refresh — Flush pending output to the screen
 *
 * Currently a no-op stub. Will be implemented when the platform screen
 * layer is fully integrated.
 * ------------------------------------------------------------------------- */

void nextglk_screen_refresh(void)
{
}

/* -------------------------------------------------------------------------
 * nextglk_put_char — Write a single character to the platform screen
 *
 * On Linux, writes to stdout via putchar().
 * On the ZX Spectrum Next, this will write to the framebuffer.
 *
 * Increments the test counter for verification.
 * ------------------------------------------------------------------------- */

void nextglk_put_char(char ch)
{
    putchar(ch);
    nextglk_put_char_count++;
}

/* -------------------------------------------------------------------------
 * nextglk_put_string — Write a null-terminated string to the platform screen
 *
 * On Linux, writes to stdout via fputs().
 * On the ZX Spectrum Next, this will write to the framebuffer.
 *
 * Increments the test counter for verification.
 * ------------------------------------------------------------------------- */

void nextglk_put_string(const char* str)
{
    fputs(str, stdout);
    nextglk_put_string_count++;
}

/* -------------------------------------------------------------------------
 * nextglk_put_buffer — Write a buffer of known length to the platform screen
 *
 * On Linux, writes to stdout via fwrite().
 * On the ZX Spectrum Next, this will write to the framebuffer.
 *
 * Increments the test counter for verification.
 * ------------------------------------------------------------------------- */

void nextglk_put_buffer(const char* buffer, uint16_t length)
{
    fwrite(buffer, 1, length, stdout);
    nextglk_put_buffer_count++;
}