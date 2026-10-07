/* ISP_C 主界面 — Win32，对齐 Qt 1.2 左右布局（P2 增强） */
#include "ui_main.h"
#include "ui_debug.h"
#include "ui_optbytes.h"
#include "ui_hexlog.h"
#include "ui_input.h"
#include "ui_lang.h"
#include "isp_logname.h"
#include "accel_ids.h"
#include "app_logic.h"
#include "isp_config.h"
#include "serial_port.h"
#include "bl_control.h"
#include "log.h"
#include <commctrl.h>
#include <commdlg.h>
#include <richedit.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* forward: 主题刷新时通知工具窗 */
void ui_debug_theme_refresh(void);
void ui_hexlog_theme_refresh(void);

#define MARGIN 10
#define LEFT_W 360
#define BTN_H  28
#define LOG_MAX_CHARS 180000

static HWND g_main;
static HWND g_hPort, g_hBaud, g_hCustomChk, g_hCustomEdit;
static HWND g_hSerialBtn, g_hMode, g_hDelay;
static HWND g_hDtrBtn, g_hRtsBtn, g_hDtrState, g_hRtsState;
static HWND g_hPid, g_hName, g_hFlash, g_hBlVer, g_hProt;
static HWND g_hFile, g_hFileInfo;
static HWND g_hLog, g_hProg, g_hStatus;
static HWND g_hCancel, g_hVerifyDl, g_hRunAfter;
static HWND g_hGrpSerial, g_hGrpBl, g_hGrpChip, g_hGrpFw, g_hGrpAct, g_hGrpLog;
static HFONT g_font;
static HFONT g_log_font;
static HBRUSH g_brGreen, g_brRed, g_brGray, g_brWhite;
static HBRUSH g_brLightGreen, g_brLightRed;
static volatile LONG g_busy;
static volatile LONG g_cancel_flag;
static int g_last_w, g_last_h;
static HWND g_dbgWnd;
static HACCEL g_accel;
static char g_bin_addr_text[32] = "0x08000000";
static isp_cfg_t g_cfg;
static int get_selected_baud(void);
static int get_mode(void);
static int get_delay(void);

HACCEL ui_get_accel(void) { return g_accel; }

/* ── 系统主题 ── */
/* 固定浅色主题（已去掉深色跟随） */
static HBRUSH g_brThemeBg;
static HBRUSH g_brThemeEdit;

static void theme_rebuild_brushes(void)
{
    if (!g_brThemeBg) g_brThemeBg = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    if (!g_brThemeEdit) g_brThemeEdit = CreateSolidBrush(RGB(255, 255, 255));
}

int ui_theme_is_dark(void) { return 0; }
COLORREF ui_theme_bg(void) { return GetSysColor(COLOR_BTNFACE); }
COLORREF ui_theme_fg(void) { return GetSysColor(COLOR_WINDOWTEXT); }
COLORREF ui_theme_edit_bg(void) { return RGB(255, 255, 255); }
COLORREF ui_theme_edit_fg(void) { return RGB(20, 20, 20); }
HBRUSH ui_theme_bg_brush(void) { theme_rebuild_brushes(); return g_brThemeBg; }
HBRUSH ui_theme_edit_brush(void) { theme_rebuild_brushes(); return g_brThemeEdit; }
void ui_theme_reload(void) {}

HBRUSH ui_main_btn_brush(int high)
{
    if (high) {
        if (!g_brLightGreen) g_brLightGreen = CreateSolidBrush(RGB(220, 245, 225));
        return g_brLightGreen;
    }
    if (!g_brLightRed) g_brLightRed = CreateSolidBrush(RGB(255, 230, 230));
    return g_brLightRed;
}
HWND ui_main_hwnd(void)
{
    return g_main;
}

void ui_snap_to_main(HWND hwnd, int side)
{
    HWND main = g_main;
    RECT mr, cr;
    if (!hwnd || !IsWindow(hwnd)) return;
    if (!main || !IsWindow(main)) return;
    if (!GetWindowRect(main, &mr)) return;
    if (!GetWindowRect(hwnd, &cr)) return;

    int mainH = mr.bottom - mr.top;
    int w = cr.right - cr.left;
    /* 吸附时高度与主窗口一致 */
    int h = mainH;
    if (h < 360) h = 360;

    int x, y = mr.top;
    if (side < 0) {
        x = mr.left - w;
        if (x < 0) x = 0;
    } else {
        x = mr.right;
        int sw = GetSystemMetrics(SM_CXSCREEN);
        if (x + w > sw) x = sw - w;
        if (x < 0) x = 0;
    }
    int sh = GetSystemMetrics(SM_CYSCREEN);
    if (y + h > sh) y = sh - h;
    if (y < 0) y = 0;

    SetWindowPos(hwnd, HWND_NOTOPMOST, x, y, w, h, SWP_NOACTIVATE);
}

/* 由 ui_debug / ui_hexlog 提供：可见时重新吸附 */
void ui_debug_resnap(void);
void ui_hexlog_resnap(void);

void ui_resnap_tools(void)
{
    ui_debug_resnap();
    ui_hexlog_resnap();
}

/* ACP/GBK → UTF-16 转换已并入日志追加流程；此处不再单独提供 */

static const int BAUDS[] = { 115200, 9600, 14400, 19200, 28800, 38400, 57600, 230400, 460800 };
char g_pending_path[MAX_PATH];
static void start_op(int op);
static void ui_log_append_now(int level, const char *msg)
{
    static const char *tag[] = { "", "[OK] ", "[!] ", "[ERR] ", ">> ", "... " };
    char line[1400];
    const char *t = (level >= 0 && level <= 5) ? tag[level] : "";

    /* 协议明细只进全局日志；调试面板仅在面板自身操作时接收 */
    if (level == LOG_CMD || level == LOG_TRACE) {
        ui_hexlog_append(level, msg);
        if (ui_debug_capture_active() && g_dbgWnd && IsWindow(g_dbgWnd))
            ui_debug_log(level, msg);
        return;
    }

    snprintf(line, sizeof(line), "%s%s", t, msg ? msg : "");
    ui_hexlog_append(level, msg);
    /* 下载/主窗口操作不刷调试面板，避免固件收发刷屏 */
    if (ui_debug_capture_active() && g_dbgWnd && IsWindow(g_dbgWnd))
        ui_debug_log(level, msg);

    if (!g_hLog || !IsWindow(g_hLog)) return;

    /* ListBox：避免 RichEdit 在部分系统上背景错乱/发黑 */
    SendMessageA(g_hLog, LB_ADDSTRING, 0, (LPARAM)line);
    int cnt = (int)SendMessageA(g_hLog, LB_GETCOUNT, 0, 0);
    while (cnt > 800) {
        SendMessageA(g_hLog, LB_DELETESTRING, 0, 0);
        cnt--;
    }
    SendMessageA(g_hLog, LB_SETTOPINDEX, (WPARAM)(cnt > 0 ? cnt - 1 : 0), 0);
}

void ui_log_append(int level, const char *msg)
{
    if (!g_main || !IsWindow(g_main)) return;
    ui_log_append_now(level, msg);
}

void ui_set_status(const char *text)
{
    if (g_hStatus) SetWindowTextA(g_hStatus, text ? text : "");
}

void ui_set_progress(int pct, const char *status)
{
    if (g_hProg) SendMessage(g_hProg, PBM_SETPOS, (WPARAM)pct, 0);
    if (status) ui_set_status(status);
}

static void ui_log_sink(int level, const char *msg)
{
    ui_log_append(level, msg);
}

void ui_update_dtr_rts(void)
{
    int dtr = logic_dtr_level();
    int rts = logic_rts_level();
    /* 状态只显示在开关按钮上，不再单独重复一排指示灯 */
    if (g_hDtrBtn) {
        char b[32];
        snprintf(b, sizeof(b), "DTR: %s", dtr ? "HIGH" : "LOW");
        SetWindowTextA(g_hDtrBtn, b);
        SendMessageA(g_hDtrBtn, BM_SETCHECK, dtr ? BST_CHECKED : BST_UNCHECKED, 0);
        InvalidateRect(g_hDtrBtn, NULL, TRUE);
    }
    if (g_hRtsBtn) {
        char b[32];
        snprintf(b, sizeof(b), "RTS: %s", rts ? "HIGH" : "LOW");
        SetWindowTextA(g_hRtsBtn, b);
        SendMessageA(g_hRtsBtn, BM_SETCHECK, rts ? BST_CHECKED : BST_UNCHECKED, 0);
        InvalidateRect(g_hRtsBtn, NULL, TRUE);
    }
    ui_debug_update_dtr_rts();
    if (g_hDtrState) {
        SetWindowTextA(g_hDtrState, dtr ? "HIGH" : "LOW");
        InvalidateRect(g_hDtrState, NULL, TRUE);
        UpdateWindow(g_hDtrState);
    }
    if (g_hRtsState) {
        SetWindowTextA(g_hRtsState, rts ? "HIGH" : "LOW");
        InvalidateRect(g_hRtsState, NULL, TRUE);
        UpdateWindow(g_hRtsState);
    }
}

void ui_update_chip_info(void)
{
    chip_ui_info_t ci;
    logic_get_chip_info(&ci);
    char b[128];

    if (g_hPid) {
        if (ci.pid)
            snprintf(b, sizeof(b), "0x%04X (%u)", ci.pid, ci.pid);
        else
            snprintf(b, sizeof(b), "---");
        SetWindowTextA(g_hPid, b);
    }
    if (g_hName)
        SetWindowTextA(g_hName, ci.name[0] ? ci.name : "---");
    if (g_hFlash) {
        if (ci.flash_kb >= 1024)
            snprintf(b, sizeof(b), "%u.%u MB (%u KB)",
                     ci.flash_kb / 1024, (ci.flash_kb % 1024) / 128, (unsigned)ci.flash_kb);
        else if (ci.flash_kb)
            snprintf(b, sizeof(b), "%u KB", (unsigned)ci.flash_kb);
        else
            snprintf(b, sizeof(b), "---");
        SetWindowTextA(g_hFlash, b);
    }
    if (g_hBlVer) {
        if (ci.bl_ver)
            snprintf(b, sizeof(b), "0x%02X", ci.bl_ver);
        else
            snprintf(b, sizeof(b), "---");
        SetWindowTextA(g_hBlVer, b);
    }
    if (g_hProt) {
        if (!ci.connected && !ci.pid) {
            SetWindowTextA(g_hProt, "---");
        } else if (!ci.has_protection) {
            SetWindowTextA(g_hProt, "无保护");
        } else {
            snprintf(b, sizeof(b), "有保护 (0x%02X / L%d)", ci.rdp_raw, ci.rdp_level);
            SetWindowTextA(g_hProt, b);
        }
        InvalidateRect(g_hProt, NULL, TRUE);
    }
}

void ui_update_serial_ui(void)
{
    int open = logic_serial_is_open();
    if (g_hSerialBtn)
        SetWindowTextA(g_hSerialBtn, open ? "关闭串口" : "打开串口");
    EnableWindow(g_hPort, open ? FALSE : TRUE);
    EnableWindow(g_hBaud, open ? FALSE : TRUE);
    EnableWindow(g_hCustomChk, open ? FALSE : TRUE);
    if (g_hCustomEdit) {
        int custom = (int)SendMessageA(g_hCustomChk, BM_GETCHECK, 0, 0) == BST_CHECKED;
        EnableWindow(g_hCustomEdit, (!open && custom) ? TRUE : FALSE);
    }
    ui_update_dtr_rts();
}

static int str_contains_ci(const char *hay, const char *needle)
{
    if (!hay || !needle || !*needle) return 0;
    size_t nl = strlen(needle);
    for (const char *p = hay; *p; p++) {
        size_t i = 0;
        while (i < nl && p[i]) {
            char a = p[i], b = needle[i];
            if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
            if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
            if (a != b) break;
            i++;
        }
        if (i == nl) return 1;
    }
    return 0;
}

static void cfg_save_from_ui(void)
{
    char b[64];
    if (g_hPort) {
        int idx = (int)SendMessageA(g_hPort, CB_GETCURSEL, 0, 0);
        if (idx >= 0) {
            char line[160];
            SendMessageA(g_hPort, CB_GETLBTEXT, (WPARAM)idx, (LPARAM)line);
            const char *p = strstr(line, "COM");
            if (p) {
                int k = 0;
                while (p[k] && k < 31 &&
                       ((p[k] >= '0' && p[k] <= '9') || p[k]=='C'||p[k]=='O'||p[k]=='M')) {
                    g_cfg.port[k] = p[k];
                    k++;
                }
                g_cfg.port[k] = 0;
            }
        }
    }
    g_cfg.baud = get_selected_baud();
    g_cfg.mode = get_mode();
    g_cfg.delay_ms = get_delay();
    if (g_hFile) {
        GetWindowTextA(g_hFile, g_cfg.file, sizeof(g_cfg.file));
    }
    if (g_hVerifyDl)
        g_cfg.verify_dl = (int)SendMessageA(g_hVerifyDl, BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (g_hRunAfter)
        g_cfg.run_after = (int)SendMessageA(g_hRunAfter, BM_GETCHECK, 0, 0) == BST_CHECKED;
    (void)b;
    isp_cfg_save(&g_cfg);
}

void ui_refresh_ports(void)
{
    serial_port_item_t items[32];
    int n;
    if (!g_hPort) return;
    char prevPort[32] = {0};
    {
        int idx = (int)SendMessageA(g_hPort, CB_GETCURSEL, 0, 0);
        if (idx >= 0) {
            char line[160];
            SendMessageA(g_hPort, CB_GETLBTEXT, (WPARAM)idx, (LPARAM)line);
            const char *p = strstr(line, "COM");
            if (p) {
                int k = 0;
                while (p[k] && k < 31 &&
                       ((p[k] >= '0' && p[k] <= '9') || p[k] == 'C' || p[k] == 'O' || p[k] == 'M')) {
                    prevPort[k] = p[k];
                    k++;
                }
                prevPort[k] = 0;
            }
        }
    }
    SendMessageA(g_hPort, CB_RESETCONTENT, 0, 0);
    n = serial_list_ports_info(items, 32);

    int sel = -1;
    int ch340_idx = -1;
    int prev_idx = -1;

    for (int i = 0; i < n; i++) {
        char line[200];
        if (items[i].desc[0])
            snprintf(line, sizeof(line), "%s - %s", items[i].port, items[i].desc);
        else
            snprintf(line, sizeof(line), "%s", items[i].port);
        SendMessageA(g_hPort, CB_ADDSTRING, 0, (LPARAM)line);
        SendMessageA(g_hPort, CB_SETITEMDATA, (WPARAM)i, (LPARAM)i);

        /* 优先 CH340 / WCH 系列 */
        if (ch340_idx < 0) {
            const char *d = items[i].desc;
            if (d && (str_contains_ci(d, "CH340") || str_contains_ci(d, "CH341") ||
                      str_contains_ci(d, "CH34") || str_contains_ci(d, "WCH")))
                ch340_idx = i;
        }
        if (prevPort[0] && strcmp(items[i].port, prevPort) == 0)
            prev_idx = i;
        /* 配置中记住的串口 */
        if (g_cfg.port[0] && strcmp(items[i].port, g_cfg.port) == 0 && prev_idx < 0)
            prev_idx = i;
    }
    SendMessageA(g_hPort, CB_SETDROPPEDWIDTH, 320, 0);

    /* 无历史端口时优先 CH340（控制板 USB 串口） */
    if (prev_idx < 0 && ch340_idx >= 0) sel = ch340_idx;
    else if (prev_idx >= 0) sel = prev_idx;
    else if (n > 0) sel = 0;
    if (n > 0 && sel >= 0) SendMessageA(g_hPort, CB_SETCURSEL, sel, 0);

    /* 操作日志：列出扫描到的串口，并说明当前选择 */
    if (n <= 0) {
        ui_log_append(LOG_WARN, "扫描串口: 未发现可用 COM 口");
        return;
    }
    {
        char head[80];
        snprintf(head, sizeof(head), "扫描串口: 共 %d 个", n);
        ui_log_append(LOG_INFO, head);
    }
    for (int i = 0; i < n; i++) {
        char line[200];
        if (items[i].desc[0])
            snprintf(line, sizeof(line), "  %s - %s", items[i].port, items[i].desc);
        else
            snprintf(line, sizeof(line), "  %s", items[i].port);
        if (i == sel && ch340_idx == i)
            snprintf(line + strlen(line), sizeof(line) - strlen(line), "  [已选中 CH340]");
        else if (i == sel)
            snprintf(line + strlen(line), sizeof(line) - strlen(line), "  [已选中]");
        else if (i == ch340_idx)
            snprintf(line + strlen(line), sizeof(line) - strlen(line), "  [CH340]");
        ui_log_append(LOG_INFO, line);
    }
    if (sel >= 0) {
        char msg[200];
        if (items[sel].desc[0])
            snprintf(msg, sizeof(msg), "当前串口: %s - %s%s",
                     items[sel].port, items[sel].desc,
                     (sel == ch340_idx) ? " (优先 CH340)" : "");
        else
            snprintf(msg, sizeof(msg), "当前串口: %s", items[sel].port);
        ui_log_append(LOG_SUCCESS, msg);
        cfg_save_from_ui();
    }
}

/* ── 控件辅助 ── */

static void set_busy(int busy)
{
    InterlockedExchange(&g_busy, busy);
    EnableWindow(GetDlgItem(g_main, IDC_ERASE), !busy);
    EnableWindow(GetDlgItem(g_main, IDC_DOWNLOAD), !busy);
    EnableWindow(GetDlgItem(g_main, IDC_VERIFY), !busy);
    EnableWindow(GetDlgItem(g_main, IDC_RUN), !busy);
    EnableWindow(GetDlgItem(g_main, IDC_SERIAL_TOGGLE), !busy);
    EnableWindow(GetDlgItem(g_main, IDC_BROWSE), !busy);
    EnableWindow(GetDlgItem(g_main, IDC_DBG_PANEL), !busy);
    EnableWindow(GetDlgItem(g_main, IDC_OPTBYTES), !busy);
    EnableWindow(GetDlgItem(g_main, IDC_READFLASH), !busy);
    EnableWindow(g_hCancel, busy ? TRUE : FALSE);
    ui_update_serial_ui();
}

static HWND mk(HWND parent, const char *cls, const char *text, DWORD style,
               DWORD ex, int x, int y, int w, int ht, int id)
{
    HWND h = CreateWindowExA(ex, cls, text, WS_CHILD | WS_VISIBLE | style,
                             x, y, w, ht, parent, (HMENU)(INT_PTR)id,
                             GetModuleHandle(NULL), NULL);
    if (h && g_font) SendMessage(h, WM_SETFONT, (WPARAM)g_font, TRUE);
    return h;
}

/* MSVC/MinGW C 下不能用 lambda — 用普通回调 */
static BOOL CALLBACK enum_font_proc(HWND h, LPARAM p)
{
    /* 日志控件使用等宽字体，不要被全局 UI 字体覆盖 */
    if (h == g_hLog) return TRUE;
    SendMessage(h, WM_SETFONT, (WPARAM)p, TRUE);
    return TRUE;
}

static void apply_font_all(HWND root)
{
    if (g_font && root)
        EnumChildWindows(root, enum_font_proc, (LPARAM)g_font);
}

static void get_combo_text(HWND h, char *buf, int buflen)
{
    int idx = (int)SendMessageA(h, CB_GETCURSEL, 0, 0);
    if (idx < 0) { buf[0] = 0; return; }
    /* 串口下拉显示为 "COM13 - 驱动名"，打开时只取 COMxx */
    char line[160];
    SendMessageA(h, CB_GETLBTEXT, (WPARAM)idx, (LPARAM)line);
    if (h == g_hPort) {
        const char *p = strstr(line, "COM");
        if (p) {
            int k = 0;
            while (p[k] && k < buflen - 1 &&
                   ((p[k] >= '0' && p[k] <= '9') || p[k] == 'C' || p[k] == 'O' || p[k] == 'M')) {
                buf[k] = p[k];
                k++;
            }
            buf[k] = 0;
            return;
        }
    }
    snprintf(buf, buflen, "%s", line);
}

static int get_selected_baud(void)
{
    if (g_hCustomChk &&
        (int)SendMessageA(g_hCustomChk, BM_GETCHECK, 0, 0) == BST_CHECKED) {
        char b[32];
        GetWindowTextA(g_hCustomEdit, b, sizeof(b));
        int v = atoi(b);
        if (v > 0) return v;
    }
    int idx = (int)SendMessageA(g_hBaud, CB_GETCURSEL, 0, 0);
    if (idx < 0 || idx >= (int)(sizeof(BAUDS) / sizeof(BAUDS[0]))) return 115200;
    return BAUDS[idx];
}

static int get_mode(void)
{
    return (int)SendMessageA(g_hMode, CB_GETCURSEL, 0, 0);
}

static int get_delay(void)
{
    char b[32];
    GetWindowTextA(g_hDelay, b, sizeof(b));
    int v = atoi(b);
    if (v < 1) v = 1;
    if (v > 1000) v = 1000;
    return v;
}

static void sync_bl_cfg(void)
{
    logic_set_mode(get_mode(), get_delay());
}

static void progress_cb(int pct, const char *st)
{
    ui_set_progress(pct, st);
}

/* ── 布局 ── */

/* 紧凑布局高度（保证默认窗口客户区能完整放下左栏） */
#define H_SER   104
#define H_BL    108
#define H_CHIP  104
#define H_FW    68
#define H_ACT   164
#define ROW_BTN 32

static int left_content_height(void)
{
    return MARGIN + H_SER + 4 + H_BL + 4 + H_CHIP + 4 + H_FW + 4 + H_ACT + MARGIN;
}

static void ensure_window_fits_client(HWND hwnd)
{
    int needW = LEFT_W + MARGIN + 460; /* 右栏日志宽度 */
    int needH = left_content_height() + 24;
    if (needH < 620) needH = 620;

    DWORD style = (DWORD)GetWindowLongPtrA(hwnd, GWL_STYLE);
    DWORD ex = (DWORD)GetWindowLongPtrA(hwnd, GWL_EXSTYLE);
    RECT rc = { 0, 0, needW, needH };
    AdjustWindowRectEx(&rc, style, TRUE, ex);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    SetWindowPos(hwnd, NULL, 0, 0, w, h,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

static void relayout(int cx, int cy)
{
    if (cx < 700) cx = 700;
    int minH = left_content_height();
    if (cy < minH) cy = minH;
    g_last_w = cx;
    g_last_h = cy;

    int lw = LEFT_W;
    int x = MARGIN;
    int y = MARGIN;
    int iw = lw - 2 * MARGIN;
    int rx = lw + MARGIN;
    int rw = cx - rx - MARGIN;
    if (rw < 220) rw = 220;

    HWND lb;

    /* ── 串口设置 ── */
    MoveWindow(g_hGrpSerial, 4, y - 4, lw, H_SER, TRUE);
    {
        int cy1 = y + 16;
        lb = GetDlgItem(g_main, 3001); if (lb) MoveWindow(lb, x, cy1 + 3, 40, 18, TRUE);
        /* 收起时也尽量显示 COMx - 名称，宽度给足 */
        MoveWindow(g_hPort, x + 42, cy1, 220, 240, TRUE);
        MoveWindow(GetDlgItem(g_main, IDC_REFRESH), x + 268, cy1 - 2, 52, 24, TRUE);
        SendMessageA(g_hPort, CB_SETDROPPEDWIDTH, 320, 0);
        lb = GetDlgItem(g_main, 3002); if (lb) MoveWindow(lb, x, cy1 + 30, 48, 18, TRUE);
        MoveWindow(g_hBaud, x + 50, cy1 + 27, 90, 200, TRUE);
        MoveWindow(GetDlgItem(g_main, IDC_SERIAL_TOGGLE), x + 148, cy1 + 25, 96, 24, TRUE);
        MoveWindow(g_hCustomChk, x, cy1 + 54, 70, 20, TRUE);
        MoveWindow(g_hCustomEdit, x + 74, cy1 + 52, 100, 22, TRUE);
    }
    y += H_SER + 4;

    /* ── Bootloader ── */
    MoveWindow(g_hGrpBl, 4, y - 4, lw, H_BL, TRUE);
    {
        int by = y + 16;
        lb = GetDlgItem(g_main, 3003); if (lb) MoveWindow(lb, x, by + 3, 40, 18, TRUE);
        MoveWindow(g_hMode, x + 42, by, iw - 42, 24, TRUE);
        lb = GetDlgItem(g_main, 3004); if (lb) MoveWindow(lb, x, by + 30, 88, 18, TRUE);
        MoveWindow(g_hDelay, x + 92, by + 28, 60, 22, TRUE);
        /* DTR/RTS 状态合并到按钮文字，不再单独一排指示灯 */
        MoveWindow(g_hDtrBtn, x, by + 54, 140, 28, TRUE);
        MoveWindow(g_hRtsBtn, x + 148, by + 54, 140, 28, TRUE);
        if (g_hDtrState) ShowWindow(g_hDtrState, SW_HIDE);
        if (g_hRtsState) ShowWindow(g_hRtsState, SW_HIDE);
        {
            HWND lb;
            lb = GetDlgItem(g_main, 3005); if (lb) ShowWindow(lb, SW_HIDE);
            lb = GetDlgItem(g_main, 3006); if (lb) ShowWindow(lb, SW_HIDE);
        }
    }
    y += H_BL + 4;

    /* ── 芯片信息 ── */
    MoveWindow(g_hGrpChip, 4, y - 4, lw, H_CHIP, TRUE);
    {
        int gy = y + 14;
        int col2 = x + 155;
        lb = GetDlgItem(g_main, 3101); if (lb) MoveWindow(lb, x, gy, 36, 18, TRUE);
        MoveWindow(g_hPid, x + 38, gy, 108, 18, TRUE);
        lb = GetDlgItem(g_main, 3102); if (lb) MoveWindow(lb, col2, gy, 36, 18, TRUE);
        MoveWindow(g_hName, col2 + 38, gy, 130, 18, TRUE);
        gy += 22;
        lb = GetDlgItem(g_main, 3103); if (lb) MoveWindow(lb, x, gy, 44, 18, TRUE);
        MoveWindow(g_hFlash, x + 46, gy, 100, 18, TRUE);
        lb = GetDlgItem(g_main, 3104); if (lb) MoveWindow(lb, col2, gy, 36, 18, TRUE);
        MoveWindow(g_hBlVer, col2 + 38, gy, 80, 18, TRUE);
        gy += 22;
        lb = GetDlgItem(g_main, 3105); if (lb) MoveWindow(lb, x, gy, 44, 18, TRUE);
        MoveWindow(g_hProt, x + 46, gy, iw - 46, 18, TRUE);
    }
    y += H_CHIP + 4;

    /* ── 固件 ── */
    MoveWindow(g_hGrpFw, 4, y - 4, lw, H_FW, TRUE);
    {
        MoveWindow(g_hFile, x, y + 16, iw - 70, 22, TRUE);
        MoveWindow(GetDlgItem(g_main, IDC_BROWSE), x + iw - 66, y + 15, 66, 24, TRUE);
        MoveWindow(g_hFileInfo, x, y + 42, iw, 18, TRUE);
    }
    y += H_FW + 4;

    /* ── 操作 ── */
    MoveWindow(g_hGrpAct, 4, y - 4, lw, H_ACT, TRUE);
    {
        int by = y + 16;
        int bw = (iw - 8) / 2;
        MoveWindow(GetDlgItem(g_main, IDC_ERASE), x, by, bw, 26, TRUE);
        MoveWindow(GetDlgItem(g_main, IDC_DOWNLOAD), x + bw + 8, by, bw, 26, TRUE);
        MoveWindow(GetDlgItem(g_main, IDC_VERIFY), x, by + ROW_BTN, bw, 26, TRUE);
        MoveWindow(GetDlgItem(g_main, IDC_RUN), x + bw + 8, by + ROW_BTN, bw, 26, TRUE);
        MoveWindow(GetDlgItem(g_main, IDC_OPTBYTES), x, by + ROW_BTN * 2, bw, 26, TRUE);
        MoveWindow(GetDlgItem(g_main, IDC_READFLASH), x + bw + 8, by + ROW_BTN * 2, bw, 26, TRUE);
        MoveWindow(g_hVerifyDl, x, by + ROW_BTN * 3, 110, 20, TRUE);
        MoveWindow(g_hRunAfter, x + 118, by + ROW_BTN * 3, 110, 20, TRUE);
        MoveWindow(GetDlgItem(g_main, IDC_DBG_PANEL), x + 240, by + ROW_BTN * 3, 52, 22, TRUE);
        MoveWindow(g_hCancel, x, by + ROW_BTN * 4, bw, 26, TRUE);
        MoveWindow(GetDlgItem(g_main, IDC_STATUS), x + bw + 8, by + ROW_BTN * 4 + 2, bw, 20, TRUE);
    }
    y += H_ACT + 4;
    (void)y;

    /* ── 右侧日志 ── */
    int ly = MARGIN - 4;
    int progY = cy - MARGIN - 28;
    if (progY < ly + 80) progY = ly + 80;
    int lh = progY - ly - 28;
    if (lh < 80) lh = 80;
    MoveWindow(g_hGrpLog, rx - 4, ly, rw + 8, lh + 36, TRUE);
    MoveWindow(g_hLog, rx, ly + 22, rw, lh, TRUE);
    MoveWindow(g_hProg, rx, ly + 26 + lh, rw, 18, TRUE);

    apply_font_all(g_main);
}

static HMENU build_menu(void)
{
    HMENU bar = CreateMenu();
    HMENU mFile = CreatePopupMenu();
    HMENU mTool = CreatePopupMenu();
    HMENU mHelp = CreatePopupMenu();

    AppendMenuA(mFile, MF_STRING, IDM_REFRESH_PORTS, "刷新串口列表");
    AppendMenuA(mFile, MF_STRING, IDM_EXPORT_LOG, "导出日志...");
    AppendMenuA(mFile, MF_SEPARATOR, 0, NULL);
    AppendMenuA(mFile, MF_STRING, IDM_EXIT, "退出");

    AppendMenuA(mTool, MF_STRING, IDM_DEBUG_PANEL, "调试面板\tF9");
    AppendMenuA(mTool, MF_STRING, IDM_HEX_LOG, "全局日志\tF10");

    AppendMenuA(mHelp, MF_STRING, IDM_LANG_CN, L("中文", "Chinese"));
    AppendMenuA(mHelp, MF_STRING, IDM_LANG_EN, L("English", "English"));
    AppendMenuA(mHelp, MF_SEPARATOR, 0, NULL);
    AppendMenuA(mHelp, MF_STRING, IDM_ABOUT, L("关于", "About"));
    AppendMenuA(mHelp, MF_STRING, IDM_ACCEL_HINT, L("快捷键说明", "Hotkeys"));

    AppendMenuA(bar, MF_POPUP, (UINT_PTR)mFile, "文件");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)mTool, "工具");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)mHelp, "帮助");
    return bar;
}

