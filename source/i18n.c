#include <windows.h>
#include "i18n.h"

typedef struct { const wchar_t *zh; const wchar_t *en; } UiString;
#define UI_STRING(zh, en) { L##zh, L##en },
static const UiString strings[] = {
#include "ui_strings.def"
};
#undef UI_STRING
static int language = UI_LANGUAGE_ZH;

void ui_language_init(void) {
    language = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_CHINESE
        ? UI_LANGUAGE_ZH : UI_LANGUAGE_EN;
}

void ui_language_set(int value) {
    if (value == UI_LANGUAGE_ZH || value == UI_LANGUAGE_EN) language = value;
}

int ui_language_get(void) { return language; }

const wchar_t *ui_text(const wchar_t *text) {
    if (!text) return text;
    for (size_t i = 0; i < sizeof(strings) / sizeof(strings[0]); ++i) {
        /* First match by Chinese key; some translations share English wording. */
        if (wcscmp(text, strings[i].zh) == 0)
            return language == UI_LANGUAGE_EN ? strings[i].en : strings[i].zh;
    }
    for (size_t i = 0; i < sizeof(strings) / sizeof(strings[0]); ++i) {
        if (wcscmp(text, strings[i].en) == 0)
            return language == UI_LANGUAGE_EN ? strings[i].en : strings[i].zh;
    }
    return text;
}
