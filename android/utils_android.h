#ifndef __UTILS_ANDROID_H__
#define __UTILS_ANDROID_H__

#pragma once

#define DATADIR ""
#define BACKEND_ADDR ""
#define LOCALSTATEDIR "/data/vendor/telemetrics"

#ifndef telem_log
#define telem_log(priority, ...) do {} while (0);
#endif

#ifndef telem_perror
#define telem_perror(msg) do {} while (0);
#endif

inline int malloc_trim(int size) {return size;}

/* qsort_r is not available in Android, so redefine it */
inline void qsort_r(void *__base, size_t __nmemb, size_t __size,
                    int * function, void *__arg) {}

#endif /* __UTILS_ANDROID_H__ */