static HACCEL build_accel(void)
{
    ACCEL acc[] = {
        { FVIRTKEY, VK_F5,  IDM_ACCEL_DOWNLOAD },
        { FVIRTKEY, VK_F6,  IDM_ACCEL_ERASE },
        { FVIRTKEY, VK_F7,  IDM_ACCEL_VERIFY },
        { FVIRTKEY, VK_F8,  IDM_ACCEL_RUN },
        { FVIRTKEY, VK_F9,  IDM_ACCEL_DEBUG },
        { FVIRTKEY, VK_F10, IDM_ACCEL_HEXLOG },
        { FVIRTKEY, VK_ESCAPE, IDM_ACCEL_CANCEL },
        { FCONTROL | FVIRTKEY, 'E', IDM_EXPORT_LOG },
    };
    return CreateAcceleratorTableA(acc, (int)(sizeof(acc) / sizeof(acc[0])));
}

static HWND mk_label(HWND parent, const char *text, int x, int y, int w, int h, int id)
{
    return mk(parent, "STATIC", text, SS_LEFT, 0, x, y, w, h, id);
}

static void layout_create(HWND hwnd)
{
    int x = MARGIN, y = MARGIN, iw = LEFT_W - 2 * MARGIN;

    g_font = CreateFontA(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                         "Microsoft YaHei UI");
    if (!g_font)
        g_font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    /* 日志用等宽字体，中文用宋体回退，避免 YaHei 在 EDIT 上发黑 */
    g_log_font = CreateFontA(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN,
                             "Consolas");
    if (!g_log_font)
        g_log_font = (HFONT)GetStockObject(ANSI_FIXED_FONT);
    g_brGreen = CreateSolidBrush(RGB(40, 160, 80));
    g_brRed   = CreateSolidBrush(RGB(200, 60, 60));
    g_brGray  = CreateSolidBrush(RGB(160, 160, 160));
    g_brWhite = CreateSolidBrush(RGB(255, 255, 255));
    g_brLightGreen = CreateSolidBrush(RGB(220, 245, 225));
    g_brLightRed = CreateSolidBrush(RGB(255, 230, 230));

    g_hGrpSerial = mk(hwnd, "BUTTON", "串口设置", BS_GROUPBOX, 0, 4, y - 4, LEFT_W, H_SER, 0);
    mk_label(hwnd, "串口:", x, y + 16, 40, 18, 3001);
    g_hPort = mk(hwnd, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL, 0,
                 x + 42, y + 14, 220, 240, IDC_PORT_COMBO);
    mk(hwnd, "BUTTON", "刷新", BS_PUSHBUTTON, 0, x + 268, y + 14, 52, 24, IDC_REFRESH);
    if (g_hPort) SendMessageA(g_hPort, CB_SETDROPPEDWIDTH, 320, 0);
    mk_label(hwnd, "波特率:", x, y + 44, 48, 18, 3002);
    g_hBaud = mk(hwnd, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL, 0,
                 x + 50, y + 41, 90, 240, IDC_BAUD_COMBO);
    g_hSerialBtn = mk(hwnd, "BUTTON", "打开串口", BS_PUSHBUTTON, 0,
                      x + 148, y + 39, 96, 24, IDC_SERIAL_TOGGLE);
    g_hCustomChk = mk(hwnd, "BUTTON", "自定义", BS_AUTOCHECKBOX, 0,
                      x, y + 70, 70, 20, IDC_CUSTOM_BAUD_CHK);
    g_hCustomEdit = mk(hwnd, "EDIT", "", ES_AUTOHSCROLL | ES_NUMBER, 0,
                       x + 74, y + 68, 100, 22, IDC_CUSTOM_BAUD_EDIT);

    y += H_SER + 4;
    g_hGrpBl = mk(hwnd, "BUTTON", "Bootloader 控制", BS_GROUPBOX, 0, 4, y - 4, LEFT_W, H_BL, 0);
    mk_label(hwnd, "模式:", x, y + 16, 40, 18, 3003);
    g_hMode = mk(hwnd, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL, 0,
                 x + 42, y + 14, iw - 42, 24, IDC_MODE_COMBO);
    mk_label(hwnd, "步骤延时(ms):", x, y + 44, 88, 18, 3004);
    g_hDelay = mk(hwnd, "EDIT", "100", ES_AUTOHSCROLL | ES_NUMBER, 0,
                  x + 92, y + 42, 60, 22, IDC_STEP_DELAY);
    g_hDtrBtn = mk(hwnd, "BUTTON", "DTR: LOW", BS_AUTOCHECKBOX | BS_PUSHLIKE, 0,
                   x, y + 70, 140, 28, IDC_DTR_TOGGLE);
    g_hRtsBtn = mk(hwnd, "BUTTON", "RTS: LOW", BS_AUTOCHECKBOX | BS_PUSHLIKE, 0,
                   x + 148, y + 70, 140, 28, IDC_RTS_TOGGLE);
    /* 状态灯控件保留句柄但隐藏，避免与按钮重复 */
    mk_label(hwnd, "DTR:", x, y + 100, 40, 20, 3005);
    g_hDtrState = mk(hwnd, "STATIC", "LOW", SS_CENTER | SS_CENTERIMAGE, 0,
                     x + 42, y + 98, 88, 22, IDC_DTR_STATE);
    mk_label(hwnd, "RTS:", x + 148, y + 100, 40, 20, 3006);
    g_hRtsState = mk(hwnd, "STATIC", "LOW", SS_CENTER | SS_CENTERIMAGE, 0,
                     x + 190, y + 98, 88, 22, IDC_RTS_STATE);
    if (g_hDtrState) ShowWindow(g_hDtrState, SW_HIDE);
    if (g_hRtsState) ShowWindow(g_hRtsState, SW_HIDE);
    {
        HWND lb;
        lb = GetDlgItem(hwnd, 3005); if (lb) ShowWindow(lb, SW_HIDE);
        lb = GetDlgItem(hwnd, 3006); if (lb) ShowWindow(lb, SW_HIDE);
    }

    y += H_BL + 4;
    g_hGrpChip = mk(hwnd, "BUTTON", "芯片信息", BS_GROUPBOX, 0, 4, y - 4, LEFT_W, H_CHIP, 0);
    mk_label(hwnd, "PID:", x, y + 14, 36, 18, 3101);
    g_hPid = mk_label(hwnd, "---", x + 38, y + 14, 108, 18, IDC_PID_VAL);
    mk_label(hwnd, "型号:", x + 155, y + 14, 36, 18, 3102);
    g_hName = mk_label(hwnd, "---", x + 193, y + 14, 130, 18, IDC_NAME_VAL);
    mk_label(hwnd, "Flash:", x, y + 36, 44, 18, 3103);
    g_hFlash = mk_label(hwnd, "---", x + 46, y + 36, 100, 18, IDC_FLASH_VAL);
    mk_label(hwnd, "BL:", x + 155, y + 36, 36, 18, 3104);
    g_hBlVer = mk_label(hwnd, "---", x + 193, y + 36, 80, 18, IDC_BLVER_VAL);
    mk_label(hwnd, "保护:", x, y + 58, 44, 18, 3105);
    g_hProt = mk_label(hwnd, "---", x + 46, y + 58, iw - 46, 18, IDC_PROT_VAL);

    y += H_CHIP + 4;
    g_hGrpFw = mk(hwnd, "BUTTON", "固件文件", BS_GROUPBOX, 0, 4, y - 4, LEFT_W, H_FW, 0);
    g_hFile = mk(hwnd, "EDIT", "", ES_AUTOHSCROLL | ES_READONLY, 0,
                 x, y + 16, iw - 70, 22, IDC_FILE_EDIT);
    mk(hwnd, "BUTTON", "浏览...", BS_PUSHBUTTON, 0,
       x + iw - 66, y + 15, 66, 24, IDC_BROWSE);
    g_hFileInfo = mk_label(hwnd, "", x, y + 42, iw, 18, IDC_FILE_INFO);

    y += H_FW + 4;
    g_hGrpAct = mk(hwnd, "BUTTON", "操作", BS_GROUPBOX, 0, 4, y - 4, LEFT_W, H_ACT, 0);
    {
        int by = y + 16;
        int bw = (iw - 8) / 2;
        mk(hwnd, "BUTTON", "擦除芯片", BS_PUSHBUTTON, 0, x, by, bw, 26, IDC_ERASE);
        mk(hwnd, "BUTTON", "擦除并下载", BS_PUSHBUTTON, 0, x + bw + 8, by, bw, 26, IDC_DOWNLOAD);
        mk(hwnd, "BUTTON", "校验数据", BS_PUSHBUTTON, 0, x, by + ROW_BTN, bw, 26, IDC_VERIFY);
        mk(hwnd, "BUTTON", "退出并运行", BS_PUSHBUTTON, 0, x + bw + 8, by + ROW_BTN, bw, 26, IDC_RUN);
        mk(hwnd, "BUTTON", "选项字节", BS_PUSHBUTTON, 0, x, by + ROW_BTN * 2, bw, 26, IDC_OPTBYTES);
        mk(hwnd, "BUTTON", "读取 Flash", BS_PUSHBUTTON, 0, x + bw + 8, by + ROW_BTN * 2, bw, 26, IDC_READFLASH);
        g_hVerifyDl = mk(hwnd, "BUTTON", "下载时校验", BS_AUTOCHECKBOX, 0,
                         x, by + ROW_BTN * 3, 110, 20, IDC_VERIFY_DL);
        g_hRunAfter = mk(hwnd, "BUTTON", "下载后运行", BS_AUTOCHECKBOX, 0,
                         x + 118, by + ROW_BTN * 3, 110, 20, IDC_RUN_AFTER);
        /* 默认勾选（不依赖 ini） */
        if (g_hVerifyDl) SendMessageA(g_hVerifyDl, BM_SETCHECK, BST_CHECKED, 0);
        if (g_hRunAfter) SendMessageA(g_hRunAfter, BM_SETCHECK, BST_CHECKED, 0);
        mk(hwnd, "BUTTON", "调试", BS_PUSHBUTTON, 0, x + 240, by + ROW_BTN * 3, 52, 22, IDC_DBG_PANEL);
        g_hCancel = mk(hwnd, "BUTTON", "取消", BS_PUSHBUTTON, 0,
                       x, by + ROW_BTN * 4, bw, 26, IDC_CANCEL);
        g_hStatus = mk_label(hwnd, "就绪", x + bw + 8, by + ROW_BTN * 4 + 2, bw, 20, IDC_STATUS);
    }

    int rx = LEFT_W + MARGIN;
    int rw = 900 - rx - MARGIN;
    if (rw < 220) rw = 220;
    g_hGrpLog = mk(hwnd, "BUTTON", "操作日志  (协议/读写明细 → F10 全局日志)",
                   BS_GROUPBOX, 0, rx - 4, MARGIN - 4, rw + 8, 500, 0);
    g_hLog = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "",
                             WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP |
                                 LBS_NOINTEGRALHEIGHT | LBS_HASSTRINGS | LBS_USETABSTOPS,
                             rx, MARGIN + 20, rw, 400, hwnd,
                             (HMENU)(INT_PTR)IDC_LOG,
                             GetModuleHandle(NULL), NULL);
    if (g_hLog) {
        SendMessageA(g_hLog, WM_SETFONT, (WPARAM)g_font, TRUE);
        SendMessageA(g_hLog, LB_SETCOUNT, 0, 0);
        int tab = 72;
        SendMessageA(g_hLog, LB_SETTABSTOPS, 1, (LPARAM)&tab);
    }g_hProg = CreateWindowExA(0, PROGRESS_CLASSA, "", WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
                              rx, 430, rw, 20, hwnd,
                              (HMENU)(INT_PTR)IDC_PROGRESS,
                              GetModuleHandle(NULL), NULL);
    if (g_hProg) {
        SendMessage(g_hProg, PBM_SETRANGE32, 0, 100);
        SendMessage(g_hProg, PBM_SETPOS, 0, 0);
        SendMessage(g_hProg, PBM_SETBARCOLOR, 0, (LPARAM)RGB(40, 140, 220));
        SendMessage(g_hProg, PBM_SETBKCOLOR, 0, (LPARAM)RGB(230, 230, 230));
    }

    for (int i = 0; i < (int)(sizeof(BAUDS) / sizeof(BAUDS[0])); i++) {
        char b[16];
        snprintf(b, sizeof(b), "%d", BAUDS[i]);
        SendMessageA(g_hBaud, CB_ADDSTRING, 0, (LPARAM)b);
    }
    SendMessageA(g_hBaud, CB_SETCURSEL, 0, 0);
    for (int m = 0; m <= 16; m++)
        SendMessageA(g_hMode, CB_ADDSTRING, 0, (LPARAM)bl_mode_name(m));

    /* 配置记忆：启动时恢复串口/波特率/模式/延时/固件/选项 */
    isp_cfg_load(&g_cfg);
    if (g_cfg.baud > 0 && g_hBaud) {
        char bb[16];
        snprintf(bb, sizeof(bb), "%d", g_cfg.baud);
        int bi = (int)SendMessageA(g_hBaud, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)bb);
        if (bi >= 0) SendMessageA(g_hBaud, CB_SETCURSEL, bi, 0);
        else {
            SetWindowTextA(g_hBaud, bb);
        }
    }
    if (g_cfg.mode >= 0 && g_cfg.mode <= 16 && g_hMode)
        SendMessageA(g_hMode, CB_SETCURSEL, g_cfg.mode, 0);
    else
        SendMessageA(g_hMode, CB_SETCURSEL, 1, 0);
    if (g_hDelay && g_cfg.delay_ms > 0) {
        char d[16];
        snprintf(d, sizeof(d), "%d", g_cfg.delay_ms);
        SetWindowTextA(g_hDelay, d);
    } else if (g_hDelay) {
        SetWindowTextA(g_hDelay, "100");
    }
    if (g_hVerifyDl)
        SendMessageA(g_hVerifyDl, BM_SETCHECK, BST_CHECKED, 0);
    if (g_hRunAfter)
        SendMessageA(g_hRunAfter, BM_SETCHECK, BST_CHECKED, 0);
    g_cfg.verify_dl = 1;
    g_cfg.run_after = 1;
    if (g_hFile && g_cfg.file[0]) {
        SetWindowTextA(g_hFile, g_cfg.file);
        if (logic_load_firmware(g_cfg.file, 0x08000000u, NULL, 0))
            SetWindowTextA(g_hFileInfo, "固件已加载(上次)");
    }

    ui_refresh_ports();
    ui_update_serial_ui();
    ui_update_chip_info();
    ensure_window_fits_client(hwnd);
    {
        RECT crc;
        if (GetClientRect(hwnd, &crc))
            relayout(crc.right - crc.left, crc.bottom - crc.top);
    }
    if (g_hVerifyDl) SendMessageA(g_hVerifyDl, BM_SETCHECK, BST_CHECKED, 0);
    if (g_hRunAfter) SendMessageA(g_hRunAfter, BM_SETCHECK, BST_CHECKED, 0);
    InvalidateRect(hwnd, NULL, TRUE);
    if (g_hVerifyDl) SendMessageA(g_hVerifyDl, BM_SETCHECK, BST_CHECKED, 0);
    if (g_hRunAfter) SendMessageA(g_hRunAfter, BM_SETCHECK, BST_CHECKED, 0);
}

