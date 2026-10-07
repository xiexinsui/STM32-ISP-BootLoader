#include "isp_config.h"
#include <string.h>

/* 不读写 ini；仅提供运行时默认值 */

void isp_cfg_defaults(isp_cfg_t *c)
{
    if (!c) return;
    memset(c, 0, sizeof(*c));
    c->baud = 115200;
    c->mode = 1;
    c->delay_ms = 100;
    c->verify_dl = 1;
    c->run_after = 1;
    c->lang = 0;
}

void isp_cfg_load(isp_cfg_t *c)
{
    isp_cfg_defaults(c);
}

void isp_cfg_save(const isp_cfg_t *c)
{
    (void)c;
}
