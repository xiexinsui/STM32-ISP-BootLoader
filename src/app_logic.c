#include "app_logic.h"
#include "serial_port.h"
#include "an3155.h"
#include "flash_prog.h"
#include "chipdb.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

static serial_t g_ser;
static an3155_t g_bl;
static bl_config_t g_blcfg = { 1, 100 };
static fw_image_t g_fw;
static volatile LONG *g_cancel;
static void (*g_prog)(int, const char *);
static chip_ui_info_t g_chip;
static int g_bl_connected;

void logic_set_mode(int mode, int step_delay_ms)
{
    g_blcfg.mode = mode;
    g_blcfg.step_delay_ms = step_delay_ms;
}

void logic_set_cancel_ptr(volatile LONG *flag)
{
    g_cancel = flag;
    an3155_init(&g_bl, &g_ser, flag);
}

void logic_set_progress_sink(void (*fn)(int, const char *))
{
    g_prog = fn;
}

bool logic_open_serial(const char *port, int baud)
{
    if (serial_is_open(&g_ser)) serial_close(&g_ser);
    if (!serial_open(&g_ser, port, baud)) {
        log_msg(LOG_ERROR, "打开串口失败: %s @ %d 8E1", port, baud);
        return false;
    }
    an3155_init(&g_bl, &g_ser, g_cancel);
    g_bl_connected = 0;
    log_msg(LOG_SUCCESS, "已打开串口 %s, 波特率 %d (8E1)", port, baud);
    return true;
}

void logic_close_serial(void)
{
    g_bl_connected = 0;
    serial_close(&g_ser);
    log_msg(LOG_INFO, "串口已关闭");
}

bool logic_serial_is_open(void) { return serial_is_open(&g_ser); }

bool logic_set_dtr(bool level)
{
    bool ok = serial_set_dtr(&g_ser, level);
    log_msg(LOG_TRACE, "DTR -> %s", level ? "HIGH" : "LOW");
    return ok;
}

bool logic_set_rts(bool level)
{
    bool ok = serial_set_rts(&g_ser, level);
    log_msg(LOG_TRACE, "RTS -> %s", level ? "HIGH" : "LOW");
    return ok;
}

static void on_progress(int pct, const char *st)
{
    if (g_prog) g_prog(pct, st);
}

static int cmd_supported(uint8_t c)
{
    for (int i = 0; i < g_chip.ncmds; i++)
        if (g_chip.cmds[i] == c) return 1;
    return 0;
}

static void apply_rdp(uint8_t raw)
{
    g_chip.rdp_raw = raw;
    if (raw == 0x00 || raw == 0xA5 || raw == 0xAA || raw == 0xFF) {
        g_chip.rdp_level = 0;
        g_chip.has_protection = 0;
        log_msg(LOG_INFO, "保护状态: 无保护 (option=0x%02X)", raw);
    } else if (raw == 0xCC || raw == 0x33) {
        g_chip.rdp_level = 2;
        g_chip.has_protection = 1;
        log_msg(LOG_WARN, "保护状态: 疑似 Level2 (0x%02X)", raw);
    } else {
        g_chip.rdp_level = 1;
        g_chip.has_protection = 1;
        log_msg(LOG_WARN, "保护状态: 有保护 (0x%02X, 按 Level1 处理)", raw);
    }
}

