#include "an3155.h"
#include "log.h"
#include <stdio.h>

void an3155_init(an3155_t *a, serial_t *s, volatile LONG *cancel)
{
    a->serial = s;
    a->cancel = cancel;
}

static void log_hex(const char *tag, const uint8_t *d, int n, const char *desc)
{
    char buf[512];
    int pos = 0;
    pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", tag);
    for (int i = 0; i < n && pos < (int)sizeof(buf) - 8; i++)
        pos += snprintf(buf + pos, sizeof(buf) - pos, "0x%02X ", d[i]);
    if (desc && desc[0])
        snprintf(buf + pos, sizeof(buf) - pos, " (%s)", desc);
    log_msg(LOG_CMD, "%s", buf);
}

static bool canceled(an3155_t *a)
{
    return a->cancel && *a->cancel;
}

static bool send_raw(an3155_t *a, const uint8_t *d, int n, const char *desc)
{
    log_hex(">> 发送:", d, n, desc);
    return serial_send(a->serial, d, n);
}

static bool wait_ack(an3155_t *a, int timeout_ms)
{
    int elapsed = 0;
    const int poll = 250;
    while (elapsed < timeout_ms) {
        if (canceled(a)) {
            log_msg(LOG_WARN, "操作已取消");
            return false;
        }
        int chunk = poll;
        if (chunk > timeout_ms - elapsed) chunk = timeout_ms - elapsed;
        uint8_t b = 0;
        int n = serial_recv(a->serial, &b, 1, chunk);
        if (n == 1) {
            log_hex("[收]", &b, 1, b == BL_ACK ? "ACK" : (b == BL_NACK ? "NACK" : "未知"));
            return b == BL_ACK;
        }
        elapsed += chunk;
    }
    log_msg(LOG_ERROR, "超时: 未收到响应");
    return false;
}

static bool send_cmd(an3155_t *a, uint8_t cmd)
{
    uint8_t pkt[2] = { cmd, (uint8_t)(cmd ^ 0xFF) };
    char desc[32];
    snprintf(desc, sizeof(desc), "命令 0x%02X", cmd);
    if (!send_raw(a, pkt, 2, desc)) return false;
    return wait_ack(a, DEFAULT_TIMEOUT_MS);
}

static bool send_addr(an3155_t *a, uint32_t addr)
{
    uint8_t pkt[5];
    pkt[0] = (uint8_t)((addr >> 24) & 0xFF);
    pkt[1] = (uint8_t)((addr >> 16) & 0xFF);
    pkt[2] = (uint8_t)((addr >> 8) & 0xFF);
    pkt[3] = (uint8_t)(addr & 0xFF);
    pkt[4] = (uint8_t)(pkt[0] ^ pkt[1] ^ pkt[2] ^ pkt[3]);
    char desc[40];
    snprintf(desc, sizeof(desc), "地址 0x%08X", (unsigned)addr);
    if (!send_raw(a, pkt, 5, desc)) return false;
    return wait_ack(a, DEFAULT_TIMEOUT_MS);
}

bool an3155_sync(an3155_t *a, int timeout_ms)
{
    uint8_t b = 0x7F;
    serial_clear(a->serial);
    log_msg(LOG_TRACE, "发送 0x7F 进行波特率同步...");
    if (!send_raw(a, &b, 1, "波特率同步")) return false;
    if (!wait_ack(a, timeout_ms)) {
        log_msg(LOG_ERROR, "同步失败: 未收到 ACK");
        return false;
    }
    log_msg(LOG_TRACE, "Bootloader 同步成功");
    return true;
}

bool an3155_get_command(an3155_t *a, uint8_t *version, uint8_t *cmds, int *cmd_count)
{
    log_msg(LOG_TRACE, "获取 Bootloader 版本和支持的命令...");
    if (!send_cmd(a, CMD_GET)) return false;
    uint8_t n = 0;
    if (serial_recv_exact(a->serial, &n, 1, DEFAULT_TIMEOUT_MS) < 1) return false;
    log_hex("[收]", &n, 1, "N=命令数");
    uint8_t data[32];
    if (n + 1 > (int)sizeof(data)) return false;
    if (serial_recv_exact(a->serial, data, n + 1, DEFAULT_TIMEOUT_MS) < n + 1) {
        log_msg(LOG_ERROR, "GET 响应数据不完整");
        return false;
    }
    log_hex("[收]", data, n + 1, "版本+命令");
    if (version) *version = data[0];
    if (cmds && cmd_count) {
        int c = n;
        if (c > 32) c = 32;
        for (int i = 0; i < c; i++) cmds[i] = data[i + 1];
        *cmd_count = c;
    }
    if (!wait_ack(a, DEFAULT_TIMEOUT_MS)) return false;
    log_msg(LOG_TRACE, "Bootloader 版本: 0x%02X, 支持 %u 个命令", data[0], (unsigned)n);
    return true;
}

bool an3155_get_id(an3155_t *a, uint16_t *pid)
{
    log_msg(LOG_TRACE, "获取芯片 ID...");
    if (!send_cmd(a, CMD_GET_ID)) return false;
    uint8_t n = 0;
    if (serial_recv_exact(a->serial, &n, 1, DEFAULT_TIMEOUT_MS) < 1) return false;
    log_hex("[收]", &n, 1, "N=1");
    uint8_t pidb[2];
    if (serial_recv_exact(a->serial, pidb, 2, DEFAULT_TIMEOUT_MS) < 2) {
        log_msg(LOG_ERROR, "GET_ID 响应不完整");
        return false;
    }
    log_hex("[收]", pidb, 2, "PID");
    if (pid) *pid = (uint16_t)((pidb[0] << 8) | pidb[1]);
    if (!wait_ack(a, DEFAULT_TIMEOUT_MS)) return false;
    log_msg(LOG_TRACE, "芯片 PID: 0x%04X (%u)", pid ? *pid : 0, pid ? *pid : 0);
    return true;
}

bool an3155_get_version(an3155_t *a, uint8_t *ver, uint8_t *opt1, uint8_t *opt2)
{
    log_msg(LOG_TRACE, "获取版本信息...");
    if (!send_cmd(a, CMD_GET_VERSION)) return false;
    uint8_t data[3];
    if (serial_recv_exact(a->serial, data, 3, DEFAULT_TIMEOUT_MS) < 3) {
        log_msg(LOG_ERROR, "GET_VERSION 响应不完整");
        return false;
    }
    log_hex("[收]", data, 3, "版本+选项字节");
    if (ver) *ver = data[0];
    if (opt1) *opt1 = data[1];
    if (opt2) *opt2 = data[2];
    if (!wait_ack(a, DEFAULT_TIMEOUT_MS)) return false;
    log_msg(LOG_TRACE, "版本: 0x%02X, 选项字节: 0x%02X 0x%02X", data[0], data[1], data[2]);
    return true;
}

bool an3155_erase(an3155_t *a)
{
    uint8_t param[2] = { 0xFF, 0x00 };
    log_msg(LOG_INFO, "执行标准全局擦除...");
    if (!send_cmd(a, CMD_ERASE)) return false;
    if (!send_raw(a, param, 2, "全局擦除参数")) return false;
    if (!wait_ack(a, 15000)) {
        log_msg(LOG_ERROR, "擦除操作失败或超时");
        return false;
    }
    log_msg(LOG_SUCCESS, "全局擦除完成");
    return true;
}

bool an3155_extended_erase(an3155_t *a)
{
    uint8_t param[3] = { 0xFF, 0xFF, 0x00 };
    log_msg(LOG_INFO, "执行扩展全局擦除...");
    if (!send_cmd(a, CMD_EXTENDED_ERASE)) return false;
    if (!send_raw(a, param, 3, "扩展全局擦除参数")) return false;
    if (!wait_ack(a, 30000)) {
        log_msg(LOG_ERROR, "扩展擦除操作失败或超时");
        return false;
    }
    log_msg(LOG_SUCCESS, "扩展全局擦除完成");
    return true;
}

