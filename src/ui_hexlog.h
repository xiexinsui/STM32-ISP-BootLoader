#ifndef UI_HEXLOG_H
#define UI_HEXLOG_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

HWND ui_hexlog_open(HWND parent);
void ui_hexlog_close(void);
int  ui_hexlog_visible(void);
void ui_hexlog_toggle(HWND parent);
void ui_hexlog_append(int level, const char *msg);
void ui_hexlog_clear(void);
void ui_hexlog_export(HWND owner);
void ui_hexlog_resnap(void);
void ui_hexlog_theme_refresh(void);

#ifdef __cplusplus
}
#endif

#endif
