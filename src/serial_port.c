#include "serial_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setupapi.h>
#include <devguid.h>

#ifndef strcasecmp
#define strcasecmp _stricmp
#endif

static void set_timeouts(HANDLE h, int read_ms)
{
    COMMTIMEOUTS t;
    memset(&t, 0, sizeof(t));
    /* 分段超时：便于取消轮询 */
    t.ReadIntervalTimeout = 50;
    t.ReadTotalTimeoutConstant = (DWORD)read_ms;
    t.ReadTotalTimeoutMultiplier = 0;
    t.WriteTotalTimeoutConstant = 2000;
    t.WriteTotalTimeoutMultiplier = 0;
    SetCommTimeouts(h, &t);
}

bool serial_open(serial_t *s, const char *port, int baud)
{
    char path[64];
    DCB dcb;
    memset(s, 0, sizeof(*s));
    if (strncmp(port, "\\\\.\\", 4) == 0)
        snprintf(path, sizeof(path), "%s", port);
    else
        snprintf(path, sizeof(path), "\\\\.\\%s", port);

    s->h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, 0, NULL);
    if (s->h == INVALID_HANDLE_VALUE) {
        s->h = NULL;
        return false;
    }

    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(s->h, &dcb)) {
        CloseHandle(s->h);
        s->h = NULL;
        return false;
    }
    dcb.BaudRate = (DWORD)baud;
    dcb.ByteSize = 8;
    dcb.Parity = EVENPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    if (!SetCommState(s->h, &dcb)) {
        CloseHandle(s->h);
        s->h = NULL;
        return false;
    }
    SetupComm(s->h, 4096, 4096);
    set_timeouts(s->h, 200);
    PurgeComm(s->h, PURGE_RXCLEAR | PURGE_TXCLEAR);

    snprintf(s->port, sizeof(s->port), "%s", port);
    s->baud = baud;
    s->connected = true;
    s->dtr = false;
    s->rts = false;
    return true;
}

void serial_close(serial_t *s)
{
    if (s && s->h && s->h != INVALID_HANDLE_VALUE) {
        CloseHandle(s->h);
    }
    if (s) {
        s->h = NULL;
        s->connected = false;
    }
}

bool serial_is_open(const serial_t *s)
{
    return s && s->connected && s->h && s->h != INVALID_HANDLE_VALUE;
}

bool serial_send(serial_t *s, const uint8_t *data, int len)
{
    DWORD written = 0;
    if (!serial_is_open(s) || len <= 0) return false;
    if (!WriteFile(s->h, data, (DWORD)len, &written, NULL))
        return false;
    FlushFileBuffers(s->h);
    return (int)written == len;
}

int serial_recv(serial_t *s, uint8_t *buf, int max_len, int timeout_ms)
{
    DWORD n = 0;
    if (!serial_is_open(s) || max_len <= 0) return 0;
    set_timeouts(s->h, timeout_ms);
    if (!ReadFile(s->h, buf, (DWORD)max_len, &n, NULL))
        return 0;
    return (int)n;
}

int serial_recv_exact(serial_t *s, uint8_t *buf, int want, int timeout_ms)
{
    int got = 0;
    int remain_ms = timeout_ms;
    if (want <= 0) return 0;
    while (got < want && remain_ms > 0) {
        int n = serial_recv(s, buf + got, want - got, remain_ms > 200 ? 200 : remain_ms);
        if (n <= 0) {
            remain_ms -= 200;
            continue;
        }
        got += n;
    }
    return got;
}

bool serial_set_dtr(serial_t *s, bool level)
{
    if (!serial_is_open(s)) return false;
    if (!EscapeCommFunction(s->h, level ? SETDTR : CLRDTR))
        return false;
    s->dtr = level;
    return true;
}

bool serial_set_rts(serial_t *s, bool level)
{
    if (!serial_is_open(s)) return false;
    if (!EscapeCommFunction(s->h, level ? SETRTS : CLRRTS))
        return false;
    s->rts = level;
    return true;
}

void serial_clear(serial_t *s)
{
    if (serial_is_open(s))
        PurgeComm(s->h, PURGE_RXCLEAR | PURGE_TXCLEAR);
}

static void parse_com_from_text(const char *text, char *out, int outlen)
{
    /* "USB-SERIAL CH340 (COM13)" → COM13 ; "COM13" → COM13 */
    const char *p = text ? strstr(text, "COM") : NULL;
    if (!p) {
        if (outlen > 0) out[0] = 0;
        return;
    }
    int i = 0;
    while (p[i] && i < outlen - 1 &&
           ((p[i] >= '0' && p[i] <= '9') || (p[i] == 'C') || (p[i] == 'O') || (p[i] == 'M'))) {
        out[i] = p[i];
        i++;
    }
    out[i] = 0;
}

int serial_list_ports_info(serial_port_item_t *items, int max_ports)
{
    int count = 0;
    if (!items || max_ports <= 0) return 0;

    HDEVINFO h = SetupDiGetClassDevsA(&GUID_DEVINTERFACE_COMPORT, NULL, NULL,
                                       DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (h == INVALID_HANDLE_VALUE) {
        for (int i = 1; i <= 20 && count < max_ports; i++) {
            char p[32];
            snprintf(p, sizeof(p), "COM%d", i);
            char path[64];
            snprintf(path, sizeof(path), "\\\\.\\%s", p);
            HANDLE t = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                   OPEN_EXISTING, 0, NULL);
            if (t != INVALID_HANDLE_VALUE) {
                CloseHandle(t);
                snprintf(items[count].port, sizeof(items[count].port), "%s", p);
                items[count].desc[0] = 0;
                count++;
            }
        }
        return count;
    }

    SP_DEVICE_INTERFACE_DATA ifd;
    ifd.cbSize = sizeof(ifd);
    for (DWORD idx = 0; count < max_ports; idx++) {
        if (!SetupDiEnumDeviceInterfaces(h, NULL, &GUID_DEVINTERFACE_COMPORT, idx, &ifd))
            break;
        DWORD need = 0;
        SetupDiGetDeviceInterfaceDetailA(h, &ifd, NULL, 0, &need, NULL);
        if (need == 0) continue;
        SP_DEVICE_INTERFACE_DETAIL_DATA_A *detail =
            (SP_DEVICE_INTERFACE_DETAIL_DATA_A *)malloc(need);
        if (!detail) break;
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
        SP_DEVINFO_DATA di;
        di.cbSize = sizeof(di);
        if (SetupDiGetDeviceInterfaceDetailA(h, &ifd, detail, need, NULL, &di)) {
            char port[32] = {0};
            char friendly[160] = {0};
            char mfg[64] = {0};

            /* 友好名: "USB-SERIAL CH340 (COM13)" */
            DWORD ttype = REG_SZ, tsize = sizeof(friendly);
            SetupDiGetDeviceRegistryPropertyA(h, &di, SPDRP_FRIENDLYNAME,
                                              &ttype, (PBYTE)friendly, tsize, &tsize);
            if (friendly[0] == 0) {
                ttype = REG_SZ; tsize = sizeof(friendly);
                SetupDiGetDeviceRegistryPropertyA(h, &di, SPDRP_DEVICEDESC,
                                                  &ttype, (PBYTE)friendly, tsize, &tsize);
            }
            ttype = REG_SZ; tsize = sizeof(mfg);
            SetupDiGetDeviceRegistryPropertyA(h, &di, SPDRP_MFG,
                                              &ttype, (PBYTE)mfg, tsize, &tsize);

            HKEY key = SetupDiOpenDevRegKey(h, &di, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
            if (key != INVALID_HANDLE_VALUE) {
                DWORD sz = 32, type = 0;
                if (RegQueryValueExA(key, "PORTNAME", NULL, &type, (LPBYTE)port, &sz) != ERROR_SUCCESS)
                    port[0] = 0;
                RegCloseKey(key);
            }
            if (port[0] == 0)
                parse_com_from_text(friendly, port, sizeof(port));
            if (port[0] == 0)
                parse_com_from_text(detail->DevicePath, port, sizeof(port));
            if (port[0] == 0) {
                free(detail);
                continue;
            }

            snprintf(items[count].port, sizeof(items[count].port), "%s", port);
            /* 描述：尽量与设备管理器「端口」下的名称一致
               FriendlyName 常见: "USB-SERIAL CH340 (COM8)" / "蓝牙链路上的标准串行 (COM13)"
               保存为去掉 (COMx) 后缀的设备名，UI 再拼成 "COMx - 设备名" */
            {
                char desc[96] = {0};
                char tmp[160];
                snprintf(tmp, sizeof(tmp), "%s", friendly);
                if (tmp[0] == 0)
                    snprintf(tmp, sizeof(tmp), "%s", detail->DevicePath);
                /* 去掉末尾 " (COMxx)" */
                char *par = strrchr(tmp, '(');
                if (par && strstr(par, "COM")) *par = 0;
                size_t L = strlen(tmp);
                while (L && (tmp[L-1] == ' ' || tmp[L-1] == '\t')) tmp[--L] = 0;
                if (tmp[0])
                    snprintf(desc, sizeof(desc), "%s", tmp);
                else if (mfg[0])
                    snprintf(desc, sizeof(desc), "%s", mfg);
                else
                    snprintf(desc, sizeof(desc), "串口设备");
                snprintf(items[count].desc, sizeof(items[count].desc), "%s", desc);
            }
            count++;
        }
        free(detail);
    }
    SetupDiDestroyDeviceInfoList(h);
    return count;
}

int serial_list_ports(char names[][32], int max_ports)
{
    serial_port_item_t tmp[32];
    int nmax = max_ports > 32 ? 32 : max_ports;
    int n = serial_list_ports_info(tmp, nmax);
    for (int i = 0; i < n && i < max_ports; i++)
        snprintf(names[i], 32, "%s", tmp[i].port);
    return n;
}
