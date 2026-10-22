/*
 * muslimnotify.h — C ABI of the MuslimNotify Rust core (crate: mn-ffi).
 *
 * The Rust static library (libmuslimnotify_ffi.a) is linked by the Qt
 * frontend. Every function here is `extern "C"`, every string is a static
 * literal with static storage duration (caller must NOT free), and errors
 * are small integer codes.
 *
 * Times cross the boundary as seconds since LOCAL MIDNIGHT of the requested
 * civil date, so the UI can format them without timezone math. The caller
 * passes the UTC offset (seconds east) in effect on that date, which the
 * platform (QTimeZone) supplies — DST therefore stays correct.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef MUSLIMNOTIFY_H
#define MUSLIMNOTIFY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Result codes for mn_compute_times. */
#define MN_OK 0
#define MN_ERR_INVALID_DATE 1
#define MN_ERR_INVALID_COORDS 2

/* Sentinel for a slot with no valid time (polar day/night). */
#define MN_NO_TIME (-1LL)

/* Slot order used by every array-based API. */
#define MN_FAJR 0
#define MN_SUNRISE 1
#define MN_DHUHR 2
#define MN_ASR 3
#define MN_MAGHRIB 4
#define MN_ISHA 5
#define MN_PRAYER_COUNT 6

/* ---- metadata ---------------------------------------------------------- */

const char *mn_version(void);
int mn_prayer_count(void);
const char *mn_prayer_name_en(int slot);  /* slot in [0, MN_PRAYER_COUNT) */
const char *mn_prayer_name_ar(int slot);

/* Calculation methods: 12 entries, UI position == stable id. */
int mn_method_count(void);
int mn_method_id(int ui_index);
const char *mn_method_display_name(int method_id);
const char *mn_method_short_name(int method_id);

/* High-latitude rules: 0=Auto, 1=Middle of night, 2=Seventh, 3=Twilight. */
int mn_high_lat_rule_count(void);
const char *mn_high_lat_rule_name(int rule_id);

/* ---- formatting -------------------------------------------------------- */

/* Formats secs-since-local-midnight as "HH:MM" (24h) or "h:MM AM/PM".
 * Returns 0 on success, 1 when out is null or cap < 2. */
int mn_format_clock(long long secs, int use_24h, char *out, size_t cap);

/* ---- computation ------------------------------------------------------- */

/*
 * Compute Fajr..Isha (+Sunrise) for the LOCAL civil date (year, month, day).
 *
 *   tz_offset_secs  UTC offset in effect on that date, seconds east
 *                   (e.g. 10800 for UTC+3, -14400 for UTC-4). Ask the OS
 *                   once per day; DST is handled by the platform.
 *   latitude        degrees north, [-90, 90]
 *   longitude       degrees east, [-180, 180]
 *   method_id       0..11, see mn_method_display_name
 *   madhab_id       0 = Shafi (default), 1 = Hanafi (Asr shadow length)
 *   high_lat_id     0 = Auto, 1 = Middle of night, 2 = Seventh, 3 = Twilight
 *   out_secs        receives MN_PRAYER_COUNT long longs: seconds since local
 *                   midnight, or MN_NO_TIME when the sun never crosses
 *
 * Returns MN_OK, MN_ERR_INVALID_DATE or MN_ERR_INVALID_COORDS.
 */
int mn_compute_times(int year, int month, int day,
                     long long tz_offset_secs,
                     double latitude, double longitude,
                     int method_id, int madhab_id, int high_lat_id,
                     long long *out_secs);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* MUSLIMNOTIFY_H */