static void do_serial_toggle(void)
{
    if (logic_serial_is_open()) {
        logic_close_serial();
        ui_update_serial_ui();
        ui_update_chip_info();
        ui_set_status("串口已关闭");
        return;
    }
    char port[32];
    get_combo_text(g_hPort, port, sizeof(port));
    if (!port[0]) {
        ui_log_append_now(LOG_ERROR, "请先选择串口");
        return;
    }
    int baud = get_selected_baud();
    if (logic_open_serial(port, baud)) {
        ui_update_serial_ui();
        ui_set_status("串口已打开");
        cfg_save_from_ui();
    }
}

static void do_browse(void)
{
    char file[MAX_PATH] = {0};
    OPENFILENAMEA ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_main;
    ofn.lpstrFilter =
        "固件文件 (*.hex;*.bin)\0*.hex;*.bin\0"
        "Intel HEX (*.hex)\0*.hex\0"
        "BIN (*.bin)\0*.bin\0所有文件\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameA(&ofn)) return;

    SetWindowTextA(g_hFile, file);
    char err[256] = {0};
    uint32_t bin_start = 0x08000000u;
    const char *dot = strrchr(file, '.');
    if (dot && _stricmp(dot, ".bin") == 0) {
        /* 对话框失败时回退默认地址，避免父窗口被禁用后卡死 */
        if (!ui_input_hex(g_main, "BIN 起始地址",
                          "请输入 BIN 烧录起始地址 (十六进制):",
                          g_bin_addr_text, &bin_start)) {
            bin_start = 0x08000000u;
            ui_log_append_now(LOG_WARN, "未确认起始地址，使用默认 0x08000000");
        }
        snprintf(g_bin_addr_text, sizeof(g_bin_addr_text), "0x%08X",
                 (unsigned)bin_start);
        {
            char m[64];
            snprintf(m, sizeof(m), "BIN 起始地址: %s", g_bin_addr_text);
            ui_log_append_now(LOG_INFO, m);
        }
    }
    ui_set_status("正在加载固件...");
    if (logic_load_firmware(file, bin_start, err, sizeof(err))) {
        SetWindowTextA(g_hFileInfo, "固件已加载");
        ui_log_append_now(LOG_SUCCESS, "固件路径已选择");
        ui_set_status("固件已加载");
        ui_update_chip_info();
        cfg_save_from_ui();
    } else {
        SetWindowTextA(g_hFileInfo, "加载失败");
        ui_log_append_now(LOG_ERROR, err[0] ? err : "固件加载失败");
        ui_set_status("固件加载失败");
    }
}

static void do_export_log(void)
{
    char file[MAX_PATH];
    char stamp[32], defname[80], hdr[256];
    isp_log_stamp(stamp, sizeof(stamp));
#ifndef ISP_VERSION_STR
#define ISP_VERSION_STR "0.6"
#endif
    snprintf(defname, sizeof(defname), "ISP_Log_v%s_%s.txt", ISP_VERSION_STR, stamp);
    snprintf(file, sizeof(file), "%s", defname);

    OPENFILENAMEA ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_main;
    ofn.lpstrFilter = "文本文件 (*.txt)\0*.txt\0所有文件\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "txt";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameA(&ofn)) return;

    if (!g_hLog) return;
    FILE *fp = fopen(file, "wb");
    if (!fp) {
        ui_log_append_now(LOG_ERROR, "无法写入日志文件");
        return;
    }
    fputs("\xEF\xBB\xBF", fp);
    isp_log_header(hdr, sizeof(hdr));
    fputs(hdr, fp);

    int cnt = (int)SendMessageA(g_hLog, LB_GETCOUNT, 0, 0);
    for (int i = 0; i < cnt; i++) {
        char buf[1400];
        buf[0] = 0;
        SendMessageA(g_hLog, LB_GETTEXT, (WPARAM)i, (LPARAM)buf);
        fputs(buf, fp);
        fputc('\n', fp);
    }
    fclose(fp);
    ui_log_append_now(LOG_SUCCESS, "日志已导出");
}
static void do_read_flash(void)
{
    char file[MAX_PATH] = "flash_backup.hex";
    OPENFILENAMEA ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_main;
    ofn.lpstrFilter = "Intel HEX (*.hex)\0*.hex\0所有文件\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "hex";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameA(&ofn)) return;
    snprintf(g_pending_path, sizeof(g_pending_path), "%s", file);
    start_op(5);
}