bool an3155_write_memory(an3155_t *a, uint32_t addr, const uint8_t *data, int len)
{
    if (len <= 0 || len > 256) {
        log_msg(LOG_ERROR, "写入数据大小无效: %d 字节", len);
        return false;
    }
    if (!send_cmd(a, CMD_WRITE_MEMORY)) return false;
    if (!send_addr(a, addr)) return false;

    uint8_t pkt[1 + 256 + 1];
    uint8_t n = (uint8_t)(len - 1);
    uint8_t cs = n;
    pkt[0] = n;
    for (int i = 0; i < len; i++) {
        pkt[1 + i] = data[i];
        cs ^= data[i];
    }
    pkt[1 + len] = cs;
    char desc[48];
    snprintf(desc, sizeof(desc), "N=%u 数据%d字节", n, len);
    if (!send_raw(a, pkt, len + 2, desc)) return false;
    return wait_ack(a, DEFAULT_TIMEOUT_MS);
}

bool an3155_read_memory(an3155_t *a, uint32_t addr, int len, uint8_t *out)
{
    if (len <= 0 || len > 256) {
        log_msg(LOG_ERROR, "读取大小无效: %d", len);
        return false;
    }
    if (!send_cmd(a, CMD_READ_MEMORY)) return false;
    if (!send_addr(a, addr)) return false;
    uint8_t n = (uint8_t)(len - 1);
    uint8_t pkt[2] = { n, (uint8_t)(n ^ 0xFF) };
    if (!send_raw(a, pkt, 2, "字节数")) return false;
    if (!wait_ack(a, DEFAULT_TIMEOUT_MS)) return false;
    int got = serial_recv_exact(a->serial, out, len, DEFAULT_TIMEOUT_MS);
    if (got < len) {
        log_msg(LOG_ERROR, "读取数据不完整: 期望 %d, 实际 %d", len, got);
        return false;
    }
    log_hex("[收]", out, len, "数据");
    return true;
}

bool an3155_go(an3155_t *a, uint32_t addr)
{
    log_msg(LOG_TRACE, "跳转到地址 0x%08X 执行...", (unsigned)addr);
    if (!send_cmd(a, CMD_GO)) return false;
    if (!send_addr(a, addr)) return false;
    log_msg(LOG_TRACE, "跳转命令已发送");
    return true;
}

bool an3155_write_unprotect(an3155_t *a)
{
    log_msg(LOG_INFO, "解除写保护...");
    if (!send_cmd(a, CMD_WRITE_UNPROTECT)) return false;
    log_msg(LOG_INFO, "写保护已解除");
    return true;
}

bool an3155_readout_unprotect(an3155_t *a)
{
    log_msg(LOG_WARN, "解除读保护 (0x92) — 会触发全片 Flash 擦除!");
    if (!send_cmd(a, CMD_READOUT_UNPROTECT)) return false;
    log_msg(LOG_SUCCESS, "读保护解除命令已发送");
    return true;
}

uint32_t an3155_read_flash_size(an3155_t *a, uint32_t reg_addr)
{
    log_msg(LOG_TRACE, "读取Flash大小寄存器 0x%08X...", (unsigned)reg_addr);
    if (!send_cmd(a, CMD_READ_MEMORY)) return 0;
    if (!send_addr(a, reg_addr)) return 0;
    uint8_t n = 1;
    uint8_t pkt[2] = { n, (uint8_t)(n ^ 0xFF) };
    if (!serial_send(a->serial, pkt, 2)) return 0;
    if (!wait_ack(a, DEFAULT_TIMEOUT_MS)) return 0;
    uint8_t data[2];
    if (serial_recv_exact(a->serial, data, 2, DEFAULT_TIMEOUT_MS) < 2) return 0;
    uint32_t kb = (uint32_t)data[0] | ((uint32_t)data[1] << 8);
    log_msg(LOG_SUCCESS, "芯片实际Flash大小: %u KB", (unsigned)kb);
    return kb;
}

bool an3155_verify_erase(an3155_t *a, uint32_t addr, int size)
{
    if (size <= 0) size = 256;
    if (size > 256) size = 256;
    uint8_t data[256];
    if (!an3155_read_memory(a, addr, size, data)) return false;
    for (int i = 0; i < size; i++) {
        if (data[i] != 0xFF) {
            log_msg(LOG_ERROR, "擦除验证失败: 地址 0x%08X 处值为 0x%02X",
                    (unsigned)(addr + i), data[i]);
            return false;
        }
    }
    log_msg(LOG_SUCCESS, "擦除验证通过: 0x%08X 处 %d 字节全为 0xFF", (unsigned)addr, size);
    return true;
}
