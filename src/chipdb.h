#ifndef CHIPDB_H
#define CHIPDB_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t pid;
    const char *name;
    const char *series;
    uint32_t flash_size_kb;
    uint32_t page_size;
    uint32_t sram_kb;
    uint32_t flash_size_reg;
    uint32_t option_bytes_addr;
} chip_entry_t;

typedef struct {
    bool has_extended_erase;
    uint32_t flash_size_kb; /* 0 = unknown */
} chip_ctx_t;

const chip_entry_t *chipdb_lookup(uint16_t pid);
const chip_entry_t *chipdb_lookup_ex(uint16_t pid, const chip_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif
