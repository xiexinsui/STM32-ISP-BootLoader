#ifndef BL_CONTROL_H
#define BL_CONTROL_H

#include "serial_port.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int mode;          /* 0..16, see Qt BootloaderMode */
    int step_delay_ms; /* 1..1000 */
} bl_config_t;

const char *bl_mode_name(int mode);
bool bl_enter(serial_t *s, const bl_config_t *cfg);
bool bl_exit(serial_t *s, const bl_config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif
