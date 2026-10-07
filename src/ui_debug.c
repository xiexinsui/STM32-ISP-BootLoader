/* 调试面板：无日志窗；DTR/RTS 与主界面一致（按钮显示 HIGH/LOW） */
#include "ui_debug.h"
#include "ui_main.h"
#include "app_logic.h"
#include "log.h"
#include "bl_control.h"
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    IDB_SYNC = 4001,
    IDB_ENTER, IDB_EXIT,
    IDB_GET, IDB_ID, IDB_VER,
    IDB_ER_STD, IDB_ER_EXT,
    IDB_WU, IDB_RU,
    IDB_FLSZ, IDB_GO,
    IDE_SEND, IDB_SEND,
    IDE_MODE, IDE_DELAY,
    IDB_DTR, IDB_RTS,
    IDT_STATUS,
    IDG_PROTO = 4200, IDG_ERASE, IDG_DTR, IDG_SEND
};

#define DBG_W  360
#define DBG_H  480
#define LEFT_W 340

static HWND g_dbg;
static HWND g_hStatus, g_hSend, g_hMode, g_hDelay;
static HWND g_hDtrBtn, g_hRtsBtn;
static volatile LONG g_capture;

int ui_debug_capture_active(void)
{
    return g_capture != 0;
}

/* 调试面板不单独显示日志；协议日志走主窗口「全局日志 F10」 */
void ui_debug_log(int level, const char *msg)
{
    (void)level;
    (void)msg;
}

void ui_debug_update_dtr_rts(void)
{
    int dtr = logic_dtr_level();
    int rts = logic_rts_level();
    if (g_hDtrBtn && IsWindow(g_hDtrBtn)) {
        char b[32];
        snprintf(b, sizeof(b), "DTR: %s", dtr ? "HIGH" : "LOW");
        SetWindowTextA(g_hDtrBtn, b);
        SendMessageA(g_hDtrBtn, BM_SETCHECK, dtr ? BST_CHECKED : BST_UNCHECKED, 0);
        InvalidateRect(g_hDtrBtn, NULL, TRUE);
    }
    if (g_hRtsBtn && IsWindow(g_hRtsBtn)) {
        char b[32];
        snprintf(b, sizeof(b), "RTS: %s", rts ? "HIGH" : "LOW");
        SetWindowTextA(g_hRtsBtn, b);
        SendMessageA(g_hRtsBtn, BM_SETCHECK, rts ? BST_CHECKED : BST_UNCHECKED, 0);
        InvalidateRect(g_hRtsBtn, NULL, TRUE);
    }
}

static LRESULT ui_dbg_color_static(HWND h, HDC hdc);

static HWND mk(HWND p, const char *cls, const char *text, DWORD st,
               DWORD ex, int x, int y, int w, int ht, int id)
{
    return CreateWindowExA(ex, cls, text, WS_CHILD | WS_VISIBLE | st,
                           x, y, w, ht, p, (HMENU)(INT_PTR)id,
                           GetModuleHandle(NULL), NULL);
}

static HWND mkbtn(HWND p, const char *text, int x, int y, int w, int h, int id)
{
    return mk(p, "BUTTON", text, BS_PUSHBUTTON, 0, x, y, w, h, id);
}

static int read_int(HWND h, int defv)
{
    char b[32];
    if (!h) return defv;
    GetWindowTextA(h, b, sizeof(b));
    int v = atoi(b);
    return v > 0 ? v : defv;
}

static void set_status(const char *s)
{
    if (g_hStatus) SetWindowTextA(g_hStatus, s ? s : "就绪");
}

