/* 全局协议/操作日志窗口 — 使用 ListBox，避免 RichEdit 背景异常 */
#include "ui_hexlog.h"
#include "isp_logname.h"
#include "ui_main.h"
#include <commctrl.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { IDH_LOG = 5001, IDH_EXPORT, IDH_CLEAR, IDH_CLOSE };

static HWND g_hex;
static HWND g_hLog;

void ui_hexlog_append(int level, const char *msg)
{
    static const char *tag[] = { "", "[OK] ", "[!] ", "[ERR] ", ">> ", "... " };
    char line[1400];
    const char *t = (level >= 0 && level <= 5) ? tag[level] : "";
    if (!g_hex || !IsWindow(g_hLog)) return;
    snprintf(line, sizeof(line), "%s%s", t, msg ? msg : "");
    SendMessageA(g_hLog, LB_ADDSTRING, 0, (LPARAM)line);
    int cnt = (int)SendMessageA(g_hLog, LB_GETCOUNT, 0, 0);
    while (cnt > 2500) {
        SendMessageA(g_hLog, LB_DELETESTRING, 0, 0);
        cnt--;
    }
    SendMessageA(g_hLog, LB_SETTOPINDEX, (WPARAM)(cnt > 0 ? cnt - 1 : 0), 0);
}

void ui_hexlog_clear(void)
{
    if (g_hLog) SendMessageA(g_hLog, LB_RESETCONTENT, 0, 0);
}

void ui_hexlog_export(HWND owner)
{
    char file[MAX_PATH];
    char stamp[32], defname[80], hdr[256];
    isp_log_stamp(stamp, sizeof(stamp));
    snprintf(defname, sizeof(defname), "ISP_HexLog_v%s_%s.txt", ISP_VERSION_STR, stamp);
    snprintf(file, sizeof(file), "%s", defname);
    OPENFILENAMEA ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = "文本 (*.txt)\0*.txt\0所有文件\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "txt";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameA(&ofn) || !g_hLog) return;
    FILE *fp = fopen(file, "wb");
    if (!fp) return;
    fputs("\xEF\xBB\xBF", fp);
    isp_log_header(hdr, sizeof(hdr));
    fputs(hdr, fp);
    int n = (int)SendMessageA(g_hLog, LB_GETCOUNT, 0, 0);
    for (int i = 0; i < n; i++) {
        char buf[1400];
        buf[0] = 0;
        SendMessageA(g_hLog, LB_GETTEXT, (WPARAM)i, (LPARAM)buf);
        fputs(buf, fp);
        fputc('\n', fp);
    }
    fclose(fp);
}

int ui_hexlog_visible(void)
{
    return g_hex && IsWindow(g_hex) && IsWindowVisible(g_hex);
}

void ui_hexlog_toggle(HWND parent)
{
    if (ui_hexlog_visible()) {
        ui_hexlog_close();
        return;
    }
    ui_hexlog_open(parent);
}

