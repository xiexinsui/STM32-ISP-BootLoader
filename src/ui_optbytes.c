#include "ui_optbytes.h"
#include "app_logic.h"
#include "log.h"
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    IDE_VAL0 = 4100, /* 8*2 edits: val + comp */
    IDE_COMP0 = 4108,
    IDB_OB_READ = 4200,
    IDB_OB_WRITE,
    IDB_OB_CLOSE,
    IDT_OB_INFO
};

static HWND g_val[8], g_comp[8], g_info;
static uint8_t g_data[16];
static int g_updating;

static HWND mk(HWND p, const char *cls, const char *t, DWORD st,
               int x, int y, int w, int h, int id)
{
    return CreateWindowExA(0, cls, t, WS_CHILD | WS_VISIBLE | st,
                           x, y, w, h, p, (HMENU)(INT_PTR)id,
                           GetModuleHandle(NULL), NULL);
}

static void fill_from_data(void)
{
    char b[8];
    g_updating = 1;
    for (int i = 0; i < 8; i++) {
        snprintf(b, sizeof(b), "0x%02X", g_data[i * 2]);
        SetWindowTextA(g_val[i], b);
        snprintf(b, sizeof(b), "0x%02X", g_data[i * 2 + 1]);
        SetWindowTextA(g_comp[i], b);
    }
    g_updating = 0;
}

static int parse_hex_byte(HWND h, uint8_t *out)
{
    char b[32];
    GetWindowTextA(h, b, sizeof(b));
    char *end = NULL;
    long v = strtol(b, &end, 16);
    if (end == b) return 0;
    *out = (uint8_t)(v & 0xFF);
    return 1;
}

