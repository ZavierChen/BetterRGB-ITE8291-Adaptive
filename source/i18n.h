#ifndef BETTERRGB_I18N_H
#define BETTERRGB_I18N_H
#include <wchar.h>

enum { UI_LANGUAGE_ZH = 0, UI_LANGUAGE_EN = 1 };
void ui_language_init(void);
void ui_language_set(int language);
int ui_language_get(void);
/* Accepts either language, allowing existing controls to be relabeled in place. */
const wchar_t *ui_text(const wchar_t *text);
#endif
