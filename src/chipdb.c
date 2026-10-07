#include "chipdb.h"
#include <string.h>

/* 与 Qt ISP_1.2 chipdb.cpp 对齐（含 F429 PID 0x419 等） */
static const chip_entry_t TABLE[] = {
    /* F0 */
    { 0x0440, "STM32F030xC", "F0", 256, 2048, 32, 0x1FFFF7CC, 0x1FFFF800 },
    { 0x0441, "STM32F05xxx", "F0", 64, 1024, 8, 0x1FFFF7CC, 0x1FFFF800 },
    { 0x0442, "STM32F091xC", "F0", 256, 2048, 32, 0x1FFFF7CC, 0x1FFFF800 },
    { 0x0444, "STM32F030x8", "F0", 64, 1024, 8, 0x1FFFF7CC, 0x1FFFF800 },
    { 0x0445, "STM32F070x6/F030x6", "F0", 32, 1024, 6, 0x1FFFF7CC, 0x1FFFF800 },
    { 0x0448, "STM32F070xB/F072xB", "F0", 128, 2048, 16, 0x1FFFF7CC, 0x1FFFF800 },
    { 0x044C, "STM32F03xxx", "F0", 32, 1024, 8, 0x1FFFF7CC, 0x1FFFF800 },
    { 0x045C, "STM32F04xxx", "F0", 32, 1024, 6, 0x1FFFF7CC, 0x1FFFF800 },
    /* F1 */
    { 0x0410, "STM32F10x MD", "F1", 128, 1024, 20, 0x1FFFF7E0, 0x1FFFF800 },
    { 0x0412, "STM32F10x LD", "F1", 32, 1024, 10, 0x1FFFF7E0, 0x1FFFF800 },
    { 0x0414, "STM32F10x HD", "F1", 256, 2048, 64, 0x1FFFF7E0, 0x1FFFF800 },
    { 0x0416, "STM32F10x VL MD", "F1", 128, 1024, 24, 0x1FFFF7E0, 0x1FFFF800 },
    { 0x0418, "STM32F10x VL LD", "F1", 32, 1024, 8, 0x1FFFF7E0, 0x1FFFF800 },
    { 0x041B, "STM32F10x VL HD", "F1", 512, 2048, 64, 0x1FFFF7E0, 0x1FFFF800 },
    { 0x0420, "STM32F10x VL HD", "F1", 512, 2048, 64, 0x1FFFF7E0, 0x1FFFF800 },
    { 0x0428, "STM32F10x XL", "F1", 1024, 2048, 96, 0x1FFFF7E0, 0x1FFFF800 },
    { 0x0430, "STM32F10x XL", "F1", 1024, 2048, 96, 0x1FFFF7E0, 0x1FFFF800 },
    { 0x0411, "STM32F10x CL", "F1", 256, 1024, 48, 0x1FFFF7E0, 0x1FFFF800 },
    /* F2 */
    { 0x0411, "STM32F205/207/215/217", "F2", 1024, 16384, 128, 0x1FFF7A22, 0x1FFFC000 },
    /* F3 */
    { 0x0422, "STM32F302xB(C)/303xB(C)", "F3", 256, 2048, 40, 0x1FFFF7CC, 0x1FFFF800 },
    { 0x0432, "STM32F37x/F378", "F3", 256, 2048, 32, 0x1FFFF7CC, 0x1FFFF800 },
    { 0x0438, "STM32F303x4(6/8)/334", "F3", 64, 2048, 16, 0x1FFFF7CC, 0x1FFFF800 },
    { 0x0439, "STM32F30x/F33x (64K)", "F3", 64, 2048, 16, 0x1FFFF7CC, 0x1FFFF800 },
    { 0x0446, "STM32F302x6(8)/303x6(8)", "F3", 64, 2048, 16, 0x1FFFF7CC, 0x1FFFF800 },
    /* F4 — AN2606: F42x/43x PID = 0x419 */
    { 0x0413, "STM32F405xx/407xx", "F4", 1024, 16384, 128, 0x1FFF7A22, 0x1FFFC000 },
    { 0x0419, "STM32F42xxx/43xxx (F429)", "F4", 2048, 16384, 256, 0x1FFF7A22, 0x1FFFC000 },
    { 0x0415, "STM32F427/429/437/439", "F4", 2048, 16384, 256, 0x1FFF7A22, 0x1FFFC000 },
    { 0x0417, "STM32F401xB(C)", "F4", 256, 16384, 64, 0x1FFF7A22, 0x1FFFC000 },
    { 0x0421, "STM32F446xx", "F4", 512, 16384, 128, 0x1FFF7A22, 0x1FFFC000 },
    { 0x0423, "STM32F401xD(E)", "F4", 512, 16384, 96, 0x1FFF7A22, 0x1FFFC000 },
    { 0x0431, "STM32F410/411", "F4", 256, 16384, 128, 0x1FFF7A22, 0x1FFFC000 },
    { 0x0433, "STM32F412xG", "F4", 1024, 16384, 256, 0x1FFF7A22, 0x1FFFC000 },
    { 0x0434, "STM32F469/479", "F4", 2048, 16384, 320, 0x1FFF7A22, 0x1FFFC000 },
    { 0x0441, "STM32F412xG (rev Z)", "F4", 1024, 16384, 256, 0x1FFF7A22, 0x1FFFC000 },
    { 0x0458, "STM32F410x8(B) (rev A)", "F4", 128, 16384, 32, 0x1FFF7A22, 0x1FFFC000 },
    { 0x0463, "STM32F413/423", "F4", 512, 16384, 320, 0x1FFF7A22, 0x1FFFC000 },
    /* F7 */
    { 0x0449, "STM32F745/746/756", "F7", 1024, 32768, 320, 0x1FF0F442, 0x1FFF0000 },
    { 0x0451, "STM32F765/767/768/769", "F7", 2048, 32768, 512, 0x1FF0F442, 0x1FFF0000 },
    { 0x0452, "STM32F722/723/732/733", "F7", 512, 32768, 256, 0x1FF0F442, 0x1FFF0000 },
    { 0x0453, "STM32F765/767 (rev A)", "F7", 2048, 32768, 512, 0x1FF0F442, 0x1FFF0000 },
    { 0x0455, "STM32F72x/73x", "F7", 512, 32768, 256, 0x1FF0F442, 0x1FFF0000 },
    { 0x0459, "STM32F74x/75x", "F7", 1024, 32768, 320, 0x1FF0F442, 0x1FFF0000 },
    /* L0 */
    { 0x0425, "STM32L031xx/041xx", "L0", 32, 128, 8, 0x1FF8007C, 0x1FF80000 },
    { 0x0417, "STM32L07xxx/08xxx", "L0", 192, 128, 20, 0x1FF8007C, 0x1FF80000 },
    { 0x0447, "STM32L05xxx/06xxx", "L0", 64, 128, 8, 0x1FF8007C, 0x1FF80000 },
    { 0x0456, "STM32L04xxx/05xxx", "L0", 32, 128, 8, 0x1FF8007C, 0x1FF80000 },
    { 0x0457, "STM32L05xxx/06xxx (Cat3)", "L0", 32, 128, 8, 0x1FF8007C, 0x1FF80000 },
    { 0x0465, "STM32L01xxx/02xxx", "L0", 32, 128, 8, 0x1FF8007C, 0x1FF80000 },
    { 0x0470, "STM32L07xxx/08xxx (Cat5)", "L0", 192, 128, 20, 0x1FF8007C, 0x1FF80000 },
    /* L1 */
    { 0x0416, "STM32L1xxx6(8/B) MD", "L1", 128, 256, 32, 0x1FF8004C, 0x1FF80000 },
    { 0x0429, "STM32L1xxx6(8/B) MD-A", "L1", 128, 256, 32, 0x1FF8004C, 0x1FF80000 },
    { 0x042B, "STM32L1xxx6(8/B) MD (rev Y)", "L1", 128, 256, 32, 0x1FF8004C, 0x1FF80000 },
    { 0x0436, "STM32L1xxxD", "L1", 384, 256, 48, 0x1FF8004C, 0x1FF80000 },
    { 0x0437, "STM32L1xxxC", "L1", 256, 256, 48, 0x1FF8004C, 0x1FF80000 },
    /* L4 — 0x0415 与 F42x 冲突，lookup_ex 消歧 */
    { 0x0415, "STM32L47xxx/48xxx", "L4", 1024, 2048, 128, 0x1FFF75E0, 0x1FFF7800 },
    { 0x0435, "STM32L43xxx/44xxx", "L4", 256, 2048, 64, 0x1FFF75E0, 0x1FFF7800 },
    { 0x0461, "STM32L496xx/4A6xx", "L4", 1024, 2048, 320, 0x1FFF75E0, 0x1FFF7800 },
    { 0x0462, "STM32L45xxx/46xxx", "L4", 512, 2048, 160, 0x1FFF75E0, 0x1FFF7800 },
    { 0x0464, "STM32L41xxx/42xxx", "L4", 128, 2048, 40, 0x1FFF75E0, 0x1FFF7800 },
    { 0x046D, "STM32L47x/48x (rev B)", "L4", 1024, 2048, 128, 0x1FFF75E0, 0x1FFF7800 },
    { 0x046F, "STM32L43x/44x (rev B)", "L4", 256, 2048, 64, 0x1FFF75E0, 0x1FFF7800 },
    { 0x0471, "STM32L412/422", "L4", 128, 2048, 40, 0x1FFF75E0, 0x1FFF7800 },
    { 0x0474, "STM32L4P5/Q5", "L4+", 1024, 2048, 320, 0x1FFF75E0, 0x1FFF7800 },
    { 0x0475, "STM32L4R5/S5", "L4+", 2048, 2048, 640, 0x1FFF75E0, 0x1FFF7800 },
    /* L5 / U5 */
    { 0x0472, "STM32L552/562", "L5", 512, 2048, 256, 0x1FFF75E0, 0x1FFF7800 },
    { 0x0482, "STM32U575/585", "U5", 2048, 8192, 768, 0x0BF96E80, 0x0BF96E80 },
    /* G0 / G4 */
    { 0x044B, "STM32G05x/06x", "G0", 64, 2048, 18, 0x1FFF75E0, 0x1FFF7800 },
    { 0x0460, "STM32G07xxx/08xxx", "G0", 64, 2048, 36, 0x1FFF75E0, 0x1FFF7800 },
    { 0x0466, "STM32G03xxx/04xxx", "G0", 32, 2048, 8, 0x1FFF75E0, 0x1FFF7800 },
    { 0x0467, "STM32G0Bxxx/Cxxx", "G0", 512, 2048, 144, 0x1FFF75E0, 0x1FFF7800 },
    { 0x046C, "STM32G0B0/C0", "G0", 512, 2048, 144, 0x1FFF75E0, 0x1FFF7800 },
    { 0x0468, "STM32G431/441", "G4", 128, 2048, 32, 0x1FFF75E0, 0x1FFF7800 },
    { 0x0469, "STM32G47xxx/48xxx", "G4", 512, 2048, 128, 0x1FFF75E0, 0x1FFF7800 },
    { 0x0478, "STM32G484", "G4", 512, 2048, 128, 0x1FFF75E0, 0x1FFF7800 },
    { 0x0479, "STM32G491/4A1", "G4", 512, 2048, 128, 0x1FFF75E0, 0x1FFF7800 },
    /* H5 / H7 */
    { 0x0486, "STM32H503", "H5", 128, 8192, 32, 0x1FF1E880, 0x1FF00000 },
    { 0x0487, "STM32H563/573", "H5", 2048, 8192, 640, 0x1FF1E880, 0x1FF00000 },
    { 0x0488, "STM32H562", "H5", 1024, 8192, 320, 0x1FF1E880, 0x1FF00000 },
    { 0x0489, "STM32H5xx", "H5", 2048, 8192, 640, 0x1FF1E880, 0x1FF00000 },
    { 0x0450, "STM32H743/753/750", "H7", 2048, 131072, 1024, 0x1FF1E880, 0x1FF00000 },
    { 0x0480, "STM32H7A3/7B3/7B0", "H7", 2048, 131072, 1024, 0x1FF1E880, 0x1FF00000 },
    { 0x0481, "STM32H742/743 (rev V)", "H7", 2048, 131072, 688, 0x1FF1E880, 0x1FF00000 },
    { 0x0483, "STM32H723/725/733/735", "H7", 1024, 131072, 560, 0x1FF1E880, 0x1FF00000 },
    { 0x0490, "STM32H7Ax/7Bx (rev Z)", "H7", 2048, 131072, 1024, 0x1FF1E880, 0x1FF00000 },
    /* C0 / WB / WBA / WL */
    { 0x0443, "STM32C011/031", "C0", 32, 2048, 12, 0, 0 },
    { 0x0493, "STM32WB35/30", "WB", 512, 4096, 96, 0, 0 },
    { 0x0494, "STM32WB55xx", "WB", 1024, 4096, 256, 0, 0 },
    { 0x0495, "STM32WB15xx/10xx", "WB", 320, 4096, 48, 0, 0 },
    { 0x0485, "STM32WBA52", "WBA", 1024, 8192, 96, 0, 0 },
    { 0x0492, "STM32WBA54/55", "WBA", 1024, 8192, 128, 0, 0 },
    { 0x0496, "STM32WLEx", "WL", 256, 2048, 48, 0, 0 },
    { 0x0497, "STM32WL55xx", "WL", 256, 2048, 64, 0, 0 },
    { 0x0498, "STM32WLE5xx/WLE4xx", "WL", 256, 2048, 64, 0, 0 },
    { 0x0499, "STM32WL5x", "WL", 256, 2048, 64, 0, 0 },
};

