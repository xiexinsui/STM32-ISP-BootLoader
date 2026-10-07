#include "bl_control.h"
#include "log.h"

static void sleep_ms(int ms)
{
    if (ms > 0) Sleep((DWORD)ms);
}

const char *bl_mode_name(int mode)
{
    static const char *names[] = {
        "0 不使用RTS和DTR",
        "1 RTS高+DTR低(BOOT0)+RTS低 进入BL",
        "2 RTS低+DTR低(BOOT0)+RTS高 进入BL",
        "3 RTS高+DTR高(BOOT0)+RTS低 进入BL",
        "4 RTS低+DTR高(BOOT0)+RTS高 进入BL",
        "5 DTR高+RTS低(BOOT0)+DTR低 进入BL",
        "6 DTR低+RTS低(BOOT0)+DTR高 进入BL",
        "7 DTR高+RTS高(BOOT0)+DTR低 进入BL",
        "8 DTR低+RTS高(BOOT0)+DTR高 进入BL",
        "9 DTR低+DTR高 进入BL 不用RTS",
        "10 DTR高+DTR低 进入BL 不用RTS",
        "11 RTS低+RTS高 进入BL 不用DTR",
        "12 RTS高+RTS低 进入BL 不用DTR",
        "13 DTR低电平 不用RTS",
        "14 DTR高电平 不用RTS",
        "15 RTS低电平 不用DTR",
        "16 RTS高电平 不用DTR",
    };
    if (mode < 0 || mode > 16) return "未知模式";
    return names[mode];
}

bool bl_enter(serial_t *s, const bl_config_t *cfg)
{
    int mode = cfg->mode;
    int delay = cfg->step_delay_ms > 0 ? cfg->step_delay_ms : 100;

    if (!serial_is_open(s)) {
        log_msg(LOG_ERROR, "串口未连接，无法进入 Bootloader");
        return false;
    }
    log_msg(LOG_INFO, "进入 Bootloader，模式: %s", bl_mode_name(mode));

    if (mode == 0) {
        log_msg(LOG_INFO, "模式 0: 不使用 DTR/RTS，跳过");
        return true;
    }

    if (mode >= 1 && mode <= 4) {
        int dtr_boot0 = (mode == 3 || mode == 4);
        int rts_steady = (mode == 2 || mode == 4);
        int rts_rev = !rts_steady;
        serial_set_rts(s, rts_rev != 0);
        log_msg(LOG_TRACE, "  RTS -> %s (NRST 反向)", rts_rev ? "HIGH" : "LOW");
        sleep_ms(delay);
        serial_set_dtr(s, dtr_boot0 != 0);
        log_msg(LOG_TRACE, "  DTR -> %s (BOOT0)", dtr_boot0 ? "HIGH" : "LOW");
        sleep_ms(delay);
        serial_set_rts(s, rts_steady != 0);
        log_msg(LOG_TRACE, "  RTS -> %s (NRST 稳态)", rts_steady ? "HIGH" : "LOW");
        sleep_ms(delay);
    } else if (mode >= 5 && mode <= 8) {
        int rts_boot0 = (mode == 7 || mode == 8);
        int dtr_steady = (mode == 6 || mode == 8);
        int dtr_rev = !dtr_steady;
        serial_set_dtr(s, dtr_rev != 0);
        log_msg(LOG_TRACE, "  DTR -> %s (NRST 反向)", dtr_rev ? "HIGH" : "LOW");
        sleep_ms(delay);
        serial_set_rts(s, rts_boot0 != 0);
        log_msg(LOG_TRACE, "  RTS -> %s (BOOT0)", rts_boot0 ? "HIGH" : "LOW");
        sleep_ms(delay);
        serial_set_dtr(s, dtr_steady != 0);
        log_msg(LOG_TRACE, "  DTR -> %s (NRST 稳态)", dtr_steady ? "HIGH" : "LOW");
        sleep_ms(delay);
    } else if (mode >= 9 && mode <= 12) {
        if (mode == 9) {
            serial_set_dtr(s, false); log_msg(LOG_TRACE, "  DTR -> LOW (NRST 反向)");
            sleep_ms(delay);
            serial_set_dtr(s, true);  log_msg(LOG_TRACE, "  DTR -> HIGH (NRST 稳态)");
        } else if (mode == 10) {
            serial_set_dtr(s, true);  log_msg(LOG_TRACE, "  DTR -> HIGH (NRST 反向)");
            sleep_ms(delay);
            serial_set_dtr(s, false); log_msg(LOG_TRACE, "  DTR -> LOW (NRST 稳态)");
        } else if (mode == 11) {
            serial_set_rts(s, false); log_msg(LOG_TRACE, "  RTS -> LOW (NRST 反向)");
            sleep_ms(delay);
            serial_set_rts(s, true);  log_msg(LOG_TRACE, "  RTS -> HIGH (NRST 稳态)");
        } else {
            serial_set_rts(s, true);  log_msg(LOG_TRACE, "  RTS -> HIGH (NRST 反向)");
            sleep_ms(delay);
            serial_set_rts(s, false); log_msg(LOG_TRACE, "  RTS -> LOW (NRST 稳态)");
        }
        sleep_ms(delay);
    } else if (mode >= 13 && mode <= 16) {
        if (mode == 13) { serial_set_dtr(s, false); log_msg(LOG_TRACE, "  DTR -> LOW (BOOT0)"); }
        else if (mode == 14) { serial_set_dtr(s, true); log_msg(LOG_TRACE, "  DTR -> HIGH (BOOT0)"); }
        else if (mode == 15) { serial_set_rts(s, false); log_msg(LOG_TRACE, "  RTS -> LOW (BOOT0)"); }
        else { serial_set_rts(s, true); log_msg(LOG_TRACE, "  RTS -> HIGH (BOOT0)"); }
        sleep_ms(delay);
    }

    serial_clear(s);
    log_msg(LOG_INFO, "进入 Bootloader 完成");
    return true;
}

