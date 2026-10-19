/* Hardware-free integration test: never calls WinMain, HID init or worker threads. */
#include "../source/better_rgb.c"
#include <assert.h>

static int checked;
static BOOL CALLBACK check_english(HWND control, LPARAM unused) {
    (void)unused;
    wchar_t text[1024], cls[32];
    if (GetDlgCtrlID(control) == IDC_COMBO_LANGUAGE) return TRUE;
    GetWindowTextW(control, text, 1024);
    if (wcscmp(text, L"Language / 语言") == 0) return TRUE;
    for (const wchar_t *p = text; *p; ++p) assert(*p < 0x4e00 || *p > 0x9fff);
    GetClassNameW(control, cls, 32);
    if (wcscmp(cls, L"ComboBox") == 0) {
        LRESULT count = SendMessageW(control, CB_GETCOUNT, 0, 0);
        for (LRESULT i = 0; i < count; ++i) {
            assert(SendMessageW(control, CB_GETLBTEXTLEN, (WPARAM)i, 0) < 1024);
            SendMessageW(control, CB_GETLBTEXT, (WPARAM)i, (LPARAM)text);
            for (const wchar_t *p = text; *p; ++p) assert(*p < 0x4e00 || *p > 0x9fff);
        }
    }
    ++checked;
    return TRUE;
}

static LRESULT CALLBACK test_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_TIMER || msg == WM_DEVICECHANGE || msg == WM_POWERBROADCAST) return 0;
    if (msg == WM_DESTROY) {
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        return 0;
    }
    return WndProc(hwnd, msg, wp, lp);
}

static void snapshot(HWND hwnd, const char *path) {
    RECT rc; GetWindowRect(hwnd, &rc);
    int width = rc.right - rc.left, height = rc.bottom - rc.top;
    HDC dc = GetDC(hwnd), memory = CreateCompatibleDC(dc);
    BITMAPINFO info = {0};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    void *pixels = NULL;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, NULL, 0);
    assert(bitmap);
    HGDIOBJ old = SelectObject(memory, bitmap);
    assert(PrintWindow(hwnd, memory, 0));
    BITMAPFILEHEADER header = {0};
    header.bfType = 0x4d42;
    header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
    header.bfSize = header.bfOffBits + (DWORD)(width * height * 4);
    FILE *f = fopen(path, "wb"); assert(f);
    fwrite(&header, sizeof(header), 1, f);
    fwrite(&info.bmiHeader, sizeof(BITMAPINFOHEADER), 1, f);
    fwrite(pixels, (size_t)width * height * 4, 1, f);
    fclose(f);
    SelectObject(memory, old); DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(hwnd, dc);
}

int main(void) {
    /* Config/log files belong to this test's output directory, never the installed app. */
    build_sibling_path(g_cfgPath, "test-language.cfg");
    build_sibling_path(g_logPath, "test-ui.log");
    ui_language_init();
    ui_language_set(UI_LANGUAGE_ZH);
    g_activeMode = -1;
    g_acTouchbarMode = TOUCHBAR_CPU;
    g_batteryTouchbarMode = TOUCHBAR_AUDIO;
    g_waveAngle = 123;
    g_lightningConcurrent = 4;
    g_brightness = 73;
    g_whiteGainR = 644; g_whiteGainG = 1000; g_whiteGainB = 791;
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_BAR_CLASSES};
    assert(InitCommonControlsEx(&icc));
    HINSTANCE instance = GetModuleHandleW(NULL);
    WNDCLASSW swc = {0};
    swc.lpfnWndProc = SwatchProc; swc.hInstance = instance; swc.lpszClassName = L"SwatchClass";
    assert(RegisterClassW(&swc));
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = test_wndproc; wc.hInstance = instance; wc.lpszClassName = L"BetterRGBUiTest";
    g_bgBrush = CreateSolidBrush(CLR_BG); wc.hbrBackground = g_bgBrush;
    assert(RegisterClassW(&wc));
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"UI test", WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,
        0, 0, 760, 540, NULL, NULL, instance, NULL);
    assert(hwnd);
    HWND combo = GetDlgItem(hwnd, IDC_COMBO_LANGUAGE);
    assert(combo);
    for (int pass = 0; pass < 4; ++pass) {
        int language = pass % 2 == 0 ? UI_LANGUAGE_EN : UI_LANGUAGE_ZH;
        SendMessageW(combo, CB_SETCURSEL, language, 0);
        SendMessageW(hwnd, WM_COMMAND, MAKEWPARAM(IDC_COMBO_LANGUAGE, CBN_SELCHANGE), (LPARAM)combo);
        assert(ui_language_get() == language);
        assert(g_brightness == 73 && g_whiteGainR == 644 && g_whiteGainB == 791);
        assert(g_acTouchbarMode == TOUCHBAR_CPU && g_batteryTouchbarMode == TOUCHBAR_AUDIO);
        assert(g_waveAngle == 123 && g_lightningConcurrent == 4 && g_activeMode == -1);
        assert(SendMessageW(g_hComboMode, CB_GETCOUNT, 0, 0) == MODE_COUNT);
        if (language == UI_LANGUAGE_EN) EnumChildWindows(hwnd, check_english, 0);
        ui_language_set(1 - language); load_config(); assert(ui_language_get() == language);
    }
    ui_language_set(UI_LANGUAGE_EN); refresh_ui_language(hwnd);
    for (int page = 0; page < 5; ++page) {
        show_ui_page(page);
        if (page == UI_PAGE_EFFECTS) show_panel_for_mode(MODE_SCREEN_AMBIENT);
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN);
        char path[64]; snprintf(path, sizeof(path), "ui-en-%d.bmp", page);
        snapshot(hwnd, path);
    }
    ui_language_set(UI_LANGUAGE_ZH); refresh_ui_language(hwnd);
    show_ui_page(UI_PAGE_PROGRAM); snapshot(hwnd, "ui-zh-settings.bmp");
    DestroyWindow(hwnd);
    for (int i = 0; i < 256; ++i)
        assert(linear_channel_to_srgb(srgb_channel_to_linear((unsigned char)i)) == i);
    LONG r, g, b;
    assert(calculate_white_calibration(RGB(255,255,255), &r, &g, &b));
    assert(r == 1000 && g == 1000 && b == 1000);
    assert(calculate_white_calibration(RGB(255,210,233), &r, &g, &b));
    assert(r == 644 && g == 1000 && b == 791);
    assert(!calculate_white_calibration(RGB(0,0,0), &r, &g, &b));
    printf("PASS: %d controls; EN/ZH switching, config, profiles, calibration, sRGB round-trip.\n", checked);
    return 0;
}
