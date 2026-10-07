#ifndef UI_OPTBYTES_H
#define UI_OPTBYTES_H

#include <windows.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 模态选项字节对话框（F1 式 8 组值+反码） */
void ui_optbytes_run(HWND parent);

#ifdef __cplusplus
}
#endif

#endif
