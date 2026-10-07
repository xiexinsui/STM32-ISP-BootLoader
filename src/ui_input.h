#ifndef UI_INPUT_H
#define UI_INPUT_H

#include <windows.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 模态十六进制输入框，成功返回 true */
bool ui_input_hex(HWND parent, const char *title, const char *prompt,
                  const char *default_text, uint32_t *out_value);

#ifdef __cplusplus
}
#endif

#endif