static void run_btn(int id)
{
    int mode = read_int(g_hMode, 1);
    int delay = read_int(g_hDelay, 100);
    logic_dbg_config(mode, delay);
    set_status("执行中...");
    InterlockedExchange(&g_capture, 1);
    char resp[512] = {0};
    switch (id) {
    case IDB_SYNC: logic_dbg_sync(); break;
    case IDB_ENTER:
        if (!logic_serial_is_open()) {
            set_status("请先在主窗口打开串口");
            break;
        }
        logic_set_mode(mode, delay);
        logic_ensure_bl();
        break;
    case IDB_EXIT: logic_exit_and_run(); break;
    case IDB_GET: logic_dbg_get(); break;
    case IDB_ID: logic_dbg_get_id(); break;
    case IDB_VER: logic_dbg_get_version(); break;
    case IDB_ER_STD: logic_dbg_erase_std(); break;
    case IDB_ER_EXT: logic_dbg_erase_ext(); break;
    case IDB_WU: logic_dbg_write_unprotect(); break;
    case IDB_RU: logic_dbg_read_unprotect(); break;
    case IDB_FLSZ: logic_dbg_read_flash_size(); break;
    case IDB_GO: logic_dbg_go(); break;
    case IDB_SEND: {
        char hex[256] = {0};
        GetWindowTextA(g_hSend, hex, sizeof(hex));
        logic_dbg_send_hex(hex, resp, sizeof(resp));
        break;
    }
    case IDB_DTR:
        logic_set_dtr(SendMessageA(g_hDtrBtn, BM_GETCHECK, 0, 0) == BST_CHECKED);
        break;
    case IDB_RTS:
        logic_set_rts(SendMessageA(g_hRtsBtn, BM_GETCHECK, 0, 0) == BST_CHECKED);
        break;
    default: break;
    }
    InterlockedExchange(&g_capture, 0);
    ui_debug_update_dtr_rts();
    ui_update_chip_info();
    set_status("就绪");
}

static BOOL CALLBACK theme_enum_child(HWND h, LPARAM lp)
{
    (void)lp;
    InvalidateRect(h, NULL, TRUE);
    return TRUE;
}

static void layout_dbg(HWND hwnd)
{
    RECT rc;
    int cw = DBG_W - 16, ch = DBG_H - 40;
    if (GetClientRect(hwnd, &rc)) {
        cw = rc.right - rc.left;
        ch = rc.bottom - rc.top;
    }
    int x = 12;
    int iw = cw - 24;
    if (iw < 200) iw = 200;
    int bw = (iw - 10) / 3;
    int bw2 = (iw - 8) / 2;
    int rh = 28;
    int col1 = x, col2 = x + bw + 5, col3 = x + (bw + 5) * 2;

#define MV(id,xx,yy,ww,hh) do { HWND _h=GetDlgItem(hwnd,(id)); if(_h) MoveWindow(_h,(xx),(yy),(ww),(hh),TRUE);} while(0)

    MV(IDG_PROTO, 4, 6, cw + 8, 128);
    MV(IDB_SYNC, col1, 22, bw, rh);
    MV(IDB_ENTER, col2, 22, bw, rh);
    MV(IDB_EXIT, col3, 22, bw, rh);
    MV(IDB_GET, col1, 56, bw, rh);
    MV(IDB_ID, col2, 56, bw, rh);
    MV(IDB_VER, col3, 56, bw, rh);
    {
        HWND lb = GetWindow(hwnd, GW_CHILD);
        while (lb) {
            char cls[32], t[40];
            GetClassNameA(lb, cls, sizeof(cls));
            GetWindowTextA(lb, t, sizeof(t));
            if (lstrcmpiA(cls, "STATIC") == 0 && strcmp(t, "模式:") == 0)
                MoveWindow(lb, x, 94, 40, 18, TRUE);
            if (lstrcmpiA(cls, "STATIC") == 0 && strstr(t, "延时"))
                MoveWindow(lb, x + 96, 94, 70, 18, TRUE);
            lb = GetWindow(lb, GW_HWNDNEXT);
        }
    }
    MV(IDE_MODE, x + 40, 90, 48, 22);
    MV(IDE_DELAY, x + 168, 90, 56, 22);

    MV(IDG_ERASE, 4, 142, cw + 8, 128);
    MV(IDB_ER_STD, col1, 158, bw, rh);
    MV(IDB_ER_EXT, col2, 158, bw, rh);
    MV(IDB_FLSZ, col3, 158, bw, rh);
    MV(IDB_WU, col1, 192, bw, rh);
    MV(IDB_RU, col2, 192, bw, rh);
    MV(IDB_GO, col3, 192, bw, rh);

    /* DTR/RTS — 与主界面相同：两枚开关按钮 */
    MV(IDG_DTR, 4, 278, cw + 8, 64);
    MV(IDB_DTR, col1, 294, bw2, rh);
    MV(IDB_RTS, col1 + bw2 + 8, 294, bw2, rh);

    MV(IDG_SEND, 4, 350, cw + 8, 72);
    MV(IDE_SEND, col1, 366, iw - 70, 24);
    MV(IDB_SEND, col1 + iw - 64, 366, 64, 24);

    MV(IDT_STATUS, x, ch - 24, iw, 20);
#undef MV
}

