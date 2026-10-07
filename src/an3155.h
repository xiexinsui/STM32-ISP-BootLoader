#ifndef AN3155_H
#define AN3155_H

#include "serial_port.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BL_ACK  0x79
#define BL_NACK 0x1F

#define CMD_GET              0x00
#define CMD_GET_VERSION      0x01
#define CMD_GET_ID           0x02
#define CMD_READ_MEMORY      0x11
#define CMD_GO               0x21
#define CMD_WRITE_MEMORY     0x31
#define CMD_ERASE            0x43
#define CMD_EXTENDED_ERASE   0x44
#define CMD_WRITE_UNPROTECT  0x73
#define CMD_READOUT_UNPROTECT 0x92

#define SYNC_RETRY_COUNT    20
#define SYNC_RETRY_INTERVAL 500
#define DEFAULT_TIMEOUT_MS  2000

typedef struct {
    serial_t *serial;
    volatile LONG *cancel; /* optional */
} an3155_t;

void an3155_init(an3155_t *a, serial_t *s, volatile LONG *cancel);

bool an3155_sync(an3155_t *a, int timeout_ms);
bool an3155_get_command(an3155_t *a, uint8_t *version, uint8_t *cmds, int *cmd_count);
bool an3155_get_id(an3155_t *a, uint16_t *pid);
bool an3155_get_version(an3155_t *a, uint8_t *ver, uint8_t *opt1, uint8_t *opt2);
bool an3155_erase(an3155_t *a);
bool an3155_extended_erase(an3155_t *a);
bool an3155_write_memory(an3155_t *a, uint32_t addr, const uint8_t *data, int len);
bool an3155_read_memory(an3155_t *a, uint32_t addr, int len, uint8_t *out);
bool an3155_go(an3155_t *a, uint32_t addr);
bool an3155_write_unprotect(an3155_t *a);
bool an3155_readout_unprotect(an3155_t *a);
uint32_t an3155_read_flash_size(an3155_t *a, uint32_t reg_addr);
bool an3155_verify_erase(an3155_t *a, uint32_t addr, int size);

#ifdef __cplusplus
}
#endif

#endif
