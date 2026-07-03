/*
 * nextglk_stubs.c — Stub implementations for optional Glk modules.
 *
 * This file provides compileable stub implementations for all
 * optional Glk API functions that NextGlk does not implement yet.
 *
 * Module coverage:
 *   - Unicode (16 functions)        #ifdef GLK_MODULE_UNICODE
 *   - UnicodeNorm (2 functions)     #ifdef GLK_MODULE_UNICODE_NORM
 *   - Sound (9 functions)           #ifdef GLK_MODULE_SOUND
 *   - Sound2 (5 functions)          #ifdef GLK_MODULE_SOUND2
 *   - Hyperlinks (4 functions)      #ifdef GLK_MODULE_HYPERLINKS
 *   - DateTime (10 functions)       #ifdef GLK_MODULE_DATETIME
 *   - Resource Stream (2 functions) #ifdef GLK_MODULE_RESOURCE_STREAM
 *
 * Plus:
 *   - gidispatch_set_retained_registry()  (always needed)
 *
 * See docs/integration-attempt-1.md §3e–§3j, §5 for details.
 */

#include <stddef.h>
#include "glk.h"
#include "gi_dispa.h"
#include "gi_blorb.h"

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* =====================================================================
 * Blorb platform-layer functions (declared in gi_blorb.h)
 * ===================================================================== */

static giblorb_map_t *blorbmap = NULL;

giblorb_err_t giblorb_set_resource_map(strid_t file)
{
    (void)file;

    /* Stub — no actual Blorb resource map is created. */
    blorbmap = NULL;
    return giblorb_err_None;
}

giblorb_map_t *giblorb_get_resource_map(void)
{
    return blorbmap;
}

/* =====================================================================
 * gidispatch_set_retained_registry (always needed, see §3b)
 * ===================================================================== */

void gidispatch_set_retained_registry(
    gidispatch_rock_t (*regi)(void *array, glui32 len, char *typecode),
    void (*unregi)(void *array, glui32 len, char *typecode,
        gidispatch_rock_t objrock))
{
    (void)regi;
    (void)unregi;

    /* Stub — no retained array tracking. */
}

/* =====================================================================
 * GLK_MODULE_UNICODE — 16 functions (§3e)
 * ===================================================================== */

#ifdef GLK_MODULE_UNICODE

glui32 glk_buffer_to_lower_case_uni(glui32 *buf, glui32 len, glui32 numchars)
{
    (void)buf;
    (void)len;
    (void)numchars;

    /* Stub — returns numchars unchanged. */
    return numchars;
}

glui32 glk_buffer_to_upper_case_uni(glui32 *buf, glui32 len, glui32 numchars)
{
    (void)buf;
    (void)len;
    (void)numchars;

    /* Stub — returns numchars unchanged. */
    return numchars;
}

glui32 glk_buffer_to_title_case_uni(glui32 *buf, glui32 len, glui32 numchars,
    glui32 lowerrest)
{
    (void)buf;
    (void)len;
    (void)numchars;
    (void)lowerrest;

    /* Stub — returns numchars unchanged. */
    return numchars;
}

void glk_put_char_uni(glui32 ch)
{
    (void)ch;

    /* Stub — no-op. */
}

void glk_put_string_uni(glui32 *s)
{
    (void)s;

    /* Stub — no-op. */
}

void glk_put_buffer_uni(glui32 *buf, glui32 len)
{
    (void)buf;
    (void)len;

    /* Stub — no-op. */
}

void glk_put_char_stream_uni(strid_t str, glui32 ch)
{
    (void)str;
    (void)ch;

    /* Stub — no-op. */
}

void glk_put_string_stream_uni(strid_t str, glui32 *s)
{
    (void)str;
    (void)s;

    /* Stub — no-op. */
}

void glk_put_buffer_stream_uni(strid_t str, glui32 *buf, glui32 len)
{
    (void)str;
    (void)buf;
    (void)len;

    /* Stub — no-op. */
}

glsi32 glk_get_char_stream_uni(strid_t str)
{
    (void)str;

    /* Stub — returns -1 (no character available). */
    return -1;
}

glui32 glk_get_buffer_stream_uni(strid_t str, glui32 *buf, glui32 len)
{
    (void)str;
    (void)buf;
    (void)len;

    /* Stub — returns 0 (no data read). */
    return 0;
}

glui32 glk_get_line_stream_uni(strid_t str, glui32 *buf, glui32 len)
{
    (void)str;
    (void)buf;
    (void)len;

    /* Stub — returns 0 (no data read). */
    return 0;
}

