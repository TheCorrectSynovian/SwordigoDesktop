#include "log.h"
#include <stdio.h>

int __android_log_print(int prio, const char *tag, const char *fmt, ...) {
    va_list ap;
    printf("[%s] ", tag);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
    return 0;
}

int __android_log_vprint(int prio, const char *tag, const char *fmt, va_list ap) {
    printf("[%s] ", tag);
    vprintf(fmt, ap);
    printf("\n");
    return 0;
}
