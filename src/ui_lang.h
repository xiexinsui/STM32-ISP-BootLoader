#ifndef UI_LANG_H
#define UI_LANG_H

#ifdef __cplusplus
extern "C" {
#endif

/* 0=中文, 1=English */
extern int g_lang;

const char *L(const char *zh, const char *en);
void ui_lang_set(int lang);
int ui_lang_get(void);

#ifdef __cplusplus
}
#endif

#endif
