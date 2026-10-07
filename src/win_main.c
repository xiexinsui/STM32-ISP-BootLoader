#include <windows.h>
#include "ui_main.h"
#include "isp_cli.h"

int APIENTRY WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    (void)prev;
    /* 产线命令行模式：无 GUI，跑完直接退出 */
    if (isp_cli_run(cmd))
        return 0;

    HWND hwnd = ui_create_main(inst);
    if (!hwnd) return 1;
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    HACCEL accel = ui_get_accel();
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        if (accel && TranslateAccelerator(hwnd, accel, &msg))
            continue;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