static int series_f1_like(const char *s)
{
    return !strcmp(s, "F0") || !strcmp(s, "F1") || !strcmp(s, "F3");
}

static int series_f4_like(const char *s)
{
    return !strcmp(s, "F2") || (s[0] == 'F' && s[1] == '4');
}

const chip_entry_t *chipdb_lookup(uint16_t pid)
{
    for (size_t i = 0; i < sizeof(TABLE) / sizeof(TABLE[0]); i++)
        if (TABLE[i].pid == pid)
            return &TABLE[i];
    return NULL;
}

const chip_entry_t *chipdb_lookup_ex(uint16_t pid, const chip_ctx_t *ctx)
{
    const chip_entry_t *best = NULL;
    int best_score = -1000000000;
    int found = 0;

    for (size_t i = 0; i < sizeof(TABLE) / sizeof(TABLE[0]); i++) {
        const chip_entry_t *c = &TABLE[i];
        if (c->pid != pid) continue;
        found++;
        if (!ctx) {
            if (!best) best = c;
            continue;
        }
        int score = 0;
        int f1 = series_f1_like(c->series);
        int f4 = series_f4_like(c->series);
        if (ctx->has_extended_erase) {
            if (f4) score += 50;
            if (c->series[0] == 'L' && c->series[1] == '4') score += 20;
            if (f1) score -= 40;
        } else {
            if (f1) score += 50;
            if (f4) score -= 40;
        }
        if (ctx->flash_size_kb > 0 && c->flash_size_kb > 0) {
            uint32_t a = c->flash_size_kb, b = ctx->flash_size_kb;
            uint32_t diff = a > b ? a - b : b - a;
            if (diff == 0) score += 30;
            else if (diff <= 128) score += 10;
            else if (diff >= 1024) score -= 20;
            if (ctx->flash_size_kb >= 1024 && f4) score += 15;
            if (ctx->flash_size_kb <= 512 && f1 && !ctx->has_extended_erase) score += 10;
        }
        if (score > best_score) {
            best_score = score;
            best = c;
        }
    }
    if (found == 0) return NULL;
    return best ? best : chipdb_lookup(pid);
}
