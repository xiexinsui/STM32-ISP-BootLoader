#ifndef ISP_LOGNAME_H
#define ISP_LOGNAME_H

#ifdef __cplusplus
extern "C" {
#endif

/* 形如 20260920_153000_123（精确到毫秒） */
void isp_log_stamp(char *buf, int buflen);

/* 版本 + 中文编译日期 + 日志时间 头部两行左右 */
void isp_log_header(char *buf, int buflen);

#ifdef __cplusplus
}
#endif

#endif