static void create_controls(HWND hwnd)
{
    int x = 12, cw = DBG_W - 16;
    int iw = cw - 24;
    int bw = (iw - 10) / 3;
    int bw2 = (iw - 8) / 2;
    int rh = 28;
    int col1 = x, col2 = x + bw + 5, col3 = x + (bw + 5) * 2;

    mk(hwnd, "BUTTON", "协议", BS_GROUPBOX, 0, 4, 6, cw + 8, 128, IDG_PROTO);
    mkbtn(hwnd, "0x7F 同步", col1, 22, bw, rh, IDB_SYNC);
    mkbtn(hwnd, "进入 BL", col2, 22, bw, rh, IDB_ENTER);
    mkbtn(hwnd, "退出 BL", col3, 22, bw, rh, IDB_EXIT);
    mkbtn(hwnd, "GET", col1, 56, bw, rh, IDB_GET);
    mkbtn(hwnd, "GET_ID", col2, 56, bw, rh, IDB_ID);
    mkbtn(hwnd, "GET_VER", col3, 56, bw, rh, IDB_VER);
    mk(hwnd, "STATIC", "模式:", SS_LEFT, 0, x, 94, 40, 18, 0);
    g_hMode = mk(hwnd, "EDIT", "1", ES_AUTOHSCROLL | ES_NUMBER | ES_CENTER, 0,
                 x + 40, 90, 48, 22, IDE_MODE);
    mk(hwnd, "STATIC", "延时(ms):", SS_LEFT, 0, x + 96, 94, 70, 18, 0);
    g_hDelay = mk(hwnd, "EDIT", "100", ES_AUTOHSCROLL | ES_NUMBER | ES_CENTER, 0,
                  x + 168, 90, 56, 22, IDE_DELAY);

    mk(hwnd, "BUTTON", "擦除 / 保护", BS_GROUPBOX, 0, 4, 142, cw + 8, 128, IDG_ERASE);
    mkbtn(hwnd, "标准擦除", col1, 158, bw, rh, IDB_ER_STD);
    mkbtn(hwnd, "扩展擦除", col2, 158, bw, rh, IDB_ER_EXT);
    mkbtn(hwnd, "Flash大小", col3, 158, bw, rh, IDB_FLSZ);
    mkbtn(hwnd, "解写保护", col1, 192, bw, rh, IDB_WU);
    mkbtn(hwnd, "解读保护", col2, 192, bw, rh, IDB_RU);
    mkbtn(hwnd, "GO", col3, 192, bw, rh, IDB_GO);

    mk(hwnd, "BUTTON", "DTR / RTS", BS_GROUPBOX, 0, 4, 278, cw + 8, 64, IDG_DTR);
    g_hDtrBtn = mk(hwnd, "BUTTON", "DTR: LOW",
                   BS_AUTOCHECKBOX | BS_PUSHLIKE, 0,
                   col1, 294, bw2, rh, IDB_DTR);
    g_hRtsBtn = mk(hwnd, "BUTTON", "RTS: LOW",
                   BS_AUTOCHECKBOX | BS_PUSHLIKE, 0,
                   col1 + bw2 + 8, 294, bw2, rh, IDB_RTS);
    ui_debug_update_dtr_rts();

    mk(hwnd, "BUTTON", "手动发送 (hex，空格分隔)", BS_GROUPBOX, 0,
       4, 350, cw + 8, 72, IDG_SEND);
    g_hSend = mk(hwnd, "EDIT", "7F", ES_AUTOHSCROLL, 0,
                 col1, 366, iw - 70, 24, IDE_SEND);
    mkbtn(hwnd, "发送", col1 + iw - 64, 366, 64, 24, IDB_SEND);

    g_hStatus = mk(hwnd, "STATIC", "就绪 — 协议日志见主界面 F10 全局日志",
                   SS_LEFT, 0, x, DBG_H - 56, iw, 20, IDT_STATUS);

    layout_dbg(hwnd);
}

