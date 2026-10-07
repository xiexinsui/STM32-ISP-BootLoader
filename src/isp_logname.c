#include "isp_logname.h"
#include <windows.h>
#include <stdio.h>

#ifndef ISP_VERSION_STR
#define ISP_VERSION_STR "0.6"
#endif

void isp_log_stamp(char *buf, int buflen)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(buf, buflen, "%04u%02u%02u_%02u%02u%02u_%03u",
             (unsigned)st.wYear, (unsigned)st.wMonth, (unsigned)st.wDay,
             (unsigned)st.wHour, (unsigned)st.wMinute, (unsigned)st.wSecond,
             (unsigned)st.wMilliseconds);
}

void isp_log_header(char *buf, int buflen)
{
    /* __DATE__ = "Sep 20 2026" → 2026年9月20日 */
    char mon[4] = {0};
    int day = 0, year = 0;
    sscanf(__DATE__, "%3s %d %d", mon, &day, &year);
    static const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    int m = 0;
    for (int i = 0; i < 12; i++) {
        if (_strnicmp(mon, months + i * 3, 3) == 0) { m = i + 1; break; }
    }
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(buf, buflen,
             "版本: v%s\n编译日期: %d年%d月%d日 %s\n"
             "日志时间: %d年%d月%d日 %02d:%02d:%02d.%03d\n"
             "----------------------------------------\n",
             ISP_VERSION_STR,
             year, m, day, __TIME__,
             (int)st.wYear, (int)st.wMonth, (int)st.wDay,
             (int)st.wHour, (int)st.wMinute, (int)st.wSecond, (int)st.wMilliseconds);
}
