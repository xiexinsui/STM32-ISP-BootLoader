#ifndef UI_DEBUG_H
#define UI_DEBUG_H

#include <windows.h>
#include "app_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

HWND ui_debug_open(HWND parent);
void ui_debug_close(void);
int  ui_debug_visible(void);
void ui_debug_toggle(HWND parent);
void ui_debug_log(int level, const char *msg);
void ui_debug_resnap(void);
void ui_debug_update_dtr_rts(void);
int  ui_debug_capture_active(void);
void ui_debug_theme_refresh(void);
HBRUSH ui_main_btn_brush(int high);

#ifdef __cplusplus
}
#endif

#endif
