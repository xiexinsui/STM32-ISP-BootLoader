#include "ui_input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { IDE_IN = 6001, IDB_OK, IDB_CANCEL };

static HWND g_edit;
static char g_title[64];
static char g_prompt[128];
static char g_def[32];
static volatile int g_ok;
static volatile int g_done;
static uint32_t g_value;
static HWND g_dlg;

static LRESULT CALLBACK in_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        CreateWindowExA(0, "STATIC", g_prompt, SS_LEFT,
                        12, 12, 280, 20, hwnd, NULL,
                        GetModuleHandle(NULL), NULL);
        g_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", g_def,
                                 WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                 12, 36, 280, 24, hwnd, (HMENU)(INT_PTR)IDE_IN,
                                 GetModuleHandle(NULL), NULL);
        CreateWindowExA(0, "BUTTON", "确定", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                        60, 72, 80, 28, hwnd, (HMENU)(INT_PTR)IDB_OK,
                        GetModuleHandle(NULL), NULL);
        CreateWindowExA(0, "BUTTON", "取消", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        156, 72, 80, 28, hwnd, (HMENU)(INT_PTR)IDB_CANCEL,
                        GetModuleHandle(NULL), NULL);
        if (g_edit) {
            SetFocus(g_edit);
            SendMessageA(g_edit, EM_SETSEL, 0, -1);
        }
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == IDB_OK) {
            char b[32] = {0};
            if (g_edit) GetWindowTextA(g_edit, b, sizeof(b));
            char *end = NULL;
            unsigned long v = strtoul(b, &end, 0);
            if (end == b || b[0] == 0) {
                MessageBoxA(hwnd, "请输入有效地址，如 0x08000000", g_title,
                            MB_OK | MB_ICONWARNING);
                return 0;
            }
            g_value = (uint32_t)v;
            g_ok = 1;
            g_done = 1;
            DestroyWindow(hwnd);
            return 0;
        }
        if (LOWORD(wp) == IDB_CANCEL) {
            g_ok = 0;
            g_done = 1;
            DestroyWindow(hwnd);
            return 0;
        }
        return 0;
    case WM_CLOSE:
        g_ok = 0;
        g_done = 1;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        g_done = 1;
        if (g_dlg == hwnd) g_dlg = NULL;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

bool ui_input_hex(HWND parent, const char *title, const char *prompt,
                  const char *default_text, uint32_t *out_value)
{
    snprintf(g_title, sizeof(g_title), "%s", title ? title : "输入");
    snprintf(g_prompt, sizeof(g_prompt), "%s", prompt ? prompt : "");
    snprintf(g_def, sizeof(g_def), "%s", default_text && default_text[0]
             ? default_text : "0x08000000");
    g_ok = 0;
    g_done = 0;
    g_value = 0;
    g_edit = NULL;
    g_dlg = NULL;

    HINSTANCE inst = GetModuleHandle(NULL);
    static int reg;
    if (!reg) {
        WNDCLASSA wc;
        memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = in_proc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "ISP_C_InputHex";
        RegisterClassA(&wc);
        reg = 1;
    }

    /* 必须 WS_VISIBLE，否则父窗口被禁用后主线程 GetMessage 空转卡死 */
    g_dlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "ISP_C_InputHex",
        g_title,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        260, 220, 320, 140,
        parent, NULL, inst, NULL);

    if (!g_dlg) {
        /* 创建失败时不要禁用父窗口；此处尚未禁用 */
        return false;
    }

    ShowWindow(g_dlg, SW_SHOW);
    UpdateWindow(g_dlg);
    SetForegroundWindow(g_dlg);
    if (g_edit) SetFocus(g_edit);

    if (parent) EnableWindow(parent, FALSE);

    MSG msg;
    while (!g_done && IsWindow(g_dlg)) {
        BOOL gm = GetMessage(&msg, NULL, 0, 0);
        if (gm == 0 || gm == -1) break; /* WM_QUIT 或错误 */
        if (msg.message == WM_KEYDOWN && (msg.hwnd == g_dlg || IsChild(g_dlg, msg.hwnd))) {
            if (msg.wParam == VK_RETURN) {
                SendMessage(g_dlg, WM_COMMAND, IDB_OK, 0);
                continue;
            }
            if (msg.wParam == VK_ESCAPE) {
                g_ok = 0;
                g_done = 1;
                DestroyWindow(g_dlg);
                continue;
            }
        }
        if (!IsDialogMessage(g_dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if (parent) {
        EnableWindow(parent, TRUE);
        SetForegroundWindow(parent);
    }

    if (!g_ok) return false;
    if (out_value) *out_value = g_value;
    return true;
}