bool logic_ensure_bl(void)
{
    if (g_bl_connected) return true;
    if (!serial_is_open(&g_ser)) {
        log_msg(LOG_ERROR, "请先打开串口");
        return false;
    }
    log_msg(LOG_CMD, "自动进入 Bootloader...");
    if (!bl_enter(&g_ser, &g_blcfg)) return false;

    log_msg(LOG_INFO, "正在连接 Bootloader...");
    memset(&g_chip, 0, sizeof(g_chip));

    int synced = 0;
    if (g_blcfg.mode == 0) {
        synced = an3155_sync(&g_bl, 300);
        for (int i = 1; !synced && i < SYNC_RETRY_COUNT; i++) {
            if (g_cancel && *g_cancel) return false;
            Sleep(SYNC_RETRY_INTERVAL);
            synced = an3155_sync(&g_bl, 300);
        }
    } else {
        synced = an3155_sync(&g_bl, DEFAULT_TIMEOUT_MS);
    }
    if (!synced) {
        log_msg(LOG_ERROR, "Bootloader 连接失败");
        return false;
    }

    uint8_t ver = 0;
    if (!an3155_get_command(&g_bl, &ver, g_chip.cmds, &g_chip.ncmds)) return false;
    g_chip.bl_ver = ver;

    uint16_t pid = 0;
    if (!an3155_get_id(&g_bl, &pid)) return false;
    g_chip.pid = pid;

    chip_ctx_t ctx;
    ctx.has_extended_erase = cmd_supported(CMD_EXTENDED_ERASE) != 0;
    ctx.flash_size_kb = 0;
    const chip_entry_t *chip = chipdb_lookup_ex(pid, &ctx);
    if (chip) {
        snprintf(g_chip.name, sizeof(g_chip.name), "%s", chip->name);
        snprintf(g_chip.series, sizeof(g_chip.series), "%s", chip->series);
        g_chip.flash_kb = chip->flash_size_kb;
        log_msg(LOG_SUCCESS, "识别芯片: %s (系列 %s), Flash后备: %u KB",
                chip->name, chip->series, (unsigned)chip->flash_size_kb);
    } else {
        snprintf(g_chip.name, sizeof(g_chip.name), "未知型号");
        log_msg(LOG_WARN, "未识别的芯片 PID: 0x%04X", pid);
    }

    uint8_t o1 = 0, o2 = 0, bver = 0;
    if (an3155_get_version(&g_bl, &bver, &o1, &o2)) {
        g_chip.bl_ver = bver;
        apply_rdp(o1);
    }

    if (chip && chip->flash_size_reg) {
        uint32_t kb = an3155_read_flash_size(&g_bl, chip->flash_size_reg);
        if (kb > 0) {
            g_chip.flash_kb = kb;
            chip_ctx_t ctx2 = ctx;
            ctx2.flash_size_kb = kb;
            const chip_entry_t *c2 = chipdb_lookup_ex(pid, &ctx2);
            if (c2) {
                snprintf(g_chip.name, sizeof(g_chip.name), "%s", c2->name);
                snprintf(g_chip.series, sizeof(g_chip.series), "%s", c2->series);
            }
        }
    }

    g_bl_connected = 1;
    log_msg(LOG_SUCCESS, "Bootloader 连接成功");
    return true;
}

bool logic_load_firmware(const char *path, uint32_t bin_start, char *err, int err_len)
{
    const char *dot = strrchr(path, '.');
    fw_image_free(&g_fw);
    bool ok;
    if (dot && (_stricmp(dot, ".hex") == 0))
        ok = hex_parse_file(path, &g_fw, err, err_len);
    else if (dot && (_stricmp(dot, ".bin") == 0))
        ok = bin_parse_file(path, bin_start, &g_fw, err, err_len);
    else {
        snprintf(err, err_len, "不支持的文件格式");
        return false;
    }
    if (!ok) {
        fw_image_free(&g_fw);
        return false;
    }

    uint32_t mn = 0, mx = 0;
    int total = 0;
    if (fw_image_range(&g_fw, &mn, &mx)) {
        for (int i = 0; i < g_fw.count; i++) total += g_fw.segs[i].size;
        log_msg(LOG_SUCCESS, "固件加载成功: %s (%d 字节), 0x%08X-0x%08X",
                path, total, (unsigned)mn, (unsigned)mx);
    }
    return true;
}

bool logic_validate_fw(void)
{
    if (g_fw.count == 0) {
        log_msg(LOG_ERROR, "请先选择固件文件");
        return false;
    }
    uint32_t mn = 0, mx = 0;
    fw_image_range(&g_fw, &mn, &mx);
    const uint32_t base = 0x08000000u;
    uint32_t kb = g_chip.flash_kb ? g_chip.flash_kb : 512;
    uint32_t end = base + kb * 1024u;
    if (mn < base) {
        log_msg(LOG_ERROR, "固件起始地址 0x%08X 低于 Flash 基址", (unsigned)mn);
        return false;
    }
    if (mx > end) {
        log_msg(LOG_ERROR, "固件超出 Flash 范围: 0x%08X > 0x%08X (%u KB)",
                (unsigned)mx, (unsigned)end, (unsigned)kb);
        return false;
    }
    log_msg(LOG_INFO, "固件地址校验通过: 0x%08X-0x%08X (Flash %u KB)",
            (unsigned)mn, (unsigned)mx, (unsigned)kb);
    return true;
}

bool logic_erase(void)
{
    if (!logic_ensure_bl()) return false;
    flash_ctx_t fc = { g_cancel, on_progress };
    return flash_erase(&g_bl, g_chip.cmds, g_chip.ncmds, &fc);
}

bool logic_download(const download_opts_t *opt)
{
    if (!logic_ensure_bl()) return false;
    if (!logic_validate_fw()) return false;

    flash_ctx_t fc = { g_cancel, on_progress };

    if (g_chip.rdp_level != 0) {
        log_msg(LOG_WARN, "检测到读保护，将先解除写保护再继续");
        if (!an3155_write_unprotect(&g_bl)) {
            log_msg(LOG_ERROR, "解除写保护失败");
            return false;
        }
        g_bl_connected = 0;
        if (!logic_ensure_bl()) return false;
    }

    if (!flash_erase(&g_bl, g_chip.cmds, g_chip.ncmds, &fc)) {
        /* 一次自动重试：解保护后重入再擦 */
        log_msg(LOG_WARN, "擦除失败，尝试解除写保护后自动重试一次...");
        if (an3155_write_unprotect(&g_bl)) {
            g_bl_connected = 0;
            if (!logic_ensure_bl()) return false;
            if (!flash_erase(&g_bl, g_chip.cmds, g_chip.ncmds, &fc))
                return false;
        } else {
            return false;
        }
    }

    g_bl_connected = 0;
    if (!logic_ensure_bl()) {
        log_msg(LOG_ERROR, "重新进入 Bootloader 失败");
        return false;
    }

    int verify = opt && opt->verify_each_page;
    ULONGLONG t_write0 = GetTickCount64();
    if (!flash_write_image(&g_bl, &g_fw, verify != 0, &fc))
        return false;
    {
        ULONGLONG ms = GetTickCount64() - t_write0;
        unsigned sec = (unsigned)(ms / 1000);
        unsigned tenth = (unsigned)((ms % 1000) / 100);
        if (sec >= 60)
            log_msg(LOG_SUCCESS, "固件写入完成，下载用时 %u 分 %u.%u 秒",
                    sec / 60, sec % 60, tenth);
        else
            log_msg(LOG_SUCCESS, "固件写入完成，下载用时 %u.%u 秒", sec, tenth);
    }

    if (opt && opt->run_after) {
        if (g_blcfg.mode == 0) {
            if (!an3155_go(&g_bl, 0x08000000u)) {
                log_msg(LOG_ERROR, "GO 0x08000000 失败");
                return false;
            }
        } else {
            if (cmd_supported(CMD_GO))
                an3155_go(&g_bl, 0x08000000u);
            bl_exit(&g_ser, &g_blcfg);
        }
        g_bl_connected = 0;
        log_msg(LOG_SUCCESS, "已退出并准备运行");
    }
    log_msg(LOG_SUCCESS, "所有操作已完成");
    return true;
}

bool logic_verify(void)
{
    if (!logic_ensure_bl()) return false;
    if (!logic_validate_fw()) return false;
    flash_ctx_t fc = { g_cancel, on_progress };
    return flash_verify_image(&g_bl, &g_fw, &fc);
}

bool logic_exit_and_run(void)
{
    if (!serial_is_open(&g_ser)) {
        log_msg(LOG_ERROR, "请先打开串口");
        return false;
    }
    if (g_blcfg.mode == 0) {
        if (g_bl_connected) {
            if (!an3155_go(&g_bl, 0x08000000u)) {
                log_msg(LOG_ERROR, "GO 命令失败");
                return false;
            }
        }
    } else {
        if (g_bl_connected && cmd_supported(CMD_GO))
            an3155_go(&g_bl, 0x08000000u);
        bl_exit(&g_ser, &g_blcfg);
    }
    g_bl_connected = 0;
    log_msg(LOG_SUCCESS, "已退出 Bootloader，芯片从 Flash 启动");
    return true;
}

void logic_get_chip_info(chip_ui_info_t *out)
{
    if (out) *out = g_chip;
}

bool logic_dtr_level(void)
{
    return g_ser.dtr;
}

bool logic_rts_level(void)
{
    return g_ser.rts;
}

void logic_dbg_config(int mode, int delay)
{
    logic_set_mode(mode, delay);
}

static an3155_t *dbg_bl(void)
{
    an3155_init(&g_bl, &g_ser, g_cancel);
    return &g_bl;
}

bool logic_dbg_sync(void)
{
    if (!serial_is_open(&g_ser)) {
        log_msg(LOG_ERROR, "串口未连接");
        return false;
    }
    return an3155_sync(dbg_bl(), g_blcfg.mode == 0 ? 300 : DEFAULT_TIMEOUT_MS);
}

bool logic_dbg_get(void)
{
    uint8_t ver = 0, cmds[32];
    int n = 0;
    if (!an3155_get_command(dbg_bl(), &ver, cmds, &n)) return false;
    g_chip.bl_ver = ver;
    g_chip.ncmds = n;
    memcpy(g_chip.cmds, cmds, n > 32 ? 32 : (size_t)n);
    return true;
}

bool logic_dbg_get_id(void)
{
    uint16_t pid = 0;
    if (!an3155_get_id(dbg_bl(), &pid)) return false;
    g_chip.pid = pid;
    chip_ctx_t ctx = { cmd_supported(CMD_EXTENDED_ERASE) != 0, g_chip.flash_kb };
    const chip_entry_t *c = chipdb_lookup_ex(pid, &ctx);
    if (c) {
        snprintf(g_chip.name, sizeof(g_chip.name), "%s", c->name);
        snprintf(g_chip.series, sizeof(g_chip.series), "%s", c->series);
        g_chip.flash_kb = c->flash_size_kb;
    }
    g_chip.connected = 1;
    return true;
}

bool logic_dbg_get_version(void)
{
    uint8_t v, o1, o2;
    if (!an3155_get_version(dbg_bl(), &v, &o1, &o2)) return false;
    g_chip.bl_ver = v;
    apply_rdp(o1);
    return true;
}

bool logic_dbg_erase_std(void)
{
    return an3155_erase(dbg_bl());
}

bool logic_dbg_erase_ext(void)
{
    return an3155_extended_erase(dbg_bl());
}

bool logic_dbg_go(void)
{
    return an3155_go(dbg_bl(), 0x08000000u);
}

bool logic_dbg_write_unprotect(void)
{
    return an3155_write_unprotect(dbg_bl());
}

bool an3155_readout_unprotect(an3155_t *a);

bool logic_dbg_read_unprotect(void)
{
    log_msg(LOG_WARN, "解读保护 (0x92) 将触发全片擦除！");
    return an3155_readout_unprotect(dbg_bl());
}

bool logic_dbg_read_flash_size(void)
{
    chip_ctx_t ctx = { cmd_supported(CMD_EXTENDED_ERASE) != 0, 0 };
    const chip_entry_t *c = chipdb_lookup_ex(g_chip.pid, &ctx);
    uint32_t reg = c ? c->flash_size_reg : 0;
    if (!reg) {
        log_msg(LOG_ERROR, "未知 Flash 容量寄存器地址 (PID=0x%04X)", g_chip.pid);
        return false;
    }
    uint32_t kb = an3155_read_flash_size(dbg_bl(), reg);
    if (kb) {
        g_chip.flash_kb = kb;
        return true;
    }
    return false;
}

