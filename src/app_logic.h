#ifndef APP_LOGIC_H
#define APP_LOGIC_H

#include <stdint.h>
#include <stdbool.h>
#include "hex_parser.h"
#include "bl_control.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t pid;
    char name[64];
    char series[16];
    uint32_t flash_kb;
    uint8_t bl_ver;
    uint8_t rdp_raw;
    int rdp_level;
    int has_protection;
    uint8_t cmds[32];
    int ncmds;
    int connected;
} chip_ui_info_t;

typedef struct {
    int verify_each_page;
    int run_after;
} download_opts_t;

void logic_set_mode(int mode, int step_delay_ms);
void logic_set_cancel_ptr(volatile LONG *flag);
void logic_set_progress_sink(void (*fn)(int pct, const char *st));

bool logic_open_serial(const char *port, int baud);
void logic_close_serial(void);
bool logic_serial_is_open(void);
bool logic_set_dtr(bool level);
bool logic_set_rts(bool level);

bool logic_load_firmware(const char *path, uint32_t bin_start, char *err, int err_len);
bool logic_validate_fw(void);
bool logic_ensure_bl(void);
bool logic_erase(void);
bool logic_download(const download_opts_t *opt);
bool logic_verify(void);
bool logic_exit_and_run(void);
void logic_get_chip_info(chip_ui_info_t *out);
bool logic_dtr_level(void);
bool logic_rts_level(void);

/* P4 调试 */
bool logic_dbg_sync(void);
bool logic_dbg_get(void);
bool logic_dbg_get_id(void);
bool logic_dbg_get_version(void);
bool logic_dbg_erase_std(void);
bool logic_dbg_erase_ext(void);
bool logic_dbg_go(void);
bool logic_dbg_write_unprotect(void);
bool logic_dbg_read_unprotect(void);
bool logic_dbg_read_flash_size(void);
bool logic_dbg_send_hex(const char *hex, char *resp, int resp_len);
void logic_dbg_config(int mode, int delay);

/* P5 */
uint32_t logic_option_bytes_addr(void);
bool logic_read_option_bytes(uint8_t *out16);
bool logic_write_option_bytes(const uint8_t *data16);
bool logic_read_flash_to_hex(const char *out_path);
const char *logic_chip_series(void);

#ifdef __cplusplus
}
#endif

#endif
