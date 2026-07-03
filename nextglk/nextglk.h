#ifndef NEXTGLK_H
#define NEXTGLK_H

#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------------- */

bool nextglk_init(void);
void nextglk_shutdown(void);

/* -------------------------------------------------------------------------
 * Screen
 * ------------------------------------------------------------------------- */

void nextglk_screen_clear(void);
void nextglk_screen_refresh(void);

void nextglk_put_char(char ch);
void nextglk_put_string(const char* str);
void nextglk_put_buffer(const char* buffer, uint16_t length);

/* Test counters — exposed so test code can verify platform-layer routing */
extern int nextglk_put_char_count;
extern int nextglk_put_string_count;
extern int nextglk_put_buffer_count;

/* -------------------------------------------------------------------------
 * Input
 * ------------------------------------------------------------------------- */

char nextglk_wait_char(void);

uint16_t nextglk_read_line(
    char* buffer,
    uint16_t buffer_size);

/* -------------------------------------------------------------------------
 * File Access
 * ------------------------------------------------------------------------- */

typedef struct NextGlkFile NextGlkFile;

NextGlkFile* nextglk_file_open_read(
    const char* path);

NextGlkFile* nextglk_file_open_write(
    const char* path);

uint32_t nextglk_file_read(
    NextGlkFile* file,
    void* buffer,
    uint32_t length);

uint32_t nextglk_file_write(
    NextGlkFile* file,
    const void* buffer,
    uint32_t length);

void nextglk_file_close(
    NextGlkFile* file);

/* -------------------------------------------------------------------------
 * Images
 * ------------------------------------------------------------------------- */

bool nextglk_image_init(void);

bool nextglk_image_show(
    uint32_t image_id);

bool nextglk_image_get_info(
    uint32_t image_id,
    uint16_t* width,
    uint16_t* height);

void nextglk_image_clear(void);

/* -------------------------------------------------------------------------
 * Platform
 * ------------------------------------------------------------------------- */

uint32_t nextglk_ticks(void);

#endif