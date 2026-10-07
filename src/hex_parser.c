#include "hex_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void fw_image_init(fw_image_t *img)
{
    memset(img, 0, sizeof(*img));
}

void fw_image_free(fw_image_t *img)
{
    if (!img) return;
    for (int i = 0; i < img->count; i++)
        free(img->segs[i].data);
    free(img->segs);
    memset(img, 0, sizeof(*img));
}

static int hex_nibble(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static int add_segment(fw_image_t *img, uint32_t addr, const uint8_t *data, int size)
{
    if (size <= 0) return 0;
    /* 尝试与上一段合并 */
    if (img->count > 0) {
        fw_segment_t *last = &img->segs[img->count - 1];
        if (last->start_address + (uint32_t)last->size == addr) {
            uint8_t *nd = (uint8_t *)realloc(last->data, (size_t)last->size + size);
            if (!nd) return -1;
            memcpy(nd + last->size, data, size);
            last->data = nd;
            last->size += size;
            return 0;
        }
    }
    if (img->count >= img->cap) {
        int ncap = img->cap ? img->cap * 2 : 8;
        fw_segment_t *ns = (fw_segment_t *)realloc(img->segs, sizeof(fw_segment_t) * ncap);
        if (!ns) return -1;
        img->segs = ns;
        img->cap = ncap;
    }
    fw_segment_t *seg = &img->segs[img->count];
    seg->start_address = addr;
    seg->size = size;
    seg->data = (uint8_t *)malloc((size_t)size);
    if (!seg->data) return -1;
    memcpy(seg->data, data, size);
    img->count++;
    return 0;
}

bool hex_parse_file(const char *path, fw_image_t *img, char *err, int err_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(err, err_len, "无法打开文件: %s", path);
        return false;
    }
    fw_image_init(img);

    char line[600];
    uint32_t base = 0;
    int line_num = 0;
    bool ok = true;

    while (fgets(line, sizeof(line), f)) {
        line_num++;
        /* trim */
        char *p = line;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        size_t L = strlen(p);
        while (L && (p[L - 1] == '\r' || p[L - 1] == '\n' || p[L - 1] == ' '))
            p[--L] = 0;
        if (!L) continue;
        if (*p != ':') {
            snprintf(err, err_len, "第 %d 行: 格式错误，必须以 ':' 开头", line_num);
            ok = false;
            break;
        }
        p++;
        size_t hexlen = strlen(p);
        if (hexlen < 10 || (hexlen % 2) != 0) {
            snprintf(err, err_len, "第 %d 行: 长度无效", line_num);
            ok = false;
            break;
        }
        uint8_t rec[300];
        int nbytes = (int)(hexlen / 2);
        if (nbytes > (int)sizeof(rec)) {
            snprintf(err, err_len, "第 %d 行: 记录过长", line_num);
            ok = false;
            break;
        }
        for (int i = 0; i < nbytes; i++) {
            int hi = hex_nibble(p[i * 2]);
            int lo = hex_nibble(p[i * 2 + 1]);
            if (hi < 0 || lo < 0) {
                snprintf(err, err_len, "第 %d 行: 无效十六进制", line_num);
                ok = false;
                break;
            }
            rec[i] = (uint8_t)((hi << 4) | lo);
        }
        if (!ok) break;

        uint8_t ll = rec[0];
        uint16_t aaa = (uint16_t)((rec[1] << 8) | rec[2]);
        uint8_t tt = rec[3];
        if (nbytes < ll + 5) {
            snprintf(err, err_len, "第 %d 行: 数据长度不足", line_num);
            ok = false;
            break;
        }
        uint8_t sum = 0;
        for (int i = 0; i < ll + 5; i++) sum = (uint8_t)(sum + rec[i]);
        if (sum != 0) {
            snprintf(err, err_len, "第 %d 行: 校验和错误", line_num);
            ok = false;
            break;
        }

        if (tt == 0x00) {
            if (add_segment(img, base + aaa, rec + 4, ll) < 0) {
                snprintf(err, err_len, "内存不足");
                ok = false;
                break;
            }
        } else if (tt == 0x01) {
            break;
        } else if (tt == 0x02) {
            base = ((uint32_t)rec[4] << 8 | rec[5]) << 4;
        } else if (tt == 0x04) {
            base = ((uint32_t)rec[4] << 8 | rec[5]) << 16;
        } else if (tt == 0x05) {
            /* start address — ignore for flash write */
        }
    }
    fclose(f);
    if (ok && img->count == 0) {
        snprintf(err, err_len, "文件中无有效数据记录");
        return false;
    }
    return ok;
}

