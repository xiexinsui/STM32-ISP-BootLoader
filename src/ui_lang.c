#include "ui_lang.h"

int g_lang = 0;

const char *L(const char *zh, const char *en)
{
    return (g_lang == 1 && en) ? en : zh;
}

void ui_lang_set(int lang)
{
    g_lang = (lang == 1) ? 1 : 0;
}

int ui_lang_get(void)
{
    return g_lang;
}
