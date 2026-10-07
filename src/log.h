#ifndef ISP_LOG_H
#define ISP_LOG_H

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    LOG_INFO = 0,
    LOG_SUCCESS,
    LOG_WARN,
    LOG_ERROR,
    LOG_CMD,     /* 协议帧 hex — 仅全局日志 */
    LOG_TRACE    /* 协议步骤明细 — 仅全局日志 */
};

typedef void (*isp_log_fn)(int level, const char *msg);

void log_set_sink(isp_log_fn fn);
void log_msg(int level, const char *fmt, ...);
void log_pump_win(void); /* optional: UI may poll; no-op if sink handles sync */

#ifdef __cplusplus
}
#endif

#endif