strid_t glk_stream_open_file_uni(frefid_t fileref, glui32 fmode, glui32 rock)
{
    (void)fileref;
    (void)fmode;
    (void)rock;

    /* Stub — returns NULL. */
    return NULL;
}

strid_t glk_stream_open_memory_uni(glui32 *buf, glui32 buflen, glui32 fmode,
    glui32 rock)
{
    (void)buf;
    (void)buflen;
    (void)fmode;
    (void)rock;

    /* Stub — returns NULL. */
    return NULL;
}

void glk_request_char_event_uni(winid_t win)
{
    (void)win;

    /* Stub — no-op. */
}

void glk_request_line_event_uni(winid_t win, glui32 *buf, glui32 maxlen,
    glui32 initlen)
{
    (void)win;
    (void)buf;
    (void)maxlen;
    (void)initlen;

    /* Stub — no-op. */
}

#endif /* GLK_MODULE_UNICODE */

/* =====================================================================
 * GLK_MODULE_UNICODE_NORM — 2 functions (§3f)
 * ===================================================================== */

#ifdef GLK_MODULE_UNICODE_NORM

glui32 glk_buffer_canon_decompose_uni(glui32 *buf, glui32 len, glui32 numchars)
{
    (void)buf;
    (void)len;
    (void)numchars;

    /* Stub — returns numchars unchanged (identity decomposition). */
    return numchars;
}

glui32 glk_buffer_canon_normalize_uni(glui32 *buf, glui32 len, glui32 numchars)
{
    (void)buf;
    (void)len;
    (void)numchars;

    /* Stub — returns numchars unchanged (already normalized). */
    return numchars;
}

#endif /* GLK_MODULE_UNICODE_NORM */

/* =====================================================================
 * GLK_MODULE_SOUND — 9 functions (§3g)
 * ===================================================================== */

#ifdef GLK_MODULE_SOUND

schanid_t glk_schannel_create(glui32 rock)
{
    (void)rock;

    /* Stub — returns NULL. */
    return NULL;
}

void glk_schannel_destroy(schanid_t chan)
{
    (void)chan;

    /* Stub — no-op. */
}

schanid_t glk_schannel_iterate(schanid_t chan, glui32 *rockptr)
{
    (void)chan;
    (void)rockptr;

    /* Stub — returns NULL (no sound channels exist). */
    return NULL;
}

glui32 glk_schannel_get_rock(schanid_t chan)
{
    (void)chan;

    /* Stub — returns 0. */
    return 0;
}

glui32 glk_schannel_play(schanid_t chan, glui32 snd)
{
    (void)chan;
    (void)snd;

    /* Stub — returns FALSE (play failed). */
    return FALSE;
}

glui32 glk_schannel_play_ext(schanid_t chan, glui32 snd, glui32 repeats,
    glui32 notify)
{
    (void)chan;
    (void)snd;
    (void)repeats;
    (void)notify;

    /* Stub — returns FALSE (play failed). */
    return FALSE;
}

void glk_schannel_stop(schanid_t chan)
{
    (void)chan;

    /* Stub — no-op. */
}

void glk_schannel_set_volume(schanid_t chan, glui32 vol)
{
    (void)chan;
    (void)vol;

    /* Stub — no-op. */
}

void glk_sound_load_hint(glui32 snd, glui32 flag)
{
    (void)snd;
    (void)flag;

    /* Stub — no-op. */
}

/* =================================================================
 * GLK_MODULE_SOUND2 — 5 functions (nested inside GLK_MODULE_SOUND)
 * ================================================================= */

#ifdef GLK_MODULE_SOUND2

schanid_t glk_schannel_create_ext(glui32 rock, glui32 volume)
{
    (void)rock;
    (void)volume;

    /* Stub — returns NULL. */
    return NULL;
}

glui32 glk_schannel_play_multi(schanid_t *chanarray, glui32 chancount,
    glui32 *sndarray, glui32 soundcount, glui32 notify)
{
    (void)chanarray;
    (void)chancount;
    (void)sndarray;
    (void)soundcount;
    (void)notify;

    /* Stub — returns FALSE (play failed). */
    return FALSE;
}

void glk_schannel_pause(schanid_t chan)
{
    (void)chan;

    /* Stub — no-op. */
}

void glk_schannel_unpause(schanid_t chan)
{
    (void)chan;

    /* Stub — no-op. */
}

void glk_schannel_set_volume_ext(schanid_t chan, glui32 vol, glui32 duration,
    glui32 notify)
{
    (void)chan;
    (void)vol;
    (void)duration;
    (void)notify;

    /* Stub — no-op. */
}