static DWORD WINAPI worker_proc(LPVOID param)
{
    int op = (int)(INT_PTR)param;
    InterlockedExchange(&g_cancel_flag, 0);
    sync_bl_cfg();
    switch (op) {
    case 1: logic_erase(); break;
    case 2: {
        download_opts_t o;
        o.verify_each_page = 1;
        o.run_after = 1;
        if (g_hVerifyDl)
            o.verify_each_page = (int)SendMessageA(g_hVerifyDl, BM_GETCHECK, 0, 0) == BST_CHECKED;
        if (g_hRunAfter)
            o.run_after = (int)SendMessageA(g_hRunAfter, BM_GETCHECK, 0, 0) == BST_CHECKED;
        logic_download(&o);
        break;
    }
    case 3: logic_verify(); break;
    case 4: logic_exit_and_run(); break;
    case 5: {
        /* 读取 Flash — path 在 g_pending_path */
        extern char g_pending_path[MAX_PATH];
        if (g_pending_path[0])
            logic_read_flash_to_hex(g_pending_path);
        break;
    }
    default: break;
    }
    ui_update_chip_info();
    ui_update_dtr_rts();
    PostMessage(g_main, WM_APP_OP_DONE, 0, 0);
    return 0;
}

char g_pending_path[MAX_PATH];

static void start_op(int op)
{
    if (InterlockedCompareExchange(&g_busy, 1, 0) != 0) {
        ui_log_append_now(LOG_WARN, "已有操作进行中");
        return;
    }
    set_busy(1);
    EnableWindow(g_hCancel, TRUE);
    InterlockedExchange(&g_cancel_flag, 0);
    HANDLE th = CreateThread(NULL, 0, worker_proc, (LPVOID)(INT_PTR)op, 0, NULL);
    if (th) CloseHandle(th);
}

static LRESULT on_color_static(HWND h, HDC hdc)
{
    /* DTR/RTS 状态灯 */
        if (h == g_hDtrBtn || h == g_hRtsBtn) {
        char buf[32];
        GetWindowTextA(h, buf, sizeof(buf));
        int high = strstr(buf, "HIGH") != NULL;
        SetBkMode(hdc, OPAQUE);
        if (high) {
            SetBkColor(hdc, RGB(220, 245, 225));
            SetTextColor(hdc, RGB(20, 120, 40));
            return (LRESULT)g_brLightGreen;
        }
        SetBkColor(hdc, RGB(255, 230, 230));
        SetTextColor(hdc, RGB(160, 40, 40));
        return (LRESULT)g_brLightRed;
    }
    if (h == g_hDtrState || h == g_hRtsState) {
        char buf[16];
        GetWindowTextA(h, buf, sizeof(buf));
        int high = strstr(buf, "HIGH") != NULL;
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, high ? RGB(220, 245, 225) : RGB(255, 230, 230));
        SetTextColor(hdc, high ? RGB(20, 120, 40) : RGB(160, 40, 40));
        return (LRESULT)(high ? g_brLightGreen : g_brLightRed);
    }
    /* 保护状态文字颜色 */
    if (h == g_hProt) {
        char buf[64];
        GetWindowTextA(h, buf, sizeof(buf));
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, ui_theme_bg());
        if (strstr(buf, "无保护")) {
            SetTextColor(hdc, RGB(20, 120, 40));
        } else if (strstr(buf, "有保护") || strstr(buf, "Level") || strstr(buf, "L1") || strstr(buf, "L2")) {
            SetTextColor(hdc, RGB(180, 80, 0));
        } else {
            SetTextColor(hdc, ui_theme_fg());
        }
        return (LRESULT)ui_theme_bg_brush();
    }
    /* 标签 / GroupBox / 其它：跟随系统浅色/深色 */
    SetBkMode(hdc, OPAQUE);
    SetBkColor(hdc, ui_theme_bg());
    SetTextColor(hdc, ui_theme_fg());
    return (LRESULT)ui_theme_bg_brush();
}

