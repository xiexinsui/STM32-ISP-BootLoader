#ifndef ISP_CLI_H
#define ISP_CLI_H

#include <windows.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 命令行产线下载：有 --port/--file 等参数时返回 true 并已执行完 */
bool isp_cli_run(LPSTR cmd_line);

#ifdef __cplusplus
}
#endif

#endif