static LRESULT CALLBACK hex_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE: {
        HFONT font = CreateFontA(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
        g_hLog = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
            LBS_NOINTEGRALHEIGHT | LBS_HASSTRINGS | LBS_USETABSTOPS,
            8, 8, 560, 320, hwnd, (HMENU)(INT_PTR)IDH_LOG,
            GetModuleHandle(NULL), NULL);
        if (g_hLog && font) SendMessage(g_hLog, WM_SETFONT, (WPARAM)font, TRUE);
        if (g_hLog) {
            int tab = 80;
            SendMessageA(g_hLog, LB_SETTABSTOPS, 1, (LPARAM)&tab);
        }
        CreateWindowExA(0, "BUTTON", "导出", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        8, 336, 80, 28, hwnd, (HMENU)(INT_PTR)IDH_EXPORT,
                        GetModuleHandle(NULL), NULL);
        CreateWindowExA(0, "BUTTON", "清空", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        96, 336, 80, 28, hwnd, (HMENU)(INT_PTR)IDH_CLEAR,
                        GetModuleHandle(NULL), NULL);
        CreateWindowExA(0, "BUTTON", "关闭", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        184, 336, 80, 28, hwnd, (HMENU)(INT_PTR)IDH_CLOSE,
                        GetModuleHandle(NULL), NULL);
        return 0;
    }
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = (HDC)wp;
        SetBkColor(hdc, ui_theme_edit_bg());
        SetTextColor(hdc, ui_theme_edit_fg());
        return (LRESULT)ui_theme_edit_brush();
    }
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp;
        SetBkColor(hdc, ui_theme_bg());
        SetTextColor(hdc, ui_theme_fg());
        return (LRESULT)ui_theme_bg_brush();
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wp;
        RECT cr;
        GetClientRect(hwnd, &cr);
        FillRect(hdc, &cr, ui_theme_bg_brush());
        return 1;
    }
    case WM_SIZE: {
        int w = LOWORD(lp), h = HIWORD(lp);
        int btnY = h - 36;
        if (btnY < 40) btnY = 40;
        if (g_hLog) MoveWindow(g_hLog, 8, 8, w - 16, btnY - 16, TRUE);
        HWND b;
        b = GetDlgItem(hwnd, IDH_EXPORT); if (b) MoveWindow(b, 8, btnY, 80, 28, TRUE);
        b = GetDlgItem(hwnd, IDH_CLEAR);  if (b) MoveWindow(b, 96, btnY, 80, 28, TRUE);
        b = GetDlgItem(hwnd, IDH_CLOSE);  if (b) MoveWindow(b, 184, btnY, 80, 28, TRUE);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDH_EXPORT: ui_hexlog_export(hwnd); break;
        case IDH_CLEAR: ui_hexlog_clear(); break;
        case IDH_CLOSE:
        case IDCANCEL:
            ShowWindow(hwnd, SW_HIDE);
            break;
        }
        return 0;
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

HWND ui_hexlog_open(HWND parent)
{
    HWND main = parent ? parent : ui_main_hwnd();
    if (g_hex && IsWindow(g_hex)) {
        ShowWindow(g_hex, SW_SHOW);
        ui_snap_to_main(g_hex, +1); /* 右侧 */
        SetForegroundWindow(g_hex);
        return g_hex;
    }
    HINSTANCE inst = GetModuleHandle(NULL);
    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = hex_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "ISP_C_HexLog";
    RegisterClassA(&wc);
    g_hex = CreateWindowExA(WS_EX_APPWINDOW, wc.lpszClassName,
                            "全局日志 — 协议与操作",
                            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                            0, 0, 640, 460,
                            main, NULL, inst, NULL);
    if (!g_hex) return NULL;
    HICON icon = LoadIcon(NULL, IDI_APPLICATION);
    if (icon) {
        SendMessage(g_hex, WM_SETICON, ICON_BIG, (LPARAM)icon);
        SendMessage(g_hex, WM_SETICON, ICON_SMALL, (LPARAM)icon);
    }
    ShowWindow(g_hex, SW_SHOW);
    ui_snap_to_main(g_hex, +1); /* 右侧 */
    return g_hex;
}

void ui_hexlog_resnap(void)
{
    if (g_hex && IsWindow(g_hex) && IsWindowVisible(g_hex))
        ui_snap_to_main(g_hex, +1);
}

void ui_hexlog_close(void)
{
    if (g_hex && IsWindow(g_hex)) DestroyWindow(g_hex);
    g_hex = NULL;
}

static BOOL CALLBACK hex_theme_enum(HWND h, LPARAM lp)
{
    (void)lp;
    InvalidateRect(h, NULL, TRUE);
    return TRUE;
}

void ui_hexlog_theme_refresh(void)
{
    if (g_hex && IsWindow(g_hex)) {
        InvalidateRect(g_hex, NULL, TRUE);
        UpdateWindow(g_hex);
        EnumChildWindows(g_hex, hex_theme_enum, 0);
    }
}
