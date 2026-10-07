#include "flash_prog.h"
#include "log.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>

#define PAGE_SIZE 256

static int canceled(const flash_ctx_t *c)
{
    return c && c->cancel && *c->cancel;
}

static void prog(const flash_ctx_t *c, int pct, const char *st)
{
    if (c && c->progress) c->progress(pct, st);
}

static int cmd_has(const uint8_t *cmds, int n, uint8_t c)
{
    for (int i = 0; i < n; i++)
        if (cmds[i] == c) return 1;
    return 0;
}

bool flash_erase(an3155_t *a, const uint8_t *cmds, int ncmds, const flash_ctx_t *ctx)
{
    int erased = 0;
    prog(ctx, 0, "正在擦除...");
    log_msg(LOG_INFO, "开始擦除芯片...");

    if (cmd_has(cmds, ncmds, CMD_EXTENDED_ERASE)) {
        log_msg(LOG_INFO, "尝试扩展擦除 (0x44)...");
        if (canceled(ctx)) return false;
        if (an3155_extended_erase(a)) {
            erased = 1;
        } else {
            log_msg(LOG_WARN, "扩展擦除失败");
        }
    }
    if (!erased && cmd_has(cmds, ncmds, CMD_ERASE)) {
        if (canceled(ctx)) return false;
        if (an3155_erase(a)) {
            erased = 1;
        } else {
            log_msg(LOG_WARN, "标准擦除失败");
        }
    }
    if (!erased) {
        log_msg(LOG_ERROR, "错误: 芯片不支持擦除命令或无响应");
        if (ncmds > 0) {
            log_msg(LOG_INFO, "尝试 WRITE_UNPROTECT (0x73)...");
            an3155_write_unprotect(a);
        }
        prog(ctx, 0, "擦除失败");
        return false;
    }

    prog(ctx, 50, "正在验证擦除...");
    log_msg(LOG_INFO, "擦除验证中...");
    if (an3155_verify_erase(a, 0x08000000u, 256)) {
        prog(ctx, 100, "擦除完成");
        log_msg(LOG_SUCCESS, "擦除成功");
        return true;
    }
    log_msg(LOG_WARN, "警告: 擦除验证未通过");
    prog(ctx, 100, "擦除完成(验证未通过)");
    return true;
}

bool flash_write_image(an3155_t *a, const fw_image_t *img, bool per_page_verify,
                       const flash_ctx_t *ctx)
{
    /* 统计页数 */
    int total_pages = 0;
    for (int s = 0; s < img->count; s++) {
        int rem = img->segs[s].size;
        while (rem > 0) {
            total_pages++;
            rem -= PAGE_SIZE;
        }
    }
    if (total_pages == 0) {
        log_msg(LOG_ERROR, "没有数据需要写入");
        return false;
    }

    log_msg(LOG_INFO, "开始写入固件，共 %d 页%s...", total_pages,
            per_page_verify ? ", 逐页校验" : "");
    prog(ctx, 0, "写入中...");

    int written = 0;
    uint8_t page[PAGE_SIZE];
    ULONGLONG t0 = GetTickCount64();

    for (int s = 0; s < img->count; s++) {
        const fw_segment_t *seg = &img->segs[s];
        int offset = 0;
        while (offset < seg->size) {
            if (canceled(ctx)) {
                log_msg(LOG_WARN, "写入操作被用户中止");
                return false;
            }
            int chunk = seg->size - offset;
            if (chunk > PAGE_SIZE) chunk = PAGE_SIZE;
            memset(page, 0xFF, PAGE_SIZE);
            memcpy(page, seg->data + offset, chunk);
            uint32_t addr = seg->start_address + (uint32_t)offset;

            if (!an3155_write_memory(a, addr, page, PAGE_SIZE)) {
                log_msg(LOG_ERROR, "写入失败: 地址 0x%08X", (unsigned)addr);
                return false;
            }
            written++;

            if (per_page_verify) {
                uint8_t rb[PAGE_SIZE];
                if (!an3155_read_memory(a, addr, PAGE_SIZE, rb)) {
                    log_msg(LOG_ERROR, "逐页校验读取失败: 0x%08X", (unsigned)addr);
                    return false;
                }
                if (memcmp(page, rb, PAGE_SIZE) != 0) {
                    for (int i = 0; i < PAGE_SIZE; i++) {
                        if (page[i] != rb[i]) {
                            log_msg(LOG_ERROR,
                                    "逐页校验错误: 地址 0x%08X, 期望 0x%02X, 实际 0x%02X",
                                    (unsigned)(addr + i), page[i], rb[i]);
                            return false;
                        }
                    }
                }
            }

            offset += chunk;
            int pct = (written * 100) / total_pages;
            double sec = (double)(GetTickCount64() - t0) / 1000.0;
            char st[96];
            snprintf(st, sizeof(st), "%s中... %d/%d 页  已用时 %.1fs",
                     per_page_verify ? "写入+校验" : "写入", written, total_pages, sec);
            prog(ctx, pct, st);
        }
    }

    log_msg(LOG_SUCCESS, "固件写入完成，共 %d 页%s", written,
            per_page_verify ? " (已逐页校验)" : "");
    prog(ctx, 100, "写入完成");
    return true;
}

bool flash_verify_image(an3155_t *a, const fw_image_t *img, const flash_ctx_t *ctx)
{
    int total = 0;
    for (int s = 0; s < img->count; s++) total += img->segs[s].size;
    if (total == 0) return false;

    log_msg(LOG_INFO, "开始回读校验，共 %d 字节...", total);
    prog(ctx, 0, "校验中...");

    int verified = 0, errors = 0;
    for (int s = 0; s < img->count; s++) {
        const fw_segment_t *seg = &img->segs[s];
        int offset = 0;
        while (offset < seg->size) {
            if (canceled(ctx)) return false;
            int chunk = seg->size - offset;
            if (chunk > PAGE_SIZE) chunk = PAGE_SIZE;
            uint32_t addr = seg->start_address + (uint32_t)offset;
            uint8_t rb[PAGE_SIZE];
            if (!an3155_read_memory(a, addr, chunk, rb)) {
                log_msg(LOG_ERROR, "读取失败: 地址 0x%08X", (unsigned)addr);
                return false;
            }
            for (int i = 0; i < chunk; i++) {
                if (seg->data[offset + i] != rb[i]) {
                    errors++;
                    if (errors <= 10)
                        log_msg(LOG_ERROR, "校验错误: 0x%08X 期望0x%02X 实际0x%02X",
                                (unsigned)(addr + i), seg->data[offset + i], rb[i]);
                }
            }
            offset += chunk;
            verified += chunk;
            int pct = (verified * 100) / total;
            prog(ctx, pct, "校验中...");
        }
    }
    if (errors) {
        log_msg(LOG_ERROR, "校验失败，共 %d 处不匹配", errors);
        return false;
    }
    log_msg(LOG_SUCCESS, "校验通过，数据完全一致");
    prog(ctx, 100, "校验完成");
    return true;
}