static LRESULT CALLBACK dbg_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        create_controls(hwnd);
        return 0;
    case WM_SIZE:
        layout_dbg(hwnd);
        return 0;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORLISTBOX:
        return ui_dbg_color_static((HWND)lp, (HDC)wp);
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
    case WM_COMMAND:
        if (HIWORD(wp) == BN_CLICKED)
            run_btn(LOWORD(wp));
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        g_capture = 0;
        if (g_dbg == hwnd) g_dbg = NULL;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

LRESULT ui_dbg_color_static(HWND h, HDC hdc)
{
    if (h == g_hDtrBtn || h == g_hRtsBtn) {
        char buf[32];
        GetWindowTextA(h, buf, sizeof(buf));
        int high = strstr(buf, "HIGH") != NULL;
        SetBkMode(hdc, OPAQUE);
        if (high) {
            SetBkColor(hdc, RGB(220, 245, 225));
            SetTextColor(hdc, RGB(20, 120, 40));
            return (LRESULT)ui_main_btn_brush(1);
        }
        SetBkColor(hdc, RGB(255, 230, 230));
        SetTextColor(hdc, RGB(160, 40, 40));
        return (LRESULT)ui_main_btn_brush(0);
    }
    SetBkMode(hdc, OPAQUE);
    SetBkColor(hdc, ui_theme_bg());
    SetTextColor(hdc, ui_theme_fg());
    return (LRESULT)ui_theme_bg_brush();
}

HWND ui_debug_open(HWND parent)
{
    HWND main = parent ? parent : ui_main_hwnd();
    if (g_dbg && IsWindow(g_dbg)) {
        ShowWindow(g_dbg, SW_SHOW);
        ui_snap_to_main(g_dbg, -1);
        layout_dbg(g_dbg);
        ui_debug_update_dtr_rts();
        SetForegroundWindow(g_dbg);
        return g_dbg;
    }
    HINSTANCE inst = GetModuleHandle(NULL);
    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = dbg_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.lpszClassName = "ISP_C_DebugPanel";
    RegisterClassA(&wc);

    /* 与主窗口/全局日志同类：可缩放、最大化/最小化，任务栏可见 */
    g_dbg = CreateWindowExA(
        WS_EX_APPWINDOW, wc.lpszClassName, "调试面板 — AN3155",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        0, 0, DBG_W, DBG_H,
        main, NULL, inst, NULL);
    if (!g_dbg) return NULL;
    HICON icon = LoadIcon(NULL, IDI_APPLICATION);
    if (icon) {
        SendMessage(g_dbg, WM_SETICON, ICON_BIG, (LPARAM)icon);
        SendMessage(g_dbg, WM_SETICON, ICON_SMALL, (LPARAM)icon);
    }
    ShowWindow(g_dbg, SW_SHOW);
    ui_snap_to_main(g_dbg, -1);
    layout_dbg(g_dbg);
    ui_debug_update_dtr_rts();
    return g_dbg;
}

void ui_debug_resnap(void)
{
    if (g_dbg && IsWindow(g_dbg) && IsWindowVisible(g_dbg)) {
        ui_snap_to_main(g_dbg, -1);
        layout_dbg(g_dbg);
    }
}

void ui_debug_close(void)
{
    if (g_dbg && IsWindow(g_dbg)) DestroyWindow(g_dbg);
    g_dbg = NULL;
    g_capture = 0;
}

int ui_debug_visible(void)
{
    return g_dbg && IsWindow(g_dbg) && IsWindowVisible(g_dbg);
}

void ui_debug_toggle(HWND parent)
{
    if (ui_debug_visible()) {
        ui_debug_close();
        return;
    }
    ui_debug_open(parent);
}

void ui_debug_theme_refresh(void)
{
    if (g_dbg && IsWindow(g_dbg)) {
        InvalidateRect(g_dbg, NULL, TRUE);
        UpdateWindow(g_dbg);
        EnumChildWindows(g_dbg, theme_enum_child, 0);
    }
}