bool logic_dbg_send_hex(const char *hex, char *resp, int resp_len)
{
    if (!serial_is_open(&g_ser)) {
        log_msg(LOG_ERROR, "串口未连接");
        return false;
    }
    uint8_t buf[256];
    int n = 0;
    const char *p = hex;
    while (*p && n < (int)sizeof(buf)) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        char *end = NULL;
        long v = strtol(p, &end, 16);
        if (end == p) break;
        buf[n++] = (uint8_t)(v & 0xFF);
        p = end;
    }
    if (n <= 0) {
        log_msg(LOG_ERROR, "无效的十六进制数据");
        return false;
    }
    char desc[64];
    snprintf(desc, sizeof(desc), "手动发送 %d 字节", n);
    log_msg(LOG_CMD, "%s: %s", desc, hex);
    if (!serial_send(&g_ser, buf, n)) return false;
    uint8_t rb[256];
    int got = serial_recv(&g_ser, rb, sizeof(rb), 500);
    if (resp && resp_len > 0) {
        int pos = 0;
        pos += snprintf(resp + pos, resp_len - pos, "[收] ");
        for (int i = 0; i < got && pos < resp_len - 8; i++)
            pos += snprintf(resp + pos, resp_len - pos, "0x%02X ", rb[i]);
        log_msg(LOG_CMD, "%s", resp);
    }
    return true;
}

const char *logic_chip_series(void)
{
    return g_chip.series;
}

uint32_t logic_option_bytes_addr(void)
{
    chip_ctx_t ctx = { cmd_supported(CMD_EXTENDED_ERASE) != 0, g_chip.flash_kb };
    const chip_entry_t *c = chipdb_lookup_ex(g_chip.pid, &ctx);
    if (c && c->option_bytes_addr) return c->option_bytes_addr;
    return 0x1FFFF800u; /* F1 默认 */
}

bool logic_read_option_bytes(uint8_t *out16)
{
    if (!logic_ensure_bl()) return false;
    uint32_t addr = logic_option_bytes_addr();
    log_msg(LOG_INFO, "读取选项字节 @ 0x%08X ...", (unsigned)addr);
    return an3155_read_memory(dbg_bl(), addr, 16, out16);
}

bool logic_write_option_bytes(const uint8_t *data16)
{
    if (!logic_ensure_bl()) return false;
    uint32_t addr = logic_option_bytes_addr();
    log_msg(LOG_WARN, "写入选项字节 @ 0x%08X (需复位生效)", (unsigned)addr);
    return an3155_write_memory(dbg_bl(), addr, data16, 16);
}

bool logic_read_flash_to_hex(const char *out_path)
{
    if (!logic_ensure_bl()) return false;
    uint32_t kb = g_chip.flash_kb ? g_chip.flash_kb : 512;
    uint32_t start = 0x08000000u;
    uint32_t total = kb * 1024u;

    log_msg(LOG_INFO, "开始读取 Flash: 0x%08X, %u KB...", (unsigned)start, (unsigned)kb);

    fw_image_t img;
    fw_image_init(&img);
    fw_segment_t seg;
    seg.start_address = start;
    seg.size = (int)total;
    seg.data = (uint8_t *)malloc(total);
    if (!seg.data) {
        log_msg(LOG_ERROR, "内存不足");
        return false;
    }
    memset(seg.data, 0xFF, total);
    img.segs = &seg;
    img.count = 1;
    img.cap = 1;

    int done = 0;
    while ((uint32_t)done < total) {
        if (g_cancel && *g_cancel) {
            free(seg.data);
            img.segs = NULL;
            img.count = 0;
            log_msg(LOG_WARN, "读取已取消");
            return false;
        }
        int chunk = 256;
        if ((uint32_t)(done + chunk) > total) chunk = (int)(total - done);
        if (!an3155_read_memory(dbg_bl(), start + (uint32_t)done, chunk, seg.data + done)) {
            log_msg(LOG_ERROR, "读取失败: 地址 0x%08X", (unsigned)(start + done));
            free(seg.data);
            img.segs = NULL;
            img.count = 0;
            return false;
        }
        done += chunk;
        if (g_prog) g_prog((int)((uint64_t)done * 100 / total), "读取 Flash...");
    }

    char err[256];
    bool ok = hex_save_file(out_path, &img, err, sizeof(err));
    free(seg.data);
    img.segs = NULL;
    img.count = 0;
    if (!ok) {
        log_msg(LOG_ERROR, "%s", err);
        return false;
    }
    log_msg(LOG_SUCCESS, "Flash 已保存: %s (%u KB)", out_path, (unsigned)kb);
    return true;
}
