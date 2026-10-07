#ifndef UI_MAIN_H
#define UI_MAIN_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 控件 ID */
enum {
    IDC_PORT_COMBO = 1001,
    IDC_REFRESH,
    IDC_BAUD_COMBO,
    IDC_CUSTOM_BAUD_CHK,
    IDC_CUSTOM_BAUD_EDIT,
    IDC_SERIAL_TOGGLE,
    IDC_MODE_COMBO,
    IDC_STEP_DELAY,
    IDC_DTR_TOGGLE,
    IDC_RTS_TOGGLE,
    IDC_DTR_STATE,
    IDC_RTS_STATE,
    IDC_PID_VAL,
    IDC_NAME_VAL,
    IDC_FLASH_VAL,
    IDC_BLVER_VAL,
    IDC_PROT_VAL,
    IDC_FILE_EDIT,
    IDC_BROWSE,
    IDC_FILE_INFO,
    IDC_ERASE,
    IDC_DOWNLOAD,
    IDC_VERIFY,
    IDC_RUN,
    IDC_OPTBYTES,
    IDC_READFLASH,
    IDC_VERIFY_DL,
    IDC_RUN_AFTER,
    IDC_CANCEL,
    IDC_LOG,
    IDC_PROGRESS,
    IDC_STATUS,
    IDC_DBG_PANEL
};

/* 菜单 */
enum {
    IDM_EXIT = 2001,
    IDM_REFRESH_PORTS,
    IDM_DEBUG_PANEL,
    IDM_HEX_LOG,
    IDM_EXPORT_LOG,
    IDM_ABOUT,
    IDM_ACCEL_HINT,
    IDM_LANG_CN,
    IDM_LANG_EN
};

/* 快捷键命令 */
enum {
    IDM_ACCEL_DOWNLOAD = 2101,
    IDM_ACCEL_ERASE,
    IDM_ACCEL_VERIFY,
    IDM_ACCEL_RUN,
    IDM_ACCEL_DEBUG,
    IDM_ACCEL_HEXLOG,
    IDM_ACCEL_CANCEL
};

/* 跨线程 UI 消息 */
#define WM_APP_LOG      (WM_APP + 1)
#define WM_APP_PROGRESS (WM_APP + 2)
#define WM_APP_OP_DONE  (WM_APP + 3)

typedef struct {
    int level;
    char msg[1024];
} ui_log_item_t;

typedef struct {
    int pct;
    char status[128];
} ui_prog_item_t;

HWND ui_create_main(HINSTANCE inst);
HACCEL ui_get_accel(void);
HWND ui_main_hwnd(void);
/* 将子窗口吸附到主窗口两侧：side <0 左侧，side >0 右侧 */
void ui_snap_to_main(HWND hwnd, int side);
/* 主窗口移动/缩放时重新吸附已打开的工具窗 */
void ui_resnap_tools(void);
void ui_log_append(int level, const char *msg);
void ui_set_status(const char *text);
void ui_set_progress(int pct, const char *status);
void ui_refresh_ports(void);
void ui_update_dtr_rts(void);
void ui_update_chip_info(void);
void ui_update_serial_ui(void);

/* 系统主题（AppsUseLightTheme）：0=浅色, 1=深色 */
int ui_theme_is_dark(void);
COLORREF ui_theme_bg(void);
COLORREF ui_theme_fg(void);
COLORREF ui_theme_edit_bg(void);
COLORREF ui_theme_edit_fg(void);
HBRUSH ui_theme_bg_brush(void);
HBRUSH ui_theme_edit_brush(void);
void ui_theme_reload(void);
HBRUSH ui_main_btn_brush(int high);

#ifdef __cplusplus
}
#endif

#endif
