#ifndef FLASH_PROG_H
#define FLASH_PROG_H

#include "an3155.h"
#include "hex_parser.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    volatile LONG *cancel;
    void (*progress)(int percent, const char *status);
} flash_ctx_t;

/* 优先 0x44，否则 0x43；失败时尝试 write_unprotect 并返回 false */
bool flash_erase(an3155_t *a, const uint8_t *cmds, int ncmds, const flash_ctx_t *ctx);
bool flash_write_image(an3155_t *a, const fw_image_t *img, bool per_page_verify,
                       const flash_ctx_t *ctx);
bool flash_verify_image(an3155_t *a, const fw_image_t *img, const flash_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif
