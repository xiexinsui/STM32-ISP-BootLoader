#ifndef ISP_CONFIG_H
#define ISP_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char port[32];
    int baud;
    int mode;
    int delay_ms;
    char file[MAX_PATH];
    int verify_dl;
    int run_after;
    int lang; /* 0=中文 1=English */
} isp_cfg_t;

void isp_cfg_defaults(isp_cfg_t *c);
void isp_cfg_load(isp_cfg_t *c);
void isp_cfg_save(const isp_cfg_t *c);

#ifdef __cplusplus
}
#endif

#endif