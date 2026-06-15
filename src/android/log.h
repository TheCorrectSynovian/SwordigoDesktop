#ifndef ANDROID_LOG_H
#define ANDROID_LOG_H

#include <stdarg.h>

typedef enum android_LogPriority {
    ANDROID_LOG_UNKNOWN = 0,
    ANDROID_LOG_DEFAULT,
    ANDROID_LOG_VERBOSE,
    ANDROID_LOG_DEBUG,
    ANDROID_LOG_INFO,
    ANDROID_LOG_WARN,
    ANDROID_LOG_ERROR,
    ANDROID_LOG_FATAL,
    ANDROID_LOG_SILENT,
} android_LogPriority;

int __android_log_print(int prio, const char *tag, const char *fmt, ...);
int __android_log_vprint(int prio, const char *tag, const char *fmt, va_list ap);

#endif
