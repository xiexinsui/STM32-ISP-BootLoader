#include "log.h"
#include <stdio.h>
#include <string.h>

static isp_log_fn g_sink;

void log_set_sink(isp_log_fn fn)
{
    g_sink = fn;
}

void log_msg(int level, const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (g_sink)
        g_sink(level, buf);
    else
        fputs(buf, stderr);
}

void log_pump_win(void) {}