#endif /* GLK_MODULE_SOUND2 */
#endif /* GLK_MODULE_SOUND */

/* =====================================================================
 * GLK_MODULE_HYPERLINKS — 4 functions (§3h)
 * ===================================================================== */

#ifdef GLK_MODULE_HYPERLINKS

void glk_set_hyperlink(glui32 linkval)
{
    (void)linkval;

    /* Stub — no-op. */
}

void glk_set_hyperlink_stream(strid_t str, glui32 linkval)
{
    (void)str;
    (void)linkval;

    /* Stub — no-op. */
}

void glk_request_hyperlink_event(winid_t win)
{
    (void)win;

    /* Stub — no-op. */
}

void glk_cancel_hyperlink_event(winid_t win)
{
    (void)win;

    /* Stub — no-op. */
}

#endif /* GLK_MODULE_HYPERLINKS */

/* =====================================================================
 * GLK_MODULE_DATETIME — 10 functions (§3i)
 * ===================================================================== */

#ifdef GLK_MODULE_DATETIME

void glk_current_time(glktimeval_t *time)
{
    (void)time;

    /* Stub — zeroes the time structure. */
    if (time) {
        time->high_sec = 0;
        time->low_sec = 0;
        time->microsec = 0;
    }
}

glsi32 glk_current_simple_time(glui32 factor)
{
    (void)factor;

    /* Stub — returns 0. */
    return 0;
}

void glk_time_to_date_utc(glktimeval_t *time, glkdate_t *date)
{
    (void)time;

    /* Stub — zeroes the date structure. */
    if (date) {
        date->year = 0;
        date->month = 0;
        date->day = 0;
        date->weekday = 0;
        date->hour = 0;
        date->minute = 0;
        date->second = 0;
        date->microsec = 0;
    }
}

void glk_time_to_date_local(glktimeval_t *time, glkdate_t *date)
{
    (void)time;

    /* Stub — zeroes the date structure. */
    if (date) {
        date->year = 0;
        date->month = 0;
        date->day = 0;
        date->weekday = 0;
        date->hour = 0;
        date->minute = 0;
        date->second = 0;
        date->microsec = 0;
    }
}

void glk_simple_time_to_date_utc(glsi32 time, glui32 factor, glkdate_t *date)
{
    (void)time;
    (void)factor;

    /* Stub — zeroes the date structure. */
    if (date) {
        date->year = 0;
        date->month = 0;
        date->day = 0;
        date->weekday = 0;
        date->hour = 0;
        date->minute = 0;
        date->second = 0;
        date->microsec = 0;
    }
}

void glk_simple_time_to_date_local(glsi32 time, glui32 factor, glkdate_t *date)
{
    (void)time;
    (void)factor;

    /* Stub — zeroes the date structure. */
    if (date) {
        date->year = 0;
        date->month = 0;
        date->day = 0;
        date->weekday = 0;
        date->hour = 0;
        date->minute = 0;
        date->second = 0;
        date->microsec = 0;
    }
}

void glk_date_to_time_utc(glkdate_t *date, glktimeval_t *time)
{
    (void)date;

    /* Stub — zeroes the time structure. */
    if (time) {
        time->high_sec = 0;
        time->low_sec = 0;
        time->microsec = 0;
    }
}

void glk_date_to_time_local(glkdate_t *date, glktimeval_t *time)
{
    (void)date;

    /* Stub — zeroes the time structure. */
    if (time) {
        time->high_sec = 0;
        time->low_sec = 0;
        time->microsec = 0;
    }
}

glsi32 glk_date_to_simple_time_utc(glkdate_t *date, glui32 factor)
{
    (void)date;
    (void)factor;

    /* Stub — returns 0. */
    return 0;
}

glsi32 glk_date_to_simple_time_local(glkdate_t *date, glui32 factor)
{
    (void)date;
    (void)factor;

    /* Stub — returns 0. */
    return 0;
}

#endif /* GLK_MODULE_DATETIME */

/* =====================================================================
 * GLK_MODULE_RESOURCE_STREAM — 2 functions (§3j)
 * ===================================================================== */

#ifdef GLK_MODULE_RESOURCE_STREAM

strid_t glk_stream_open_resource(glui32 filenum, glui32 rock)
{
    (void)filenum;
    (void)rock;

    /* Stub — returns NULL. */
    return NULL;
}

strid_t glk_stream_open_resource_uni(glui32 filenum, glui32 rock)
{
    (void)filenum;
    (void)rock;

    /* Stub — returns NULL. */
    return NULL;
}

#endif /* GLK_MODULE_RESOURCE_STREAM */