static void describe_row(int i, uint32_t base, char *out, int n)
{
    uint8_t v = g_data[i * 2];
    uint8_t c2 = g_data[i * 2 + 1];
    uint32_t addr = base + (uint32_t)i * 2;

    /* STM32F4 OPTCR @ 0x1FFFC000 按寄存器语义 */
    if (base == 0x1FFFC000u) {
        switch (i) {
        case 0: {
            int lock = v & 0x01;
            int strt = (v >> 1) & 0x01;
            int bor = (v >> 2) & 0x03;
            static const char *bor_s[4] = {
                "BOR Lv0", "BOR Lv1", "BOR Lv2", "BOR Lv3"
            };
            snprintf(out, n, "OPTCR LOCK=%d STRT=%d %s",
                     lock, strt, bor_s[bor & 3]);
            return;
        }
        case 1:
            snprintf(out, n, "USER WDG_SW=%d nRST_STOP=%d nRST_STDBY=%d BFB2=%d",
                     (v >> 6) & 1, (v >> 5) & 1, (v >> 4) & 1, (v >> 7) & 1);
            return;
        case 2:
            snprintf(out, n, "nWRP[7:0]=0x%02X (0=prot 1=open)", v);
            return;
        case 3:
            if (v == 0xAA) snprintf(out, n, "RDP Level0 0xAA (no protect)");
            else if (v == 0xCC) snprintf(out, n, "RDP Level2 0xCC (permanent)");
            else snprintf(out, n, "RDP=0x%02X (non-0xAA often L1)", v);
            return;
        case 4:
            snprintf(out, n, "OPTCR1[7:0]=0x%02X", v);
            return;
        case 5:
            snprintf(out, n, "OPTCR1[15:8]=0x%02X", v);
            return;
        case 6:
            snprintf(out, n, "OPTCR1[23:16]=0x%02X / n=0x%02X", v, c2);
            return;
        case 7:
            snprintf(out, n, "OPTCR1[31:24]=0x%02X / n=0x%02X", v, c2);
            return;
        default:
            snprintf(out, n, "0x%08X=0x%02X", addr, v);
            return;
        }
    }

    /* F1 @ 0x1FFFF800 */
    if (base == 0x1FFFF800u) {
        if (i == 0) {
            if (v == 0xA5) snprintf(out, n, "RDP L0 no protect");
            else if (v == 0xCC) snprintf(out, n, "RDP L2 permanent");
            else snprintf(out, n, "RDP: 0x%02X", v);
        } else if (i == 2) {
            snprintf(out, n, "USER: 0x%02X", v);
        } else {
            snprintf(out, n, "0x%08X=0x%02X/0x%02X", addr, v, c2);
        }
        return;
    }

    snprintf(out, n, "0x%08X=0x%02X/0x%02X", addr, v, c2);
}
static LRESULT CALLBACK ob_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE: {
        uint32_t base = logic_option_bytes_addr();
        const char *series = logic_chip_series();
        char info[160];
        snprintf(info, sizeof(info), "选项字节基址: 0x%08X   系列: %s",
                 (unsigned)base, series && series[0] ? series : "?");
        g_info = mk(hwnd, "STATIC", info, SS_LEFT, 12, 10, 460, 20, IDT_OB_INFO);
        mk(hwnd, "STATIC", "地址", SS_LEFT, 12, 36, 80, 18, 0);
        mk(hwnd, "STATIC", "值", SS_LEFT, 100, 36, 70, 18, 0);
        mk(hwnd, "STATIC", "反码", SS_LEFT, 180, 36, 70, 18, 0);
        mk(hwnd, "STATIC", "说明", SS_LEFT, 270, 36, 180, 18, 0);

        for (int i = 0; i < 8; i++) {
            int y = 56 + i * 28;
            char a[24];
            snprintf(a, sizeof(a), "0x%08X", (unsigned)(base + i * 2));
            mk(hwnd, "STATIC", a, SS_LEFT, 12, y + 3, 84, 18, 0);
            g_val[i] = mk(hwnd, "EDIT", "--", ES_AUTOHSCROLL,
                          100, y, 70, 22, IDE_VAL0 + i);
            g_comp[i] = mk(hwnd, "EDIT", "--", ES_AUTOHSCROLL,
                           180, y, 70, 22, IDE_COMP0 + i);
            mk(hwnd, "STATIC", "", SS_LEFT, 270, y + 3, 200, 18, 4300 + i);
        }
        mk(hwnd, "BUTTON", "读取", BS_PUSHBUTTON, 12, 290, 80, 28, IDB_OB_READ);
        mk(hwnd, "BUTTON", "写入", BS_PUSHBUTTON, 102, 290, 80, 28, IDB_OB_WRITE);
        mk(hwnd, "BUTTON", "关闭", BS_PUSHBUTTON, 192, 290, 80, 28, IDB_OB_CLOSE);
        memset(g_data, 0xFF, 16);
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        int code = HIWORD(wp);
        if (code == EN_CHANGE && id >= IDE_VAL0 && id < IDE_VAL0 + 8 && !g_updating) {
            int row = id - IDE_VAL0;
            uint8_t v;
            if (parse_hex_byte(g_val[row], &v)) {
                char b[8];
                snprintf(b, sizeof(b), "0x%02X", (uint8_t)(~v));
                g_updating = 1;
                SetWindowTextA(g_comp[row], b);
                g_updating = 0;
                g_data[row * 2] = v;
                g_data[row * 2 + 1] = (uint8_t)(~v);
            }
            return 0;
        }
        switch (id) {
        case IDB_OB_READ: {
            if (!logic_read_option_bytes(g_data)) {
                MessageBoxA(hwnd, "读取选项字节失败，请先连接芯片", "选项字节",
                            MB_OK | MB_ICONWARNING);
                break;
            }
            fill_from_data();
            uint32_t base = logic_option_bytes_addr();
            for (int i = 0; i < 8; i++) {
                char d[80];
                describe_row(i, base, d, sizeof(d));
                SetWindowTextA(GetDlgItem(hwnd, 4300 + i), d);
            }
            break;
        }
        case IDB_OB_WRITE: {
            uint8_t data[16];
            int f1 = (logic_option_bytes_addr() == 0x1FFFF800u);
            /* F4 OPTCR 区非「值+反码」结构，写入时不强制反码校验 */
            int f4 = (logic_option_bytes_addr() == 0x1FFFC000u);
            for (int i = 0; i < 8; i++) {
                uint8_t v, c;
                if (!parse_hex_byte(g_val[i], &v) || !parse_hex_byte(g_comp[i], &c)) {
                    MessageBoxA(hwnd, "存在无效的十六进制数值", "选项字节",
                                MB_OK | MB_ICONWARNING);
                    return 0;
                }
                if (f1 && (uint8_t)(v ^ 0xFF) != c) {
                    char m[80];
                    snprintf(m, sizeof(m), "第 %d 行反码不匹配 (值 0x%02X 应对应 0x%02X)",
                             i + 1, v, (uint8_t)(~v));
                    MessageBoxA(hwnd, m, "选项字节", MB_OK | MB_ICONWARNING);
                    return 0;
                }
                data[i * 2] = v;
                data[i * 2 + 1] = c;
            }
            int ret = MessageBoxA(hwnd,
                "写入选项字节可能需重新上电生效。\n"
                "错误的选项字节可能导致芯片无法启动！\n\n确定写入？",
                "确认写入", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
            if (ret != IDYES) break;
            if (!logic_write_option_bytes(data))
                MessageBoxA(hwnd, "写入失败", "选项字节", MB_OK | MB_ICONERROR);
            else
                MessageBoxA(hwnd, "写入命令已发送，请复位后回读确认", "选项字节",
                            MB_OK | MB_ICONINFORMATION);
            break;
        }
        case IDB_OB_CLOSE:
            DestroyWindow(hwnd);
            break;
        }
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

void ui_optbytes_run(HWND parent)
{
    HINSTANCE inst = GetModuleHandle(NULL);
    WNDCLASSA wc;
    static int registered;
    if (!registered) {
        memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = ob_proc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "ISP_C_OptionBytes";
        RegisterClassA(&wc);
        registered = 1;
    }
    HWND dlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, "ISP_C_OptionBytes", "选项字节",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        120, 80, 500, 370,
        parent, NULL, inst, NULL);
    if (!dlg) {
        if (parent) EnableWindow(parent, TRUE);
        return;
    }
    ShowWindow(dlg, SW_SHOW);
    UpdateWindow(dlg);
    SetForegroundWindow(dlg);

    /* 简易模态：禁用父窗口 */
    if (parent) EnableWindow(parent, FALSE);
    MSG msg;
    while (IsWindow(dlg) && GetMessage(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessage(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    if (parent) {
        EnableWindow(parent, TRUE);
        SetForegroundWindow(parent);
    }
}
