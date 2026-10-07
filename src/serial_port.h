#ifndef SERIAL_PORT_H
#define SERIAL_PORT_H

#include <windows.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    HANDLE h;
    char port[32];
    int baud;
    bool connected;
    bool dtr;
    bool rts;
} serial_t;

bool serial_open(serial_t *s, const char *port, int baud);
void serial_close(serial_t *s);
bool serial_is_open(const serial_t *s);

bool serial_send(serial_t *s, const uint8_t *data, int len);
/* 读最多 max_len 字节，等待 timeout_ms；返回读到的字节数 */
int  serial_recv(serial_t *s, uint8_t *buf, int max_len, int timeout_ms);
/* 读恰好 want 字节（超时返回已读数量） */
int  serial_recv_exact(serial_t *s, uint8_t *buf, int want, int timeout_ms);

bool serial_set_dtr(serial_t *s, bool level);
bool serial_set_rts(serial_t *s, bool level);
void serial_clear(serial_t *s);

/* 枚举 COM 口：names[i] 形如 "COM10"，返回个数 */
int serial_list_ports(char names[][32], int max_ports);

/* 枚举 COM 口 + 驱动/设备描述（如 USB-SERIAL CH340） */
typedef struct {
    char port[32];   /* COM13 */
    char desc[96];   /* 驱动/设备描述，可为空 */
} serial_port_item_t;

int serial_list_ports_info(serial_port_item_t *items, int max_ports);

#ifdef __cplusplus
}
#endif

#endif