bool bl_exit(serial_t *s, const bl_config_t *cfg)
{
    int mode = cfg->mode;
    int delay = cfg->step_delay_ms > 0 ? cfg->step_delay_ms : 100;

    if (!serial_is_open(s)) {
        log_msg(LOG_ERROR, "串口未连接，无法退出 Bootloader");
        return false;
    }
    log_msg(LOG_INFO, "退出 Bootloader，模式: %s", bl_mode_name(mode));
    if (mode == 0) {
        log_msg(LOG_INFO, "模式 0: 不使用 DTR/RTS，跳过");
        return true;
    }

    if (mode >= 1 && mode <= 4) {
        int dtr_boot0 = (mode == 3 || mode == 4);
        int rts_steady = (mode == 2 || mode == 4);
        int rts_rev = !rts_steady;
        serial_set_rts(s, rts_rev != 0);
        log_msg(LOG_TRACE, "  RTS -> %s (NRST 反向)", rts_rev ? "HIGH" : "LOW");
        sleep_ms(delay);
        serial_set_dtr(s, !dtr_boot0);
        log_msg(LOG_TRACE, "  DTR -> %s (BOOT0=Flash)", (!dtr_boot0) ? "HIGH" : "LOW");
        sleep_ms(delay);
        serial_set_rts(s, rts_steady != 0);
        log_msg(LOG_TRACE, "  RTS -> %s (NRST 稳态)", rts_steady ? "HIGH" : "LOW");
        sleep_ms(delay);
    } else if (mode >= 5 && mode <= 8) {
        int rts_boot0 = (mode == 7 || mode == 8);
        int dtr_steady = (mode == 6 || mode == 8);
        int dtr_rev = !dtr_steady;
        serial_set_dtr(s, dtr_rev != 0);
        log_msg(LOG_TRACE, "  DTR -> %s (NRST 反向)", dtr_rev ? "HIGH" : "LOW");
        sleep_ms(delay);
        serial_set_rts(s, !rts_boot0);
        log_msg(LOG_TRACE, "  RTS -> %s (BOOT0=Flash)", (!rts_boot0) ? "HIGH" : "LOW");
        sleep_ms(delay);
        serial_set_dtr(s, dtr_steady != 0);
        log_msg(LOG_TRACE, "  DTR -> %s (NRST 稳态)", dtr_steady ? "HIGH" : "LOW");
        sleep_ms(delay);
    } else if (mode >= 9 && mode <= 12) {
        if (mode == 9) {
            serial_set_dtr(s, true); sleep_ms(delay);
            serial_set_dtr(s, false);
        } else if (mode == 10) {
            serial_set_dtr(s, false); sleep_ms(delay);
            serial_set_dtr(s, true);
        } else if (mode == 11) {
            serial_set_rts(s, true); sleep_ms(delay);
            serial_set_rts(s, false);
        } else {
            serial_set_rts(s, false); sleep_ms(delay);
            serial_set_rts(s, true);
        }
        sleep_ms(delay);
    } else if (mode >= 13 && mode <= 16) {
        if (mode == 13) serial_set_dtr(s, true);
        else if (mode == 14) serial_set_dtr(s, false);
        else if (mode == 15) serial_set_rts(s, true);
        else serial_set_rts(s, false);
        sleep_ms(delay);
    }

    serial_clear(s);
    log_msg(LOG_INFO, "退出 Bootloader 完成");
    return true;
}
