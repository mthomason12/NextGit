#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include "glk.h"
#include "cheapglk.h"

#ifdef GLK_MODULE_DATETIME

#ifdef __SPECTRUM_NEXT__
/*
 * Spectrum Next date/time implementation.
 *
 * z88dk provides a subset of the C time library. If the target
 * platform does not have a real-time clock, we return the Unix
 * epoch (Jan 1, 1970) as a safe default.
 *
 * Functions implemented:
 *   timespec_get  -> approximated
 *   localtime_r   -> returns UTC (no timezone support)
 *   gmtime_r      -> used for both
 *   mktime        -> standard C, used for local time
 *   timegm        -> custom implementation
 */
#else
/*
 * Linux date/time implementation.
 * Uses POSIX timespec_get, gmtime_r, localtime_r, mktime, timegm.
 */
#include <time.h>
#include <sys/time.h>

#ifndef NO_TIMEGM_AVAIL
extern time_t timegm(struct tm *tm);
#endif /* NO_TIMEGM_AVAIL */

#endif /* __SPECTRUM_NEXT__ */

#ifdef __SPECTRUM_NEXT__

/*
 * Spectrum Next: minimal time support.
 *
 * We rely on whatever time functions z88dk provides for the target.
 * If time() and related functions exist, use them.
 * We use a simple has_time check and provide fallbacks.
 */

#include <time.h>

/* Check if we have a working clock. We use a simple heuristic:
   if time() returns a value > 0 and not -1, we assume it works. */
static int gli_has_clock(void)
{
    time_t t = time(NULL);
    return (t > 0) ? 1 : 0;
}

static struct tm *gli_gmtime_r(const time_t *timer, struct tm *result)
{
    /* On z88dk, gmtime_r may or may not be available.
       We provide a simple fallback using gmtime if needed. */
#ifdef gmtime_r
    return gmtime_r(timer, result);
#else
    struct tm *gm = gmtime(timer);
    if (!gm) {
        bzero(result, sizeof(*result));
        return result;
    }
    *result = *gm;
    return result;
#endif
}

static struct tm *gli_localtime_r(const time_t *timer, struct tm *result)
{
    /* On z88dk, localtime_r may not be available.
       Fall back to gmtime which should exist on all C platforms. */
#ifdef localtime_r
    return localtime_r(timer, result);
#else
    struct tm *loc = localtime(timer);
    if (!loc) {
        bzero(result, sizeof(*result));
        return result;
    }
    *result = *loc;
    return result;
#endif
}

static int gli_timespec_get(struct timespec *ts)
{
    if (!gli_has_clock()) {
        ts->tv_sec = 0;
        ts->tv_nsec = 0;
        return 0;
    }
    ts->tv_sec = time(NULL);
    ts->tv_nsec = 0;
    return 1;
}

static time_t gli_timegm(struct tm *tm)
{
    /*
     * timegm is the UTC equivalent of mktime.
     * Since Spectrum Next has no timezone, mktime effectively
     * already gives UTC. If the platform does differentiate,
     * we'd need to handle this, but for now we just call mktime.
     */
    tm->tm_isdst = 0;
    return mktime(tm);
}

#define gmtime_r(t, r)      gli_gmtime_r((t), (r))
#define localtime_r(t, r)   gli_localtime_r((t), (r))
#define timespec_get(ts, b) gli_timespec_get((ts))
#define timegm(tm)           gli_timegm((tm))

#else
/* Linux: use the system-provided time functions directly. */
#include <time.h>
#include <sys/time.h>

#ifdef NO_TIMEGM_AVAIL
static time_t gli_timegm(struct tm *tm)
{
    time_t res;
    char *origtz = getenv("TZ");
    setenv("TZ", "", 1);
    tzset();
    res = mktime(tm);
    if (origtz)
        setenv("TZ", origtz, 1);
    else
        unsetenv("TZ");
    tzset();
    return res;
}
#define timegm(tm) gli_timegm((tm))
#endif /* NO_TIMEGM_AVAIL */

#endif /* __SPECTRUM_NEXT__ */

/* Copy a POSIX tm structure to a glkdate. */
static void gli_date_from_tm(glkdate_t *date, struct tm *tm)
{
    date->year = 1900 + tm->tm_year;
    date->month = 1 + tm->tm_mon;
    date->day = tm->tm_mday;
    date->weekday = tm->tm_wday;
    date->hour = tm->tm_hour;
    date->minute = tm->tm_min;
    date->second = tm->tm_sec;
}

/* Copy a glkdate to a POSIX tm structure. 
   This is used in the "glk_date_to_..." functions, which are supposed
   to normalize the glkdate. We're going to rely on the mktime() / 
   timegm() functions to do that -- except they don't handle microseconds.
   So we'll have to do that normalization here, adjust the tm_sec value,
   and return the normalized number of microseconds.
*/
static glsi32 gli_date_to_tm(glkdate_t *date, struct tm *tm)
{
    glsi32 microsec;

    bzero(tm, sizeof(*tm));
    tm->tm_year = date->year - 1900;
    tm->tm_mon = date->month - 1;
    tm->tm_mday = date->day;
    tm->tm_wday = date->weekday;
    tm->tm_hour = date->hour;
    tm->tm_min = date->minute;
    tm->tm_sec = date->second;
    microsec = date->microsec;

    if (microsec >= 1000000) {
        tm->tm_sec += (microsec / 1000000);
        microsec = microsec % 1000000;
    }
    else if (microsec < 0) {
        microsec = -1 - microsec;
        tm->tm_sec -= (1 + microsec / 1000000);
        microsec = 999999 - (microsec % 1000000);
    }

    return microsec;
}

/* Convert a Unix timestamp, along with a microseconds value, to
   a glktimeval. 
*/
static void gli_timestamp_to_time(time_t timestamp, glsi32 microsec, 
    glktimeval_t *time)
{
    if (sizeof(timestamp) <= 4) {
        /* This platform has 32-bit time, but we can't do anything
           about that. Hope it's not 2038 yet. */
        if (timestamp >= 0)
            time->high_sec = 0;
        else
            time->high_sec = -1;
        time->low_sec = timestamp;
    }
    else {
        /* The cast to int64_t shouldn't be necessary, but it
           suppresses a pointless warning in the 32-bit case.
           (Remember that we won't be executing this line in the
           32-bit case.) */
        time->high_sec = (((int64_t)timestamp) >> 32) & 0xFFFFFFFF;
        time->low_sec = timestamp & 0xFFFFFFFF;
    }

    time->microsec = microsec;
}

/* Divide a Unix timestamp by a (positive) value. */
static glsi32 gli_simplify_time(time_t timestamp, glui32 factor)
{
    /* We want to round towards negative infinity, which takes a little
       bit of fussing. */
    if (timestamp >= 0) {
        return timestamp / (time_t)factor;
    }
    else {
        return -1 - (((time_t)-1 - timestamp) / (time_t)factor);
    }
}

void glk_current_time(glktimeval_t *time)
{
    struct timespec ts;

    if (!timespec_get(&ts, TIME_UTC)) {
        gli_timestamp_to_time(0, 0, time);
        gli_strict_warning("current_time: timespec_get() failed.");
        return;
    }

    gli_timestamp_to_time(ts.tv_sec, ts.tv_nsec/1000, time);
}

glsi32 glk_current_simple_time(glui32 factor)
{
    struct timespec ts;

    if (factor == 0) {
        gli_strict_warning("current_simple_time: factor cannot be zero.");
        return 0;
    }

    if (!timespec_get(&ts, TIME_UTC)) {
        gli_strict_warning("current_simple_time: timespec_get() failed.");
        return 0;
    }

    return gli_simplify_time(ts.tv_sec, factor);
}

void glk_time_to_date_utc(glktimeval_t *time, glkdate_t *date)
{
    time_t timestamp;
    struct tm tm;

    timestamp = time->low_sec;
    if (sizeof(timestamp) > 4) {
        timestamp += ((int64_t)time->high_sec << 32);
    }

    gmtime_r(&timestamp, &tm);

    gli_date_from_tm(date, &tm);
    date->microsec = time->microsec;
}

void glk_time_to_date_local(glktimeval_t *time, glkdate_t *date)
{
    time_t timestamp;
    struct tm tm;

    timestamp = time->low_sec;
    if (sizeof(timestamp) > 4) {
        timestamp += ((int64_t)time->high_sec << 32);
    }

    localtime_r(&timestamp, &tm);

    gli_date_from_tm(date, &tm);
    date->microsec = time->microsec;
}

void glk_simple_time_to_date_utc(glsi32 time, glui32 factor, 
    glkdate_t *date)
{
    time_t timestamp = (time_t)time * factor;
    struct tm tm;

    gmtime_r(&timestamp, &tm);

    gli_date_from_tm(date, &tm);
    date->microsec = 0;
}

void glk_simple_time_to_date_local(glsi32 time, glui32 factor, 
    glkdate_t *date)
{
    time_t timestamp = (time_t)time * factor;
    struct tm tm;

    localtime_r(&timestamp, &tm);

    gli_date_from_tm(date, &tm);
    date->microsec = 0;
}

void glk_date_to_time_utc(glkdate_t *date, glktimeval_t *time)
{
    time_t timestamp;
    struct tm tm;
    glsi32 microsec;

    microsec = gli_date_to_tm(date, &tm);
    /* The timegm function is not standard POSIX. If it's not available
       on your platform, try setting the env var "TZ" to "", calling
       mktime(), and then resetting "TZ". */
    tm.tm_isdst = 0;
    timestamp = timegm(&tm);

    gli_timestamp_to_time(timestamp, microsec, time);
}

void glk_date_to_time_local(glkdate_t *date, glktimeval_t *time)
{
    time_t timestamp;
    struct tm tm;
    glsi32 microsec;

    microsec = gli_date_to_tm(date, &tm);
    tm.tm_isdst = -1;
    timestamp = mktime(&tm);

    gli_timestamp_to_time(timestamp, microsec, time);
}

glsi32 glk_date_to_simple_time_utc(glkdate_t *date, glui32 factor)
{
    time_t timestamp;
    struct tm tm;

    if (factor == 0) {
        gli_strict_warning("date_to_simple_time_utc: factor cannot be zero.");
        return 0;
    }

    gli_date_to_tm(date, &tm);
    /* The timegm function is not standard POSIX. If it's not available
       on your platform, try setting the env var "TZ" to "", calling
       mktime(), and then resetting "TZ". */
    tm.tm_isdst = 0;
    timestamp = timegm(&tm);

    return gli_simplify_time(timestamp, factor);
}

glsi32 glk_date_to_simple_time_local(glkdate_t *date, glui32 factor)
{
    time_t timestamp;
    struct tm tm;

    if (factor == 0) {
        gli_strict_warning("date_to_simple_time_local: factor cannot be zero.");
        return 0;
    }

    gli_date_to_tm(date, &tm);
    tm.tm_isdst = -1;
    timestamp = mktime(&tm);

    return gli_simplify_time(timestamp, factor);
}

#endif /* GLK_MODULE_DATETIME */