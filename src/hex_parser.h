#ifndef HEX_PARSER_H
#define HEX_PARSER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t start_address;
    uint8_t *data;
    int size;
} fw_segment_t;

typedef struct {
    fw_segment_t *segs;
    int count;
    int cap;
} fw_image_t;

void fw_image_init(fw_image_t *img);
void fw_image_free(fw_image_t *img);

/* path: UTF-8 或 ANSI 路径 */
bool hex_parse_file(const char *path, fw_image_t *img, char *err, int err_len);
bool bin_parse_file(const char *path, uint32_t start_addr, fw_image_t *img,
                    char *err, int err_len);

/* 统计地址范围 */
bool fw_image_range(const fw_image_t *img, uint32_t *min_addr, uint32_t *max_end);

/* 保存为 Intel HEX（recordBytes 默认 16） */
bool hex_save_file(const char *path, const fw_image_t *img,
                   char *err, int err_len);

#ifdef __cplusplus
}
#endif

#endif