bool bin_parse_file(const char *path, uint32_t start_addr, fw_image_t *img,
                    char *err, int err_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(err, err_len, "无法打开文件: %s", path);
        return false;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) {
        fclose(f);
        snprintf(err, err_len, "文件为空");
        return false;
    }
    /* 防止异常大文件导致界面长时间无响应 */
    if (sz > 8L * 1024 * 1024) {
        fclose(f);
        snprintf(err, err_len, "BIN 过大(%ld 字节)，请确认文件是否正确", sz);
        return false;
    }
    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) {
        fclose(f);
        snprintf(err, err_len, "内存不足");
        return false;
    }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        snprintf(err, err_len, "读取失败");
        return false;
    }
    fclose(f);
    fw_image_init(img);
    if (add_segment(img, start_addr, buf, (int)sz) < 0) {
        free(buf);
        snprintf(err, err_len, "内存不足");
        return false;
    }
    free(buf);
    return true;
}

bool fw_image_range(const fw_image_t *img, uint32_t *min_addr, uint32_t *max_end)
{
    if (!img || img->count == 0) return false;
    uint32_t mn = 0xFFFFFFFFu, mx = 0;
    for (int i = 0; i < img->count; i++) {
        if (img->segs[i].start_address < mn) mn = img->segs[i].start_address;
        uint32_t end = img->segs[i].start_address + (uint32_t)img->segs[i].size;
        if (end > mx) mx = end;
    }
    if (min_addr) *min_addr = mn;
    if (max_end) *max_end = mx;
    return true;
}

static void hex_line(FILE *f, uint8_t type, uint16_t addr, const uint8_t *data, int len)
{
    uint8_t sum = (uint8_t)len + (uint8_t)((addr >> 8) & 0xFF) + (uint8_t)(addr & 0xFF) + type;
    fprintf(f, ":%02X%04X%02X", len, addr, type);
    for (int i = 0; i < len; i++) {
        fprintf(f, "%02X", data[i]);
        sum = (uint8_t)(sum + data[i]);
    }
    fprintf(f, "%02X\n", (uint8_t)(0x100 - sum) & 0xFF);
}

bool hex_save_file(const char *path, const fw_image_t *img, char *err, int err_len)
{
    if (!img || img->count == 0) {
        snprintf(err, err_len, "无数据可保存");
        return false;
    }
    FILE *f = fopen(path, "wb");
    if (!f) {
        snprintf(err, err_len, "无法创建文件: %s", path);
        return false;
    }
    const int rec = 16;
    uint16_t last_ela = 0xFFFF;
    for (int s = 0; s < img->count; s++) {
        const fw_segment_t *seg = &img->segs[s];
        int off = 0;
        while (off < seg->size) {
            uint32_t abs = seg->start_address + (uint32_t)off;
            uint16_t ela = (uint16_t)((abs >> 16) & 0xFFFF);
            if (ela != last_ela) {
                uint8_t ext[2] = { (uint8_t)(ela >> 8), (uint8_t)(ela & 0xFF) };
                hex_line(f, 0x04, 0, ext, 2);
                last_ela = ela;
            }
            int chunk = seg->size - off;
            if (chunk > rec) chunk = rec;
            hex_line(f, 0x00, (uint16_t)(abs & 0xFFFF), seg->data + off, chunk);
            off += chunk;
        }
    }
    hex_line(f, 0x01, 0, NULL, 0);
    fclose(f);
    return true;
}
