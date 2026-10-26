#ifndef MUSLIMNOTIFY_H
#define MUSLIMNOTIFY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MN_OK 0
#define MN_ERR_INVALID_DATE 1
#define MN_ERR_INVALID_COORDS 2

#define MN_NO_TIME (-1LL)

/* Slot order used by every array-based API. */
#define MN_FAJR 0
#define MN_SUNRISE 1
#define MN_DHUHR 2
#define MN_ASR 3
#define MN_MAGHRIB 4
#define MN_ISHA 5
#define MN_PRAYER_COUNT 6

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* MUSLIMNOTIFY_H */