static LRESULT CALLBACK main_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE: {
        g_main = hwnd;
        isp_cfg_load(&g_cfg);
        ui_lang_set(g_cfg.lang);
        g_accel = build_accel();
        theme_rebuild_brushes();
        SetMenu(hwnd, build_menu());
        layout_create(hwnd);
        log_set_sink(ui_log_sink);
        logic_set_cancel_ptr(&g_cancel_flag);
        logic_set_progress_sink(progress_cb);
        ui_log_append_now(LOG_INFO, "STM32 ISP Downloader v0.6");
        ui_log_append_now(LOG_INFO, "主界面仅显示操作摘要；协议/读写明细请按 F10 打开全局日志");
        ui_log_append_now(LOG_INFO, "流程: 选串口→打开→选固件→擦除并下载 (F5)");
        return 0;
    }
    case WM_SIZE:
        relayout(LOWORD(lp), HIWORD(lp));
        ui_resnap_tools();
        return 0;
    case WM_MOVE:
        ui_resnap_tools();
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *m = (MINMAXINFO *)lp;
        m->ptMinTrackSize.x = 780;
        m->ptMinTrackSize.y = left_content_height() + 80;
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORLISTBOX:
        return on_color_static((HWND)lp, (HDC)wp);
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wp;
        SetBkColor(hdc, ui_theme_edit_bg());
        SetTextColor(hdc, ui_theme_edit_fg());
        return (LRESULT)ui_theme_edit_brush();
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wp;
        RECT cr;
        GetClientRect(hwnd, &cr);
        FillRect(hdc, &cr, ui_theme_bg_brush());
        return 1;
    }
    case WM_SETTINGCHANGE:
        /* 主题固定浅色；如需跟随系统深/浅色，可在此调用 ui_theme_reload() */
        (void)lp;
        return 0;
    case WM_APP_LOG: {
        ui_log_item_t *it = (ui_log_item_t *)lp;
        if (it) {
            ui_log_append_now(it->level, it->msg);
            free(it);
        }
        return 0;
    }
    case WM_APP_PROGRESS: {
        ui_prog_item_t *p = (ui_prog_item_t *)lp;
        if (p) {
            if (g_hProg) SendMessage(g_hProg, PBM_SETPOS, (WPARAM)p->pct, 0);
            ui_set_status(p->status);
            free(p);
        }
        return 0;
    }
    case WM_APP_OP_DONE:
        set_busy(0);
        ui_set_status("就绪");
        ui_update_chip_info();
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(wp);
        int code = HIWORD(wp);
        switch (id) {
        case IDC_REFRESH:
        case IDM_REFRESH_PORTS:
            ui_refresh_ports();
            break;
        case IDC_SERIAL_TOGGLE:
            do_serial_toggle();
            break;
        case IDC_BROWSE:
            do_browse();
            break;
        case IDC_ERASE: start_op(1); break;
        case IDC_DOWNLOAD: start_op(2); break;
        case IDC_VERIFY: start_op(3); break;
        case IDC_RUN: start_op(4); break;
        case IDC_CANCEL:
            InterlockedExchange(&g_cancel_flag, 1);
            ui_log_append_now(LOG_WARN, "已请求取消...");
            break;
        case IDC_DTR_TOGGLE:
            logic_set_dtr(SendMessageA(g_hDtrBtn, BM_GETCHECK, 0, 0) == BST_CHECKED);
            ui_update_dtr_rts();
            break;
        case IDC_RTS_TOGGLE:
            logic_set_rts(SendMessageA(g_hRtsBtn, BM_GETCHECK, 0, 0) == BST_CHECKED);
            ui_update_dtr_rts();
            break;
        case IDC_MODE_COMBO:
            if (code == CBN_SELCHANGE) {
                sync_bl_cfg();
                cfg_save_from_ui();
            }
            break;
        case IDC_VERIFY_DL:
        case IDC_RUN_AFTER:
        case IDC_STEP_DELAY:
            cfg_save_from_ui();
            break;
        case IDC_CUSTOM_BAUD_CHK:
            cfg_save_from_ui();
            ui_update_serial_ui();
            break;
        case IDC_OPTBYTES:
            ui_optbytes_run(hwnd);
            ui_update_chip_info();
            break;
        case IDC_READFLASH:
            do_read_flash();
            break;
        case IDC_DBG_PANEL:
        case IDM_DEBUG_PANEL:
        case IDM_ACCEL_DEBUG:
            if (ui_debug_visible()) {
                ui_debug_close();
                g_dbgWnd = NULL;
                {
                    HWND b = GetDlgItem(hwnd, IDC_DBG_PANEL);
                    if (b) SendMessageA(b, BM_SETCHECK, BST_UNCHECKED, 0);
                }
                ui_log_append_now(LOG_INFO, "调试面板已关闭 (F9 可再打开)");
            } else {
                g_dbgWnd = ui_debug_open(hwnd);
                {
                    HWND b = GetDlgItem(hwnd, IDC_DBG_PANEL);
                    if (b) SendMessageA(b, BM_SETCHECK, BST_CHECKED, 0);
                }
                ui_log_append_now(LOG_INFO, "调试面板已打开 (F9 可关闭)");
            }
            break;
        case IDM_HEX_LOG:
        case IDM_ACCEL_HEXLOG:
            if (ui_hexlog_visible()) {
                ui_hexlog_close();
                ui_log_append_now(LOG_INFO, "全局日志已关闭 (F10 可再打开)");
            } else {
                ui_hexlog_open(hwnd);
                ui_log_append_now(LOG_INFO, "全局日志已打开 (F10 可关闭)");
            }
            break;
        case IDM_ACCEL_DOWNLOAD: start_op(2); break;
        case IDM_ACCEL_ERASE: start_op(1); break;
        case IDM_ACCEL_VERIFY: start_op(3); break;
        case IDM_ACCEL_RUN: start_op(4); break;
        case IDM_ACCEL_CANCEL:
            InterlockedExchange(&g_cancel_flag, 1);
            ui_log_append_now(LOG_WARN, "已请求取消...");
            break;
        case IDM_ACCEL_HINT:
            MessageBoxA(hwnd,
                "快捷键：\n"
                "F5  擦除并下载\n"
                "F6  擦除芯片\n"
                "F7  校验数据\n"
                "F8  退出并运行\n"
                "F9  调试面板\n"
                "F10 全局日志\n"
                "Esc 取消当前操作\n"
                "Ctrl+E 导出日志",
                "快捷键", MB_OK | MB_ICONINFORMATION);
            break;
        case IDM_EXIT:
            DestroyWindow(hwnd);
            break;
        case IDM_EXPORT_LOG:
    do_export_log();
    break;
        case IDM_LANG_CN:
        case IDM_LANG_EN: {
            ui_lang_set(id == IDM_LANG_EN ? 1 : 0);
            g_cfg.lang = ui_lang_get();
            isp_cfg_save(&g_cfg);
            SetMenu(hwnd, build_menu());
            /* 刷新关键控件文案 */
            if (GetDlgItem(g_main, IDC_ERASE))
                SetWindowTextA(GetDlgItem(g_main, IDC_ERASE), L("擦除芯片", "Erase"));
            if (GetDlgItem(g_main, IDC_DOWNLOAD))
                SetWindowTextA(GetDlgItem(g_main, IDC_DOWNLOAD), L("擦除并下载", "Erase+Download"));
            if (GetDlgItem(g_main, IDC_VERIFY))
                SetWindowTextA(GetDlgItem(g_main, IDC_VERIFY), L("校验数据", "Verify"));
            if (GetDlgItem(g_main, IDC_RUN))
                SetWindowTextA(GetDlgItem(g_main, IDC_RUN), L("退出并运行", "Exit+Run"));
            if (GetDlgItem(g_main, IDC_OPTBYTES))
                SetWindowTextA(GetDlgItem(g_main, IDC_OPTBYTES), L("选项字节", "Option Bytes"));
            if (GetDlgItem(g_main, IDC_READFLASH))
                SetWindowTextA(GetDlgItem(g_main, IDC_READFLASH), L("读取 Flash", "Read Flash"));
            if (GetDlgItem(g_main, IDC_DBG_PANEL))
                SetWindowTextA(GetDlgItem(g_main, IDC_DBG_PANEL), L("调试", "Debug"));
            if (GetDlgItem(g_main, IDC_CANCEL))
                SetWindowTextA(GetDlgItem(g_main, IDC_CANCEL), L("取消", "Cancel"));
            if (GetDlgItem(g_main, IDC_REFRESH))
                SetWindowTextA(GetDlgItem(g_main, IDC_REFRESH), L("刷新", "Refresh"));
            if (g_hSerialBtn && logic_serial_is_open())
                SetWindowTextA(g_hSerialBtn, L("关闭串口", "Close Port"));
            else if (g_hSerialBtn)
                SetWindowTextA(g_hSerialBtn, L("打开串口", "Open Port"));
            ui_log_append_now(LOG_INFO,
                ui_lang_get() ? "Language switched to English" : "已切换到中文");
            break;
        }
        case IDM_ABOUT: {
            /* __DATE__ = "Sep 19 2026" → 2026年9月19日 */
            char mon[4] = {0};
            int day = 0, year = 0;
            sscanf(__DATE__, "%3s %d %d", mon, &day, &year);
            static const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
            int m = 0;
            for (int i = 0; i < 12; i++) {
                if (_strnicmp(mon, months + i * 3, 3) == 0) { m = i + 1; break; }
            }
            char about[128];
            snprintf(about, sizeof(about),
                     "版本: v0.6\n编译日期: %d年%d月%d日 %s",
                     year, m, day, __TIME__);
            MessageBoxA(hwnd, about, "关于", MB_OK | MB_ICONINFORMATION);
            break;
        }
        }
        return 0;
    }
    case WM_DESTROY:
        logic_close_serial();
        ui_debug_close();
        ui_hexlog_close();
        if (g_accel) { DestroyAcceleratorTable(g_accel); g_accel = NULL; }
        if (g_font) DeleteObject(g_font);
        if (g_log_font) DeleteObject(g_log_font);
        if (g_brGreen) DeleteObject(g_brGreen);
        if (g_brRed) DeleteObject(g_brRed);
        if (g_brGray) DeleteObject(g_brGray);
        if (g_brWhite) DeleteObject(g_brWhite);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

HWND ui_create_main(HINSTANCE inst)
{
    WNDCLASSA wc;
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);
    LoadLibraryA("RICHED20.DLL");

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = main_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "ISP_Downloader_C_Main";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(
        0, wc.lpszClassName,
        "STM32 ISP Downloader  v0.6",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 980, 760,
        NULL, NULL, inst, NULL);
    return hwnd;
}
