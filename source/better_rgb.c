// Game Garaj (Tongfang/ITE8291) Better_RGB
// Chinese Enhanced Version 3.6.1

#define UNICODE
#define _UNICODE
#define COBJMACROS
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <dbt.h>
#include <commctrl.h>
#include <wbemidl.h>
#include <oleauto.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "hidapi.h"
#include "resource.h"

/*
 * 维护导读：本程序把 UI、灯效生成、系统指标和 HID 传输放在一个 Win32 C
 * 文件中。所有灯效先生成统一的 512 字节逻辑帧，再由 send_frame() 叠加
 * Touch Bar、启动/休眠/关机过渡与亮度，最后串行提交到 ITE RGB HID 接口。
 * 新效果不应直接调用 hidapi，以免绕过锁、重连和安全降帧逻辑。
 */

// llvm-mingw currently forward-declares IAudioMeterInformation without
// emitting its C vtable.  Keep the small official ABI definition local so
// playback/capture peak metering remains available in a strict C build.
#ifndef __IAudioMeterInformation_INTERFACE_DEFINED__
#define __IAudioMeterInformation_INTERFACE_DEFINED__
typedef struct IAudioMeterInformationVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IAudioMeterInformation *, REFIID, void **);
    ULONG (STDMETHODCALLTYPE *AddRef)(IAudioMeterInformation *);
    ULONG (STDMETHODCALLTYPE *Release)(IAudioMeterInformation *);
    HRESULT (STDMETHODCALLTYPE *GetPeakValue)(IAudioMeterInformation *, float *);
    HRESULT (STDMETHODCALLTYPE *GetMeteringChannelCount)(IAudioMeterInformation *, UINT *);
    HRESULT (STDMETHODCALLTYPE *GetChannelsPeakValues)(IAudioMeterInformation *, UINT, float *);
    HRESULT (STDMETHODCALLTYPE *QueryHardwareSupport)(IAudioMeterInformation *, DWORD *);
} IAudioMeterInformationVtbl;
interface IAudioMeterInformation { CONST_VTBL IAudioMeterInformationVtbl *lpVtbl; };
#endif

static const GUID IID_IAudioMeterInformation_Local =
    {0xc02216f6,0x8c67,0x4b5b,{0x9d,0x00,0xd0,0x08,0xe7,0x3e,0x00,0x64}};
static const GUID IID_IMMDeviceEnumerator_Local =
    {0xa95664d2,0x9614,0x4f35,{0xa7,0x46,0xde,0x8d,0xb6,0x36,0x17,0xe6}};
static const GUID CLSID_MMDeviceEnumerator_Local =
    {0xbcde0395,0xe52f,0x467c,{0x8e,0x3d,0xc4,0x57,0x92,0x91,0x69,0x2e}};
static const GUID IID_IAudioEndpointVolume_Local =
    {0x5cdf2c82,0x841e,0x4546,{0x97,0x22,0x0c,0xf7,0x40,0x78,0x22,0x9a}};

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static char g_devicePath[512] = "";

static const GUID GUID_DEVINTERFACE_HID_LOCAL =
    { 0x4D1E55B2, 0xF16F, 0x11CF, { 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 } };
#define BUF_SIZE 512
#define TARGET_FPS_MIN 15
#define TARGET_FPS_MAX 60
#define TARGET_FPS_DEFAULT 30
#define SHUTDOWN_PRIORITY_LAST_APP 0x100

#define TRANSITION_OFF 0
#define TRANSITION_FADE 1
#define TRANSITION_RIPPLE_CENTER 2
#define TRANSITION_RIPPLE_EDGES 3
#define TRANSITION_RIPPLE_TOP 4
#define TRANSITION_RIPPLE_BOTTOM 5
#define TRANSITION_RIPPLE_LEFT 6
#define TRANSITION_RIPPLE_RIGHT 7
#define TRANSITION_SNAKE_MAIN_OUT_IN 8
#define TRANSITION_SNAKE_MAIN_IN_OUT 9
#define TRANSITION_SNAKE_ANTI_OUT_IN 10
#define TRANSITION_SNAKE_ANTI_IN_OUT 11
#define TRANSITION_COUNT 12
#define TRANSITION_DEFAULT_MS 800

#define IDLE_TRANSITION_ACTIVE 0
#define IDLE_TRANSITION_FADING_OUT 1
#define IDLE_TRANSITION_ASLEEP 2
#define IDLE_TRANSITION_FADING_IN 3

#define MODE_BREATH   0
#define MODE_WAVE     1
#define MODE_SPARKLE  2
#define MODE_REACTIVE 3
#define MODE_WHEEL    4
#define MODE_LIGHTNING 5
#define MODE_FLAME     6
#define MODE_RAIN      7
#define MODE_MATRIX    8
#define MODE_STATIC    9
#define MODE_RIPPLE   10
#define MODE_RAINBOW  11
#define MODE_QUICKSAND 12
#define MODE_CURRENT   13
#define MODE_TOUCH_CURRENT 14
#define MODE_SCREEN_AMBIENT 15
#define MODE_COUNT    16

#define IDC_COMBO_MODE       200
#define IDC_BTN_STOP         201
#define IDC_SLIDER_BRIGHT    202
#define IDC_STATIC_STATUS    203
#define IDC_BTN_AUTOSTART    205
#define IDC_SLIDER_FPS       206
#define IDC_CHK_POWER_AUTO   207
#define IDC_COMBO_AC_MODE    208
#define IDC_COMBO_BAT_MODE   209
#define IDC_CHK_BREATH_RANDOM 214
#define IDC_COMBO_IDLE       215
#define IDC_SLIDER_AC_BRIGHT 216
#define IDC_SLIDER_AC_FPS    217
#define IDC_SLIDER_BAT_BRIGHT 218
#define IDC_SLIDER_BAT_FPS   219
#define IDC_PAGE_EFFECTS     190
#define IDC_PAGE_AC          191
#define IDC_PAGE_BATTERY     192
#define IDC_PAGE_TOUCHBAR    193
#define IDC_PAGE_PROGRAM     194
#define IDT_TRAY_RETRY       3
#define IDT_POWER_POLL       4

#define IDC_BTN_COLOR_BREATH 210
#define IDC_SWATCH_BREATH    211
#define IDC_SLIDER_BSPEED    212
#define IDC_SLIDER_BSMOOTH   213

#define IDC_SLIDER_ANGLE     220
#define IDC_SLIDER_WSPEED    221

#define IDC_SLIDER_SPSPEED   230
#define IDC_SLIDER_SPDENS    231

#define IDC_BTN_COLOR_REACT  240
#define IDC_SWATCH_REACT     241
#define IDC_BTN_REACT_RANDOM 242
#define IDC_RADIO_SHORT      243
#define IDC_RADIO_MEDIUM     244
#define IDC_RADIO_LONG       245

#define IDC_CHK_WHEEL_REVERSE 250
#define IDC_SLIDER_WHSPEED    251

#define IDC_SLIDER_LTSPEED    261
#define IDC_SLIDER_LTSMOOTH   262
#define IDC_SLIDER_LTWIDTH    263
#define IDC_SLIDER_LTCONCUR   264
#define IDC_SLIDER_FLSPEED    265
#define IDC_SLIDER_FLSMOOTH   266
#define IDC_SLIDER_RNSPEED    267
#define IDC_RAIN_DENSITY_BASE 400
#define IDC_RAIN_COLORCNT_BASE 410
#define IDC_RAIN_COLOR_BASE   270
#define IDC_SLIDER_MXSPEED    280
#define IDC_MATRIX_DENSITY_BASE 420
#define IDC_MATRIX_COLORCNT_BASE 430
#define IDC_COMBO_MXSTYLE     282
#define IDC_MATRIX_COLOR_BASE 285

#define IDC_BTN_STATIC_HORIZ    500
#define IDC_BTN_STATIC_VERT     501
#define IDC_SLIDER_STATIC_ZONES 502
#define IDC_COMBO_STATIC_ZONESEL 503
#define IDC_BTN_STATIC_COLOR    504

#define IDC_BTN_COLOR_RIPPLE  510
#define IDC_SWATCH_RIPPLE     511
#define IDC_BTN_RIPPLE_RANDOM 512
#define IDC_SLIDER_RIPSPEED   513
#define IDC_SLIDER_RIPWIDTH   514
#define IDC_SLIDER_RBWSPEED   515
#define IDC_SLIDER_QSSPEED    516
#define IDC_SLIDER_QSSCALE    517
#define IDC_SLIDER_CURSPEED   518
#define IDC_SLIDER_CURWIDTH   519
#define IDC_SLIDER_TCSPEED    520
#define IDC_SLIDER_TCWIDTH    521
#define IDC_SLIDER_AMBIENT_BLUR 522
#define IDC_SLIDER_AMBIENT_RESPONSE 523
#define IDC_COMBO_AC_TOUCHBAR_MODE 530
#define IDC_COMBO_AC_TOUCHBAR_DIRECTION 531
#define IDC_COMBO_BAT_TOUCHBAR_MODE 532
#define IDC_COMBO_BAT_TOUCHBAR_DIRECTION 533
#define IDC_COMBO_TRANSITION_FAMILY 534
#define IDC_COMBO_TRANSITION_DETAIL 535
#define IDC_SLIDER_TRANSITION_DURATION 536
#define IDC_EDIT_AC_TOUCHBAR_FPS_MAX 537
#define IDC_EDIT_BAT_TOUCHBAR_FPS_MAX 538
#define IDC_BTN_WHITE_CALIBRATE 539
#define IDC_BTN_WHITE_RESET 540
#define IDC_AC_TOUCHBAR_COLOR_START 630
#define IDC_AC_TOUCHBAR_COLOR_END 631
#define IDC_BAT_TOUCHBAR_COLOR_START 632
#define IDC_BAT_TOUCHBAR_COLOR_END 633
#define IDC_AC_TOUCHBAR_AUDIO_COLOR_BASE 640
#define IDC_BAT_TOUCHBAR_AUDIO_COLOR_BASE 643

#define IDC_QUICKSAND_COLOR_BASE 600
#define IDC_CURRENT_COLOR_BASE   610
#define IDC_TOUCH_CURRENT_COLOR_BASE 620

typedef struct { const char *name; int offset; int row; int col; } KeyEntry;

#define OFFSET_SHIFT_LEFT 5
#define OFFSET_SHIFT_LEFT_AUX 29
#define OFFSET_ENTER 321
#define OFFSET_ENTER_AUX 345
#define OFFSET_BACKSLASH 349

static const KeyEntry KEYMAP[] = {
    {"Esc",21,0,0},{"F1",45,0,1},{"F2",69,0,2},{"F3",93,0,3},{"F4",117,0,4},
    {"F5",141,0,5},{"F6",165,0,6},{"F7",189,0,7},{"F8",213,0,8},{"F9",237,0,9},
    {"F10",261,0,10},{"F11",285,0,11},{"F12",309,0,12},{"FnLockCam",333,0,13},
    {"PrtSc",357,0,14},{"Del",381,0,15},{"Home",405,0,16},{"PgUp",429,0,17},
    {"PgDn",453,0,18},{"End",477,0,19},
    {"Grave",17,1,0},{"1",41,1,1},{"2",65,1,2},{"3",89,1,3},{"4",113,1,4},
    {"5",137,1,5},{"6",161,1,6},{"7",185,1,7},{"8",209,1,8},{"9",233,1,9},
    {"0",257,1,10},{"Minus",281,1,11},{"Equals",305,1,12},{"Backspace",353,1,13},
    {"NumLock",377,1,14},{"NumpadDiv",401,1,15},{"NumpadMul",425,1,16},{"NumpadMinus",449,1,17},
    {"Tab",13,2,0},{"Q",61,2,1},{"W",85,2,2},{"E",109,2,3},{"R",133,2,4},
    {"T",157,2,5},{"Y",181,2,6},{"U",205,2,7},{"I",229,2,8},{"O",253,2,9},
    {"P",277,2,10},{"BracketL",301,2,11},{"BracketR",325,2,12},{"Backslash",OFFSET_BACKSLASH,2,13},
    {"Numpad7",373,2,14},{"Numpad8",397,2,15},{"Numpad9",421,2,16},{"NumpadPlus",445,2,17},
    {"CapsLock",9,3,0},{"A",57,3,1},{"S",81,3,2},{"D",105,3,3},{"F",129,3,4},
    {"G",153,3,5},{"H",177,3,6},{"J",201,3,7},{"K",225,3,8},{"L",249,3,9},
    {"Semicolon",273,3,10},{"Quote",297,3,11},{"Enter",OFFSET_ENTER,3,12},
    {"Numpad4",369,3,13},{"Numpad5",393,3,14},{"Numpad6",417,3,15},
    {"ShiftL",OFFSET_SHIFT_LEFT,4,0},{"ISO_Backslash",53,4,1},{"Z",77,4,2},{"X",101,4,3},
    {"C",125,4,4},{"V",149,4,5},{"B",173,4,6},{"N",197,4,7},{"M",221,4,8},
    {"Comma",245,4,9},{"Period",269,4,10},{"Slash",293,4,11},{"ShiftR",341,4,12},
    {"Numpad1",365,4,13},{"Numpad2",389,4,14},{"Numpad3",413,4,15},{"NumpadEnter",437,4,16},
    {"CtrlL",1,5,0},{"Fn",49,5,1},{"Windows",73,5,2},{"AltL",97,5,3},
    {"Space",169,5,4},{"AltGr",241,5,5},{"CopilotKey",289,5,6},{"ArrowUp",337,5,7},
    {"Numpad0",385,5,8},{"NumpadDecimal",409,5,9},
    {"ArrowLeft",313,6,6},{"ArrowDown",433,6,7},{"ArrowRight",361,6,8},
};
#define KEYMAP_COUNT (sizeof(KEYMAP)/sizeof(KEYMAP[0]))

static hid_device *g_h = NULL;
static HANDLE g_effectThread = NULL;
static volatile LONG g_stopFlag = 0;
static volatile LONG g_activeMode = -1;
static unsigned char g_lastMainRawFrame[BUF_SIZE] = {0};
static volatile LONG g_lastMainRawFrameValid = 0;
static unsigned char g_lastRawFrame[BUF_SIZE] = {0};
static volatile LONG g_lastRawFrameValid = 0;
static volatile LONG g_shutdownFadeStarted = 0;
static volatile LONG g_shutdownResumeMode = -1;
static volatile LONG g_powerTransitionMode = TRANSITION_FADE;
static volatile LONG g_transitionDurationMs = TRANSITION_DEFAULT_MS;
static volatile LONG g_startupTransitionActive = 1;
static volatile LONGLONG g_startupTransitionStartTick = 0;
static volatile LONG g_mainRevealActive = 0;
static volatile LONGLONG g_mainRevealStartTick = 0;

static volatile LONG g_brightness = 100;
/* 光谱白色校准：增益以千分比保存，校准期间最终帧强制输出未经修正的纯白。 */
static volatile LONG g_whiteGainR = 1000;
static volatile LONG g_whiteGainG = 1000;
static volatile LONG g_whiteGainB = 1000;
static volatile LONG g_whiteCalibrationActive = 0;
static COLORREF g_observedWhiteColor = RGB(255,255,255);
static volatile LONG g_targetFps = TARGET_FPS_DEFAULT;
static volatile LONG g_effectiveFps = TARGET_FPS_DEFAULT;
static volatile LONG g_slowFrameCount = 0;
static volatile LONG g_healthyFrameCount = 0;
static volatile LONG g_pacerResetRequested = 0;
static LARGE_INTEGER g_pacerFrequency;
static LONGLONG g_nextFrameTick = 0;

static COLORREF g_breathColor = RGB(0, 120, 255);
static volatile LONG g_breathSpeed = 12;
static volatile LONG g_breathSmooth = 60;
static volatile LONG g_breathRandomMode = 0;

static volatile LONG g_waveAngle = 0;
static volatile LONG g_waveSpeed = 4;

static volatile LONG g_sparkleSpeed = 5;
static volatile LONG g_sparkleDensity = 8;

static COLORREF g_reactiveColor = RGB(255, 255, 255);
static volatile LONG g_reactiveRandomMode = 1;
static volatile LONG g_reactiveDuration = 20;

static COLORREF g_rippleColor = RGB(0, 180, 255);
static volatile LONG g_rippleRandomMode = 1;
static volatile LONG g_rippleSpeed = 5;
static volatile LONG g_rippleWidth = 2;
static volatile LONG g_rainbowSpeed = 6;

static volatile LONG g_quicksandSpeed = 5;
static volatile LONG g_quicksandScale = 5;
static COLORREF g_quicksandColors[3] = { RGB(255,0,0), RGB(0,255,0), RGB(0,0,255) };
static HWND g_quicksandColorBtn[3];

static volatile LONG g_currentSpeed = 5;
static volatile LONG g_currentWidth = 4;
static COLORREF g_currentColors[3] = { RGB(255,0,0), RGB(0,255,0), RGB(0,0,255) };
static HWND g_currentColorBtn[3];

static volatile LONG g_touchCurrentSpeed = 5;
static volatile LONG g_touchCurrentWidth = 2;
static COLORREF g_touchCurrentColors[3] = { RGB(255,0,0), RGB(0,255,0), RGB(0,0,255) };
static HWND g_touchCurrentColorBtn[3];

static volatile LONG g_ambientBlur = 3;
static volatile LONG g_ambientResponse = 5;

#define TOUCHBAR_OFF 0
#define TOUCHBAR_BATTERY 1
#define TOUCHBAR_VOLUME 2
#define TOUCHBAR_MIC 3
#define TOUCHBAR_CPU 4
#define TOUCHBAR_GPU 5
#define TOUCHBAR_AUDIO 6
#define TOUCHBAR_FPS 7
#define TOUCHBAR_MODE_COUNT 7

#define TOUCHBAR_LEFT_TO_RIGHT 0
#define TOUCHBAR_RIGHT_TO_LEFT 1
#define TOUCHBAR_EDGES_TO_CENTER 2
#define TOUCHBAR_CENTER_TO_EDGES 3
#define TOUCHBAR_DIRECTION_COUNT 4

static volatile LONG g_touchbarMode = TOUCHBAR_OFF;
static volatile LONG g_touchbarDisplayMode = TOUCHBAR_OFF;
static volatile LONG g_touchbarDirection = TOUCHBAR_LEFT_TO_RIGHT;
static COLORREF g_touchbarColorStart = RGB(0,255,80);
static COLORREF g_touchbarColorEnd = RGB(255,20,0);
static COLORREF g_touchbarAudioColors[3] = { RGB(255,0,0), RGB(0,255,0), RGB(0,80,255) };
static volatile LONG g_acTouchbarMode = TOUCHBAR_OFF;
static volatile LONG g_batteryTouchbarMode = TOUCHBAR_OFF;
static volatile LONG g_acTouchbarDirection = TOUCHBAR_LEFT_TO_RIGHT;
static volatile LONG g_batteryTouchbarDirection = TOUCHBAR_LEFT_TO_RIGHT;
static COLORREF g_acTouchbarColorStart = RGB(0,255,80);
static COLORREF g_acTouchbarColorEnd = RGB(255,20,0);
static COLORREF g_batteryTouchbarColorStart = RGB(0,255,80);
static COLORREF g_batteryTouchbarColorEnd = RGB(255,20,0);
static COLORREF g_acTouchbarAudioColors[3] = { RGB(255,0,0), RGB(0,255,0), RGB(0,80,255) };
static COLORREF g_batteryTouchbarAudioColors[3] = { RGB(255,0,0), RGB(0,255,0), RGB(0,80,255) };
static volatile LONG g_touchbarLevel = 0;
static volatile LONG g_measuredFps = 0;
static volatile LONG g_acTouchbarFpsMax = 300;
static volatile LONG g_batteryTouchbarFpsMax = 300;
static volatile LONG g_touchbarStop = 0;
static HANDLE g_touchbarThread = NULL;
static HWND g_acTouchbarColorBtnStart, g_acTouchbarColorBtnEnd;
static HWND g_batteryTouchbarColorBtnStart, g_batteryTouchbarColorBtnEnd;
static HWND g_acTouchbarAudioColorBtn[3], g_batteryTouchbarAudioColorBtn[3];
static HWND g_hEditAcTouchbarFpsMax, g_hEditBatteryTouchbarFpsMax;

static volatile LONG g_wheelReverse = 0;
static volatile LONG g_wheelSpeed = 4;

static volatile LONG g_lightningSpeed = 5;
static volatile LONG g_lightningSmooth = 10;
static volatile LONG g_lightningWidth = 2;
static volatile LONG g_lightningConcurrent = 1;

static volatile LONG g_flameSpeed = 5;
static volatile LONG g_flameSmooth = 8;

#define MAX_RAIN_DROPS 20
static volatile LONG g_rainSpeed = 5;
static volatile LONG g_rainDensityTarget = 3;
static volatile LONG g_rainColorCount = 1;
static COLORREF g_rainColors[5] = { RGB(80,140,255), RGB(120,80,255), RGB(80,220,255), RGB(255,255,255), RGB(80,255,180) };
static HWND g_rainColorBtn[5];
static HWND g_rainDensityBtn[5];
static HWND g_rainColorCountBtn[5];

static volatile LONG g_matrixSpeed = 5;
static volatile LONG g_matrixStyle = 0;
static volatile LONG g_matrixDensityTarget = 2;
static volatile LONG g_matrixColorCount = 1;
static COLORREF g_matrixColors[3] = { RGB(0,255,60), RGB(0,180,255), RGB(255,0,120) };
static HWND g_matrixColorBtn[3];
static HWND g_matrixDensityBtn[5];
static HWND g_matrixColorCountBtn[3];
static HWND g_hComboMatrixStyle;

static volatile LONG g_staticLayout = 0;
static volatile LONG g_staticZoneCount = 1;
static COLORREF g_staticColors[10] = {
    RGB(255,255,255), RGB(255,0,0), RGB(0,255,0), RGB(0,0,255), RGB(255,255,0),
    RGB(255,0,255), RGB(0,255,255), RGB(255,140,0), RGB(140,0,255), RGB(0,140,140)
};
static volatile LONG g_staticActiveZone = 0;
static HWND g_btnStaticHoriz, g_btnStaticVert, g_hSliderStaticZones, g_hComboStaticZoneSel, g_btnStaticColor;

static volatile LONG g_autoPowerProfiles = 0;
static volatile LONG g_onAcPower = 1;
static volatile LONG g_acMode = -1;
static volatile LONG g_batteryMode = MODE_STATIC;
static volatile LONG g_acBrightness = 100;
static volatile LONG g_batteryBrightness = 40;
static volatile LONG g_acFps = TARGET_FPS_DEFAULT;
static volatile LONG g_batteryFps = TARGET_FPS_MIN;
static volatile LONG g_idleTimeoutMinutes = 0;
static volatile LONG g_idleLightsOff = 0;
static volatile LONG g_idleTransitionState = IDLE_TRANSITION_ACTIVE;
static volatile LONGLONG g_idleTransitionStartTick = 0;

#define WM_TRAYICON (WM_USER + 1)
#define WM_FPS_STATUS (WM_APP + 2)
#define WM_FN_HOTKEY (WM_APP + 3)
#define FN_SCANCODE_BRIGHTNESS_DOWN 0xB1
#define FN_SCANCODE_BRIGHTNESS_UP   0xB2
static UINT g_msgTaskbarCreated;
static UINT g_msgShowInstance;
static int g_trayRetries = 0;
#define IDC_TRAY_RESTORE 900
#define IDC_TRAY_EXIT    901

static HWND g_hStatus, g_hComboMode, g_hSliderBright, g_hSliderFps, g_hLblFps, g_hLblAngle, g_hLblConcurrent;
static HWND g_btnWhiteCalibrate, g_btnWhiteReset, g_hWhiteCalibrationSwatch;
static HWND g_hLblWhiteCalibration;
static HWND g_hMainWindow;
static HANDLE g_hotkeyThread = NULL;
static volatile LONG g_hotkeyStop = 0;
static HWND g_hSwatchBreath, g_hSwatchReact, g_hSwatchRipple;
static HWND g_hPowerStatus, g_hComboAcMode, g_hComboBatteryMode, g_hComboIdle;
static HWND g_hComboTransitionFamily, g_hComboTransitionDetail;
static HWND g_hLblTransitionDetail, g_hSliderTransitionDuration, g_hLblTransitionDuration;
static HWND g_hPageButton[5], g_hAutoPowerCheck;
static HWND g_hSliderAcBright, g_hSliderAcFps, g_hSliderBatteryBright, g_hSliderBatteryFps;
static HWND g_hLblAcFps, g_hLblBatteryFps;
static HWND g_hComboAcTouchbarMode, g_hComboAcTouchbarDirection;
static HWND g_hComboBatteryTouchbarMode, g_hComboBatteryTouchbarDirection;
static HWND g_hAcTouchbarDirectionLabel, g_hBatteryTouchbarDirectionLabel;
static HWND g_hAcTouchbarMetricLabel, g_hBatteryTouchbarMetricLabel;
static HWND g_hAcTouchbarAudioLabel, g_hBatteryTouchbarAudioLabel;
static HWND g_hAcTouchbarFpsLabel, g_hBatteryTouchbarFpsLabel;
static HWND g_hAcTouchbarHint, g_hBatteryTouchbarHint;
static HBRUSH g_bgBrush;
static NOTIFYICONDATA g_nid;

#define MAX_PANEL_CTRLS 25
static HWND g_panelBreath[MAX_PANEL_CTRLS]; static int g_panelBreathCount = 0;
static HWND g_panelWave[MAX_PANEL_CTRLS];   static int g_panelWaveCount = 0;
static HWND g_panelSparkle[MAX_PANEL_CTRLS];static int g_panelSparkleCount = 0;
static HWND g_panelReactive[MAX_PANEL_CTRLS];static int g_panelReactiveCount = 0;
static HWND g_panelWheel[MAX_PANEL_CTRLS];  static int g_panelWheelCount = 0;
static HWND g_panelLightning[MAX_PANEL_CTRLS]; static int g_panelLightningCount = 0;
static HWND g_panelFlame[MAX_PANEL_CTRLS];  static int g_panelFlameCount = 0;
static HWND g_panelRain[MAX_PANEL_CTRLS];   static int g_panelRainCount = 0;
static HWND g_panelMatrix[MAX_PANEL_CTRLS]; static int g_panelMatrixCount = 0;
static HWND g_panelStatic[MAX_PANEL_CTRLS]; static int g_panelStaticCount = 0;
static HWND g_panelRipple[MAX_PANEL_CTRLS]; static int g_panelRippleCount = 0;
static HWND g_panelRainbow[MAX_PANEL_CTRLS]; static int g_panelRainbowCount = 0;
static HWND g_panelQuicksand[MAX_PANEL_CTRLS]; static int g_panelQuicksandCount = 0;
static HWND g_panelCurrent[MAX_PANEL_CTRLS]; static int g_panelCurrentCount = 0;
static HWND g_panelTouchCurrent[MAX_PANEL_CTRLS]; static int g_panelTouchCurrentCount = 0;
static HWND g_panelScreenAmbient[MAX_PANEL_CTRLS]; static int g_panelScreenAmbientCount = 0;

#define MAX_UI_PAGE_CTRLS 40
#define UI_PAGE_EFFECTS 0
#define UI_PAGE_AC 1
#define UI_PAGE_BATTERY 2
#define UI_PAGE_TOUCHBAR 3
#define UI_PAGE_PROGRAM 4
static int g_uiPage = UI_PAGE_EFFECTS;
static HWND g_pageEffects[MAX_UI_PAGE_CTRLS]; static int g_pageEffectsCount = 0;
static HWND g_pageAc[MAX_UI_PAGE_CTRLS]; static int g_pageAcCount = 0;
static HWND g_pageBattery[MAX_UI_PAGE_CTRLS]; static int g_pageBatteryCount = 0;
static HWND g_pageTouchbar[MAX_UI_PAGE_CTRLS]; static int g_pageTouchbarCount = 0;
static HWND g_pageProgram[MAX_UI_PAGE_CTRLS]; static int g_pageProgramCount = 0;
static HWND g_pagePowerShared[MAX_UI_PAGE_CTRLS]; static int g_pagePowerSharedCount = 0;

#define CLR_BG        RGB(24,24,28)
#define CLR_PANEL     RGB(45,45,52)
#define CLR_PANEL_HOV RGB(55,55,64)
#define CLR_ACCENT    RGB(127,90,240)
#define CLR_ACCENT_LT RGB(170,140,255)
#define CLR_TEXT      RGB(235,235,240)
#define CLR_BORDER    RGB(70,70,80)

static CRITICAL_SECTION g_deviceLock;

static char g_logPath[MAX_PATH] = "";
static char g_cfgPath[MAX_PATH] = "";
static LONG g_cfgMode = -1;
static int g_startupLaunch = 0;

static void update_power_profile_ui(void);
static void apply_touchbar_overlay(unsigned char *buf);
static void apply_power_transition_mask(const unsigned char *source,
    unsigned char *target, LONG mode, double progress);
static void update_transition_detail_ui(void);
void start_mode(int mode);

/* 配置和日志与 EXE 同目录，便携运行时无需注册表；目录必须具有写权限。 */
static void build_sibling_path(char *dst, const char *name) {
    GetModuleFileNameA(NULL, dst, MAX_PATH);
    char *p = strrchr(dst, '\\');
    if (p) strcpy(p + 1, name);
    else strcpy(dst, name);
}

void log_event(const char *msg) {
    if (!g_logPath[0]) build_sibling_path(g_logPath, "rgb_engine_log.txt");
    FILE *f = fopen(g_logPath, "a");
    if (!f) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(f, "[%02d:%02d:%02d.%03d] [thread %lu] %s\n",
        t.wHour, t.wMinute, t.wSecond, t.wMilliseconds,
        (unsigned long)GetCurrentThreadId(), msg);
    fclose(f);
}

static LONG cfg_clamp(long v, long lo, long hi) {
    if (v < lo) return (LONG)lo;
    if (v > hi) return (LONG)hi;
    return (LONG)v;
}

static int query_ac_power(void) {
    SYSTEM_POWER_STATUS status;
    if (!GetSystemPowerStatus(&status) || status.ACLineStatus == 255)
        return (int)InterlockedCompareExchange(&g_onAcPower, 0, 0);
    return status.ACLineStatus == 1;
}

static void capture_current_power_profile(void) {
    LONG mode = InterlockedCompareExchange(&g_activeMode, 0, 0);
    if (InterlockedCompareExchange(&g_onAcPower, 0, 0)) {
        InterlockedExchange(&g_acMode, mode);
        InterlockedExchange(&g_acBrightness, g_brightness);
        InterlockedExchange(&g_acFps, g_targetFps);
        InterlockedExchange(&g_acTouchbarMode, g_touchbarMode);
        InterlockedExchange(&g_acTouchbarDirection, g_touchbarDirection);
        g_acTouchbarColorStart = g_touchbarColorStart;
        g_acTouchbarColorEnd = g_touchbarColorEnd;
        for (int i = 0; i < 3; i++) g_acTouchbarAudioColors[i] = g_touchbarAudioColors[i];
    } else {
        InterlockedExchange(&g_batteryMode, mode);
        InterlockedExchange(&g_batteryBrightness, g_brightness);
        InterlockedExchange(&g_batteryFps, g_targetFps);
        InterlockedExchange(&g_batteryTouchbarMode, g_touchbarMode);
        InterlockedExchange(&g_batteryTouchbarDirection, g_touchbarDirection);
        g_batteryTouchbarColorStart = g_touchbarColorStart;
        g_batteryTouchbarColorEnd = g_touchbarColorEnd;
        for (int i = 0; i < 3; i++) g_batteryTouchbarAudioColors[i] = g_touchbarAudioColors[i];
    }
}

/*
 * 配置采用 key=value 文本格式。新增字段必须同时补齐默认值、保存、读取和范围
 * 钳制；未知键应被忽略，以便旧版本配置可向前兼容。
 */
void save_config(void) {
    if (!g_cfgPath[0]) build_sibling_path(g_cfgPath, "better_rgb.cfg");
    FILE *f = fopen(g_cfgPath, "w");
    if (!f) {
        log_event("save_config: could not open config file for writing");
        return;
    }
    fprintf(f, "mode=%ld\n", (long)g_activeMode);
    fprintf(f, "brightness=%ld\n", (long)g_brightness);
    fprintf(f, "whiteObservedColor=%ld\n", (long)g_observedWhiteColor);
    fprintf(f, "whiteGainR=%ld\n", (long)g_whiteGainR);
    fprintf(f, "whiteGainG=%ld\n", (long)g_whiteGainG);
    fprintf(f, "whiteGainB=%ld\n", (long)g_whiteGainB);
    fprintf(f, "targetFps=%ld\n", (long)g_targetFps);
    fprintf(f, "breathColor=%ld\n", (long)g_breathColor);
    fprintf(f, "breathSpeed=%ld\n", (long)g_breathSpeed);
    fprintf(f, "breathSmooth=%ld\n", (long)g_breathSmooth);
    fprintf(f, "breathRandom=%ld\n", (long)g_breathRandomMode);
    fprintf(f, "waveAngle=%ld\n", (long)g_waveAngle);
    fprintf(f, "waveSpeed=%ld\n", (long)g_waveSpeed);
    fprintf(f, "sparkleSpeed=%ld\n", (long)g_sparkleSpeed);
    fprintf(f, "sparkleDensity=%ld\n", (long)g_sparkleDensity);
    fprintf(f, "reactiveColor=%ld\n", (long)g_reactiveColor);
    fprintf(f, "reactiveRandom=%ld\n", (long)g_reactiveRandomMode);
    fprintf(f, "reactiveDuration=%ld\n", (long)g_reactiveDuration);
    fprintf(f, "rippleColor=%ld\n", (long)g_rippleColor);
    fprintf(f, "rippleRandom=%ld\n", (long)g_rippleRandomMode);
    fprintf(f, "rippleSpeed=%ld\n", (long)g_rippleSpeed);
    fprintf(f, "rippleWidth=%ld\n", (long)g_rippleWidth);
    fprintf(f, "rainbowSpeed=%ld\n", (long)g_rainbowSpeed);
    fprintf(f, "quicksandSpeed=%ld\n", (long)g_quicksandSpeed);
    fprintf(f, "quicksandScale=%ld\n", (long)g_quicksandScale);
    for (int i = 0; i < 3; i++) fprintf(f, "quicksandColor%d=%ld\n", i, (long)g_quicksandColors[i]);
    fprintf(f, "currentSpeed=%ld\n", (long)g_currentSpeed);
    fprintf(f, "currentWidth=%ld\n", (long)g_currentWidth);
    for (int i = 0; i < 3; i++) fprintf(f, "currentColor%d=%ld\n", i, (long)g_currentColors[i]);
    fprintf(f, "touchCurrentSpeed=%ld\n", (long)g_touchCurrentSpeed);
    fprintf(f, "touchCurrentWidth=%ld\n", (long)g_touchCurrentWidth);
    for (int i = 0; i < 3; i++) fprintf(f, "touchCurrentColor%d=%ld\n", i, (long)g_touchCurrentColors[i]);
    fprintf(f, "ambientBlur=%ld\n", (long)g_ambientBlur);
    fprintf(f, "ambientResponse=%ld\n", (long)g_ambientResponse);
    fprintf(f, "touchbarMode=%ld\n", (long)g_touchbarMode);
    fprintf(f, "touchbarDirection=%ld\n", (long)g_touchbarDirection);
    fprintf(f, "touchbarColorStart=%ld\n", (long)g_touchbarColorStart);
    fprintf(f, "touchbarColorEnd=%ld\n", (long)g_touchbarColorEnd);
    fprintf(f, "acTouchbarMode=%ld\n", (long)g_acTouchbarMode);
    fprintf(f, "batteryTouchbarMode=%ld\n", (long)g_batteryTouchbarMode);
    fprintf(f, "acTouchbarDirection=%ld\n", (long)g_acTouchbarDirection);
    fprintf(f, "batteryTouchbarDirection=%ld\n", (long)g_batteryTouchbarDirection);
    fprintf(f, "acTouchbarColorStart=%ld\n", (long)g_acTouchbarColorStart);
    fprintf(f, "acTouchbarColorEnd=%ld\n", (long)g_acTouchbarColorEnd);
    fprintf(f, "batteryTouchbarColorStart=%ld\n", (long)g_batteryTouchbarColorStart);
    fprintf(f, "batteryTouchbarColorEnd=%ld\n", (long)g_batteryTouchbarColorEnd);
    fprintf(f, "acPresentFpsMax=%ld\n", (long)g_acTouchbarFpsMax);
    fprintf(f, "batteryPresentFpsMax=%ld\n", (long)g_batteryTouchbarFpsMax);
    for (int i = 0; i < 3; i++) fprintf(f, "acTouchbarAudioColor%d=%ld\n", i, (long)g_acTouchbarAudioColors[i]);
    for (int i = 0; i < 3; i++) fprintf(f, "batteryTouchbarAudioColor%d=%ld\n", i, (long)g_batteryTouchbarAudioColors[i]);
    fprintf(f, "wheelReverse=%ld\n", (long)g_wheelReverse);
    fprintf(f, "wheelSpeed=%ld\n", (long)g_wheelSpeed);
    fprintf(f, "lightningSpeed=%ld\n", (long)g_lightningSpeed);
    fprintf(f, "lightningSmooth=%ld\n", (long)g_lightningSmooth);
    fprintf(f, "lightningWidth=%ld\n", (long)g_lightningWidth);
    fprintf(f, "lightningConcurrent=%ld\n", (long)g_lightningConcurrent);
    fprintf(f, "flameSpeed=%ld\n", (long)g_flameSpeed);
    fprintf(f, "flameSmooth=%ld\n", (long)g_flameSmooth);
    fprintf(f, "rainSpeed=%ld\n", (long)g_rainSpeed);
    fprintf(f, "rainDensity=%ld\n", (long)g_rainDensityTarget);
    fprintf(f, "rainColorCount=%ld\n", (long)g_rainColorCount);
    for (int i = 0; i < 5; i++) fprintf(f, "rainColor%d=%ld\n", i, (long)g_rainColors[i]);
    fprintf(f, "matrixSpeed=%ld\n", (long)g_matrixSpeed);
    fprintf(f, "matrixStyle=%ld\n", (long)g_matrixStyle);
    fprintf(f, "matrixDensity=%ld\n", (long)g_matrixDensityTarget);
    fprintf(f, "matrixColorCount=%ld\n", (long)g_matrixColorCount);
    for (int i = 0; i < 3; i++) fprintf(f, "matrixColor%d=%ld\n", i, (long)g_matrixColors[i]);
    fprintf(f, "staticLayout=%ld\n", (long)g_staticLayout);
    fprintf(f, "staticZones=%ld\n", (long)g_staticZoneCount);
    for (int i = 0; i < 10; i++) fprintf(f, "staticColor%d=%ld\n", i, (long)g_staticColors[i]);
    fprintf(f, "autoPowerProfiles=%ld\n", (long)g_autoPowerProfiles);
    fprintf(f, "acMode=%ld\n", (long)g_acMode);
    fprintf(f, "batteryMode=%ld\n", (long)g_batteryMode);
    fprintf(f, "acBrightness=%ld\n", (long)g_acBrightness);
    fprintf(f, "batteryBrightness=%ld\n", (long)g_batteryBrightness);
    fprintf(f, "acFps=%ld\n", (long)g_acFps);
    fprintf(f, "batteryFps=%ld\n", (long)g_batteryFps);
    fprintf(f, "idleMinutes=%ld\n", (long)g_idleTimeoutMinutes);
    fprintf(f, "powerTransition=%ld\n", (long)g_powerTransitionMode);
    fprintf(f, "transitionDurationMs=%ld\n", (long)g_transitionDurationMs);
    fclose(f);
}

static void log_hotkey_hresult(const char *operation, HRESULT hr) {
    char text[160];
    sprintf(text, "Fn hotkey WMI: %s failed (HRESULT 0x%08lX)",
        operation, (unsigned long)hr);
    log_event(text);
}

static int hotkey_stop_wait(DWORD milliseconds) {
    DWORD waited = 0;
    while (waited < milliseconds &&
           !InterlockedCompareExchange(&g_hotkeyStop, 0, 0)) {
        DWORD slice = milliseconds - waited;
        if (slice > 100) slice = 100;
        Sleep(slice);
        waited += slice;
    }
    return InterlockedCompareExchange(&g_hotkeyStop, 0, 0) != 0;
}

static DWORD WINAPI fn_hotkey_wmi_thread(LPVOID unused) {
    (void)unused;
    HRESULT initHr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    int uninitialize = SUCCEEDED(initHr);
    if (FAILED(initHr) && initHr != RPC_E_CHANGED_MODE) {
        log_hotkey_hresult("CoInitializeEx", initHr);
        return 1;
    }

    HRESULT securityHr = CoInitializeSecurity(NULL, -1, NULL, NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE, NULL);
    if (FAILED(securityHr) && securityHr != RPC_E_TOO_LATE)
        log_hotkey_hresult("CoInitializeSecurity", securityHr);

    while (!InterlockedCompareExchange(&g_hotkeyStop, 0, 0)) {
        IWbemLocator *locator = NULL;
        IWbemServices *services = NULL;
        IEnumWbemClassObject *events = NULL;
        BSTR namespaceName = SysAllocString(L"ROOT\\WMI");
        BSTR queryLanguage = SysAllocString(L"WQL");
        BSTR query = SysAllocString(L"SELECT * FROM AcpiTest_EventULong");
        HRESULT hr = E_OUTOFMEMORY;

        if (namespaceName && queryLanguage && query) {
            hr = CoCreateInstance(&CLSID_WbemLocator, NULL,
                CLSCTX_INPROC_SERVER, &IID_IWbemLocator, (void **)&locator);
        }
        if (SUCCEEDED(hr)) {
            hr = IWbemLocator_ConnectServer(locator, namespaceName,
                NULL, NULL, NULL, 0, NULL, NULL, &services);
        }
        if (SUCCEEDED(hr)) {
            hr = CoSetProxyBlanket((IUnknown *)services,
                RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
                NULL, EOAC_NONE);
        }
        if (SUCCEEDED(hr)) {
            hr = IWbemServices_ExecNotificationQuery(services,
                queryLanguage, query,
                WBEM_FLAG_RETURN_IMMEDIATELY | WBEM_FLAG_FORWARD_ONLY,
                NULL, &events);
        }

        if (FAILED(hr)) {
            log_hotkey_hresult("subscribe", hr);
        } else {
            log_event("Fn hotkey WMI: listening for scan codes 0xB1 and 0xB2");
            while (!InterlockedCompareExchange(&g_hotkeyStop, 0, 0)) {
                IWbemClassObject *eventObject = NULL;
                ULONG returned = 0;
                hr = IEnumWbemClassObject_Next(events, 1000, 1,
                    &eventObject, &returned);
                if (hr == WBEM_S_TIMEDOUT || (SUCCEEDED(hr) && returned == 0))
                    continue;
                if (FAILED(hr)) {
                    log_hotkey_hresult("event wait", hr);
                    break;
                }
                if (eventObject && returned == 1) {
                    VARIANT value;
                    VariantInit(&value);
                    hr = IWbemClassObject_Get(eventObject, L"ULong", 0,
                        &value, NULL, NULL);
                    if (SUCCEEDED(hr)) {
                        VARIANT number;
                        VariantInit(&number);
                        if (SUCCEEDED(VariantChangeType(&number, &value, 0, VT_UI4))) {
                            ULONG scanCode = V_UI4(&number);
                            if (scanCode == FN_SCANCODE_BRIGHTNESS_DOWN ||
                                scanCode == FN_SCANCODE_BRIGHTNESS_UP) {
                                char text[96];
                                sprintf(text, "Fn hotkey WMI: received scan code 0x%02lX",
                                    (unsigned long)scanCode);
                                log_event(text);
                                if (g_hMainWindow)
                                    PostMessage(g_hMainWindow, WM_FN_HOTKEY,
                                        (WPARAM)scanCode, 0);
                            }
                        }
                        VariantClear(&number);
                    }
                    VariantClear(&value);
                }
                if (eventObject)
                    IWbemClassObject_Release(eventObject);
            }
        }

        if (events) IEnumWbemClassObject_Release(events);
        if (services) IWbemServices_Release(services);
        if (locator) IWbemLocator_Release(locator);
        if (query) SysFreeString(query);
        if (queryLanguage) SysFreeString(queryLanguage);
        if (namespaceName) SysFreeString(namespaceName);

        if (!InterlockedCompareExchange(&g_hotkeyStop, 0, 0)) {
            log_event("Fn hotkey WMI: retrying subscription in 2 seconds");
            hotkey_stop_wait(2000);
        }
    }

    log_event("Fn hotkey WMI: listener stopped");
    if (uninitialize) CoUninitialize();
    return 0;
}

/* 读取后统一恢复当前电源配置，避免插电和电池两套状态发生交叉污染。 */
void load_config(void) {
    if (!g_cfgPath[0]) build_sibling_path(g_cfgPath, "better_rgb.cfg");
    FILE *f = fopen(g_cfgPath, "r");
    if (!f) return;
    char key[64];
    long v;
    int sawPowerProfile = 0;
    int sawTouchbarProfile = 0;
    while (fscanf(f, " %63[^=]=%ld", key, &v) == 2) {
        if (!strcmp(key, "mode")) g_cfgMode = cfg_clamp(v, -1, MODE_COUNT - 1);
        else if (!strcmp(key, "brightness")) g_brightness = cfg_clamp(v, 0, 100);
        else if (!strcmp(key, "whiteObservedColor")) g_observedWhiteColor = (COLORREF)v;
        else if (!strcmp(key, "whiteGainR")) g_whiteGainR = cfg_clamp(v, 50, 1000);
        else if (!strcmp(key, "whiteGainG")) g_whiteGainG = cfg_clamp(v, 50, 1000);
        else if (!strcmp(key, "whiteGainB")) g_whiteGainB = cfg_clamp(v, 50, 1000);
        else if (!strcmp(key, "targetFps")) g_targetFps = cfg_clamp(v, TARGET_FPS_MIN, TARGET_FPS_MAX);
        else if (!strcmp(key, "breathColor")) g_breathColor = (COLORREF)v;
        else if (!strcmp(key, "breathSpeed")) g_breathSpeed = cfg_clamp(v, 1, 40);
        else if (!strcmp(key, "breathSmooth")) g_breathSmooth = cfg_clamp(v, 2, 150);
        else if (!strcmp(key, "breathRandom")) g_breathRandomMode = v ? 1 : 0;
        else if (!strcmp(key, "waveAngle")) g_waveAngle = cfg_clamp(v, 0, 359);
        else if (!strcmp(key, "waveSpeed")) g_waveSpeed = cfg_clamp(v, 1, 20);
        else if (!strcmp(key, "sparkleSpeed")) g_sparkleSpeed = cfg_clamp(v, 1, 10);
        else if (!strcmp(key, "sparkleDensity")) g_sparkleDensity = cfg_clamp(v, 3, 20);
        else if (!strcmp(key, "reactiveColor")) g_reactiveColor = (COLORREF)v;
        else if (!strcmp(key, "reactiveRandom")) g_reactiveRandomMode = v ? 1 : 0;
        else if (!strcmp(key, "reactiveDuration")) g_reactiveDuration = (v == 10 || v == 45) ? (LONG)v : 20;
        else if (!strcmp(key, "rippleColor")) g_rippleColor = (COLORREF)v;
        else if (!strcmp(key, "rippleRandom")) g_rippleRandomMode = v ? 1 : 0;
        else if (!strcmp(key, "rippleSpeed")) g_rippleSpeed = cfg_clamp(v, 1, 10);
        else if (!strcmp(key, "rippleWidth")) g_rippleWidth = cfg_clamp(v, 1, 4);
        else if (!strcmp(key, "rainbowSpeed")) g_rainbowSpeed = cfg_clamp(v, 1, 20);
        else if (!strcmp(key, "quicksandSpeed")) g_quicksandSpeed = cfg_clamp(v, 1, 10);
        else if (!strcmp(key, "quicksandScale")) g_quicksandScale = cfg_clamp(v, 1, 10);
        else if (!strncmp(key, "quicksandColor", 14) && key[14] >= '0' && key[14] <= '2' && !key[15]) g_quicksandColors[key[14] - '0'] = (COLORREF)v;
        else if (!strcmp(key, "currentSpeed")) g_currentSpeed = cfg_clamp(v, 1, 10);
        else if (!strcmp(key, "currentWidth")) g_currentWidth = cfg_clamp(v, 2, 8);
        else if (!strncmp(key, "currentColor", 12) && key[12] >= '0' && key[12] <= '2' && !key[13]) g_currentColors[key[12] - '0'] = (COLORREF)v;
        else if (!strcmp(key, "touchCurrentSpeed")) g_touchCurrentSpeed = cfg_clamp(v, 1, 10);
        else if (!strcmp(key, "touchCurrentWidth")) g_touchCurrentWidth = cfg_clamp(v, 1, 5);
        else if (!strncmp(key, "touchCurrentColor", 17) && key[17] >= '0' && key[17] <= '2' && !key[18]) g_touchCurrentColors[key[17] - '0'] = (COLORREF)v;
        else if (!strcmp(key, "ambientBlur")) g_ambientBlur = cfg_clamp(v, 1, 8);
        else if (!strcmp(key, "ambientResponse")) g_ambientResponse = cfg_clamp(v, 1, 10);
        else if (!strcmp(key, "touchbarMode")) g_touchbarMode = v == TOUCHBAR_FPS ? TOUCHBAR_OFF : cfg_clamp(v, 0, TOUCHBAR_MODE_COUNT - 1);
        else if (!strcmp(key, "touchbarDirection")) g_touchbarDirection = cfg_clamp(v, 0, TOUCHBAR_DIRECTION_COUNT - 1);
        else if (!strcmp(key, "touchbarColorStart")) g_touchbarColorStart = (COLORREF)v;
        else if (!strcmp(key, "touchbarColorEnd")) g_touchbarColorEnd = (COLORREF)v;
        else if (!strcmp(key, "acTouchbarMode")) { g_acTouchbarMode = v == TOUCHBAR_FPS ? TOUCHBAR_OFF : cfg_clamp(v, 0, TOUCHBAR_MODE_COUNT - 1); sawTouchbarProfile = 1; }
        else if (!strcmp(key, "batteryTouchbarMode")) { g_batteryTouchbarMode = v == TOUCHBAR_FPS ? TOUCHBAR_OFF : cfg_clamp(v, 0, TOUCHBAR_MODE_COUNT - 1); sawTouchbarProfile = 1; }
        else if (!strcmp(key, "acTouchbarDirection")) { g_acTouchbarDirection = cfg_clamp(v, 0, TOUCHBAR_DIRECTION_COUNT - 1); sawTouchbarProfile = 1; }
        else if (!strcmp(key, "batteryTouchbarDirection")) { g_batteryTouchbarDirection = cfg_clamp(v, 0, TOUCHBAR_DIRECTION_COUNT - 1); sawTouchbarProfile = 1; }
        else if (!strcmp(key, "acTouchbarColorStart")) { g_acTouchbarColorStart = (COLORREF)v; sawTouchbarProfile = 1; }
        else if (!strcmp(key, "acTouchbarColorEnd")) { g_acTouchbarColorEnd = (COLORREF)v; sawTouchbarProfile = 1; }
        else if (!strcmp(key, "batteryTouchbarColorStart")) { g_batteryTouchbarColorStart = (COLORREF)v; sawTouchbarProfile = 1; }
        else if (!strcmp(key, "batteryTouchbarColorEnd")) { g_batteryTouchbarColorEnd = (COLORREF)v; sawTouchbarProfile = 1; }
        else if (!strcmp(key, "acPresentFpsMax")) { g_acTouchbarFpsMax = cfg_clamp(v, 15, 500); sawTouchbarProfile = 1; }
        else if (!strcmp(key, "batteryPresentFpsMax")) { g_batteryTouchbarFpsMax = cfg_clamp(v, 15, 500); sawTouchbarProfile = 1; }
        else if (!strncmp(key, "acTouchbarAudioColor", 20) && key[20] >= '0' && key[20] <= '2' && !key[21]) { g_acTouchbarAudioColors[key[20] - '0'] = (COLORREF)v; sawTouchbarProfile = 1; }
        else if (!strncmp(key, "batteryTouchbarAudioColor", 25) && key[25] >= '0' && key[25] <= '2' && !key[26]) { g_batteryTouchbarAudioColors[key[25] - '0'] = (COLORREF)v; sawTouchbarProfile = 1; }
        else if (!strcmp(key, "wheelReverse")) g_wheelReverse = v ? 1 : 0;
        else if (!strcmp(key, "wheelSpeed")) g_wheelSpeed = cfg_clamp(v, 1, 20);
        else if (!strcmp(key, "lightningSpeed")) g_lightningSpeed = cfg_clamp(v, 1, 20);
        else if (!strcmp(key, "lightningSmooth")) g_lightningSmooth = cfg_clamp(v, 1, 40);
        else if (!strcmp(key, "lightningWidth")) g_lightningWidth = cfg_clamp(v, 1, 4);
        else if (!strcmp(key, "lightningConcurrent")) g_lightningConcurrent = cfg_clamp(v, 1, 4);
        else if (!strcmp(key, "flameSpeed")) g_flameSpeed = cfg_clamp(v, 1, 20);
        else if (!strcmp(key, "flameSmooth")) g_flameSmooth = cfg_clamp(v, 1, 40);
        else if (!strcmp(key, "rainSpeed")) g_rainSpeed = cfg_clamp(v, 1, 20);
        else if (!strcmp(key, "rainDensity")) g_rainDensityTarget = cfg_clamp(v, 1, 5);
        else if (!strcmp(key, "rainColorCount")) g_rainColorCount = cfg_clamp(v, 1, 5);
        else if (!strncmp(key, "rainColor", 9) && key[9] >= '0' && key[9] <= '4' && !key[10]) g_rainColors[key[9] - '0'] = (COLORREF)v;
        else if (!strcmp(key, "matrixSpeed")) g_matrixSpeed = cfg_clamp(v, 1, 20);
        else if (!strcmp(key, "matrixStyle")) g_matrixStyle = cfg_clamp(v, 0, 3);
        else if (!strcmp(key, "matrixDensity")) g_matrixDensityTarget = cfg_clamp(v, 1, 5);
        else if (!strcmp(key, "matrixColorCount")) g_matrixColorCount = cfg_clamp(v, 1, 3);
        else if (!strncmp(key, "matrixColor", 11) && key[11] >= '0' && key[11] <= '2' && !key[12]) g_matrixColors[key[11] - '0'] = (COLORREF)v;
        else if (!strcmp(key, "staticLayout")) g_staticLayout = cfg_clamp(v, 0, 1);
        else if (!strcmp(key, "staticZones")) g_staticZoneCount = cfg_clamp(v, 1, 10);
        else if (!strncmp(key, "staticColor", 11) && key[11] >= '0' && key[11] <= '9' && !key[12]) g_staticColors[key[11] - '0'] = (COLORREF)v;
        else if (!strcmp(key, "autoPowerProfiles")) { g_autoPowerProfiles = v ? 1 : 0; sawPowerProfile = 1; }
        else if (!strcmp(key, "acMode")) { g_acMode = cfg_clamp(v, -1, MODE_COUNT - 1); sawPowerProfile = 1; }
        else if (!strcmp(key, "batteryMode")) { g_batteryMode = cfg_clamp(v, -1, MODE_COUNT - 1); sawPowerProfile = 1; }
        else if (!strcmp(key, "acBrightness")) { g_acBrightness = cfg_clamp(v, 0, 100); sawPowerProfile = 1; }
        else if (!strcmp(key, "batteryBrightness")) { g_batteryBrightness = cfg_clamp(v, 0, 100); sawPowerProfile = 1; }
        else if (!strcmp(key, "acFps")) { g_acFps = cfg_clamp(v, TARGET_FPS_MIN, TARGET_FPS_MAX); sawPowerProfile = 1; }
        else if (!strcmp(key, "batteryFps")) { g_batteryFps = cfg_clamp(v, TARGET_FPS_MIN, TARGET_FPS_MAX); sawPowerProfile = 1; }
        else if (!strcmp(key, "idleMinutes")) g_idleTimeoutMinutes = (v == 1 || v == 5 || v == 10 || v == 30) ? (LONG)v : 0;
        else if (!strcmp(key, "powerTransition")) g_powerTransitionMode = cfg_clamp(v, 0, TRANSITION_COUNT - 1);
        else if (!strcmp(key, "transitionDurationMs")) g_transitionDurationMs = cfg_clamp(v, 200, 3000);
    }
    fclose(f);

    if (!sawPowerProfile) {
        g_acMode = g_cfgMode;
        g_batteryMode = MODE_STATIC;
        g_acBrightness = g_brightness;
        g_batteryBrightness = g_brightness < 40 ? g_brightness : 40;
        g_acFps = g_targetFps;
        g_batteryFps = TARGET_FPS_MIN;
        g_autoPowerProfiles = 0;
    }
    if (!sawTouchbarProfile) {
        g_acTouchbarMode = g_touchbarMode;
        g_batteryTouchbarMode = g_touchbarMode;
        g_acTouchbarDirection = g_touchbarDirection;
        g_batteryTouchbarDirection = g_touchbarDirection;
        g_acTouchbarColorStart = g_touchbarColorStart;
        g_acTouchbarColorEnd = g_touchbarColorEnd;
        g_batteryTouchbarColorStart = g_touchbarColorStart;
        g_batteryTouchbarColorEnd = g_touchbarColorEnd;
        for (int i = 0; i < 3; i++) {
            g_acTouchbarAudioColors[i] = g_touchbarAudioColors[i];
            g_batteryTouchbarAudioColors[i] = g_touchbarAudioColors[i];
        }
    }
    if (g_autoPowerProfiles) {
        if (g_onAcPower) {
            g_cfgMode = g_acMode;
            g_brightness = g_acBrightness;
            g_targetFps = g_acFps;
        } else {
            g_cfgMode = g_batteryMode;
            g_brightness = g_batteryBrightness;
            g_targetFps = g_batteryFps;
        }
    }
    if (g_onAcPower) {
        g_touchbarMode = g_acTouchbarMode;
        g_touchbarDirection = g_acTouchbarDirection;
        g_touchbarColorStart = g_acTouchbarColorStart;
        g_touchbarColorEnd = g_acTouchbarColorEnd;
        for (int i = 0; i < 3; i++) g_touchbarAudioColors[i] = g_acTouchbarAudioColors[i];
    } else {
        g_touchbarMode = g_batteryTouchbarMode;
        g_touchbarDirection = g_batteryTouchbarDirection;
        g_touchbarColorStart = g_batteryTouchbarColorStart;
        g_touchbarColorEnd = g_batteryTouchbarColorEnd;
        for (int i = 0; i < 3; i++) g_touchbarAudioColors[i] = g_batteryTouchbarAudioColors[i];
    }
    InterlockedExchange(&g_touchbarDisplayMode, g_touchbarMode);
    InterlockedExchange(&g_effectiveFps, g_targetFps);
    log_event("load_config: settings restored from config file");
}

/*
 * 进入 ITE 键盘的运行态逐帧模式。这里仅发送 HID Feature/Output report，
 * 不写 BIOS、EC Flash 或键盘固件；命令顺序和长度不可随意改动。
 */
int enter_custom_mode(void) {
    unsigned char step1[9] = {0x00,0x12,0x00,0x03,0x00,0x00,0x00,0x00,0x00};
    if (hid_send_feature_report(g_h, step1, 9) < 0) return -1;
    unsigned char clearFrame[65] = {0};
    hid_write(g_h, clearFrame, 65);
    unsigned char step2[9] = {0x00,0x08,0x02,0x33,0x00,0x32,0x00,0x00,0x00};
    if (hid_send_feature_report(g_h, step2, 9) < 0) return -1;
    return 0;
}

int log_exclusive_status(void) {
    HANDLE probe = CreateFileA(g_devicePath, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED, 0);
    if (probe == INVALID_HANDLE_VALUE && GetLastError() == ERROR_SHARING_VIOLATION) {
        log_event("device lock: EXCLUSIVE - no other process can open the RGB interface");
        return 1;
    }
    if (probe != INVALID_HANDLE_VALUE) CloseHandle(probe);
    log_event("device lock: SHARED - exclusive open not in effect; GCUBridge takeovers remain possible");
    return 0;
}

/*
 * 设备发现优先匹配 Usage Page 0xFF03，再回退到 MI_01，并对已验证 PID 600B
 * 加分。扩展机型应添加明确白名单，禁止把所有 048D 设备都视为 RGB 键盘。
 */
int resolve_device_path(void) {
    char lb[640];
    struct hid_device_info *list = hid_enumerate(0x048D, 0x0000);
    if (!list) {
        log_event("enum: no ITE (VID_048D) HID devices found on this system");
        return 0;
    }
    struct hid_device_info *best = NULL;
    int bestScore = 0;
    for (struct hid_device_info *d = list; d; d = d->next) {
        sprintf(lb, "enum: found VID=%04X PID=%04X usage_page=0x%04X usage=0x%04X iface=%d path=%.400s",
            d->vendor_id, d->product_id, d->usage_page, d->usage,
            d->interface_number, d->path ? d->path : "(null)");
        log_event(lb);
        int score = 0;
        if (d->usage_page == 0xFF03) score = 3;
        else if (d->interface_number == 1) score = 2;
        if (score && d->product_id == 0x600B) score++;
        if (score > bestScore) {
            bestScore = score;
            best = d;
        }
    }
    int found = 0;
    if (best && best->path) {
        strncpy(g_devicePath, best->path, sizeof(g_devicePath) - 1);
        g_devicePath[sizeof(g_devicePath) - 1] = 0;
        sprintf(lb, "enum: selected VID=%04X PID=%04X usage_page=0x%04X iface=%d",
            best->vendor_id, best->product_id, best->usage_page, best->interface_number);
        log_event(lb);
        found = 1;
    } else {
        log_event("enum: ITE device present but no vendor RGB interface (usage page 0xFF03 / MI_01) found");
    }
    hid_free_enumeration(list);
    return found;
}

int reconnect_device(void) {
    EnterCriticalSection(&g_deviceLock);
    log_event("reconnect_device: starting");

    if (g_h) {
        hid_close(g_h);
        g_h = NULL;
    }

    hid_exit();
    Sleep(100);
    hid_init();
    Sleep(20);

    if (!resolve_device_path()) {
        log_event("reconnect_device: device enumeration FAILED");
        LeaveCriticalSection(&g_deviceLock);
        return -1;
    }
    g_h = hid_open_path(g_devicePath);
    if (!g_h) {
        log_event("reconnect_device: hid_open_path FAILED");
        LeaveCriticalSection(&g_deviceLock);
        return -1;
    }
    Sleep(20);
    if (enter_custom_mode() < 0) {
        log_event("reconnect_device: enter_custom_mode FAILED");
        hid_close(g_h);
        g_h = NULL;
        LeaveCriticalSection(&g_deviceLock);
        return -1;
    }
    log_event("reconnect_device: success");
    log_exclusive_status();
    LeaveCriticalSection(&g_deviceLock);
    return 0;
}

static volatile LONG g_reconnectInProgress = 0;

DWORD WINAPI reconnect_thread_proc(LPVOID p) {
    (void)p;
    reconnect_device();
    InterlockedExchange(&g_reconnectInProgress, 0);
    return 0;
}

void request_reconnect(const char *reason) {
    char logbuf[160];
    if (InterlockedCompareExchange(&g_reconnectInProgress, 1, 0) != 0) {
        sprintf(logbuf, "request_reconnect(%s): skipped, one already in progress", reason);
        log_event(logbuf);
        return;
    }
    sprintf(logbuf, "request_reconnect(%s): spawning reconnect thread", reason);
    log_event(logbuf);
    HANDLE h = CreateThread(NULL, 0, reconnect_thread_proc, NULL, 0, NULL);
    if (h) CloseHandle(h);
}

#define TASK_NAME L"Better RGB by TheYamo"
#define VENDOR_SERVICE_DEFAULT "GCUBridge"

static HWND g_btnAutostart;

static int is_elevated(void) {
    HANDLE tok;
    TOKEN_ELEVATION te;
    DWORD n;
    int r = 0;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
        if (GetTokenInformation(tok, TokenElevation, &te, sizeof(te), &n))
            r = te.TokenIsElevated ? 1 : 0;
        CloseHandle(tok);
    }
    return r;
}

static int find_vendor_service(char *out, DWORD outLen) {
    SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE);
    if (!scm) return 0;
    SC_HANDLE svc = OpenServiceA(scm, VENDOR_SERVICE_DEFAULT, SERVICE_QUERY_STATUS);
    if (svc) {
        CloseServiceHandle(svc);
        CloseServiceHandle(scm);
        strncpy(out, VENDOR_SERVICE_DEFAULT, outLen - 1);
        out[outLen - 1] = 0;
        return 1;
    }
    int found = 0;
    DWORD bytesNeeded = 0, count = 0, resume = 0;
    EnumServicesStatusExA(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL,
        NULL, 0, &bytesNeeded, &count, &resume, NULL);
    if (bytesNeeded > 0) {
        BYTE *buf = (BYTE *)malloc(bytesNeeded);
        if (buf && EnumServicesStatusExA(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL,
                buf, bytesNeeded, &bytesNeeded, &count, &resume, NULL)) {
            ENUM_SERVICE_STATUS_PROCESSA *e = (ENUM_SERVICE_STATUS_PROCESSA *)buf;
            for (DWORD i = 0; i < count && !found; i++) {
                char low[256];
                strncpy(low, e[i].lpServiceName, sizeof(low) - 1);
                low[sizeof(low) - 1] = 0;
                for (char *p = low; *p; p++) *p = (char)tolower((unsigned char)*p);
                if (strstr(low, "gcu")) {
                    strncpy(out, e[i].lpServiceName, outLen - 1);
                    out[outLen - 1] = 0;
                    found = 1;
                }
            }
        }
        if (buf) free(buf);
    }
    CloseServiceHandle(scm);
    return found;
}

static int stop_vendor_service(const char *name) {
    SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return -1;
    SC_HANDLE svc = OpenServiceA(scm, name, SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (!svc) {
        CloseServiceHandle(scm);
        return -1;
    }
    SERVICE_STATUS ss;
    memset(&ss, 0, sizeof(ss));
    QueryServiceStatus(svc, &ss);
    if (ss.dwCurrentState != SERVICE_STOPPED) {
        ControlService(svc, SERVICE_CONTROL_STOP, &ss);
        for (int i = 0; i < 100; i++) {
            if (!QueryServiceStatus(svc, &ss)) break;
            if (ss.dwCurrentState == SERVICE_STOPPED) break;
            Sleep(100);
        }
    }
    int ok = (ss.dwCurrentState == SERVICE_STOPPED) ? 0 : -1;
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return ok;
}

static int start_vendor_service(const char *name) {
    SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return -1;
    SC_HANDLE svc = OpenServiceA(scm, name, SERVICE_START);
    if (!svc) {
        CloseServiceHandle(scm);
        return -1;
    }
    int ok = 0;
    if (!StartServiceA(svc, 0, NULL) && GetLastError() != ERROR_SERVICE_ALREADY_RUNNING)
        ok = -1;
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return ok;
}

static void restart_vendor_service_if_found(void) {
    char svcName[256];
    char lb[384];
    if (find_vendor_service(svcName, sizeof(svcName))) {
        start_vendor_service(svcName);
        sprintf(lb, "relock: vendor service '%s' restarted", svcName);
        log_event(lb);
    } else {
        log_event("relock: vendor service not found for restart");
    }
}

static int begin_relock(void) {
    char svcName[256];
    char lb[384];
    if (!find_vendor_service(svcName, sizeof(svcName))) {
        log_event("relock: vendor service not found, cannot fix automatically");
        return -1;
    }
    sprintf(lb, "relock: stopping vendor service '%s'", svcName);
    log_event(lb);
    if (stop_vendor_service(svcName) < 0) {
        log_event("relock: could not stop vendor service");
        return -1;
    }
    EnterCriticalSection(&g_deviceLock);
    if (g_h) {
        hid_close(g_h);
        g_h = NULL;
    }
    LeaveCriticalSection(&g_deviceLock);
    wchar_t exe[MAX_PATH];
    wchar_t cmdLine[MAX_PATH + 32];
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    _snwprintf(cmdLine, MAX_PATH + 31, L"\"%ls\" --relock %lu%ls", exe, (unsigned long)GetCurrentProcessId(), g_startupLaunch ? L" --startup" : L"");
    cmdLine[MAX_PATH + 31] = 0;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    if (!CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        log_event("relock: failed to spawn fresh process, restoring previous state");
        start_vendor_service(svcName);
        EnterCriticalSection(&g_deviceLock);
        g_h = hid_open_path(g_devicePath);
        if (g_h) enter_custom_mode();
        LeaveCriticalSection(&g_deviceLock);
        return -1;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    log_event("relock: fresh process spawned, this instance is exiting");
    return 0;
}

static int run_schtasks(const wchar_t *args) {
    wchar_t cmdLine[1200];
    _snwprintf(cmdLine, 1199, L"schtasks.exe %ls", args);
    cmdLine[1199] = 0;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    DWORD code = 1;
    if (CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 20000);
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    return (int)code;
}

static int autostart_task_exists(void) {
    return run_schtasks(L"/Query /TN \"" TASK_NAME L"\"") == 0;
}

static int autostart_install(void) {
    wchar_t exe[MAX_PATH];
    wchar_t user[128] = L"";
    wchar_t domain[128] = L"";
    wchar_t account[300];
    wchar_t tempDir[MAX_PATH];
    wchar_t xmlPath[MAX_PATH];
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    GetEnvironmentVariableW(L"USERDOMAIN", domain, 128);
    GetEnvironmentVariableW(L"USERNAME", user, 128);
    if (!user[0]) return -1;
    if (domain[0]) _snwprintf(account, 299, L"%ls\\%ls", domain, user);
    else _snwprintf(account, 299, L"%ls", user);
    account[299] = 0;
    if (!GetTempPathW(MAX_PATH, tempDir)) return -1;
    _snwprintf(xmlPath, MAX_PATH - 1, L"%lsbetter_rgb_task.xml", tempDir);
    xmlPath[MAX_PATH - 1] = 0;
    wchar_t xml[4096];
    _snwprintf(xml, 4095,
        L"<?xml version=\"1.0\" encoding=\"UTF-16\"?>\r\n"
        L"<Task version=\"1.2\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">\r\n"
        L"  <Triggers>\r\n"
        L"    <LogonTrigger>\r\n"
        L"      <Enabled>true</Enabled>\r\n"
        L"      <UserId>%ls</UserId>\r\n"
        L"    </LogonTrigger>\r\n"
        L"  </Triggers>\r\n"
        L"  <Principals>\r\n"
        L"    <Principal id=\"Author\">\r\n"
        L"      <UserId>%ls</UserId>\r\n"
        L"      <LogonType>InteractiveToken</LogonType>\r\n"
        L"      <RunLevel>HighestAvailable</RunLevel>\r\n"
        L"    </Principal>\r\n"
        L"  </Principals>\r\n"
        L"  <Settings>\r\n"
        L"    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>\r\n"
        L"    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>\r\n"
        L"    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>\r\n"
        L"    <AllowHardTerminate>false</AllowHardTerminate>\r\n"
        L"    <StartWhenAvailable>true</StartWhenAvailable>\r\n"
        L"    <AllowStartOnDemand>true</AllowStartOnDemand>\r\n"
        L"    <Enabled>true</Enabled>\r\n"
        L"    <Hidden>false</Hidden>\r\n"
        L"    <ExecutionTimeLimit>PT0S</ExecutionTimeLimit>\r\n"
        L"    <Priority>4</Priority>\r\n"
        L"    <RestartOnFailure>\r\n"
        L"      <Interval>PT1M</Interval>\r\n"
        L"      <Count>3</Count>\r\n"
        L"    </RestartOnFailure>\r\n"
        L"  </Settings>\r\n"
        L"  <Actions Context=\"Author\">\r\n"
        L"    <Exec>\r\n"
        L"      <Command>\"%ls\"</Command>\r\n"
        L"      <Arguments>--startup</Arguments>\r\n"
        L"    </Exec>\r\n"
        L"  </Actions>\r\n"
        L"</Task>\r\n",
        account, account, exe);
    xml[4095] = 0;
    HANDLE f = CreateFileW(xmlPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return -1;
    unsigned short bom = 0xFEFF;
    DWORD written = 0;
    WriteFile(f, &bom, 2, &written, NULL);
    WriteFile(f, xml, (DWORD)(wcslen(xml) * sizeof(wchar_t)), &written, NULL);
    CloseHandle(f);
    wchar_t args[MAX_PATH + 128];
    _snwprintf(args, MAX_PATH + 127, L"/Create /F /TN \"" TASK_NAME L"\" /XML \"%ls\"", xmlPath);
    args[MAX_PATH + 127] = 0;
    int r = run_schtasks(args);
    DeleteFileW(xmlPath);
    return r == 0 ? 0 : -1;
}

static int autostart_remove(void) {
    return run_schtasks(L"/Delete /F /TN \"" TASK_NAME L"\"") == 0 ? 0 : -1;
}

static void autostart_apply(int install) {
    int r = install ? autostart_install() : autostart_remove();
    const wchar_t *msg;
    if (r == 0 && install)
        msg = L"开机自动启动已开启。\n"
              L"每次登录 Windows 时，程序都会以管理员权限自动启动，不再显示 UAC 提示。";
    else if (r == 0)
        msg = L"开机自动启动已关闭。";
    else
        msg = L"操作失败，请重试。";
    MessageBoxW(NULL, msg, L"Better RGB 中文增强版", MB_OK | (r == 0 ? MB_ICONINFORMATION : MB_ICONERROR));
}

static int relaunch_elevated(const wchar_t *args, int waitForExit) {
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    SHELLEXECUTEINFOW sei;
    memset(&sei, 0, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = exe;
    sei.lpParameters = args;
    sei.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&sei)) return -1;
    if (waitForExit && sei.hProcess) WaitForSingleObject(sei.hProcess, 120000);
    if (sei.hProcess) CloseHandle(sei.hProcess);
    return 0;
}

static void update_autostart_button(void) {
    if (!g_btnAutostart) return;
    SetWindowTextW(g_btnAutostart, autostart_task_exists()
        ? L"开机自动启动：已开启"
        : L"开机自动启动：已关闭");
    InvalidateRect(g_btnAutostart, NULL, TRUE);
}

static volatile LONG g_monitorStop = 0;

static void hexdump_to_log(const char *prefix, const unsigned char *d, int n) {
    char line[64 + 65*3 + 16];
    int off = sprintf(line, "%s (%d bytes):", prefix, n);
    for (int i = 0; i < n && off < (int)sizeof(line) - 4; i++)
        off += sprintf(line + off, " %02X", d[i]);
    log_event(line);
}

DWORD WINAPI ec_monitor_thread(LPVOID p) {
    (void)p;
    unsigned char buf[65];
    hid_device *h = NULL;
    log_event("ec_monitor: started (passive, own read-only handle)");
    while (!g_monitorStop) {
        if (!h) {

            if (g_reconnectInProgress) { Sleep(250); continue; }
            h = hid_open_path(g_devicePath);
            if (!h) { Sleep(1000); continue; }
            log_event("ec_monitor: monitor handle opened");
        }
        int n = hid_read_timeout(h, buf, (int)sizeof(buf), 500);
        if (n > 0) {
            hexdump_to_log("ec_monitor: IN report from EC", buf, n);
        } else if (n < 0) {
            log_event("ec_monitor: read error, closing and reopening handle");
            hid_close(h);
            h = NULL;
            Sleep(500);
        }

    }
    if (h) hid_close(h);
    log_event("ec_monitor: stopped");
    return 0;
}

static void reset_frame_pacer(void) {
    QueryPerformanceFrequency(&g_pacerFrequency);
    g_nextFrameTick = 0;
}

static void notify_fps_status(void) {
    if (g_hMainWindow) PostMessage(g_hMainWindow, WM_FPS_STATUS, 0, 0);
}

static void set_effective_fps(LONG fps, const char *reason) {
    LONG target = InterlockedCompareExchange(&g_targetFps, 0, 0);
    if (fps < TARGET_FPS_MIN) fps = TARGET_FPS_MIN;
    if (fps > target) fps = target;
    LONG previous = InterlockedExchange(&g_effectiveFps, fps);
    if (previous != fps) {
        char logbuf[160];
        sprintf(logbuf, "frame pacing: effective FPS %ld -> %ld (%s)",
            (long)previous, (long)fps, reason);
        log_event(logbuf);
        InterlockedExchange(&g_pacerResetRequested, 1);
        notify_fps_status();
    }
}

static void record_transport_result(int success, ULONGLONG callTime) {
    LONG fps = InterlockedCompareExchange(&g_effectiveFps, 0, 0);
    LONG target = InterlockedCompareExchange(&g_targetFps, 0, 0);
    ULONGLONG budgetMs = 1000ULL / (fps > 0 ? (ULONGLONG)fps : TARGET_FPS_DEFAULT);

    if (!success) {
        InterlockedExchange(&g_slowFrameCount, 0);
        InterlockedExchange(&g_healthyFrameCount, 0);
        set_effective_fps(fps - 5, "HID write failure");
        return;
    }

    if (callTime * 10ULL >= budgetMs * 9ULL) {
        InterlockedExchange(&g_healthyFrameCount, 0);
        if (InterlockedIncrement(&g_slowFrameCount) >= 3) {
            InterlockedExchange(&g_slowFrameCount, 0);
            set_effective_fps(fps - 5, "transport exceeded frame budget");
        }
        return;
    }

    InterlockedExchange(&g_slowFrameCount, 0);
    if (fps < target && InterlockedIncrement(&g_healthyFrameCount) >= fps * 10) {
        InterlockedExchange(&g_healthyFrameCount, 0);
        set_effective_fps(fps + 1, "transport stable for 10 seconds");
    }
}

static void pace_frame(void) {
    if (InterlockedCompareExchange(&g_idleLightsOff, 0, 0)) {
        Sleep(100);
        InterlockedExchange(&g_pacerResetRequested, 1);
        return;
    }
    if (!g_pacerFrequency.QuadPart || InterlockedExchange(&g_pacerResetRequested, 0)) {
        reset_frame_pacer();
    }
    LONG fps = InterlockedCompareExchange(&g_effectiveFps, 0, 0);
    if (fps < TARGET_FPS_MIN) fps = TARGET_FPS_MIN;

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    LONGLONG interval = g_pacerFrequency.QuadPart / fps;
    if (!g_nextFrameTick || now.QuadPart > g_nextFrameTick + interval * 2) {
        g_nextFrameTick = now.QuadPart;
    }
    g_nextFrameTick += interval;

    while (now.QuadPart < g_nextFrameTick) {
        LONGLONG remaining = g_nextFrameTick - now.QuadPart;
        DWORD sleepMs = (DWORD)(remaining * 1000 / g_pacerFrequency.QuadPart);
        if (sleepMs > 1) Sleep(sleepMs - 1);
        else Sleep(0);
        QueryPerformanceCounter(&now);
    }
}

static int idle_timeout_reached(void) {
    LONG minutes = InterlockedCompareExchange(&g_idleTimeoutMinutes, 0, 0);
    if (minutes <= 0) return 0;
    LASTINPUTINFO info;
    info.cbSize = sizeof(info);
    if (!GetLastInputInfo(&info)) return 0;
    return (DWORD)(GetTickCount() - info.dwTime) >= (DWORD)minutes * 60U * 1000U;
}

static void apply_white_gains_at(unsigned char *buf, int offset,
                                 LONG gainR, LONG gainG, LONG gainB,
                                 unsigned char *visited) {
    if (offset < 0 || offset + 2 >= BUF_SIZE || visited[offset]) return;
    visited[offset] = 1;
    buf[offset] = (unsigned char)(((int)buf[offset] * (int)gainR + 500) / 1000);
    buf[offset + 1] = (unsigned char)(((int)buf[offset + 1] * (int)gainG + 500) / 1000);
    buf[offset + 2] = (unsigned char)(((int)buf[offset + 2] * (int)gainB + 500) / 1000);
}

/*
 * 在最终 HID 帧出口统一应用三通道逆向增益。使用独立输出缓冲，保证启动、
 * 休眠和退出过渡仍以未经校正的逻辑帧计算。
 */
static void apply_global_white_calibration(const unsigned char *source, unsigned char *output) {
    memcpy(output, source, BUF_SIZE);
    LONG gainR = cfg_clamp(InterlockedCompareExchange(&g_whiteGainR, 0, 0), 50, 1000);
    LONG gainG = cfg_clamp(InterlockedCompareExchange(&g_whiteGainG, 0, 0), 50, 1000);
    LONG gainB = cfg_clamp(InterlockedCompareExchange(&g_whiteGainB, 0, 0), 50, 1000);
    if (gainR == 1000 && gainG == 1000 && gainB == 1000) return;

    unsigned char visited[BUF_SIZE] = {0};
    for (size_t i = 0; i < KEYMAP_COUNT; i++)
        apply_white_gains_at(output, KEYMAP[i].offset, gainR, gainG, gainB, visited);
    apply_white_gains_at(output, OFFSET_SHIFT_LEFT_AUX, gainR, gainG, gainB, visited);
    apply_white_gains_at(output, OFFSET_ENTER_AUX, gainR, gainG, gainB, visited);
}

static void build_raw_white_calibration_frame(unsigned char *output) {
    memset(output, 0, BUF_SIZE);
    for (size_t i = 0; i < KEYMAP_COUNT; i++) {
        int offset = KEYMAP[i].offset;
        if (offset >= 0 && offset + 2 < BUF_SIZE)
            output[offset] = output[offset + 1] = output[offset + 2] = 255;
    }
    output[OFFSET_SHIFT_LEFT_AUX] = output[OFFSET_SHIFT_LEFT_AUX + 1] =
        output[OFFSET_SHIFT_LEFT_AUX + 2] = 255;
    output[OFFSET_ENTER_AUX] = output[OFFSET_ENTER_AUX + 1] =
        output[OFFSET_ENTER_AUX + 2] = 255;
}

/*
 * 全局唯一的帧出口：依次完成效果合成、Touch Bar 覆盖、过渡蒙版、光谱校准与
 * 亮度缩放，然后在 g_deviceLock 内把 512 字节拆成 8 个 64 字节块提交。
 * 发送失败只请求重连，不允许灯效线程自行重开设备。
 */
int send_frame(unsigned char *buf) {
    unsigned char composed[BUF_SIZE];
    unsigned char mainTransitioned[BUF_SIZE];
    unsigned char transitioned[BUF_SIZE];
    unsigned char idleTransitioned[BUF_SIZE];
    unsigned char calibrated[BUF_SIZE];
    unsigned char calibrationWhite[BUF_SIZE];
    unsigned char scaled[BUF_SIZE];
    int finishIdleSleep = 0;
    int forceIdleBlank = 0;
    int shuttingDown = InterlockedCompareExchange(&g_shutdownFadeStarted, 0, 0) != 0;
    int calibrating = InterlockedCompareExchange(&g_whiteCalibrationActive, 0, 0) != 0;
    int timedOut = !shuttingDown && !calibrating && idle_timeout_reached();
    LONG transition = InterlockedCompareExchange(&g_powerTransitionMode, 0, 0);
    LONG duration = InterlockedCompareExchange(&g_transitionDurationMs, 0, 0);
    LONG idleState = InterlockedCompareExchange(&g_idleTransitionState, 0, 0);
    LONGLONG now = (LONGLONG)GetTickCount64();

    if (duration < 1) duration = TRANSITION_DEFAULT_MS;
    if (!shuttingDown) {
        if (timedOut && idleState == IDLE_TRANSITION_ACTIVE &&
            InterlockedCompareExchange(&g_startupTransitionActive, 0, 0)) {
            InterlockedExchange(&g_startupTransitionActive, 0);
            InterlockedExchange64(&g_startupTransitionStartTick, 0);
            idleState = IDLE_TRANSITION_ASLEEP;
            InterlockedExchange(&g_idleTransitionState, idleState);
            InterlockedExchange64(&g_idleTransitionStartTick, 0);
            forceIdleBlank = 1;
            log_event("idle transition: startup already timed out, remaining asleep");
        } else if (transition == TRANSITION_OFF) {
            if (timedOut && idleState != IDLE_TRANSITION_ASLEEP) {
                idleState = IDLE_TRANSITION_ASLEEP;
                forceIdleBlank = 1;
                log_event("idle transition: timeout reached, switching off immediately");
            } else if (!timedOut && idleState != IDLE_TRANSITION_ACTIVE) {
                idleState = IDLE_TRANSITION_ACTIVE;
                InterlockedExchange(&g_pacerResetRequested, 1);
                log_event("idle transition: input detected, restoring immediately");
            }
            InterlockedExchange(&g_idleTransitionState, idleState);
            InterlockedExchange64(&g_idleTransitionStartTick, 0);
        } else if (timedOut) {
            if (idleState == IDLE_TRANSITION_ACTIVE) {
                idleState = IDLE_TRANSITION_FADING_OUT;
                InterlockedExchange(&g_idleTransitionState, idleState);
                InterlockedExchange64(&g_idleTransitionStartTick, now);
                log_event("idle transition: timeout reached, fading out");
            } else if (idleState == IDLE_TRANSITION_FADING_IN) {
                LONGLONG started = InterlockedCompareExchange64(&g_idleTransitionStartTick, 0, 0);
                LONGLONG elapsed = started > 0 ? now - started : 0;
                if (elapsed < 0) elapsed = 0;
                if (elapsed > duration) elapsed = duration;
                idleState = IDLE_TRANSITION_FADING_OUT;
                InterlockedExchange(&g_idleTransitionState, idleState);
                InterlockedExchange64(&g_idleTransitionStartTick, now - (duration - elapsed));
                log_event("idle transition: timeout returned, reversing toward sleep");
            }
        } else {
            if (idleState == IDLE_TRANSITION_ASLEEP) {
                idleState = IDLE_TRANSITION_FADING_IN;
                InterlockedExchange(&g_idleTransitionState, idleState);
                InterlockedExchange64(&g_idleTransitionStartTick, now);
                InterlockedExchange(&g_pacerResetRequested, 1);
                log_event("idle transition: input detected, fading in");
            } else if (idleState == IDLE_TRANSITION_FADING_OUT) {
                LONGLONG started = InterlockedCompareExchange64(&g_idleTransitionStartTick, 0, 0);
                LONGLONG elapsed = started > 0 ? now - started : 0;
                if (elapsed < 0) elapsed = 0;
                if (elapsed > duration) elapsed = duration;
                idleState = IDLE_TRANSITION_FADING_IN;
                InterlockedExchange(&g_idleTransitionState, idleState);
                InterlockedExchange64(&g_idleTransitionStartTick, now - (duration - elapsed));
                InterlockedExchange(&g_pacerResetRequested, 1);
                log_event("idle transition: input detected, reversing toward active");
            }
        }

        if (idleState == IDLE_TRANSITION_ASLEEP && timedOut && !forceIdleBlank) {
            InterlockedExchange(&g_idleLightsOff, 1);
            return 0;
        }
    }
    InterlockedExchange(&g_idleLightsOff, 0);

    memcpy(g_lastMainRawFrame, buf, BUF_SIZE);
    InterlockedExchange(&g_lastMainRawFrameValid, 1);
    if (!shuttingDown && InterlockedCompareExchange(&g_mainRevealActive, 0, 0)) {
        if (transition == TRANSITION_OFF) {
            InterlockedExchange(&g_mainRevealActive, 0);
        } else {
            LONGLONG started = InterlockedCompareExchange64(&g_mainRevealStartTick, now, 0);
            if (!started) {
                started = now;
                log_event("main transition: reveal started");
            }
            double progress = (double)(now - started) / (double)duration;
            if (progress >= 1.0) {
                InterlockedExchange(&g_mainRevealActive, 0);
                InterlockedExchange64(&g_mainRevealStartTick, 0);
                log_event("main transition: reveal completed");
            } else {
                if (progress < 0.0) progress = 0.0;
                apply_power_transition_mask(buf, mainTransitioned, transition, progress);
                buf = mainTransitioned;
            }
        }
    }

    if (!shuttingDown && InterlockedCompareExchange(&g_touchbarDisplayMode, 0, 0) != TOUCHBAR_OFF) {
        memcpy(composed, buf, BUF_SIZE);
        apply_touchbar_overlay(composed);
        buf = composed;
    }
    memcpy(g_lastRawFrame, buf, BUF_SIZE);
    InterlockedExchange(&g_lastRawFrameValid, 1);
    if (!InterlockedCompareExchange(&g_shutdownFadeStarted, 0, 0) &&
        InterlockedCompareExchange(&g_startupTransitionActive, 0, 0)) {
        if (transition == TRANSITION_OFF) {
            InterlockedExchange(&g_startupTransitionActive, 0);
        } else {
            LONGLONG started = InterlockedCompareExchange64(
                &g_startupTransitionStartTick, now, 0);
            if (!started) {
                started = now;
                log_event("startup transition: started");
            }
            double progress = duration > 0 ? (double)(now - started) / (double)duration : 1.0;
            if (progress >= 1.0) {
                InterlockedExchange(&g_startupTransitionActive, 0);
                log_event("startup transition: completed");
            } else {
                if (progress < 0.0) progress = 0.0;
                apply_power_transition_mask(buf, transitioned, transition, progress);
                buf = transitioned;
            }
        }
    }

    if (!shuttingDown) {
        if (forceIdleBlank) {
            memset(idleTransitioned, 0, BUF_SIZE);
            buf = idleTransitioned;
            finishIdleSleep = 1;
        } else if (idleState == IDLE_TRANSITION_FADING_OUT ||
                   idleState == IDLE_TRANSITION_FADING_IN) {
            LONGLONG started = InterlockedCompareExchange64(&g_idleTransitionStartTick, 0, 0);
            double elapsed = started > 0 ? (double)(now - started) : 0.0;
            double progress = elapsed / (double)duration;
            if (progress < 0.0) progress = 0.0;
            if (progress > 1.0) progress = 1.0;
            if (idleState == IDLE_TRANSITION_FADING_OUT) {
                progress = 1.0 - progress;
                if (progress <= 0.0) finishIdleSleep = 1;
            } else if (progress >= 1.0) {
                InterlockedExchange(&g_idleTransitionState, IDLE_TRANSITION_ACTIVE);
                InterlockedExchange64(&g_idleTransitionStartTick, 0);
                log_event("idle transition: wake completed");
            }
            apply_power_transition_mask(buf, idleTransitioned, transition, progress);
            buf = idleTransitioned;
        }
    }

    if (calibrating) {
        build_raw_white_calibration_frame(calibrationWhite);
        buf = calibrationWhite;
    } else {
        apply_global_white_calibration(buf, calibrated);
        buf = calibrated;
    }

    ULONGLONG tStart = GetTickCount64();
    EnterCriticalSection(&g_deviceLock);
    ULONGLONG tLocked = GetTickCount64();
    int bright = calibrating ? 100 : (int)g_brightness;
    if (bright < 100) {
        for (int i = 0; i < BUF_SIZE; i++) scaled[i] = (unsigned char)((int)buf[i] * bright / 100);
        buf = scaled;
    }
    unsigned char step3[9] = {0x00,0x12,0x00,0x00,0x08,0x00,0x00,0x00,0x00};
    if (hid_send_feature_report(g_h, step3, 9) < 0) {
        log_event("send_frame: feature report failed, attempting reconnect");
        if (reconnect_device() < 0) { LeaveCriticalSection(&g_deviceLock); record_transport_result(0, 0); return -1; }
        if (hid_send_feature_report(g_h, step3, 9) < 0) { LeaveCriticalSection(&g_deviceLock); record_transport_result(0, 0); return -1; }
    }
    for (int i = 0; i < 8; i++) {
        unsigned char chunk[65];
        chunk[0] = 0x00;
        memcpy(chunk+1, buf + i*64, 64);
        if (hid_write(g_h, chunk, 65) < 0) {
            log_event("send_frame: chunk write failed, attempting reconnect");
            if (reconnect_device() < 0) { LeaveCriticalSection(&g_deviceLock); record_transport_result(0, 0); return -1; }
            LeaveCriticalSection(&g_deviceLock);
            record_transport_result(0, 0);
            return -1;
        }
    }
    ULONGLONG tDone = GetTickCount64();
    LeaveCriticalSection(&g_deviceLock);

    ULONGLONG lockWait = tLocked - tStart;
    ULONGLONG callTime = tDone - tLocked;
    if (lockWait > 200 || callTime > 200) {
        char logbuf[160];
        sprintf(logbuf, "send_frame SLOW: lock_wait=%llums call_time=%llums",
            (unsigned long long)lockWait, (unsigned long long)callTime);
        log_event(logbuf);
    }
    record_transport_result(1, callTime);
    if (finishIdleSleep) {
        InterlockedExchange(&g_idleTransitionState, IDLE_TRANSITION_ASLEEP);
        InterlockedExchange64(&g_idleTransitionStartTick, 0);
        InterlockedExchange(&g_idleLightsOff, 1);
        log_event("idle transition: sleep completed");
    }
    return 0;
}

void set_key(unsigned char *buf, int offset, unsigned char r, unsigned char g, unsigned char b) {
    if (offset < 0 || offset+2 >= BUF_SIZE) return;
    buf[offset] = r; buf[offset+1] = g; buf[offset+2] = b;

    // Wide keys use a second LED slot on some ANSI/ISO ITE8291 layouts.
    int auxOffset = -1;
    if (offset == OFFSET_SHIFT_LEFT) auxOffset = OFFSET_SHIFT_LEFT_AUX;
    else if (offset == OFFSET_ENTER) auxOffset = OFFSET_ENTER_AUX;
    if (auxOffset >= 0 && auxOffset + 2 < BUF_SIZE) {
        buf[auxOffset] = r; buf[auxOffset+1] = g; buf[auxOffset+2] = b;
    }
}

void hsv_to_rgb(double h, unsigned char *r, unsigned char *g, unsigned char *b) {
    h = fmod(h, 360.0); if (h < 0) h += 360.0;
    double c = 1.0, x = c * (1 - fabs(fmod(h/60.0, 2)-1));
    double rr,gg,bb;
    if (h < 60){rr=c;gg=x;bb=0;} else if (h<120){rr=x;gg=c;bb=0;}
    else if (h<180){rr=0;gg=c;bb=x;} else if (h<240){rr=0;gg=x;bb=c;}
    else if (h<300){rr=x;gg=0;bb=c;} else {rr=c;gg=0;bb=x;}
    *r=(unsigned char)(rr*255); *g=(unsigned char)(gg*255); *b=(unsigned char)(bb*255);
}

static double wrap_unit(double value) {
    value = fmod(value, 1.0);
    return value < 0.0 ? value + 1.0 : value;
}

static double smooth_unit(double value) {
    if (value <= 0.0) return 0.0;
    if (value >= 1.0) return 1.0;
    return value * value * (3.0 - 2.0 * value);
}

static double srgb_channel_to_linear(unsigned char value) {
    double c = (double)value / 255.0;
    return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

static unsigned char linear_channel_to_srgb(double value) {
    if (value <= 0.0) return 0;
    if (value >= 1.0) return 255;
    double c = value <= 0.0031308 ? value * 12.92 : 1.055 * pow(value, 1.0 / 2.4) - 0.055;
    int result = (int)(c * 255.0 + 0.5);
    if (result < 0) result = 0;
    if (result > 255) result = 255;
    return (unsigned char)result;
}

static LONG white_gain_from_linear(double weakest, double channel) {
    if (channel <= 0.000001) return 1000;
    LONG gain = (LONG)(weakest / channel * 1000.0 + 0.5);
    return cfg_clamp(gain, 50, 1000);
}

/*
 * 用户在光谱中选择“纯白指令下肉眼看到的实际颜色”。先转到线性光空间，
 * 再以最弱通道为基准反算三个独立增益；只衰减过强通道，避免任何通道溢出。
 */
static int calculate_white_calibration(COLORREF observed,
                                       LONG *gainR, LONG *gainG, LONG *gainB) {
    double r = srgb_channel_to_linear(GetRValue(observed));
    double g = srgb_channel_to_linear(GetGValue(observed));
    double b = srgb_channel_to_linear(GetBValue(observed));
    double strongest = fmax(r, fmax(g, b));
    double weakest = fmin(r, fmin(g, b));
    if (strongest < 0.01 || weakest < 0.0005) return 0;
    *gainR = white_gain_from_linear(weakest, r);
    *gainG = white_gain_from_linear(weakest, g);
    *gainB = white_gain_from_linear(weakest, b);
    return 1;
}

static void sample_palette_linear(const COLORREF colors[3], double phase,
                                  double *r, double *g, double *b) {
    double scaled = wrap_unit(phase) * 3.0;
    int first = (int)floor(scaled);
    if (first > 2) first = 2;
    int second = (first + 1) % 3;
    double mix = smooth_unit(scaled - floor(scaled));
    COLORREF a = colors[first], next = colors[second];
    *r = srgb_channel_to_linear(GetRValue(a)) * (1.0 - mix) +
         srgb_channel_to_linear(GetRValue(next)) * mix;
    *g = srgb_channel_to_linear(GetGValue(a)) * (1.0 - mix) +
         srgb_channel_to_linear(GetGValue(next)) * mix;
    *b = srgb_channel_to_linear(GetBValue(a)) * (1.0 - mix) +
         srgb_channel_to_linear(GetBValue(next)) * mix;
}

static void set_key_from_linear(unsigned char *buf, const KeyEntry *key,
                                double r, double g, double b) {
    set_key(buf, key->offset,
        linear_channel_to_srgb(r),
        linear_channel_to_srgb(g),
        linear_channel_to_srgb(b));
}

static void sample_touchbar_gradient(double phase, double *r, double *g, double *b) {
    if (phase < 0.0) phase = 0.0;
    if (phase > 1.0) phase = 1.0;
    phase = smooth_unit(phase);
    COLORREF a = g_touchbarColorStart;
    COLORREF z = g_touchbarColorEnd;
    *r = srgb_channel_to_linear(GetRValue(a)) * (1.0 - phase) +
         srgb_channel_to_linear(GetRValue(z)) * phase;
    *g = srgb_channel_to_linear(GetGValue(a)) * (1.0 - phase) +
         srgb_channel_to_linear(GetGValue(z)) * phase;
    *b = srgb_channel_to_linear(GetBValue(a)) * (1.0 - phase) +
         srgb_channel_to_linear(GetBValue(z)) * phase;
}

static void apply_touchbar_overlay(unsigned char *buf) {
    enum { TOP_KEY_COUNT = 20 };
    LONG levelRaw = InterlockedCompareExchange(&g_touchbarLevel, 0, 0);
    LONG mode = InterlockedCompareExchange(&g_touchbarDisplayMode, 0, 0);
    LONG direction = InterlockedCompareExchange(&g_touchbarDirection, 0, 0);
    double level = (double)levelRaw / 10000.0;
    if (level < 0.0) level = 0.0;
    if (level > 1.0) level = 1.0;

    int units = (direction == TOUCHBAR_EDGES_TO_CENTER ||
                 direction == TOUCHBAR_CENTER_TO_EDGES) ? TOP_KEY_COUNT / 2 : TOP_KEY_COUNT;
    double filled = level * (double)units;
    double uniformR = 0.0, uniformG = 0.0, uniformB = 0.0;
    if (mode != TOUCHBAR_AUDIO)
        sample_touchbar_gradient(mode == TOUCHBAR_BATTERY ? 1.0 - level : level,
            &uniformR, &uniformG, &uniformB);
    double audioTime = wrap_unit((double)GetTickCount64() / 12000.0);
    for (int i = 0; i < TOP_KEY_COUNT; i++) {
        int rank;
        if (direction == TOUCHBAR_RIGHT_TO_LEFT) rank = TOP_KEY_COUNT - 1 - i;
        else if (direction == TOUCHBAR_EDGES_TO_CENTER) rank = i < TOP_KEY_COUNT - 1 - i ? i : TOP_KEY_COUNT - 1 - i;
        else if (direction == TOUCHBAR_CENTER_TO_EDGES) {
            int edgeRank = i < TOP_KEY_COUNT - 1 - i ? i : TOP_KEY_COUNT - 1 - i;
            rank = units - 1 - edgeRank;
        } else rank = i;

        double coverage = smooth_unit(filled - (double)rank);
        double gradientPhase = units > 1 ? (double)rank / (double)(units - 1) : 0.0;
        double r, g, b;
        if (mode == TOUCHBAR_AUDIO) {
            sample_palette_linear(g_touchbarAudioColors,
                audioTime - gradientPhase * 0.72, &r, &g, &b);
        } else {
            r = uniformR;
            g = uniformG;
            b = uniformB;
        }
        set_key_from_linear(buf, &KEYMAP[i], r * coverage, g * coverage, b * coverage);
    }
}

int compute_prob_count(int target, int minCount, int maxCount) {
    if (target >= maxCount) return maxCount;
    int r = rand() % 100;
    if (r < 25) { int v = target-1; return (v < minCount) ? minCount : v; }
    else if (r < 75) return target;
    else { int v = target+1; return (v > maxCount) ? maxCount : v; }
}

double get_key_x(const KeyEntry *k) {
    // Physical key-centre coordinates, measured in one-key pitches.  KEYMAP's
    // row/column values describe HID slots, not the staggered keyboard geometry.
    if (strcmp(k->name,"NumLock")==0 || strcmp(k->name,"Numpad7")==0 ||
        strcmp(k->name,"Numpad4")==0 || strcmp(k->name,"Numpad1")==0) return 15.5;
    if (strcmp(k->name,"NumpadDiv")==0 || strcmp(k->name,"Numpad8")==0 ||
        strcmp(k->name,"Numpad5")==0 || strcmp(k->name,"Numpad2")==0) return 16.5;
    if (strcmp(k->name,"NumpadMul")==0 || strcmp(k->name,"Numpad9")==0 ||
        strcmp(k->name,"Numpad6")==0 || strcmp(k->name,"Numpad3")==0 ||
        strcmp(k->name,"NumpadDecimal")==0) return 17.5;
    if (strcmp(k->name,"NumpadMinus")==0 || strcmp(k->name,"NumpadPlus")==0 ||
        strcmp(k->name,"NumpadEnter")==0) return 18.5;
    if (strcmp(k->name,"Numpad0")==0) return 16.0;

    if (strcmp(k->name,"Backspace")==0) return 13.5;
    if (strcmp(k->name,"Enter")==0) return 13.5;
    if (strcmp(k->name,"ShiftL")==0) return 0.125;
    if (strcmp(k->name,"ShiftR")==0) return 13.375;

    if (strcmp(k->name,"CtrlL")==0)      return 0.125;
    if (strcmp(k->name,"Fn")==0)         return 1.25;
    if (strcmp(k->name,"Windows")==0)    return 2.25;
    if (strcmp(k->name,"AltL")==0)       return 3.25;
    if (strcmp(k->name,"Space")==0)      return 6.25;
    if (strcmp(k->name,"AltGr")==0)      return 9.5;
    if (strcmp(k->name,"CopilotKey")==0) return 10.75;
    if (strcmp(k->name,"ArrowUp")==0)    return 12.75;
    if (strcmp(k->name,"ArrowLeft")==0)  return 12.25;
    if (strcmp(k->name,"ArrowDown")==0)  return 12.75;
    if (strcmp(k->name,"ArrowRight")==0) return 13.25;

    if (k->row == 2) return (double)k->col + 0.25;
    if (k->row == 3) return (double)k->col + 0.375;
    if (k->row == 4) return (double)k->col + 0.25;
    return (double)k->col;
}

double get_key_y(const KeyEntry *k) {
    // Tall and half-height keys are centred between their logical HID rows.
    if (strcmp(k->name,"Enter")==0 || strcmp(k->name,"NumpadPlus")==0) return 2.5;
    if (strcmp(k->name,"NumpadEnter")==0) return 4.5;
    if (strcmp(k->name,"ArrowUp")==0) return 4.75;
    if (strcmp(k->name,"ArrowLeft")==0 || strcmp(k->name,"ArrowDown")==0 ||
        strcmp(k->name,"ArrowRight")==0) return 5.25;
    return (double)k->row;
}

static double dual_snake_spiral_rank(double nx, double ny, int mainDiagonal,
                                     int insideOut) {
    enum { SPIRAL_SAMPLES = 120 };
    const double outerRadius = 1.4142135623730951;
    const double turns = 1.25;
    const double startAngle = mainDiagonal ? 3.0 * M_PI / 4.0 : M_PI / 4.0;
    const double direction = mainDiagonal ? -1.0 : 1.0;
    double keyYUp = -ny;
    double bestDistance = 1e30;
    double bestTime = 0.0;

    for (int sample = 0; sample < SPIRAL_SAMPLES; sample++) {
        double t = (double)sample / (double)(SPIRAL_SAMPLES - 1);
        double radius = outerRadius * (1.0 - t);
        double angle = startAngle + direction * turns * 2.0 * M_PI * t;
        double snakeX = radius * cos(angle);
        double snakeY = radius * sin(angle);
        double dxA = nx - snakeX;
        double dyA = keyYUp - snakeY;
        double dxB = nx + snakeX;
        double dyB = keyYUp + snakeY;
        double distanceA = dxA * dxA + dyA * dyA;
        double distanceB = dxB * dxB + dyB * dyB;
        double distance = distanceA < distanceB ? distanceA : distanceB;
        if (distance < bestDistance) {
            bestDistance = distance;
            bestTime = t;
        }
    }
    return insideOut ? 1.0 - bestTime : bestTime;
}

static void apply_power_transition_mask(const unsigned char *source,
                                        unsigned char *target,
                                        LONG mode, double progress) {
    double minX = get_key_x(&KEYMAP[0]), maxX = minX;
    double minY = get_key_y(&KEYMAP[0]), maxY = minY;
    double centerX = 0.0, centerY = 0.0;
    int centerKeys = 0;
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;
    memset(target, 0, BUF_SIZE);

    for (size_t i = 1; i < KEYMAP_COUNT; i++) {
        double x = get_key_x(&KEYMAP[i]);
        double y = get_key_y(&KEYMAP[i]);
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
    }
    for (size_t i = 0; i < KEYMAP_COUNT; i++) {
        if (!strcmp(KEYMAP[i].name, "J") || !strcmp(KEYMAP[i].name, "K")) {
            centerX += get_key_x(&KEYMAP[i]);
            centerY += get_key_y(&KEYMAP[i]);
            centerKeys++;
        }
    }
    if (centerKeys > 0) {
        centerX /= (double)centerKeys;
        centerY /= (double)centerKeys;
    } else {
        centerX = (minX + maxX) * 0.5;
        centerY = (minY + maxY) * 0.5;
    }
    double maxCenterDistance = 0.5;
    for (size_t i = 0; i < KEYMAP_COUNT; i++) {
        double dx = get_key_x(&KEYMAP[i]) - centerX;
        double dy = get_key_y(&KEYMAP[i]) - centerY;
        double distance = sqrt(dx * dx + dy * dy);
        if (distance > maxCenterDistance) maxCenterDistance = distance;
    }

    for (size_t i = 0; i < KEYMAP_COUNT; i++) {
        const KeyEntry *key = &KEYMAP[i];
        double keyX = get_key_x(key);
        double keyY = get_key_y(key);
        double x = (keyX - minX) / (maxX - minX);
        double y = (keyY - minY) / (maxY - minY);
        double nx = (keyX - centerX) /
            (keyX < centerX ? centerX - minX : maxX - centerX);
        double ny = (keyY - centerY) /
            (keyY < centerY ? centerY - minY : maxY - centerY);
        double rank = 0.0;
        double feather = 0.18;
        double coverage;

        switch (mode) {
            case TRANSITION_FADE:
                coverage = smooth_unit(progress);
                break;
            case TRANSITION_RIPPLE_CENTER: {
                double dx = keyX - centerX;
                double dy = keyY - centerY;
                double distance = sqrt(dx * dx + dy * dy);
                rank = fmax(0.0, distance - 0.5) / (maxCenterDistance - 0.5);
                goto ranked_transition;
            }
            case TRANSITION_RIPPLE_EDGES:
                rank = 2.0 * fmin(fmin(x, 1.0 - x), fmin(y, 1.0 - y));
                goto ranked_transition;
            case TRANSITION_RIPPLE_TOP:
                rank = y;
                goto ranked_transition;
            case TRANSITION_RIPPLE_BOTTOM:
                rank = 1.0 - y;
                goto ranked_transition;
            case TRANSITION_RIPPLE_LEFT:
                rank = x;
                goto ranked_transition;
            case TRANSITION_RIPPLE_RIGHT:
                rank = 1.0 - x;
                goto ranked_transition;
            case TRANSITION_SNAKE_MAIN_OUT_IN:
            case TRANSITION_SNAKE_MAIN_IN_OUT:
            case TRANSITION_SNAKE_ANTI_OUT_IN:
            case TRANSITION_SNAKE_ANTI_IN_OUT: {
                int mainDiagonal = mode == TRANSITION_SNAKE_MAIN_OUT_IN ||
                                   mode == TRANSITION_SNAKE_MAIN_IN_OUT;
                int insideOut = mode == TRANSITION_SNAKE_MAIN_IN_OUT ||
                                mode == TRANSITION_SNAKE_ANTI_IN_OUT;
                rank = dual_snake_spiral_rank(nx, ny, mainDiagonal, insideOut);
                if (!strcmp(key->name, "J") || !strcmp(key->name, "K"))
                    rank = insideOut ? 0.0 : 1.0;
                feather = 0.09;
                goto ranked_transition;
            }
            case TRANSITION_OFF:
            default:
                coverage = progress >= 1.0 ? 1.0 : 0.0;
                break;
        }
        goto apply_coverage;

ranked_transition:
        if (rank < 0.0) rank = 0.0;
        if (rank > 1.0) rank = 1.0;
        coverage = smooth_unit((progress - rank * (1.0 - feather)) / feather);

apply_coverage:
        set_key(target, key->offset,
            (unsigned char)((double)source[key->offset] * coverage + 0.5),
            (unsigned char)((double)source[key->offset + 1] * coverage + 0.5),
            (unsigned char)((double)source[key->offset + 2] * coverage + 0.5));
    }
}

/*
 * 灯效线程契约：循环必须响应 g_stopFlag；每轮构造完整缓冲并调用 send_frame；
 * 通过统一帧节拍等待，不能忙等。以下各 effect_* 函数都遵守这一约定。
 */
DWORD WINAPI effect_breathing(LPVOID p) {
    (void)p;
    double t = 0.0;
    double previousRaw = 0.5;
    COLORREF cycleColor = g_breathColor;
    if (g_breathRandomMode) {
        unsigned char r, g, b;
        hsv_to_rgb((double)(rand() % 360), &r, &g, &b);
        cycleColor = RGB(r, g, b);
    }
    while (!g_stopFlag) {
        double raw = (sin(t) + 1.0) / 2.0;
        if (g_breathRandomMode) {
            if (raw > previousRaw && previousRaw < 0.03) {
                unsigned char r, g, b;
                hsv_to_rgb((double)(rand() % 360), &r, &g, &b);
                cycleColor = RGB(r, g, b);
            }
        } else {
            cycleColor = g_breathColor;
        }
        previousRaw = raw;
        unsigned char br = GetRValue(cycleColor), bg = GetGValue(cycleColor), bb = GetBValue(cycleColor);
        int levels = (int)g_breathSmooth;
        if (levels < 2) levels = 2;
        double bright = floor(raw * levels) / (double)levels;
        unsigned char r=(unsigned char)(br*bright), g=(unsigned char)(bg*bright), b=(unsigned char)(bb*bright);
        unsigned char buf[BUF_SIZE] = {0};
        for (size_t i=0;i<KEYMAP_COUNT;i++) set_key(buf, KEYMAP[i].offset, r,g,b);
        send_frame(buf);
        t += (double)g_breathSpeed / 100.0;
        pace_frame();
    }
    return 0;
}

DWORD WINAPI effect_rainbow(LPVOID p) {
    (void)p;
    double hue = 0.0;
    ULONGLONG previousTick = GetTickCount64();
    while (!g_stopFlag) {
        ULONGLONG now = GetTickCount64();
        double elapsedSeconds = (double)(now - previousTick) / 1000.0;
        previousTick = now;
        double degreesPerSecond = 4.0 + (double)g_rainbowSpeed * 2.0;
        hue = fmod(hue + elapsedSeconds * degreesPerSecond, 360.0);

        unsigned char r, g, b;
        hsv_to_rgb(hue, &r, &g, &b);
        unsigned char buf[BUF_SIZE] = {0};
        for (size_t i=0; i<KEYMAP_COUNT; i++)
            set_key(buf, KEYMAP[i].offset, r, g, b);
        send_frame(buf);
        pace_frame();
    }
    return 0;
}

// Each LED represents a sizeable key cap rather than an infinitesimal point.
// Sampling four positions and averaging in linear light softens moving borders
// in the same way that multisample anti-aliasing softens polygon edges.
static const double KEY_SAMPLE_X[4] = {-0.28, 0.28, -0.28, 0.28};
static const double KEY_SAMPLE_Y[4] = {-0.22, -0.22, 0.22, 0.22};
static const double CURRENT_SAMPLE_X[8] = {-0.44, -0.31, -0.19, -0.06, 0.06, 0.19, 0.31, 0.44};

static int effect_visual_row(const KeyEntry *key) {
    // Group currents by the legends printed on the physical keyboard.
    if (!strcmp(key->name, "Enter")) return 3;
    if (!strcmp(key->name, "Backslash")) return 2;
    if (!strcmp(key->name, "ArrowLeft") || !strcmp(key->name, "ArrowDown") ||
        !strcmp(key->name, "ArrowRight"))
        return 6;
    return key->row;
}

DWORD WINAPI effect_quicksand(LPVOID p) {
    (void)p;
    ULONGLONG startedAt = GetTickCount64();
    while (!g_stopFlag) {
        double seconds = (double)(GetTickCount64() - startedAt) / 1000.0;
        double speed = 0.22 + (double)g_quicksandSpeed * 0.16;
        double wavelength = 1.15 + (double)g_quicksandScale * 0.42;
        double flow = seconds * speed;
        unsigned char buf[BUF_SIZE] = {0};

        for (size_t i=0; i<KEYMAP_COUNT; i++) {
            double sumR=0.0, sumG=0.0, sumB=0.0;
            for (int sample=0; sample<4; sample++) {
                double x = get_key_x(&KEYMAP[i]) + KEY_SAMPLE_X[sample];
                double y = get_key_y(&KEYMAP[i]) + KEY_SAMPLE_Y[sample];
                double warpedY = y - flow +
                    0.24 * sin(x * 0.68 + flow * 0.55) +
                    0.10 * sin(x * 1.73 - flow * 0.31);
                double band = warpedY / wavelength;
                double phase = band + x * 0.035;
                double curtain = (sin(band * 2.0 * M_PI) + 1.0) * 0.5;
                double intensity = 0.08 + 0.92 * smooth_unit(curtain);
                double grain = 0.90 + 0.10 * sin(x * 2.41 + y * 3.17 + flow * 1.2);
                double r,g,b;
                sample_palette_linear(g_quicksandColors, phase, &r, &g, &b);
                sumR += r * intensity * grain;
                sumG += g * intensity * grain;
                sumB += b * intensity * grain;
            }
            set_key_from_linear(buf, &KEYMAP[i], sumR/4.0, sumG/4.0, sumB/4.0);
        }
        send_frame(buf);
        pace_frame();
    }
    return 0;
}

DWORD WINAPI effect_current(LPVOID p) {
    (void)p;
    ULONGLONG startedAt = GetTickCount64();
    double previousR[KEYMAP_COUNT] = {0};
    double previousG[KEYMAP_COUNT] = {0};
    double previousB[KEYMAP_COUNT] = {0};
    while (!g_stopFlag) {
        double seconds = (double)(GetTickCount64() - startedAt) / 1000.0;
        double travelSpeed = 0.50 + (double)g_currentSpeed * 0.40;
        double width = (double)g_currentWidth;
        double cycle = 22.0 + width;
        unsigned char buf[BUF_SIZE] = {0};

        for (size_t i=0; i<KEYMAP_COUNT; i++) {
            int row = effect_visual_row(&KEYMAP[i]);
            double sumR=0.0, sumG=0.0, sumB=0.0;
            for (int sample=0; sample<8; sample++) {
                double x = get_key_x(&KEYMAP[i]) + CURRENT_SAMPLE_X[sample];
                double travelX = (row & 1) ? 19.0 - x : x;
                double head = fmod(seconds * travelSpeed + row * 3.35, cycle) - width;
                double distanceBehind = head - travelX;
                double intensity = 0.0;
                if (distanceBehind > -0.75 && distanceBehind < width + 0.75) {
                    double headCover = distanceBehind < 0.0
                        ? smooth_unit((distanceBehind + 0.75) / 0.75) : 1.0;
                    double tailCover = distanceBehind > width
                        ? smooth_unit((width + 0.75 - distanceBehind) / 0.75) : 1.0;
                    double decayDistance = distanceBehind > 0.0 ? distanceBehind : 0.0;
                    intensity = exp(-decayDistance / (0.42 * width + 0.45)) * headCover * tailCover;
                }
                double r,g,b;
                double phase = (double)row / 7.0 + travelX * 0.045 + seconds * 0.075;
                sample_palette_linear(g_currentColors, phase, &r, &g, &b);
                sumR += r * intensity;
                sumG += g * intensity;
                sumB += b * intensity;
            }
            double targetR = sumR / 8.0;
            double targetG = sumG / 8.0;
            double targetB = sumB / 8.0;
            double targetLevel = targetR + targetG + targetB;
            double previousLevel = previousR[i] + previousG[i] + previousB[i];
            double smoothing = targetLevel > previousLevel ? 0.30 : 0.14;
            previousR[i] += (targetR - previousR[i]) * smoothing;
            previousG[i] += (targetG - previousG[i]) * smoothing;
            previousB[i] += (targetB - previousB[i]) * smoothing;
            set_key_from_linear(buf, &KEYMAP[i], previousR[i], previousG[i], previousB[i]);
        }
        send_frame(buf);
        pace_frame();
    }
    return 0;
}

DWORD WINAPI effect_wave(LPVOID p) {
    (void)p;
    double t = 0.0;
    while (!g_stopFlag) {
        double angleRad = (double)g_waveAngle * M_PI / 180.0;
        double dirX = cos(angleRad), dirY = sin(angleRad);
        unsigned char buf[BUF_SIZE] = {0};
        for (size_t i=0;i<KEYMAP_COUNT;i++) {
            double x = get_key_x(&KEYMAP[i]);
            double y = get_key_y(&KEYMAP[i]);
            double proj = x*dirX + y*dirY;
            double hue = t + proj * 15.0;
            unsigned char r,g,b;
            hsv_to_rgb(hue,&r,&g,&b);
            set_key(buf, KEYMAP[i].offset, r,g,b);
        }
        send_frame(buf);
        t += (double)g_waveSpeed;
        pace_frame();
    }
    return 0;
}

DWORD WINAPI effect_wheel(LPVOID p) {
    (void)p;
    double cx = 0, cy = 0;
    for (size_t i=0;i<KEYMAP_COUNT;i++) { cx += get_key_x(&KEYMAP[i]); cy += get_key_y(&KEYMAP[i]); }
    cx /= KEYMAP_COUNT; cy /= KEYMAP_COUNT;

    double t = 0.0;
    while (!g_stopFlag) {
        double dirMul = g_wheelReverse ? -1.0 : 1.0;
        unsigned char buf[BUF_SIZE] = {0};
        for (size_t i=0;i<KEYMAP_COUNT;i++) {
            double x = get_key_x(&KEYMAP[i]) - cx;
            double y = get_key_y(&KEYMAP[i]) - cy;
            double angleDeg = atan2(y, x) * 180.0 / M_PI;
            double hue = t*dirMul + angleDeg;
            unsigned char r,g,b;
            hsv_to_rgb(hue,&r,&g,&b);
            set_key(buf, KEYMAP[i].offset, r,g,b);
        }
        send_frame(buf);
        t += (double)g_wheelSpeed;
        pace_frame();
    }
    return 0;
}

#define MAX_STRIKES 4
DWORD WINAPI effect_lightning(LPVOID p) {
    (void)p;
    double headPos[MAX_STRIKES], boltX[MAX_STRIKES], bendSeed[MAX_STRIKES];
    double previous[KEYMAP_COUNT] = {0};
    int countdown[MAX_STRIKES];
    for (int i=0;i<MAX_STRIKES;i++) {
        headPos[i] = 100.0;
        boltX[i] = 10.0;
        bendSeed[i] = (double)(rand() % 628) / 100.0;
        countdown[i] = 10 + rand()%30;
    }

    while (!g_stopFlag) {
        double speed = 0.05 + (double)g_lightningSpeed * 0.08;
        double bandWidth = 0.4 + (double)g_lightningSmooth * 0.15;
        double tailLen = bandWidth * 2.5;
        double finishPos = 6.0 + tailLen + 0.5;
        double xSigma = 0.18 + (double)g_lightningWidth * 0.10;
        int concurrent = (int)g_lightningConcurrent;
        if (concurrent < 1) concurrent = 1;
        if (concurrent > MAX_STRIKES) concurrent = MAX_STRIKES;

        double bright[KEYMAP_COUNT];
        for (size_t i=0;i<KEYMAP_COUNT;i++) bright[i] = 0;

        for (int s=0; s<concurrent; s++) {
            if (headPos[s] > finishPos) {
                countdown[s]--;
                if (countdown[s] <= 0) {
                    headPos[s] = -2.0;
                    boltX[s] = (double)(rand() % 1900) / 100.0;
                    bendSeed[s] = (double)(rand() % 628) / 100.0;
                    countdown[s] = 20 + rand()%40;
                }
            }
            if (headPos[s] <= finishPos) {
                for (size_t i=0;i<KEYMAP_COUNT;i++) {
                    double y = get_key_y(&KEYMAP[i]);
                    double dv = headPos[s] - y;
                    double centerX = boltX[s] + sin(y * 2.35 + bendSeed[s]) * 0.32;
                    double dh = get_key_x(&KEYMAP[i]) - centerX;
                    double front = dv < 0.0 ? smooth_unit((dv + 0.85) / 0.85) : 1.0;
                    double tail = dv > 0.0 ? exp(-dv / (tailLen * 0.52)) : 1.0;
                    double horizontal = exp(-0.5 * (dh / xSigma) * (dh / xSigma));
                    if (horizontal < 0.02) continue;
                    double b = front * tail * horizontal;
                    if (b > bright[i]) bright[i] = b;
                }
                headPos[s] += speed;
            }
        }

        unsigned char buf[BUF_SIZE] = {0};
        double smoothValue = (double)g_lightningSmooth;
        double attack = 0.62 / (1.0 + smoothValue * 0.065);
        double release = 0.24 / (1.0 + smoothValue * 0.055);
        for (size_t i=0;i<KEYMAP_COUNT;i++) {
            double target = bright[i] < 0.002 ? 0.0 : bright[i];
            double coefficient = target > previous[i] ? attack : release;
            previous[i] += (target - previous[i]) * coefficient;
            double glow = previous[i];
            set_key_from_linear(buf, &KEYMAP[i],
                0.0012 + glow * 0.90,
                0.0012 + glow * 0.90,
                0.0044 + glow);
        }
        send_frame(buf);
        pace_frame();
    }
    return 0;
}

DWORD WINAPI effect_flame(LPVOID p) {
    (void)p;
    double heat[KEYMAP_COUNT];
    for (size_t i=0;i<KEYMAP_COUNT;i++) heat[i] = 100.0 + rand()%60;
    int frameCounter = 0;

    while (!g_stopFlag) {
        double speed = (double)g_flameSpeed;
        double smooth = (double)g_flameSmooth;
        double alpha = 1.0 / (1.0 + smooth*0.6);
        double volatility = 40.0 + speed*18.0;
        int updateInterval = 11 - (int)speed;
        if (updateInterval < 1) updateInterval = 1;

        frameCounter++;
        if (frameCounter % updateInterval == 0) {
            for (size_t i=0;i<KEYMAP_COUNT;i++) {
                double rowFrac = KEYMAP[i].row / 6.0;
                double bias = 40.0 * rowFrac;
                double target = 90.0 + bias + (rand()%(int)volatility - volatility/2.0);
                heat[i] = heat[i]*(1.0-alpha) + target*alpha;
                if (heat[i] < 0) heat[i] = 0;
                if (heat[i] > 255) heat[i] = 255;
            }
        }

        unsigned char buf[BUF_SIZE] = {0};
        for (size_t i=0;i<KEYMAP_COUNT;i++) {
            int h = (int)heat[i];
            unsigned char r,g,b;
            if (h < 85) { r=(unsigned char)(h*3); g=0; b=0; }
            else if (h < 170) { r=255; g=(unsigned char)((h-85)*3); b=0; }
            else { r=255; g=255; b=(unsigned char)((h-170)*3); }
            set_key(buf, KEYMAP[i].offset, r,g,b);
        }
        send_frame(buf);
        pace_frame();
    }
    return 0;
}

#define RAIN_DROPS MAX_RAIN_DROPS
DWORD WINAPI effect_rain(LPVOID p) {
    (void)p;
    double dropX[RAIN_DROPS], dropRow[RAIN_DROPS];
    int dropColorIdx[RAIN_DROPS];
    for (int i=0;i<RAIN_DROPS;i++) {
        dropX[i] = rand()%20;
        dropRow[i] = -(double)(rand()%8);
        dropColorIdx[i] = rand() % (int)g_rainColorCount;
    }
    int activeDrops = (int)g_rainDensityTarget * 3;
    double previousR[KEYMAP_COUNT] = {0};
    double previousG[KEYMAP_COUNT] = {0};
    double previousB[KEYMAP_COUNT] = {0};

    while (!g_stopFlag) {
        double speed = 0.05 + (double)g_rainSpeed*0.03;
        int colorCount = (int)g_rainColorCount;
        if (colorCount < 1) colorCount = 1;
        if (colorCount > 5) colorCount = 5;

        int target = (int)g_rainDensityTarget;
        if (target < 1) target = 1;
        if (target > 5) target = 5;
        activeDrops = target * 3;
        if (activeDrops > MAX_RAIN_DROPS) activeDrops = MAX_RAIN_DROPS;

        double targetR[KEYMAP_COUNT] = {0};
        double targetG[KEYMAP_COUNT] = {0};
        double targetB[KEYMAP_COUNT] = {0};
        for (int d=0; d<activeDrops; d++) {
            COLORREF col = g_rainColors[dropColorIdx[d] % colorCount];
            double cr = srgb_channel_to_linear(GetRValue(col));
            double cg = srgb_channel_to_linear(GetGValue(col));
            double cb = srgb_channel_to_linear(GetBValue(col));
            for (size_t i=0;i<KEYMAP_COUNT;i++) {
                double dx = (get_key_x(&KEYMAP[i]) - dropX[d]) / 0.46;
                double dy = dropRow[d] - get_key_y(&KEYMAP[i]);
                double horizontal = exp(-0.5 * dx * dx);
                double head = exp(-0.5 * (dy / 0.34) * (dy / 0.34));
                double tail = dy > 0.0 ? 0.52 * exp(-dy / 1.15) : 0.0;
                double intensity = horizontal * (head + tail);
                targetR[i] += cr * intensity;
                targetG[i] += cg * intensity;
                targetB[i] += cb * intensity;
            }
            dropRow[d] += speed;
            if (dropRow[d] > 8) {
                dropRow[d] = -(double)(rand()%5);
                dropX[d] = rand()%20;
                dropColorIdx[d] = rand() % colorCount;
            }
        }
        unsigned char buf[BUF_SIZE] = {0};
        for (size_t i=0;i<KEYMAP_COUNT;i++) {
            double targetLevel = targetR[i] + targetG[i] + targetB[i];
            double previousLevel = previousR[i] + previousG[i] + previousB[i];
            double coefficient = targetLevel > previousLevel ? 0.42 : 0.19;
            previousR[i] += (targetR[i] - previousR[i]) * coefficient;
            previousG[i] += (targetG[i] - previousG[i]) * coefficient;
            previousB[i] += (targetB[i] - previousB[i]) * coefficient;
            set_key_from_linear(buf, &KEYMAP[i], previousR[i], previousG[i], previousB[i]);
        }
        send_frame(buf);
        pace_frame();
    }
    return 0;
}

#define MATRIX_STREAKS 5
#define MATRIX_COLUMNS 24
DWORD WINAPI effect_matrix(LPVOID p) {
    (void)p;

    double angle[MATRIX_STREAKS], pos[MATRIX_STREAKS], spd[MATRIX_STREAKS];
    int streakColorIdx[MATRIX_STREAKS];
    for (int i=0;i<MATRIX_STREAKS;i++) {
        angle[i] = (rand()%360) * M_PI / 180.0;
        pos[i] = -30.0;
        spd[i] = 0.2 + (rand()%100)/100.0;
        streakColorIdx[i] = rand() % (int)g_matrixColorCount;
    }
    int activeStreaks = (int)g_matrixDensityTarget;
    int rerollCounter = 0;

    double colPos[MATRIX_COLUMNS]; double colSpeed[MATRIX_COLUMNS]; int colColorIdx[MATRIX_COLUMNS];
    for (int i=0;i<MATRIX_COLUMNS;i++) {
        colPos[i] = -(double)(rand()%8);
        colSpeed[i] = 0.05 + (rand()%100)/300.0;
        colColorIdx[i] = rand() % (int)g_matrixColorCount;
    }

    #define MAX_LASERS 3
    double laserAngle[MAX_LASERS], laserPos[MAX_LASERS], laserSpd[MAX_LASERS];
    int laserColorIdx[MAX_LASERS];
    for (int i=0;i<MAX_LASERS;i++) {
        laserAngle[i] = (rand()%4) * (M_PI/2.0);
        laserPos[i] = -30.0 - i*8.0;
        laserSpd[i] = 0.6 + (rand()%100)/100.0;
        laserColorIdx[i] = rand() % (int)g_matrixColorCount;
    }

    #define MAX_RIPPLES 3
    double rippleX[MAX_RIPPLES], rippleY[MAX_RIPPLES], rippleR[MAX_RIPPLES];
    int rippleColorIdx[MAX_RIPPLES];
    for (int i=0;i<MAX_RIPPLES;i++) {
        rippleX[i] = rand()%20; rippleY[i] = rand()%7; rippleR[i] = i*8.0;
        rippleColorIdx[i] = rand() % (int)g_matrixColorCount;
    }

    while (!g_stopFlag) {
        int colorCount = (int)g_matrixColorCount; if (colorCount<1) colorCount=1; if (colorCount>3) colorCount=3;
        int style = (int)g_matrixStyle;
        double speedMul = 0.3 + (double)g_matrixSpeed*0.15;
        unsigned char buf[BUF_SIZE] = {0};

        if (style == 0) {
            rerollCounter++;
            if (rerollCounter >= 60) {
                activeStreaks = compute_prob_count((int)g_matrixDensityTarget, 1, 5);
                rerollCounter = 0;
            }
            for (size_t i=0;i<KEYMAP_COUNT;i++) {
                double x = get_key_x(&KEYMAP[i]), y = get_key_y(&KEYMAP[i]);
                double bestBright = 0; int bestColor = 0;
                for (int s=0;s<activeStreaks;s++) {
                    double dirX = cos(angle[s]), dirY = sin(angle[s]);
                    double proj = x*dirX + y*dirY;
                    double dist = fabs(proj - pos[s]);
                    if (dist < 1.5) {
                        double b = 1.0 - dist/1.5;
                        if (b > bestBright) { bestBright = b; bestColor = streakColorIdx[s] % colorCount; }
                    }
                }
                if (bestBright > 1) bestBright = 1;
                COLORREF col = g_matrixColors[bestColor];
                unsigned char cr=(unsigned char)(GetRValue(col)*bestBright), cg=(unsigned char)(GetGValue(col)*bestBright), cb=(unsigned char)(GetBValue(col)*bestBright);
                set_key(buf, KEYMAP[i].offset, cr, cg, cb);
            }
            for (int s=0;s<activeStreaks;s++) {
                pos[s] += spd[s]*speedMul;
                if (pos[s] > 30) { pos[s]=-30; angle[s]=(rand()%360)*M_PI/180.0; spd[s]=0.2+(rand()%100)/100.0; streakColorIdx[s]=rand()%colorCount; }
            }
        } else if (style == 1) {

            for (size_t i=0;i<KEYMAP_COUNT;i++) {
                double x = get_key_x(&KEYMAP[i]);
                int col = (int)(x + 0.5);
                if (col < 0) col = 0; if (col >= MATRIX_COLUMNS) col = MATRIX_COLUMNS-1;
                double dv = colPos[col] - KEYMAP[i].row;
                double bright = 0;
                if (dv >= -0.3 && dv < 4.0) bright = (dv < 0) ? 1.0 : (1.0 - dv/4.0);
                if (bright < 0) bright = 0;
                COLORREF cc = g_matrixColors[colColorIdx[col] % colorCount];
                set_key(buf, KEYMAP[i].offset, (unsigned char)(GetRValue(cc)*bright), (unsigned char)(GetGValue(cc)*bright), (unsigned char)(GetBValue(cc)*bright));
            }
            for (int i=0;i<MATRIX_COLUMNS;i++) {
                colPos[i] += colSpeed[i]*speedMul;
                if (colPos[i] > 10) { colPos[i] = -(double)(rand()%6); colSpeed[i]=0.05+(rand()%100)/300.0; colColorIdx[i]=rand()%colorCount; }
            }
        } else if (style == 2) {

            int target = (int)g_matrixDensityTarget;
            int laserCount = (target >= 4) ? 3 : 2;
            for (size_t i=0;i<KEYMAP_COUNT;i++) {
                double x = get_key_x(&KEYMAP[i]), y = get_key_y(&KEYMAP[i]);
                double bestBright = 0; int bestColor = 0;
                for (int s=0;s<laserCount;s++) {
                    double dirX = cos(laserAngle[s]), dirY = sin(laserAngle[s]);
                    double proj = x*dirX + y*dirY;
                    double dist = fabs(proj - laserPos[s]);
                    if (dist < 0.6) {
                        double b = 1.0 - dist/0.6;
                        if (b > bestBright) { bestBright = b; bestColor = laserColorIdx[s] % colorCount; }
                    }
                }
                if (bestBright > 1) bestBright = 1;
                COLORREF col = g_matrixColors[bestColor];
                unsigned char cr=(unsigned char)(GetRValue(col)*bestBright), cg=(unsigned char)(GetGValue(col)*bestBright), cb=(unsigned char)(GetBValue(col)*bestBright);
                set_key(buf, KEYMAP[i].offset, cr, cg, cb);
            }
            for (int s=0;s<laserCount;s++) {
                laserPos[s] += laserSpd[s]*speedMul*1.5;
                if (laserPos[s] > 30) {
                    laserPos[s] = -30.0 - rand()%20;
                    laserAngle[s] = (rand()%4) * (M_PI/2.0);
                    laserSpd[s] = 0.6 + (rand()%100)/100.0;
                    laserColorIdx[s] = rand()%colorCount;
                }
            }
        } else {

            for (size_t i=0;i<KEYMAP_COUNT;i++) {
                double x = get_key_x(&KEYMAP[i]), y = get_key_y(&KEYMAP[i]);
                double bestBright = 0; int bestColor = 0;
                for (int s=0;s<MAX_RIPPLES;s++) {
                    double dx = x - rippleX[s], dy = y - rippleY[s];
                    double dist = sqrt(dx*dx + dy*dy);
                    double ringDist = fabs(dist - rippleR[s]);
                    if (ringDist < 1.2) {
                        double b = 1.0 - ringDist/1.2;
                        if (b > bestBright) { bestBright = b; bestColor = rippleColorIdx[s] % colorCount; }
                    }
                }
                if (bestBright > 1) bestBright = 1;
                COLORREF col = g_matrixColors[bestColor];
                unsigned char cr=(unsigned char)(GetRValue(col)*bestBright), cg=(unsigned char)(GetGValue(col)*bestBright), cb=(unsigned char)(GetBValue(col)*bestBright);
                set_key(buf, KEYMAP[i].offset, cr, cg, cb);
            }
            for (int s=0;s<MAX_RIPPLES;s++) {
                rippleR[s] += speedMul * 0.25;
                if (rippleR[s] > 24) {
                    rippleX[s] = rand()%20; rippleY[s] = rand()%7; rippleR[s] = 0;
                    rippleColorIdx[s] = rand()%colorCount;
                }
            }
        }
        send_frame(buf);
        pace_frame();
    }
    return 0;
}

int horizontal_zone_for_row(int row, int zoneCount) {
    switch (zoneCount) {
        case 1: return 0;
        case 2: return (row <= 3) ? 0 : 1;
        case 3: if (row <= 1) return 0; if (row <= 4) return 1; return 2;
        case 4: if (row == 0) return 0; if (row <= 2) return 1; if (row <= 4) return 2; return 3;
        case 5: if (row == 0) return 0; if (row == 1) return 1; if (row == 2) return 2; if (row <= 4) return 3; return 4;
        case 6: if (row == 0) return 0; if (row == 1) return 1; if (row == 2) return 2; if (row == 3) return 3; if (row == 4) return 4; return 5;
    }
    return 0;
}

DWORD WINAPI effect_static(LPVOID p) {
    (void)p;
    static int xRank[KEYMAP_COUNT];
    static int xRankReady = 0;
    if (!xRankReady) {
        int idx[KEYMAP_COUNT];
        for (int i=0;i<(int)KEYMAP_COUNT;i++) idx[i] = i;
        for (int i=1;i<(int)KEYMAP_COUNT;i++) {
            int cur = idx[i];
            double curX = get_key_x(&KEYMAP[cur]);
            int j = i;
            while (j > 0 && get_key_x(&KEYMAP[idx[j-1]]) > curX) { idx[j] = idx[j-1]; j--; }
            idx[j] = cur;
        }
        for (int i=0;i<(int)KEYMAP_COUNT;i++) xRank[idx[i]] = i;
        xRankReady = 1;
    }

    while (!g_stopFlag) {
        int layout = (int)g_staticLayout;
        int maxZones = (layout == 0) ? 6 : 10;
        int zoneCount = (int)g_staticZoneCount;
        if (zoneCount < 1) zoneCount = 1;
        if (zoneCount > maxZones) zoneCount = maxZones;

        unsigned char buf[BUF_SIZE] = {0};
        for (size_t i=0;i<KEYMAP_COUNT;i++) {
            int zone = 0;
            if (zoneCount > 1) {
                if (layout == 0) {
                    zone = horizontal_zone_for_row(KEYMAP[i].row, zoneCount);
                } else {
                    zone = (xRank[i] * zoneCount) / (int)KEYMAP_COUNT;
                    if (zone >= zoneCount) zone = zoneCount - 1;
                }
            }
            COLORREF c = g_staticColors[zone];
            set_key(buf, KEYMAP[i].offset, GetRValue(c), GetGValue(c), GetBValue(c));
        }
        send_frame(buf);
        pace_frame();
    }
    return 0;
}

DWORD WINAPI effect_sparkle(LPVOID p) {
    (void)p;
    int *age = (int*)calloc(KEYMAP_COUNT, sizeof(int));
    int *duration = (int*)calloc(KEYMAP_COUNT, sizeof(int));
    unsigned char *rr=(unsigned char*)calloc(KEYMAP_COUNT,1), *gg=(unsigned char*)calloc(KEYMAP_COUNT,1), *bb=(unsigned char*)calloc(KEYMAP_COUNT,1);
    if (!age || !duration || !rr || !gg || !bb) {
        free(age); free(duration); free(rr); free(gg); free(bb);
        return 1;
    }
    while (!g_stopFlag) {
        int target = (int)g_sparkleDensity;
        int speed = (int)g_sparkleSpeed;
        int lifeDuration = 70 - speed*5;
        if (lifeDuration < 18) lifeDuration = 18;

        int aliveCount = 0;
        for (size_t i=0;i<KEYMAP_COUNT;i++) if (age[i] > 0) aliveCount++;
        int deficit = target - aliveCount;

        for (int n = 0; n < deficit; n++) {
            int idx = -1;
            for (int attempt = 0; attempt < 20; attempt++) {
                int c = rand() % KEYMAP_COUNT;
                if (age[c] == 0) { idx = c; break; }
            }
            if (idx < 0) break;
            age[idx] = 1;
            duration[idx] = lifeDuration + rand() % 12;
            hsv_to_rgb((double)(rand() % 360), &rr[idx], &gg[idx], &bb[idx]);
        }

        unsigned char buf[BUF_SIZE] = {0};
        for (size_t i=0;i<KEYMAP_COUNT;i++) {
            if (age[i] > 0) {
                double progress = (double)age[i] / (double)duration[i];
                double envelope = sin(M_PI * progress);
                envelope *= envelope;
                set_key_from_linear(buf, &KEYMAP[i],
                    srgb_channel_to_linear(rr[i]) * envelope,
                    srgb_channel_to_linear(gg[i]) * envelope,
                    srgb_channel_to_linear(bb[i]) * envelope);
                age[i]++;
                if (age[i] >= duration[i]) age[i] = 0;
            }
        }
        send_frame(buf);
        pace_frame();
    }
    free(age); free(duration); free(rr); free(gg); free(bb);
    return 0;
}

int vk_to_keymap_index(int vk) {
    for (size_t i=0;i<KEYMAP_COUNT;i++) {
        const char *n = KEYMAP[i].name;
        if (((vk>='A'&&vk<='Z')||(vk>='0'&&vk<='9')) && n[0]==(char)vk && n[1]=='\0') return (int)i;
    }
    struct { int vk; const char* name; } table[] = {
        {VK_SPACE,"Space"},{VK_RETURN,"Enter"},{VK_BACK,"Backspace"},{VK_TAB,"Tab"},
        {VK_LSHIFT,"ShiftL"},{VK_RSHIFT,"ShiftR"},{VK_CAPITAL,"CapsLock"},{VK_LCONTROL,"CtrlL"},
        {VK_LMENU,"AltL"},{VK_RMENU,"AltGr"},{VK_LEFT,"ArrowLeft"},{VK_RIGHT,"ArrowRight"},
        {VK_UP,"ArrowUp"},{VK_DOWN,"ArrowDown"},{VK_OEM_102,"ISO_Backslash"},{VK_LWIN,"Windows"},
        {VK_OEM_4,"BracketL"},{VK_OEM_6,"BracketR"},{VK_OEM_1,"Semicolon"},{VK_OEM_7,"Quote"},
        {VK_OEM_5,"Backslash"},{VK_OEM_COMMA,"Comma"},{VK_OEM_PERIOD,"Period"},{VK_OEM_2,"Slash"},
        {VK_OEM_MINUS,"Minus"},{VK_OEM_PLUS,"Equals"},{VK_OEM_3,"Grave"},
        {VK_ESCAPE,"Esc"},{VK_F1,"F1"},{VK_F2,"F2"},{VK_F3,"F3"},{VK_F4,"F4"},{VK_F5,"F5"},
        {VK_F6,"F6"},{VK_F7,"F7"},{VK_F8,"F8"},{VK_F9,"F9"},{VK_F10,"F10"},{VK_F11,"F11"},{VK_F12,"F12"},
        {VK_SNAPSHOT,"PrtSc"},{VK_DELETE,"Del"},{VK_HOME,"Home"},{VK_PRIOR,"PgUp"},{VK_NEXT,"PgDn"},{VK_END,"End"},
        {VK_NUMPAD0,"Numpad0"},{VK_NUMPAD1,"Numpad1"},{VK_NUMPAD2,"Numpad2"},{VK_NUMPAD3,"Numpad3"},
        {VK_NUMPAD4,"Numpad4"},{VK_NUMPAD5,"Numpad5"},{VK_NUMPAD6,"Numpad6"},{VK_NUMPAD7,"Numpad7"},
        {VK_NUMPAD8,"Numpad8"},{VK_NUMPAD9,"Numpad9"},{VK_DECIMAL,"NumpadDecimal"},{VK_ADD,"NumpadPlus"},
        {VK_SUBTRACT,"NumpadMinus"},{VK_MULTIPLY,"NumpadMul"},{VK_DIVIDE,"NumpadDiv"}
    };
    for (size_t k=0;k<sizeof(table)/sizeof(table[0]);k++) {
        if (vk == table[k].vk) {
            for (size_t i=0;i<KEYMAP_COUNT;i++) if (strcmp(KEYMAP[i].name, table[k].name)==0) return (int)i;
        }
    }
    return -1;
}

static int fill_tracked_vk_list(int *vkList, int capacity) {
    int count = 0;
#define ADD_TRACKED_VK(vk) do { if (count < capacity) vkList[count++] = (vk); } while (0)
    for (int vk='A'; vk<='Z'; vk++) ADD_TRACKED_VK(vk);
    for (int vk='0'; vk<='9'; vk++) ADD_TRACKED_VK(vk);
    int specials[] = {VK_SPACE,VK_RETURN,VK_BACK,VK_TAB,VK_LSHIFT,VK_RSHIFT,VK_CAPITAL,VK_LCONTROL,VK_LMENU,VK_RMENU,VK_LEFT,VK_RIGHT,VK_UP,VK_DOWN,VK_OEM_102,VK_LWIN,
                       VK_OEM_4,VK_OEM_6,VK_OEM_1,VK_OEM_7,VK_OEM_5,VK_OEM_COMMA,VK_OEM_PERIOD,VK_OEM_2,VK_OEM_MINUS,VK_OEM_PLUS,VK_OEM_3,
                       VK_ESCAPE,VK_F1,VK_F2,VK_F3,VK_F4,VK_F5,VK_F6,VK_F7,VK_F8,VK_F9,VK_F10,VK_F11,VK_F12,
                       VK_SNAPSHOT,VK_DELETE,VK_HOME,VK_PRIOR,VK_NEXT,VK_END,
                       VK_NUMPAD0,VK_NUMPAD1,VK_NUMPAD2,VK_NUMPAD3,VK_NUMPAD4,VK_NUMPAD5,VK_NUMPAD6,VK_NUMPAD7,VK_NUMPAD8,VK_NUMPAD9,
                       VK_DECIMAL,VK_ADD,VK_SUBTRACT,VK_MULTIPLY,VK_DIVIDE};
    for (size_t i=0; i<sizeof(specials)/sizeof(specials[0]); i++) ADD_TRACKED_VK(specials[i]);
#undef ADD_TRACKED_VK
    return count;
}

DWORD WINAPI effect_reactive(LPVOID p) {
    (void)p;
    int *wasDown = (int*)calloc(KEYMAP_COUNT, sizeof(int));
    int *fadeLife = (int*)calloc(KEYMAP_COUNT, sizeof(int));
    unsigned char *rr=(unsigned char*)calloc(KEYMAP_COUNT,1), *gg=(unsigned char*)calloc(KEYMAP_COUNT,1), *bb=(unsigned char*)calloc(KEYMAP_COUNT,1);

    int vkList[128];
    int vkCount = fill_tracked_vk_list(vkList, (int)(sizeof(vkList)/sizeof(vkList[0])));

    while (!g_stopFlag) {
        unsigned char fr=GetRValue(g_reactiveColor), fg=GetGValue(g_reactiveColor), fb=GetBValue(g_reactiveColor);
        for (int v = 0; v < vkCount; v++) {
            int idx = vk_to_keymap_index(vkList[v]);
            if (idx < 0) continue;
            int down = (GetAsyncKeyState(vkList[v]) & 0x8000) != 0;
            if (down) {
                if (!wasDown[idx]) {
                    if (g_reactiveRandomMode) { rr[idx]=rand()%256; gg[idx]=rand()%256; bb[idx]=rand()%256; }
                    else { rr[idx]=fr; gg[idx]=fg; bb[idx]=fb; }
                }
                wasDown[idx] = 1;
                fadeLife[idx] = (int)g_reactiveDuration;
            } else {
                wasDown[idx] = 0;
            }
        }

        unsigned char buf[BUF_SIZE] = {0};
        for (size_t i=0;i<KEYMAP_COUNT;i++) {
            if (wasDown[i]) {
                set_key(buf, KEYMAP[i].offset, rr[i], gg[i], bb[i]);
            } else if (fadeLife[i] > 0) {
                double fade = fadeLife[i] / (double)g_reactiveDuration;
                set_key(buf, KEYMAP[i].offset, (unsigned char)(rr[i]*fade),(unsigned char)(gg[i]*fade),(unsigned char)(bb[i]*fade));
                fadeLife[i]--;
            }
        }
        send_frame(buf);
        pace_frame();
    }
    free(wasDown); free(fadeLife); free(rr); free(gg); free(bb);
    return 0;
}

#define MAX_TOUCH_RIPPLES 20
typedef struct {
    int active;
    double x;
    double y;
    ULONGLONG startedAt;
    unsigned char r;
    unsigned char g;
    unsigned char b;
} RippleEvent;

DWORD WINAPI effect_ripple(LPVOID p) {
    (void)p;
    int *wasDown = (int*)calloc(KEYMAP_COUNT, sizeof(int));
    RippleEvent ripples[MAX_TOUCH_RIPPLES] = {0};
    int nextSlot = 0;
    int vkList[128];
    int vkCount = fill_tracked_vk_list(vkList, (int)(sizeof(vkList)/sizeof(vkList[0])));

    while (!g_stopFlag) {
        ULONGLONG now = GetTickCount64();
        for (int v=0; v<vkCount; v++) {
            int idx = vk_to_keymap_index(vkList[v]);
            if (idx < 0) continue;
            int down = (GetAsyncKeyState(vkList[v]) & 0x8000) != 0;
            if (down && !wasDown[idx]) {
                RippleEvent *event = &ripples[nextSlot];
                event->active = 1;
                event->x = get_key_x(&KEYMAP[idx]);
                event->y = get_key_y(&KEYMAP[idx]);
                event->startedAt = now;
                if (g_rippleRandomMode) {
                    hsv_to_rgb((double)(rand() % 360), &event->r, &event->g, &event->b);
                } else {
                    event->r = GetRValue(g_rippleColor);
                    event->g = GetGValue(g_rippleColor);
                    event->b = GetBValue(g_rippleColor);
                }
                nextSlot = (nextSlot + 1) % MAX_TOUCH_RIPPLES;
            }
            wasDown[idx] = down;
        }

        unsigned char buf[BUF_SIZE] = {0};
        double speed = 0.006 + (double)g_rippleSpeed * 0.0015;
        double width = 0.35 + (double)g_rippleWidth * 0.25;
        ULONGLONG lifetime = 2300ULL - (ULONGLONG)g_rippleSpeed * 120ULL;
        for (size_t i=0; i<KEYMAP_COUNT; i++) {
            double outR=0.0, outG=0.0, outB=0.0;
            double keyX = get_key_x(&KEYMAP[i]);
            double keyY = get_key_y(&KEYMAP[i]);
            for (int r=0; r<MAX_TOUCH_RIPPLES; r++) {
                if (!ripples[r].active) continue;
                ULONGLONG elapsed = now - ripples[r].startedAt;
                if (elapsed >= lifetime) {
                    ripples[r].active = 0;
                    continue;
                }
                double dx = keyX - ripples[r].x;
                double dy = keyY - ripples[r].y;
                double distance = sqrt(dx*dx + dy*dy);
                double radius = (double)elapsed * speed;
                double band = 1.0 - fabs(distance - radius) / width;
                if (band <= 0.0) continue;
                double fade = 1.0 - (double)elapsed / (double)lifetime;
                double intensity = band * fade;
                outR += ripples[r].r * intensity;
                outG += ripples[r].g * intensity;
                outB += ripples[r].b * intensity;
            }
            if (outR > 255.0) outR = 255.0;
            if (outG > 255.0) outG = 255.0;
            if (outB > 255.0) outB = 255.0;
            set_key(buf, KEYMAP[i].offset, (unsigned char)outR, (unsigned char)outG, (unsigned char)outB);
        }
        send_frame(buf);
        pace_frame();
    }
    free(wasDown);
    return 0;
}

#define MAX_TOUCH_CURRENTS 20
typedef struct {
    int active;
    int row;
    double originX;
    double palettePhase;
    ULONGLONG startedAt;
} TouchCurrentEvent;

static int touch_current_row(const KeyEntry *key) {
    return effect_visual_row(key);
}

DWORD WINAPI effect_touch_current(LPVOID p) {
    (void)p;
    int *wasDown = (int*)calloc(KEYMAP_COUNT, sizeof(int));
    TouchCurrentEvent events[MAX_TOUCH_CURRENTS] = {0};
    int nextSlot = 0;
    int vkList[128];
    int vkCount = fill_tracked_vk_list(vkList, (int)(sizeof(vkList)/sizeof(vkList[0])));

    while (!g_stopFlag) {
        ULONGLONG now = GetTickCount64();
        for (int v=0; v<vkCount; v++) {
            int idx = vk_to_keymap_index(vkList[v]);
            if (idx < 0) continue;
            int down = (GetAsyncKeyState(vkList[v]) & 0x8000) != 0;
            if (down && !wasDown[idx]) {
                TouchCurrentEvent *event = &events[nextSlot];
                event->active = 1;
                event->row = touch_current_row(&KEYMAP[idx]);
                event->originX = get_key_x(&KEYMAP[idx]);
                event->palettePhase = (double)(rand() % 10000) / 10000.0;
                event->startedAt = now;
                nextSlot = (nextSlot + 1) % MAX_TOUCH_CURRENTS;
            }
            wasDown[idx] = down;
        }

        double travelSpeed = 0.75 + (double)g_touchCurrentSpeed * 0.58;
        double sigma = 0.20 + (double)g_touchCurrentWidth * 0.20;
        double lifetimeSeconds = (21.0 + sigma * 3.0) / travelSpeed;
        unsigned char buf[BUF_SIZE] = {0};

        for (size_t i=0; i<KEYMAP_COUNT; i++) {
            double sumR=0.0, sumG=0.0, sumB=0.0;
            int row = touch_current_row(&KEYMAP[i]);
            for (int sample=0; sample<4; sample++) {
                double sampleR=0.0, sampleG=0.0, sampleB=0.0;
                double x = get_key_x(&KEYMAP[i]) + KEY_SAMPLE_X[sample];
                for (int e=0; e<MAX_TOUCH_CURRENTS; e++) {
                    if (!events[e].active || events[e].row != row) continue;
                    double elapsed = (double)(now - events[e].startedAt) / 1000.0;
                    if (elapsed >= lifetimeSeconds) {
                        events[e].active = 0;
                        continue;
                    }
                    double radius = elapsed * travelSpeed;
                    double delta = fabs(fabs(x - events[e].originX) - radius);
                    double intensity = exp(-(delta * delta) / (2.0 * sigma * sigma));
                    double edgeFade = 1.0 - smooth_unit(elapsed / lifetimeSeconds);
                    double r,g,b;
                    double phase = events[e].palettePhase +
                        (x - events[e].originX) * 0.045 + elapsed * 0.12;
                    sample_palette_linear(g_touchCurrentColors, phase, &r, &g, &b);
                    sampleR += r * intensity * edgeFade;
                    sampleG += g * intensity * edgeFade;
                    sampleB += b * intensity * edgeFade;
                }
                if (sampleR > 1.0) sampleR = 1.0;
                if (sampleG > 1.0) sampleG = 1.0;
                if (sampleB > 1.0) sampleB = 1.0;
                sumR += sampleR;
                sumG += sampleG;
                sumB += sampleB;
            }
            set_key_from_linear(buf, &KEYMAP[i], sumR/4.0, sumG/4.0, sumB/4.0);
        }
        send_frame(buf);
        pace_frame();
    }
    free(wasDown);
    return 0;
}

DWORD WINAPI effect_screen_ambient(LPVOID p) {
    (void)p;
    enum { CAPTURE_WIDTH = 96, CAPTURE_HEIGHT = 54 };
    HDC screen = GetDC(NULL);
    HDC memory = screen ? CreateCompatibleDC(screen) : NULL;
    BITMAPINFO info;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = CAPTURE_WIDTH;
    info.bmiHeader.biHeight = -CAPTURE_HEIGHT;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    unsigned char *pixels = NULL;
    HBITMAP bitmap = memory ? CreateDIBSection(memory, &info, DIB_RGB_COLORS, (void **)&pixels, NULL, 0) : NULL;
    HGDIOBJ oldBitmap = bitmap ? SelectObject(memory, bitmap) : NULL;
    double previousR[KEYMAP_COUNT] = {0};
    double previousG[KEYMAP_COUNT] = {0};
    double previousB[KEYMAP_COUNT] = {0};
    int initialized = 0;

    if (!screen || !memory || !bitmap || !pixels) {
        log_event("screen ambient: GDI capture initialization failed");
        InterlockedExchange(&g_stopFlag, 1);
    } else {
        SetStretchBltMode(memory, HALFTONE);
        SetBrushOrgEx(memory, 0, 0, NULL);
        log_event("screen ambient: in-memory capture started");
        char dimensions[160];
        sprintf(dimensions, "screen ambient: source physical=%dx%d metrics=%dx%d, sample=%dx%d",
            GetDeviceCaps(screen, DESKTOPHORZRES), GetDeviceCaps(screen, DESKTOPVERTRES),
            GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
            CAPTURE_WIDTH, CAPTURE_HEIGHT);
        log_event(dimensions);
    }

    while (!InterlockedCompareExchange(&g_stopFlag, 0, 0)) {
        int screenWidth = GetDeviceCaps(screen, DESKTOPHORZRES);
        int screenHeight = GetDeviceCaps(screen, DESKTOPVERTRES);
        if (screenWidth <= 0) screenWidth = GetSystemMetrics(SM_CXSCREEN);
        if (screenHeight <= 0) screenHeight = GetSystemMetrics(SM_CYSCREEN);
        BOOL captured = StretchBlt(memory, 0, 0, CAPTURE_WIDTH, CAPTURE_HEIGHT,
            screen, 0, 0, screenWidth, screenHeight, SRCCOPY | CAPTUREBLT);
        if (!captured) {
            Sleep(100);
            continue;
        }

        unsigned char frame[BUF_SIZE] = {0};
        int radius = (int)cfg_clamp(g_ambientBlur, 1, 8);
        double response = 0.08 + (double)cfg_clamp(g_ambientResponse, 1, 10) * 0.052;
        for (size_t i = 0; i < KEYMAP_COUNT; i++) {
            double keyX = get_key_x(&KEYMAP[i]);
            double keyY = get_key_y(&KEYMAP[i]);
            int centerX = (int)(keyX / 19.0 * (CAPTURE_WIDTH - 1) + 0.5);
            int centerY = (int)(keyY / 5.25 * (CAPTURE_HEIGHT - 1) + 0.5);
            if (centerX < 0) centerX = 0;
            if (centerX >= CAPTURE_WIDTH) centerX = CAPTURE_WIDTH - 1;
            if (centerY < 0) centerY = 0;
            if (centerY >= CAPTURE_HEIGHT) centerY = CAPTURE_HEIGHT - 1;

            double sumR = 0.0, sumG = 0.0, sumB = 0.0, sumWeight = 0.0;
            for (int dy = -radius; dy <= radius; dy++) {
                int sy = centerY + dy;
                if (sy < 0 || sy >= CAPTURE_HEIGHT) continue;
                for (int dx = -radius; dx <= radius; dx++) {
                    int sx = centerX + dx;
                    if (sx < 0 || sx >= CAPTURE_WIDTH) continue;
                    double distance = sqrt((double)(dx * dx + dy * dy));
                    double weight = 1.0 - distance / (double)(radius + 1);
                    if (weight <= 0.0) continue;
                    const unsigned char *pixel = pixels + (sy * CAPTURE_WIDTH + sx) * 4;
                    sumB += srgb_channel_to_linear(pixel[0]) * weight;
                    sumG += srgb_channel_to_linear(pixel[1]) * weight;
                    sumR += srgb_channel_to_linear(pixel[2]) * weight;
                    sumWeight += weight;
                }
            }
            double targetR = sumWeight > 0.0 ? sumR / sumWeight : 0.0;
            double targetG = sumWeight > 0.0 ? sumG / sumWeight : 0.0;
            double targetB = sumWeight > 0.0 ? sumB / sumWeight : 0.0;
            if (!initialized) {
                previousR[i] = targetR;
                previousG[i] = targetG;
                previousB[i] = targetB;
            } else {
                previousR[i] += (targetR - previousR[i]) * response;
                previousG[i] += (targetG - previousG[i]) * response;
                previousB[i] += (targetB - previousB[i]) * response;
            }
            set_key_from_linear(frame, &KEYMAP[i], previousR[i], previousG[i], previousB[i]);
        }
        initialized = 1;
        send_frame(frame);
        pace_frame();
    }

    if (oldBitmap) SelectObject(memory, oldBitmap);
    if (bitmap) DeleteObject(bitmap);
    if (memory) DeleteDC(memory);
    if (screen) ReleaseDC(NULL, screen);
    log_event("screen ambient: capture stopped");
    return 0;
}

static ULONGLONG filetime_value(FILETIME value) {
    ULARGE_INTEGER integer;
    integer.LowPart = value.dwLowDateTime;
    integer.HighPart = value.dwHighDateTime;
    return integer.QuadPart;
}

static double query_cpu_usage(FILETIME *previousIdle, FILETIME *previousKernel,
                              FILETIME *previousUser, int *initialized) {
    FILETIME idle, kernel, user;
    if (!GetSystemTimes(&idle, &kernel, &user)) return 0.0;
    if (!*initialized) {
        *previousIdle = idle;
        *previousKernel = kernel;
        *previousUser = user;
        *initialized = 1;
        return 0.0;
    }
    ULONGLONG idleDelta = filetime_value(idle) - filetime_value(*previousIdle);
    ULONGLONG kernelDelta = filetime_value(kernel) - filetime_value(*previousKernel);
    ULONGLONG userDelta = filetime_value(user) - filetime_value(*previousUser);
    ULONGLONG total = kernelDelta + userDelta;
    *previousIdle = idle;
    *previousKernel = kernel;
    *previousUser = user;
    if (!total || idleDelta >= total) return idleDelta >= total ? 0.0 : 1.0;
    return (double)(total - idleDelta) / (double)total;
}

static double query_gpu_usage(PDH_HCOUNTER counter) {
    if (!counter) return 0.0;
    DWORD bytes = 0, count = 0;
    PDH_STATUS status = PdhGetFormattedCounterArrayW(counter,
        PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &bytes, &count, NULL);
    if (status != (PDH_STATUS)PDH_MORE_DATA || !bytes) return 0.0;
    PPDH_FMT_COUNTERVALUE_ITEM_W items = (PPDH_FMT_COUNTERVALUE_ITEM_W)malloc(bytes);
    if (!items) return 0.0;
    status = PdhGetFormattedCounterArrayW(counter,
        PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &bytes, &count, items);
    double total = 0.0;
    if (status == ERROR_SUCCESS) {
        for (DWORD i = 0; i < count; i++) {
            if (items[i].FmtValue.CStatus != PDH_CSTATUS_VALID_DATA &&
                items[i].FmtValue.CStatus != PDH_CSTATUS_NEW_DATA) continue;
            if (wcsstr(items[i].szName, L"engtype_3D") ||
                wcsstr(items[i].szName, L"engtype_Compute"))
                total += items[i].FmtValue.doubleValue;
        }
    }
    free(items);
    if (total < 0.0) total = 0.0;
    if (total > 100.0) total = 100.0;
    return total / 100.0;
}

/*
 * 游戏 FPS 采集实验区。PresentMon/FrameView 在不同游戏与权限模型下不够可靠，
 * 当前使用 #if 0 完全排除，不进入发布二进制；UI 的 FPS 仅指 RGB 引擎帧率。
 */
#if 0
#define PRESENTMON_BUFFER_SIZE 65536
typedef struct {
    HANDLE process;
    HANDLE output;
    HANDLE diagnosticOutput;
    wchar_t executable[MAX_PATH];
    wchar_t outputPath[MAX_PATH];
    wchar_t sessionName[64];
    char buffer[PRESENTMON_BUFFER_SIZE];
    size_t bufferLength;
    int processIdColumn;
    DWORD foregroundProcessId;
    LONG frameCount;
    ULONGLONG windowStart;
    ULONGLONG startedAt;
    int diagnosticLines;
    int measurementLogged;
    int waitingLogged;
} PresentMonCapture;

static int csv_copy_field(const char *line, int targetIndex,
                          char *output, size_t outputSize) {
    const char *cursor = line;
    int index = 0;
    if (!outputSize) return 0;
    while (*cursor) {
        size_t length = 0;
        int quoted = *cursor == '"';
        if (quoted) cursor++;
        while (*cursor) {
            if (quoted) {
                if (*cursor == '"' && cursor[1] == '"') {
                    if (index == targetIndex && length + 1 < outputSize)
                        output[length++] = '"';
                    cursor += 2;
                    continue;
                }
                if (*cursor == '"') {
                    cursor++;
                    break;
                }
            } else if (*cursor == ',') {
                break;
            }
            if (index == targetIndex && length + 1 < outputSize)
                output[length++] = *cursor;
            cursor++;
        }
        if (index == targetIndex) {
            output[length] = '\0';
            return 1;
        }
        while (*cursor && *cursor != ',') cursor++;
        if (*cursor == ',') cursor++;
        index++;
    }
    output[0] = '\0';
    return 0;
}

static int find_presentmon_path(wchar_t *path, size_t pathCount) {
    wchar_t candidate[MAX_PATH];
    GetModuleFileNameW(NULL, candidate, MAX_PATH);
    wchar_t *slash = wcsrchr(candidate, L'\\');
    if (slash) {
        *(slash + 1) = L'\0';
        lstrcatW(candidate, L"PresentMon_legacy_x64.exe");
        if (GetFileAttributesW(candidate) != INVALID_FILE_ATTRIBUTES) {
            lstrcpynW(path, candidate, (int)pathCount);
            return 1;
        }
    }

    wchar_t programFiles[MAX_PATH];
    DWORD length = GetEnvironmentVariableW(L"ProgramFiles", programFiles, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        _snwprintf(candidate, MAX_PATH - 1,
            L"%ls\\NVIDIA Corporation\\FrameViewSDK\\bin\\PresentMon_x64.exe",
            programFiles);
        candidate[MAX_PATH - 1] = L'\0';
        if (GetFileAttributesW(candidate) != INVALID_FILE_ATTRIBUTES) {
            lstrcpynW(path, candidate, (int)pathCount);
            return 1;
        }
    }
    return 0;
}

static void cleanup_bundled_presentmon_instances(const wchar_t *executable) {
    const wchar_t *expectedName = wcsrchr(executable, L'\\');
    expectedName = expectedName ? expectedName + 1 : executable;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W entry = {0};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, expectedName) != 0) continue;
            HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION |
                PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
            if (!process) continue;
            wchar_t imagePath[MAX_PATH];
            DWORD imagePathLength = MAX_PATH;
            if (QueryFullProcessImageNameW(process, 0, imagePath,
                    &imagePathLength) && _wcsicmp(imagePath, executable) == 0) {
                TerminateProcess(process, 0);
                WaitForSingleObject(process, 500);
                log_event("PresentMon FPS: removed a stale bundled capture process");
            }
            CloseHandle(process);
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
}

static void stop_presentmon_capture(PresentMonCapture *capture) {
    if (capture->process) {
        if (WaitForSingleObject(capture->process, 0) == WAIT_TIMEOUT) {
            STARTUPINFOW startup = {0};
            PROCESS_INFORMATION processInfo = {0};
            wchar_t command[768];
            startup.cb = sizeof(startup);
            _snwprintf(command, 767,
                L"\"%ls\" -terminate_existing -session_name %ls",
                capture->executable, capture->sessionName);
            command[767] = L'\0';
            if (CreateProcessW(capture->executable, command, NULL, NULL, FALSE,
                    CREATE_NO_WINDOW, NULL, NULL, &startup, &processInfo)) {
                CloseHandle(processInfo.hThread);
                WaitForSingleObject(processInfo.hProcess, 2000);
                CloseHandle(processInfo.hProcess);
            }
            if (WaitForSingleObject(capture->process, 1200) == WAIT_TIMEOUT)
                TerminateProcess(capture->process, 0);
            WaitForSingleObject(capture->process, 500);
        }
        CloseHandle(capture->process);
    }
    if (capture->output) CloseHandle(capture->output);
    if (capture->diagnosticOutput) CloseHandle(capture->diagnosticOutput);
    if (capture->outputPath[0]) DeleteFileW(capture->outputPath);
    memset(capture, 0, sizeof(*capture));
    InterlockedExchange(&g_measuredFps, 0);
}

static int start_presentmon_capture(PresentMonCapture *capture, ULONGLONG now) {
    wchar_t executable[MAX_PATH];
    if (!find_presentmon_path(executable, MAX_PATH)) {
        log_event("PresentMon FPS: executable not found");
        return 0;
    }

    cleanup_bundled_presentmon_instances(executable);
    wchar_t oldBundledPath[MAX_PATH];
    GetModuleFileNameW(NULL, oldBundledPath, MAX_PATH);
    wchar_t *oldBundledName = wcsrchr(oldBundledPath, L'\\');
    if (oldBundledName) {
        lstrcpynW(oldBundledName + 1, L"PresentMon_x64.exe",
            (int)(MAX_PATH - (oldBundledName + 1 - oldBundledPath)));
        cleanup_bundled_presentmon_instances(oldBundledPath);
    }

    if (!GetModuleFileNameW(NULL, capture->outputPath, MAX_PATH)) {
        log_event("PresentMon FPS: application path unavailable");
        return 0;
    }
    wchar_t *outputName = wcsrchr(capture->outputPath, L'\\');
    if (!outputName) return 0;
    lstrcpynW(outputName + 1, L"PresentMon-live.csv",
        (int)(MAX_PATH - (outputName + 1 - capture->outputPath)));
    DeleteFileW(capture->outputPath);
    lstrcpynW(capture->sessionName, L"BetterRGBFPS", 64);
    lstrcpynW(capture->executable, executable, MAX_PATH);

    SECURITY_ATTRIBUTES security = { sizeof(security), NULL, TRUE };
    HANDLE diagnosticRead = NULL, diagnosticWrite = NULL;
    if (!CreatePipe(&diagnosticRead, &diagnosticWrite, &security, 0)) {
        log_event("PresentMon FPS: diagnostic pipe unavailable");
        return 0;
    }
    SetHandleInformation(diagnosticRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup;
    PROCESS_INFORMATION processInfo;
    memset(&startup, 0, sizeof(startup));
    memset(&processInfo, 0, sizeof(processInfo));
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = diagnosticWrite;
    startup.hStdError = diagnosticWrite;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    wchar_t command[1536];
    _snwprintf(command, 1535,
        L"\"%ls\" -output_file \"%ls\" -no_top -exclude_dropped "
        L"-session_name %ls -stop_existing_session",
        executable, capture->outputPath, capture->sessionName);
    command[1535] = L'\0';
    BOOL created = CreateProcessW(executable, command, NULL, NULL, TRUE,
        CREATE_NO_WINDOW, NULL, NULL, &startup, &processInfo);
    CloseHandle(diagnosticWrite);
    if (!created) {
        CloseHandle(diagnosticRead);
        log_event("PresentMon FPS: CreateProcess failed");
        return 0;
    }
    CloseHandle(processInfo.hThread);
    capture->process = processInfo.hProcess;
    capture->diagnosticOutput = diagnosticRead;
    capture->processIdColumn = -1;
    capture->windowStart = now;
    capture->startedAt = now;
    log_event("PresentMon FPS: capture started for foreground process");
    return 1;
}

static void process_presentmon_line(PresentMonCapture *capture,
                                    const char *line) {
    if (capture->processIdColumn < 0) {
        char field[96];
        for (int index = 0; index < 64; index++) {
            if (!csv_copy_field(line, index, field, sizeof(field))) break;
            const char *name = field;
            if (index == 0 && (unsigned char)name[0] == 0xEF &&
                (unsigned char)name[1] == 0xBB &&
                (unsigned char)name[2] == 0xBF) name += 3;
            if (_stricmp(name, "ProcessID") == 0) {
                capture->processIdColumn = index;
                log_event("PresentMon FPS: CSV stream ready");
                return;
            }
        }
        if (*line && capture->diagnosticLines < 3) {
            char message[360];
            _snprintf(message, sizeof(message) - 1,
                "PresentMon FPS output: %.300s", line);
            message[sizeof(message) - 1] = '\0';
            log_event(message);
            capture->diagnosticLines++;
        }
        return;
    }

    char processText[32];
    if (!csv_copy_field(line, capture->processIdColumn,
                        processText, sizeof(processText))) return;
    char *end = NULL;
    unsigned long processId = strtoul(processText, &end, 10);
    if (end != processText && processId == capture->foregroundProcessId)
        capture->frameCount++;
}

static int poll_presentmon_capture(PresentMonCapture *capture,
                                   ULONGLONG now) {
    if (!capture->process) return 0;
    BOOL processExited = WaitForSingleObject(capture->process, 0) == WAIT_OBJECT_0;

    if (!capture->output) {
        capture->output = CreateFileW(capture->outputPath, GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (capture->output == INVALID_HANDLE_VALUE) capture->output = NULL;
    }

    if (capture->diagnosticOutput && capture->diagnosticLines < 6) {
        DWORD available = 0;
        if (PeekNamedPipe(capture->diagnosticOutput, NULL, 0, NULL,
                &available, NULL) && available) {
            char diagnostic[384];
            DWORD requested = available < sizeof(diagnostic) - 1
                ? available : (DWORD)sizeof(diagnostic) - 1;
            DWORD read = 0;
            if (ReadFile(capture->diagnosticOutput, diagnostic, requested,
                    &read, NULL) && read) {
                diagnostic[read] = '\0';
                for (DWORD i = 0; i < read; i++)
                    if (diagnostic[i] == '\r' || diagnostic[i] == '\n')
                        diagnostic[i] = ' ';
                char message[440];
                _snprintf(message, sizeof(message) - 1,
                    "PresentMon diagnostic: %.360s", diagnostic);
                message[sizeof(message) - 1] = '\0';
                log_event(message);
                capture->diagnosticLines++;
            }
        }
    }

    DWORD foregroundProcessId = 0;
    HWND foreground = GetForegroundWindow();
    if (foreground) GetWindowThreadProcessId(foreground, &foregroundProcessId);
    if (foregroundProcessId != capture->foregroundProcessId) {
        capture->foregroundProcessId = foregroundProcessId;
        capture->frameCount = 0;
        capture->windowStart = now;
        capture->measurementLogged = 0;
        InterlockedExchange(&g_measuredFps, 0);
    }

    while (capture->output) {
        if (capture->bufferLength + 1 >= PRESENTMON_BUFFER_SIZE) {
            capture->bufferLength = 0;
            capture->processIdColumn = -1;
        }
        DWORD room = (DWORD)(PRESENTMON_BUFFER_SIZE -
                     capture->bufferLength - 1);
        DWORD read = 0;
        if (!ReadFile(capture->output,
                capture->buffer + capture->bufferLength,
                room, &read, NULL)) return 0;
        if (!read) break;
        capture->bufferLength += read;
        capture->buffer[capture->bufferLength] = '\0';
    }

    size_t lineStart = 0;
    for (size_t i = 0; i < capture->bufferLength; i++) {
        if (capture->buffer[i] != '\n') continue;
        capture->buffer[i] = '\0';
        if (i > lineStart && capture->buffer[i - 1] == '\r')
            capture->buffer[i - 1] = '\0';
        process_presentmon_line(capture, capture->buffer + lineStart);
        lineStart = i + 1;
    }
    if (lineStart) {
        capture->bufferLength -= lineStart;
        memmove(capture->buffer, capture->buffer + lineStart,
                capture->bufferLength);
    }

    ULONGLONG elapsed = now - capture->windowStart;
    if (elapsed >= 1000) {
        LONG measured = elapsed > 0
            ? (LONG)((ULONGLONG)capture->frameCount * 1000ULL / elapsed) : 0;
        InterlockedExchange(&g_measuredFps, measured);
        if (measured > 0 && !capture->measurementLogged) {
            char message[128];
            sprintf(message,
                "PresentMon FPS: foreground PID %lu measured %ld FPS",
                (unsigned long)capture->foregroundProcessId, (long)measured);
            log_event(message);
            capture->measurementLogged = 1;
        }
        capture->frameCount = 0;
        capture->windowStart = now;
    }
    if (capture->processIdColumn < 0 && now - capture->startedAt > 8000 &&
            !capture->waitingLogged) {
        log_event("PresentMon FPS: waiting for the first graphics frame");
        capture->waitingLogged = 1;
    }
    if (processExited) {
        DWORD exitCode = 0;
        GetExitCodeProcess(capture->process, &exitCode);
        char message[96];
        sprintf(message, "PresentMon FPS: process exited with code %lu",
            (unsigned long)exitCode);
        log_event(message);
        return 0;
    }
    return 1;
}

#define FV_SUCCESS 0L
#define FV_METRIC_FRAME 0x5BE0DB9EU
typedef LONG (__cdecl *FvInitializeFunction)(void);
typedef LONG (__cdecl *FvCreateSessionFunction)(void **);
typedef LONG (__cdecl *FvSessionFunction)(void *);
typedef LONG (__cdecl *FvEnableMetricsFunction)(void *, unsigned int *, unsigned int);
typedef LONG (__cdecl *FvGetPipeFunction)(void *, void **);
typedef LONG (__cdecl *FvShutdownFunction)(void);

typedef struct {
    HMODULE module;
    void *session;
    HANDLE pipe;
    FvInitializeFunction initialize;
    FvCreateSessionFunction createSession;
    FvSessionFunction startSession;
    FvEnableMetricsFunction enableMetrics;
    FvGetPipeFunction getPerFramePipe;
    FvSessionFunction stopSession;
    FvSessionFunction destroySession;
    FvShutdownFunction shutdown;
    char buffer[16384];
    size_t bufferLength;
    DWORD foregroundProcessId;
    LONG frameCount;
    ULONGLONG windowStart;
    int measurementLogged;
} FrameViewCapture;

static int find_frameview_sdk_path(wchar_t *path, size_t pathCount) {
    wchar_t programFiles[MAX_PATH];
    DWORD length = GetEnvironmentVariableW(L"ProgramFiles", programFiles, MAX_PATH);
    if (!length || length >= MAX_PATH) return 0;
    _snwprintf(path, pathCount - 1,
        L"%ls\\NVIDIA Corporation\\FrameViewSDK\\SDK\\Public_Release\\FvSDK_x64.dll",
        programFiles);
    path[pathCount - 1] = L'\0';
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

static void stop_frameview_capture(FrameViewCapture *capture) {
    if (capture->pipe) CloseHandle(capture->pipe);
    if (capture->session && capture->stopSession)
        capture->stopSession(capture->session);
    if (capture->session && capture->destroySession)
        capture->destroySession(capture->session);
    if (capture->shutdown) capture->shutdown();
    if (capture->module) FreeLibrary(capture->module);
    memset(capture, 0, sizeof(*capture));
    InterlockedExchange(&g_measuredFps, 0);
}

static int start_frameview_capture(FrameViewCapture *capture, ULONGLONG now) {
    wchar_t libraryPath[MAX_PATH];
    if (!find_frameview_sdk_path(libraryPath, MAX_PATH)) return 0;
    capture->module = LoadLibraryW(libraryPath);
    if (!capture->module) return 0;

#define LOAD_FV_FUNCTION(field, name) do { \
    FARPROC address = GetProcAddress(capture->module, name); \
    if (!address) goto failed; \
    memcpy(&capture->field, &address, sizeof(address)); \
} while (0)
    LOAD_FV_FUNCTION(initialize, "FvSDK_Initialize");
    LOAD_FV_FUNCTION(createSession, "FvSDK_CreateSession");
    LOAD_FV_FUNCTION(startSession, "FvSDK_StartSession");
    LOAD_FV_FUNCTION(enableMetrics, "FvSDK_EnableMetrics");
    LOAD_FV_FUNCTION(getPerFramePipe, "FvSDK_GetPerFrameDataPipe");
    LOAD_FV_FUNCTION(stopSession, "FvSDK_StopSession");
    LOAD_FV_FUNCTION(destroySession, "FvSDK_DestroySession");
    LOAD_FV_FUNCTION(shutdown, "FvSDK_Shutdown");
#undef LOAD_FV_FUNCTION

    if (capture->initialize() != FV_SUCCESS) goto failed;
    if (capture->createSession(&capture->session) != FV_SUCCESS) goto failed;
    if (capture->startSession(capture->session) != FV_SUCCESS) goto failed;
    unsigned int metric = FV_METRIC_FRAME;
    if (capture->enableMetrics(capture->session, &metric, 1) != FV_SUCCESS)
        goto failed;
    void *pipe = NULL;
    if (capture->getPerFramePipe(capture->session, &pipe) != FV_SUCCESS || !pipe)
        goto failed;
    capture->pipe = (HANDLE)pipe;
    capture->windowStart = now;
    log_event("FrameView FPS: per-frame SDK stream started");
    return 1;

failed:
    log_event("FrameView FPS: SDK initialization failed, using fallback");
    stop_frameview_capture(capture);
    return 0;
}

static void process_frameview_line(FrameViewCapture *capture, const char *line) {
    if (!isdigit((unsigned char)*line)) return;
    char *end = NULL;
    unsigned long processId = strtoul(line, &end, 10);
    if (end != line && *end == ',' && processId == capture->foregroundProcessId)
        capture->frameCount++;
}

static int poll_frameview_capture(FrameViewCapture *capture, ULONGLONG now) {
    if (!capture->module || !capture->pipe) return 0;
    DWORD foregroundProcessId = 0;
    HWND foreground = GetForegroundWindow();
    if (foreground) GetWindowThreadProcessId(foreground, &foregroundProcessId);
    if (foregroundProcessId != capture->foregroundProcessId) {
        capture->foregroundProcessId = foregroundProcessId;
        capture->frameCount = 0;
        capture->windowStart = now;
        capture->measurementLogged = 0;
        InterlockedExchange(&g_measuredFps, 0);
    }

    for (;;) {
        DWORD available = 0;
        if (!PeekNamedPipe(capture->pipe, NULL, 0, NULL, &available, NULL))
            return 0;
        if (!available) break;
        if (capture->bufferLength + 1 >= sizeof(capture->buffer))
            capture->bufferLength = 0;
        DWORD room = (DWORD)(sizeof(capture->buffer) - capture->bufferLength - 1);
        DWORD requested = available < room ? available : room;
        DWORD read = 0;
        if (!ReadFile(capture->pipe, capture->buffer + capture->bufferLength,
                requested, &read, NULL) || !read) return 0;
        capture->bufferLength += read;
        capture->buffer[capture->bufferLength] = '\0';
    }

    size_t lineStart = 0;
    for (size_t i = 0; i < capture->bufferLength; i++) {
        if (capture->buffer[i] != '\n') continue;
        capture->buffer[i] = '\0';
        if (i > lineStart && capture->buffer[i - 1] == '\r')
            capture->buffer[i - 1] = '\0';
        process_frameview_line(capture, capture->buffer + lineStart);
        lineStart = i + 1;
    }
    if (lineStart) {
        capture->bufferLength -= lineStart;
        memmove(capture->buffer, capture->buffer + lineStart,
            capture->bufferLength);
    }

    ULONGLONG elapsed = now - capture->windowStart;
    if (elapsed >= 1000) {
        LONG measured = elapsed > 0
            ? (LONG)((ULONGLONG)capture->frameCount * 1000ULL / elapsed) : 0;
        InterlockedExchange(&g_measuredFps, measured);
        if (measured > 0 && !capture->measurementLogged) {
            char message[128];
            sprintf(message,
                "FrameView FPS: foreground PID %lu measured %ld FPS",
                (unsigned long)capture->foregroundProcessId, (long)measured);
            log_event(message);
            capture->measurementLogged = 1;
        }
        capture->frameCount = 0;
        capture->windowStart = now;
    }
    return 1;
}
#endif

static void cleanup_retired_fps_capture_tools(void) {
    wchar_t moduleDirectory[MAX_PATH];
    if (!GetModuleFileNameW(NULL, moduleDirectory, MAX_PATH)) return;
    wchar_t *name = wcsrchr(moduleDirectory, L'\\');
    if (!name) return;
    *(name + 1) = L'\0';

    const wchar_t *retiredNames[] = {
        L"PresentMon_x64.exe", L"PresentMon_legacy_x64.exe"
    };
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry = {0};
        entry.dwSize = sizeof(entry);
        if (Process32FirstW(snapshot, &entry)) {
            do {
                for (size_t i = 0; i < sizeof(retiredNames) / sizeof(retiredNames[0]); i++) {
                    if (_wcsicmp(entry.szExeFile, retiredNames[i]) != 0) continue;
                    wchar_t expected[MAX_PATH];
                    _snwprintf(expected, MAX_PATH - 1, L"%ls%ls",
                        moduleDirectory, retiredNames[i]);
                    expected[MAX_PATH - 1] = L'\0';
                    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION |
                        PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
                    if (!process) continue;
                    wchar_t actual[MAX_PATH];
                    DWORD actualLength = MAX_PATH;
                    if (QueryFullProcessImageNameW(process, 0, actual, &actualLength) &&
                            _wcsicmp(actual, expected) == 0) {
                        TerminateProcess(process, 0);
                        WaitForSingleObject(process, 500);
                        log_event("cleanup: retired FPS capture process stopped");
                    }
                    CloseHandle(process);
                }
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }

    wchar_t marker[MAX_PATH];
    _snwprintf(marker, MAX_PATH - 1, L"%lscleanup-frameview-test.flag",
        moduleDirectory);
    marker[MAX_PATH - 1] = L'\0';
    if (GetFileAttributesW(marker) == INVALID_FILE_ATTRIBUTES) return;
    SC_HANDLE manager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (manager) {
        SC_HANDLE service = OpenServiceW(manager, L"FvSvc",
            SERVICE_STOP | SERVICE_QUERY_STATUS);
        if (service) {
            SERVICE_STATUS status;
            if (ControlService(service, SERVICE_CONTROL_STOP, &status))
                log_event("cleanup: FrameView test service stop requested");
            CloseServiceHandle(service);
        }
        CloseServiceHandle(manager);
    }
    DeleteFileW(marker);
}

static void initialize_audio_metrics(IMMDeviceEnumerator **enumerator,
                                     IAudioEndpointVolume **volume,
                                     IAudioMeterInformation **renderMeter,
                                     IAudioMeterInformation **captureMeter) {
    if (!*enumerator) {
        CoCreateInstance(&CLSID_MMDeviceEnumerator_Local, NULL, CLSCTX_ALL,
            &IID_IMMDeviceEnumerator_Local, (void **)enumerator);
    }
    if (!*enumerator) return;
    IMMDevice *device = NULL;
    if (!*volume || !*renderMeter) {
        if (SUCCEEDED(IMMDeviceEnumerator_GetDefaultAudioEndpoint(*enumerator,
                eRender, eConsole, &device)) && device) {
            if (!*volume) IMMDevice_Activate(device, &IID_IAudioEndpointVolume_Local,
                CLSCTX_ALL, NULL, (void **)volume);
            if (!*renderMeter) IMMDevice_Activate(device, &IID_IAudioMeterInformation_Local,
                CLSCTX_ALL, NULL, (void **)renderMeter);
            IMMDevice_Release(device);
            device = NULL;
        }
    }
    if (!*captureMeter && SUCCEEDED(IMMDeviceEnumerator_GetDefaultAudioEndpoint(*enumerator,
            eCapture, eConsole, &device)) && device) {
        IMMDevice_Activate(device, &IID_IAudioMeterInformation_Local,
            CLSCTX_ALL, NULL, (void **)captureMeter);
        IMMDevice_Release(device);
    }
}

static int refresh_default_render_metrics(IMMDeviceEnumerator *enumerator,
                                          IAudioEndpointVolume **volume,
                                          IAudioMeterInformation **renderMeter,
                                          LPWSTR *endpointId) {
    if (!enumerator) return 0;

    IMMDevice *device = NULL;
    if (FAILED(IMMDeviceEnumerator_GetDefaultAudioEndpoint(enumerator,
            eRender, eConsole, &device)) || !device) return 0;

    LPWSTR currentId = NULL;
    if (FAILED(IMMDevice_GetId(device, &currentId)) || !currentId) {
        IMMDevice_Release(device);
        return 0;
    }

    int changed = !*endpointId || lstrcmpW(*endpointId, currentId) != 0;
    if (changed) {
        IAudioEndpointVolume *newVolume = NULL;
        IAudioMeterInformation *newRenderMeter = NULL;
        IMMDevice_Activate(device, &IID_IAudioEndpointVolume_Local,
            CLSCTX_ALL, NULL, (void **)&newVolume);
        IMMDevice_Activate(device, &IID_IAudioMeterInformation_Local,
            CLSCTX_ALL, NULL, (void **)&newRenderMeter);

        if (*volume) IAudioEndpointVolume_Release(*volume);
        if (*renderMeter) (*renderMeter)->lpVtbl->Release(*renderMeter);
        if (*endpointId) CoTaskMemFree(*endpointId);
        *volume = newVolume;
        *renderMeter = newRenderMeter;
        *endpointId = currentId;
        log_event("touchbar metrics: default render endpoint rebound");
    } else {
        CoTaskMemFree(currentId);
    }

    IMMDevice_Release(device);
    return changed;
}

DWORD WINAPI touchbar_metrics_thread(LPVOID p) {
    (void)p;
    HRESULT comResult = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    int uninitialize = SUCCEEDED(comResult);
    IMMDeviceEnumerator *enumerator = NULL;
    IAudioEndpointVolume *volume = NULL;
    IAudioMeterInformation *renderMeter = NULL;
    IAudioMeterInformation *captureMeter = NULL;
    LPWSTR renderEndpointId = NULL;
    initialize_audio_metrics(&enumerator, &volume, &renderMeter, &captureMeter);

    PDH_HQUERY gpuQuery = NULL;
    PDH_HCOUNTER gpuCounter = NULL;
    if (PdhOpenQueryW(NULL, 0, &gpuQuery) == ERROR_SUCCESS) {
        if (PdhAddEnglishCounterW(gpuQuery,
                L"\\GPU Engine(*)\\Utilization Percentage", 0, &gpuCounter) != ERROR_SUCCESS)
            gpuCounter = NULL;
        PdhCollectQueryData(gpuQuery);
    }

    FILETIME previousIdle = {0}, previousKernel = {0}, previousUser = {0};
    int cpuInitialized = 0;
    double cpuLevel = 0.0, gpuLevel = 0.0, smoothed = 0.0;
    double cachedVolume = 0.0;
    float lastVolume = -1.0f;
    BOOL lastMute = FALSE;
    int volumeInitialized = 0;
    ULONGLONG nextSystemSample = 0, nextAudioRetry = 0, nextVolumeSample = 0;
    ULONGLONG nextEndpointCheck = 0;
    ULONGLONG volumeOverrideUntil = 0;
    int volumeOverrideActive = 0;
    LONG lastMode = TOUCHBAR_OFF;
    log_event("touchbar metrics: worker started");

    while (!InterlockedCompareExchange(&g_touchbarStop, 0, 0)) {
        ULONGLONG now = GetTickCount64();
        LONG configuredMode = InterlockedCompareExchange(&g_touchbarMode, 0, 0);
        if (now >= nextSystemSample) {
            cpuLevel = query_cpu_usage(&previousIdle, &previousKernel, &previousUser, &cpuInitialized);
            if (gpuQuery && gpuCounter) {
                PdhCollectQueryData(gpuQuery);
                gpuLevel = query_gpu_usage(gpuCounter);
            }
            nextSystemSample = now + 250;
        }
        if (now >= nextAudioRetry && (!volume || !renderMeter || !captureMeter)) {
            initialize_audio_metrics(&enumerator, &volume, &renderMeter, &captureMeter);
            nextAudioRetry = now + 5000;
        }
        if (now >= nextEndpointCheck) {
            if (refresh_default_render_metrics(enumerator, &volume, &renderMeter,
                    &renderEndpointId)) {
                volumeInitialized = 0;
                nextVolumeSample = 0;
            }
            nextEndpointCheck = now + 1000;
        }
        if (volume && now >= nextVolumeSample) {
            float value = 0.0f;
            BOOL mute = FALSE;
            HRESULT volumeResult = IAudioEndpointVolume_GetMasterVolumeLevelScalar(volume, &value);
            HRESULT muteResult = IAudioEndpointVolume_GetMute(volume, &mute);
            if (SUCCEEDED(volumeResult) && SUCCEEDED(muteResult)) {
                float displayed = mute ? 0.0f : value;
                if (volumeInitialized &&
                    (fabs((double)displayed - (double)lastVolume) >= 0.004 ||
                     mute != lastMute)) {
                    volumeOverrideUntil = now + 1800;
                    if (!volumeOverrideActive)
                        log_event("touchbar metrics: volume override started");
                    volumeOverrideActive = 1;
                }
                cachedVolume = displayed;
                lastVolume = displayed;
                lastMute = mute;
                volumeInitialized = 1;
            } else {
                IAudioEndpointVolume_Release(volume);
                volume = NULL;
                volumeInitialized = 0;
                nextAudioRetry = 0;
            }
            nextVolumeSample = now + 50;
        }

        if (volumeOverrideActive && now >= volumeOverrideUntil) {
            volumeOverrideActive = 0;
            log_event("touchbar metrics: volume override ended");
        }

        LONG mode = now < volumeOverrideUntil ? TOUCHBAR_VOLUME : configuredMode;
        InterlockedExchange(&g_touchbarDisplayMode, mode);

        double target = 0.0;
        if (mode == TOUCHBAR_BATTERY) {
            SYSTEM_POWER_STATUS status;
            if (GetSystemPowerStatus(&status) && status.BatteryLifePercent != 255)
                target = (double)status.BatteryLifePercent / 100.0;
        } else if (mode == TOUCHBAR_VOLUME && volumeInitialized) {
            target = cachedVolume;
        } else if (mode == TOUCHBAR_MIC && captureMeter) {
            float value = 0.0f;
            HRESULT result = captureMeter->lpVtbl->GetPeakValue(captureMeter, &value);
            if (SUCCEEDED(result)) target = sqrt(value);
            else {
                captureMeter->lpVtbl->Release(captureMeter);
                captureMeter = NULL;
                nextAudioRetry = 0;
            }
        } else if (mode == TOUCHBAR_CPU) target = cpuLevel;
        else if (mode == TOUCHBAR_GPU) target = gpuLevel;
        else if (mode == TOUCHBAR_FPS) {
            LONG fpsMax = InterlockedCompareExchange(&g_onAcPower, 0, 0)
                ? InterlockedCompareExchange(&g_acTouchbarFpsMax, 0, 0)
                : InterlockedCompareExchange(&g_batteryTouchbarFpsMax, 0, 0);
            if (fpsMax < 1) fpsMax = 60;
            target = (double)InterlockedCompareExchange(&g_measuredFps, 0, 0) /
                     (double)fpsMax;
        }
        else if (mode == TOUCHBAR_AUDIO && renderMeter) {
            float value = 0.0f;
            HRESULT result = renderMeter->lpVtbl->GetPeakValue(renderMeter, &value);
            if (SUCCEEDED(result)) target = sqrt(value);
            else {
                renderMeter->lpVtbl->Release(renderMeter);
                renderMeter = NULL;
                nextAudioRetry = 0;
            }
        }
        if (target < 0.0) target = 0.0;
        if (target > 1.0) target = 1.0;
        double coefficient = target > smoothed ? 0.48 : 0.14;
        if (mode != lastMode) coefficient = 1.0;
        smoothed += (target - smoothed) * coefficient;
        InterlockedExchange(&g_touchbarLevel, (LONG)(smoothed * 10000.0 + 0.5));

        if (InterlockedCompareExchange(&g_activeMode, 0, 0) < 0 &&
            !InterlockedCompareExchange(&g_shutdownFadeStarted, 0, 0) &&
            (mode != TOUCHBAR_OFF || lastMode != TOUCHBAR_OFF)) {
            unsigned char blank[BUF_SIZE] = {0};
            send_frame(blank);
        }
        lastMode = mode;
        Sleep(33);
    }

    if (gpuQuery) PdhCloseQuery(gpuQuery);
    if (captureMeter) captureMeter->lpVtbl->Release(captureMeter);
    if (renderMeter) renderMeter->lpVtbl->Release(renderMeter);
    if (volume) IAudioEndpointVolume_Release(volume);
    if (renderEndpointId) CoTaskMemFree(renderEndpointId);
    if (enumerator) IMMDeviceEnumerator_Release(enumerator);
    InterlockedExchange(&g_touchbarDisplayMode, TOUCHBAR_OFF);
    if (uninitialize) CoUninitialize();
    log_event("touchbar metrics: worker stopped");
    return 0;
}

void set_status(const wchar_t *text) {
    if (g_hStatus) SetWindowText(g_hStatus, text);
}

static void apply_fn_brightness(ULONG scanCode) {
    LONG current = InterlockedCompareExchange(&g_brightness, 0, 0);
    LONG next = current;
    if (scanCode == FN_SCANCODE_BRIGHTNESS_DOWN) {
        next = current <= 0 ? 0 : ((current - 1) / 25) * 25;
    } else if (scanCode == FN_SCANCODE_BRIGHTNESS_UP) {
        next = current >= 100 ? 100 : ((current / 25) + 1) * 25;
    } else {
        return;
    }

    InterlockedExchange(&g_brightness, next);
    InterlockedExchange(&g_idleLightsOff, 0);
    InterlockedExchange(&g_pacerResetRequested, 1);
    if (g_hSliderBright)
        SendMessage(g_hSliderBright, TBM_SETPOS, TRUE, (LPARAM)next);
    capture_current_power_profile();
    update_power_profile_ui();
    save_config();

    wchar_t status[96];
    wsprintf(status, L"快捷键：键盘亮度 %ld%%", next);
    set_status(status);
    char logText[96];
    sprintf(logText, "Fn hotkey: brightness changed from %ld to %ld",
        (long)current, (long)next);
    log_event(logText);
}

static void update_fps_label(void) {
    if (!g_hLblFps) return;
    wchar_t text[96];
    wsprintf(text, L"RGB 帧率：调度 %ld / 目标 %ld FPS",
        InterlockedCompareExchange(&g_effectiveFps, 0, 0),
        InterlockedCompareExchange(&g_targetFps, 0, 0));
    SetWindowText(g_hLblFps, text);
}

static void stop_current_effect_internal(int animate) {
    if (!g_effectThread) {
        g_activeMode = -1;
        return;
    }

    log_event(animate
        ? "stop_current_effect: stopping active effect with reverse transition"
        : "stop_current_effect: stopping active effect for direct mode switch");
    InterlockedExchange(&g_mainRevealActive, 0);
    InterlockedExchange64(&g_mainRevealStartTick, 0);
    if (animate) {
        InterlockedExchange(&g_startupTransitionActive, 0);
        InterlockedExchange64(&g_startupTransitionStartTick, 0);
    }
    InterlockedExchange(&g_stopFlag, 1);
    WaitForSingleObject(g_effectThread, 2000);
    CloseHandle(g_effectThread);
    g_effectThread = NULL;

    unsigned char blank[BUF_SIZE] = {0};
    if (animate && InterlockedCompareExchange(&g_lastMainRawFrameValid, 0, 0) &&
        InterlockedCompareExchange(&g_idleTransitionState, 0, 0) != IDLE_TRANSITION_ASLEEP) {
        unsigned char base[BUF_SIZE];
        unsigned char frame[BUF_SIZE];
        memcpy(base, g_lastMainRawFrame, BUF_SIZE);
        LONG transition = InterlockedCompareExchange(&g_powerTransitionMode, 0, 0);
        LONG duration = InterlockedCompareExchange(&g_transitionDurationMs, 0, 0);
        if (duration < 1) duration = TRANSITION_DEFAULT_MS;

        if (!idle_timeout_reached()) {
            InterlockedExchange(&g_idleTransitionState, IDLE_TRANSITION_ACTIVE);
            InterlockedExchange64(&g_idleTransitionStartTick, 0);
            InterlockedExchange(&g_idleLightsOff, 0);
            InterlockedExchange(&g_pacerResetRequested, 1);
        }
        if (transition == TRANSITION_OFF) {
            send_frame(blank);
        } else {
            int steps = (int)(duration / 25) + 1;
            if (steps < 12) steps = 12;
            if (steps > 60) steps = 60;
            DWORD delay = steps > 1 ? (DWORD)(duration / (steps - 1)) : 0;
            for (int step = steps - 1; step >= 0; step--) {
                double progress = steps > 1 ? (double)step / (double)(steps - 1) : 0.0;
                apply_power_transition_mask(base, frame, transition, progress);
                if (send_frame(frame) < 0) break;
                if (step > 0 && delay > 0) Sleep(delay);
            }
            log_event("main transition: reverse stop completed");
        }
    } else if (animate) {
        send_frame(blank);
    }
    g_activeMode = -1;
}

void stop_current_effect(void) {
    stop_current_effect_internal(1);
}

static int stop_effect_for_shutdown(void) {
    if (!g_effectThread) {
        g_activeMode = -1;
        return 1;
    }
    InterlockedExchange(&g_stopFlag, 1);
    DWORD wait = WaitForSingleObject(g_effectThread, 1000);
    if (wait != WAIT_OBJECT_0) {
        log_event("shutdown transition: effect thread did not stop in time; skipping transition");
        return 0;
    }
    CloseHandle(g_effectThread);
    g_effectThread = NULL;
    g_activeMode = -1;
    return 1;
}

static void graceful_shutdown_fade(void) {
    if (InterlockedCompareExchange(&g_shutdownFadeStarted, 1, 0) != 0) return;

    LONG mode = InterlockedCompareExchange(&g_activeMode, 0, 0);
    InterlockedExchange(&g_shutdownResumeMode, mode);
    save_config();
    if (!InterlockedCompareExchange(&g_lastRawFrameValid, 0, 0)) {
        log_event("shutdown transition: no active frame; nothing to animate");
        return;
    }

    log_event("shutdown transition: starting reverse reveal to black");
    if (!stop_effect_for_shutdown()) return;

    unsigned char base[BUF_SIZE];
    unsigned char frame[BUF_SIZE];
    memcpy(base, g_lastRawFrame, BUF_SIZE);
    LONG transition = InterlockedCompareExchange(&g_powerTransitionMode, 0, 0);
    LONG duration = InterlockedCompareExchange(&g_transitionDurationMs, 0, 0);
    int alreadyAsleep = InterlockedCompareExchange(&g_idleTransitionState, 0, 0) ==
                       IDLE_TRANSITION_ASLEEP;
    if (transition == TRANSITION_OFF || alreadyAsleep) {
        memset(frame, 0, BUF_SIZE);
        send_frame(frame);
    } else {
        int steps = (int)(duration / 25) + 1;
        if (steps < 12) steps = 12;
        if (steps > 60) steps = 60;
        DWORD delay = steps > 1 ? (DWORD)(duration / (steps - 1)) : 0;
        for (int step = steps - 1; step >= 0; step--) {
            double progress = steps > 1 ? (double)step / (double)(steps - 1) : 0.0;
            apply_power_transition_mask(base, frame, transition, progress);
            if (send_frame(frame) < 0) break;
            if (step > 0 && delay > 0) Sleep(delay);
        }
    }
    log_event("shutdown transition: completed");
}

static void cancel_shutdown_fade(void) {
    LONG mode = InterlockedCompareExchange(&g_shutdownResumeMode, -1, -1);
    if (InterlockedExchange(&g_shutdownFadeStarted, 0) && mode >= 0 && mode < MODE_COUNT) {
        InterlockedExchange(&g_startupTransitionActive, 1);
        InterlockedExchange64(&g_startupTransitionStartTick, 0);
        log_event("shutdown transition: shutdown cancelled, restoring effect");
        start_mode((int)mode);
    }
}

LPTHREAD_START_ROUTINE mode_to_fn(int mode) {
    switch (mode) {
        case MODE_BREATH: return effect_breathing;
        case MODE_WAVE: return effect_wave;
        case MODE_SPARKLE: return effect_sparkle;
        case MODE_REACTIVE: return effect_reactive;
        case MODE_WHEEL: return effect_wheel;
        case MODE_LIGHTNING: return effect_lightning;
        case MODE_FLAME: return effect_flame;
        case MODE_RAIN: return effect_rain;
        case MODE_MATRIX: return effect_matrix;
        case MODE_STATIC: return effect_static;
        case MODE_RIPPLE: return effect_ripple;
        case MODE_RAINBOW: return effect_rainbow;
        case MODE_QUICKSAND: return effect_quicksand;
        case MODE_CURRENT: return effect_current;
        case MODE_TOUCH_CURRENT: return effect_touch_current;
        case MODE_SCREEN_AMBIENT: return effect_screen_ambient;
    }
    return NULL;
}

const wchar_t* mode_to_label(int mode) {
    switch (mode) {
        case MODE_BREATH: return L"当前模式：呼吸";
        case MODE_WAVE: return L"当前模式：波浪";
        case MODE_SPARKLE: return L"当前模式：星光";
        case MODE_REACTIVE: return L"当前模式：按键响应";
        case MODE_WHEEL: return L"当前模式：色轮";
        case MODE_LIGHTNING: return L"当前模式：闪电";
        case MODE_FLAME: return L"当前模式：火焰";
        case MODE_RAIN: return L"当前模式：雨滴";
        case MODE_MATRIX: return L"当前模式：矩阵";
        case MODE_STATIC: return L"当前模式：静态";
        case MODE_RIPPLE: return L"当前模式：触碰涟漪";
        case MODE_RAINBOW: return L"当前模式：全彩渐变";
        case MODE_QUICKSAND: return L"当前模式：流沙";
        case MODE_CURRENT: return L"当前模式：电流";
        case MODE_TOUCH_CURRENT: return L"当前模式：触控电流";
        case MODE_SCREEN_AMBIENT: return L"当前模式：同步辨色";
    }
    return L"当前模式：无";
}

static const wchar_t* mode_to_name(int mode) {
    switch (mode) {
        case MODE_BREATH: return L"呼吸";
        case MODE_WAVE: return L"波浪";
        case MODE_SPARKLE: return L"星光";
        case MODE_REACTIVE: return L"按键响应";
        case MODE_WHEEL: return L"色轮";
        case MODE_LIGHTNING: return L"闪电";
        case MODE_FLAME: return L"火焰";
        case MODE_RAIN: return L"雨滴";
        case MODE_MATRIX: return L"矩阵";
        case MODE_STATIC: return L"静态";
        case MODE_RIPPLE: return L"触碰涟漪";
        case MODE_RAINBOW: return L"全彩渐变";
        case MODE_QUICKSAND: return L"流沙";
        case MODE_CURRENT: return L"电流";
        case MODE_TOUCH_CURRENT: return L"触控电流";
        case MODE_SCREEN_AMBIENT: return L"同步辨色";
    }
    return L"关闭";
}

static void populate_mode_combo(HWND combo, int includeOff) {
    if (includeOff) SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)L"关闭灯光");
    for (int mode=0; mode<MODE_COUNT; mode++)
        SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)mode_to_name(mode));
}

/* 模式切换必须先停止旧线程，再重新进入自定义模式，最后启动唯一的新效果线程。 */
void start_mode(int mode) {
    char logbuf[96];
    int revealFromBlank = InterlockedCompareExchange(&g_activeMode, 0, 0) < 0 &&
        !InterlockedCompareExchange(&g_startupTransitionActive, 0, 0) &&
        InterlockedCompareExchange(&g_idleTransitionState, 0, 0) == IDLE_TRANSITION_ACTIVE;
    sprintf(logbuf, "start_mode: switching to mode %d", mode);
    log_event(logbuf);
    stop_current_effect_internal(0);

    EnterCriticalSection(&g_deviceLock);
    int reinit = g_h ? enter_custom_mode() : -1;
    LeaveCriticalSection(&g_deviceLock);
    sprintf(logbuf, "start_mode: full EC re-init (prep1+prep2) result=%s",
        reinit < 0 ? "FAIL" : "ok");
    log_event(logbuf);
    if (reinit < 0) request_reconnect("start_mode-reinit-failed");

    InterlockedExchange(&g_stopFlag, 0);
    InterlockedExchange(&g_effectiveFps, g_targetFps);
    InterlockedExchange(&g_slowFrameCount, 0);
    InterlockedExchange(&g_healthyFrameCount, 0);
    reset_frame_pacer();
    notify_fps_status();
    g_activeMode = mode;
    if (revealFromBlank) {
        InterlockedExchange(&g_mainRevealActive, 1);
        InterlockedExchange64(&g_mainRevealStartTick, 0);
    }
    capture_current_power_profile();
    update_power_profile_ui();
    g_effectThread = CreateThread(NULL, 0, mode_to_fn(mode), NULL, 0, NULL);
    if (g_effectThread) {
        SetThreadPriority(g_effectThread, THREAD_PRIORITY_ABOVE_NORMAL);
    }
    set_status(mode_to_label(mode));
    save_config();
}

void show_only(HWND *arr, int count, BOOL show) {
    for (int i = 0; i < count; i++) ShowWindow(arr[i], show ? SW_SHOW : SW_HIDE);
}

/* 保持 UI 简洁：只显示当前灯效对应的参数组，其他组连同说明一起隐藏。 */
void show_panel_for_mode(int mode) {
    int visibleMode = g_uiPage == UI_PAGE_EFFECTS ? mode : -1;
    show_only(g_panelBreath, g_panelBreathCount, visibleMode == MODE_BREATH);
    show_only(g_panelWave, g_panelWaveCount, visibleMode == MODE_WAVE);
    show_only(g_panelSparkle, g_panelSparkleCount, visibleMode == MODE_SPARKLE);
    show_only(g_panelReactive, g_panelReactiveCount, visibleMode == MODE_REACTIVE);
    show_only(g_panelWheel, g_panelWheelCount, visibleMode == MODE_WHEEL);
    show_only(g_panelLightning, g_panelLightningCount, visibleMode == MODE_LIGHTNING);
    show_only(g_panelFlame, g_panelFlameCount, visibleMode == MODE_FLAME);
    show_only(g_panelRain, g_panelRainCount, visibleMode == MODE_RAIN);
    show_only(g_panelMatrix, g_panelMatrixCount, visibleMode == MODE_MATRIX);
    show_only(g_panelStatic, g_panelStaticCount, visibleMode == MODE_STATIC);
    show_only(g_panelRipple, g_panelRippleCount, visibleMode == MODE_RIPPLE);
    show_only(g_panelRainbow, g_panelRainbowCount, visibleMode == MODE_RAINBOW);
    show_only(g_panelQuicksand, g_panelQuicksandCount, visibleMode == MODE_QUICKSAND);
    show_only(g_panelCurrent, g_panelCurrentCount, visibleMode == MODE_CURRENT);
    show_only(g_panelTouchCurrent, g_panelTouchCurrentCount, visibleMode == MODE_TOUCH_CURRENT);
    show_only(g_panelScreenAmbient, g_panelScreenAmbientCount, visibleMode == MODE_SCREEN_AMBIENT);
    HWND top = GetAncestor(g_panelBreath[0], GA_ROOT);
    if (top) { InvalidateRect(top, NULL, TRUE); UpdateWindow(top); }
}

static void add_page_control(HWND *controls, int *count, HWND control) {
    if (control && *count < MAX_UI_PAGE_CTRLS) controls[(*count)++] = control;
}

static int transition_family_from_mode(LONG mode) {
    if (mode <= TRANSITION_OFF) return 0;
    if (mode == TRANSITION_FADE) return 1;
    if (mode >= TRANSITION_RIPPLE_CENTER && mode <= TRANSITION_RIPPLE_RIGHT) return 2;
    return 3;
}

static int transition_detail_from_mode(LONG mode) {
    int family = transition_family_from_mode(mode);
    if (family == 2) return (int)(mode - TRANSITION_RIPPLE_CENTER);
    if (family == 3) return (int)(mode - TRANSITION_SNAKE_MAIN_OUT_IN);
    return 0;
}

static LONG transition_mode_from_ui(int family, int detail) {
    if (family <= 0) return TRANSITION_OFF;
    if (family == 1) return TRANSITION_FADE;
    if (family == 2)
        return TRANSITION_RIPPLE_CENTER + cfg_clamp(detail, 0, 5);
    return TRANSITION_SNAKE_MAIN_OUT_IN + cfg_clamp(detail, 0, 3);
}

static void update_transition_duration_label(void) {
    if (!g_hLblTransitionDuration) return;
    LONG duration = InterlockedCompareExchange(&g_transitionDurationMs, 0, 0);
    wchar_t text[64];
    wsprintf(text, L"过渡时长：%ld.%01ld 秒", duration / 1000, (duration % 1000) / 100);
    SetWindowText(g_hLblTransitionDuration, text);
}

static void update_transition_detail_ui(void) {
    if (!g_hComboTransitionFamily || !g_hComboTransitionDetail ||
        !g_hLblTransitionDetail) return;
    LONG mode = InterlockedCompareExchange(&g_powerTransitionMode, 0, 0);
    int family = transition_family_from_mode(mode);
    int detail = transition_detail_from_mode(mode);
    SendMessage(g_hComboTransitionFamily, CB_SETCURSEL, family, 0);
    SendMessage(g_hComboTransitionDetail, CB_RESETCONTENT, 0, 0);

    if (family == 2) {
        const wchar_t *labels[] = {
            L"中心：J / K 向外", L"四周边缘向内", L"上到下",
            L"下到上", L"左到右", L"右到左"
        };
        SetWindowText(g_hLblTransitionDetail, L"涟漪方向：");
        for (int i = 0; i < 6; i++)
            SendMessage(g_hComboTransitionDetail, CB_ADDSTRING, 0, (LPARAM)labels[i]);
    } else if (family == 3) {
        const wchar_t *labels[] = {
            L"主对角双蛇：外向内", L"主对角双蛇：内向外",
            L"副对角双蛇：外向内", L"副对角双蛇：内向外"
        };
        SetWindowText(g_hLblTransitionDetail, L"螺旋路径：");
        for (int i = 0; i < 4; i++)
            SendMessage(g_hComboTransitionDetail, CB_ADDSTRING, 0, (LPARAM)labels[i]);
    }
    SendMessage(g_hComboTransitionDetail, CB_SETCURSEL, detail, 0);
    BOOL showDetail = g_uiPage == UI_PAGE_PROGRAM && (family == 2 || family == 3);
    ShowWindow(g_hLblTransitionDetail, showDetail ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hComboTransitionDetail, showDetail ? SW_SHOW : SW_HIDE);
    update_transition_duration_label();
}

static const wchar_t *touchbar_mode_hint(LONG mode) {
    if (mode == TOUCHBAR_OFF)
        return L"当前已关闭，不显示额外参数。";
    if (mode == TOUCHBAR_AUDIO)
        return L"音频波形只使用下方三色 RGB。调音量时会短暂切换为音量。";
    if (mode == TOUCHBAR_FPS)
        return L"仅在此模式启动 FrameView SDK，按前台应用统计真实帧率；范围 15-500。\n调音量时会短暂切换为音量。";
    return L"颜色 A/B 表示数值从低到高的渐变，调音量时会临时显示音量。";
}

/* Touch Bar 同理按“关闭/指标/音频”类型动态显示所需参数。 */
static void refresh_touchbar_option_visibility(void) {
    BOOL pageVisible = g_uiPage == UI_PAGE_TOUCHBAR;
    LONG acMode = g_acTouchbarMode;
    LONG batteryMode = g_batteryTouchbarMode;
    BOOL acEnabled = pageVisible && acMode != TOUCHBAR_OFF;
    BOOL batteryEnabled = pageVisible && batteryMode != TOUCHBAR_OFF;
    BOOL acAudio = acEnabled && acMode == TOUCHBAR_AUDIO;
    BOOL batteryAudio = batteryEnabled && batteryMode == TOUCHBAR_AUDIO;
    BOOL acMetric = acEnabled && !acAudio;
    BOOL batteryMetric = batteryEnabled && !batteryAudio;

    ShowWindow(g_hAcTouchbarDirectionLabel, acEnabled ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hComboAcTouchbarDirection, acEnabled ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBatteryTouchbarDirectionLabel, batteryEnabled ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hComboBatteryTouchbarDirection, batteryEnabled ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hAcTouchbarMetricLabel, acMetric ? SW_SHOW : SW_HIDE);
    ShowWindow(g_acTouchbarColorBtnStart, acMetric ? SW_SHOW : SW_HIDE);
    ShowWindow(g_acTouchbarColorBtnEnd, acMetric ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBatteryTouchbarMetricLabel, batteryMetric ? SW_SHOW : SW_HIDE);
    ShowWindow(g_batteryTouchbarColorBtnStart, batteryMetric ? SW_SHOW : SW_HIDE);
    ShowWindow(g_batteryTouchbarColorBtnEnd, batteryMetric ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hAcTouchbarAudioLabel, acAudio ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBatteryTouchbarAudioLabel, batteryAudio ? SW_SHOW : SW_HIDE);
    for (int i = 0; i < 3; i++) {
        ShowWindow(g_acTouchbarAudioColorBtn[i], acAudio ? SW_SHOW : SW_HIDE);
        ShowWindow(g_batteryTouchbarAudioColorBtn[i], batteryAudio ? SW_SHOW : SW_HIDE);
    }
    BOOL acFps = acMetric && acMode == TOUCHBAR_FPS;
    BOOL batteryFps = batteryMetric && batteryMode == TOUCHBAR_FPS;
    ShowWindow(g_hAcTouchbarFpsLabel, acFps ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hEditAcTouchbarFpsMax, acFps ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBatteryTouchbarFpsLabel, batteryFps ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hEditBatteryTouchbarFpsMax, batteryFps ? SW_SHOW : SW_HIDE);
    if (g_hAcTouchbarHint) {
        SetWindowText(g_hAcTouchbarHint, touchbar_mode_hint(acMode));
        ShowWindow(g_hAcTouchbarHint, pageVisible ? SW_SHOW : SW_HIDE);
    }
    if (g_hBatteryTouchbarHint) {
        SetWindowText(g_hBatteryTouchbarHint, touchbar_mode_hint(batteryMode));
        ShowWindow(g_hBatteryTouchbarHint, pageVisible ? SW_SHOW : SW_HIDE);
    }
}

static void show_ui_page(int page) {
    g_uiPage = page;
    show_only(g_pageEffects, g_pageEffectsCount, page == UI_PAGE_EFFECTS);
    show_only(g_pageAc, g_pageAcCount, page == UI_PAGE_AC);
    show_only(g_pageBattery, g_pageBatteryCount, page == UI_PAGE_BATTERY);
    show_only(g_pageTouchbar, g_pageTouchbarCount, page == UI_PAGE_TOUCHBAR);
    show_only(g_pageProgram, g_pageProgramCount, page == UI_PAGE_PROGRAM);
    show_only(g_pagePowerShared, g_pagePowerSharedCount,
        page == UI_PAGE_AC || page == UI_PAGE_BATTERY);
    if (page == UI_PAGE_EFFECTS) {
        int mode = g_hComboMode ? (int)SendMessage(g_hComboMode, CB_GETCURSEL, 0, 0) : -1;
        show_panel_for_mode(mode);
    } else {
        show_panel_for_mode(-1);
    }
    update_transition_detail_ui();
    refresh_touchbar_option_visibility();
    if (g_hMainWindow) InvalidateRect(g_hMainWindow, NULL, TRUE);
    for (int i = 0; i < 5; i++)
        if (g_hPageButton[i]) InvalidateRect(g_hPageButton[i], NULL, TRUE);
}

static void update_power_profile_ui(void) {
    if (g_hComboAcMode)
        SendMessage(g_hComboAcMode, CB_SETCURSEL, (WPARAM)(g_acMode + 1), 0);
    if (g_hComboBatteryMode)
        SendMessage(g_hComboBatteryMode, CB_SETCURSEL, (WPARAM)(g_batteryMode + 1), 0);
    if (g_hSliderAcBright) SendMessage(g_hSliderAcBright, TBM_SETPOS, TRUE, g_acBrightness);
    if (g_hSliderAcFps) SendMessage(g_hSliderAcFps, TBM_SETPOS, TRUE, g_acFps);
    if (g_hSliderBatteryBright) SendMessage(g_hSliderBatteryBright, TBM_SETPOS, TRUE, g_batteryBrightness);
    if (g_hSliderBatteryFps) SendMessage(g_hSliderBatteryFps, TBM_SETPOS, TRUE, g_batteryFps);
    if (g_hLblAcFps) {
        wchar_t text[64]; wsprintf(text, L"帧率：%ld", g_acFps); SetWindowText(g_hLblAcFps, text);
    }
    if (g_hLblBatteryFps) {
        wchar_t text[64]; wsprintf(text, L"帧率：%ld", g_batteryFps); SetWindowText(g_hLblBatteryFps, text);
    }
    if (g_hPowerStatus) {
        wchar_t text[160];
        wsprintf(text, L"当前电源：%ls　自动切换：%ls",
            g_onAcPower ? L"已插电" : L"使用电池",
            g_autoPowerProfiles ? L"开启" : L"关闭");
        SetWindowText(g_hPowerStatus, text);
    }
    if (g_hAutoPowerCheck)
        SendMessage(g_hAutoPowerCheck, BM_SETCHECK,
            g_autoPowerProfiles ? BST_CHECKED : BST_UNCHECKED, 0);
    if (g_hComboAcTouchbarMode)
        SendMessage(g_hComboAcTouchbarMode, CB_SETCURSEL, (WPARAM)g_acTouchbarMode, 0);
    if (g_hComboBatteryTouchbarMode)
        SendMessage(g_hComboBatteryTouchbarMode, CB_SETCURSEL, (WPARAM)g_batteryTouchbarMode, 0);
    if (g_hComboAcTouchbarDirection)
        SendMessage(g_hComboAcTouchbarDirection, CB_SETCURSEL, (WPARAM)g_acTouchbarDirection, 0);
    if (g_hComboBatteryTouchbarDirection)
        SendMessage(g_hComboBatteryTouchbarDirection, CB_SETCURSEL, (WPARAM)g_batteryTouchbarDirection, 0);
    if (g_hEditAcTouchbarFpsMax) {
        wchar_t text[24];
        wsprintf(text, L"%ld", g_acTouchbarFpsMax);
        SetWindowText(g_hEditAcTouchbarFpsMax, text);
    }
    if (g_hEditBatteryTouchbarFpsMax) {
        wchar_t text[24];
        wsprintf(text, L"%ld", g_batteryTouchbarFpsMax);
        SetWindowText(g_hEditBatteryTouchbarFpsMax, text);
    }
    if (g_acTouchbarColorBtnStart) InvalidateRect(g_acTouchbarColorBtnStart, NULL, TRUE);
    if (g_acTouchbarColorBtnEnd) InvalidateRect(g_acTouchbarColorBtnEnd, NULL, TRUE);
    if (g_batteryTouchbarColorBtnStart) InvalidateRect(g_batteryTouchbarColorBtnStart, NULL, TRUE);
    if (g_batteryTouchbarColorBtnEnd) InvalidateRect(g_batteryTouchbarColorBtnEnd, NULL, TRUE);
    for (int i = 0; i < 3; i++) {
        if (g_acTouchbarAudioColorBtn[i]) InvalidateRect(g_acTouchbarAudioColorBtn[i], NULL, TRUE);
        if (g_batteryTouchbarAudioColorBtn[i]) InvalidateRect(g_batteryTouchbarAudioColorBtn[i], NULL, TRUE);
    }
    refresh_touchbar_option_visibility();
}

static void apply_power_profile(int onAc, int force) {
    int oldOnAc = (int)InterlockedCompareExchange(&g_onAcPower, 0, 0);
    if (!force && oldOnAc == onAc) {
        update_power_profile_ui();
        return;
    }
    if (oldOnAc != onAc) capture_current_power_profile();
    InterlockedExchange(&g_onAcPower, onAc ? 1 : 0);
    InterlockedExchange(&g_touchbarMode, onAc ? g_acTouchbarMode : g_batteryTouchbarMode);
    InterlockedExchange(&g_touchbarDirection, onAc ? g_acTouchbarDirection : g_batteryTouchbarDirection);
    g_touchbarColorStart = onAc ? g_acTouchbarColorStart : g_batteryTouchbarColorStart;
    g_touchbarColorEnd = onAc ? g_acTouchbarColorEnd : g_batteryTouchbarColorEnd;
    for (int i = 0; i < 3; i++)
        g_touchbarAudioColors[i] = onAc ? g_acTouchbarAudioColors[i] : g_batteryTouchbarAudioColors[i];
    if (!g_autoPowerProfiles) {
        update_power_profile_ui();
        return;
    }

    LONG mode = onAc ? g_acMode : g_batteryMode;
    LONG brightness = onAc ? g_acBrightness : g_batteryBrightness;
    LONG fps = onAc ? g_acFps : g_batteryFps;
    InterlockedExchange(&g_brightness, brightness);
    InterlockedExchange(&g_targetFps, fps);
    set_effective_fps(fps, "power profile applied");
    if (g_hSliderBright) SendMessage(g_hSliderBright, TBM_SETPOS, TRUE, brightness);
    if (g_hSliderFps) SendMessage(g_hSliderFps, TBM_SETPOS, TRUE, fps);
    update_fps_label();
    update_power_profile_ui();
    if (g_hComboMode) SendMessage(g_hComboMode, CB_SETCURSEL, mode, 0);
    show_panel_for_mode((int)mode);
    if (mode >= 0 && mode < MODE_COUNT) {
        start_mode((int)mode);
    } else {
        stop_current_effect();
        capture_current_power_profile();
        set_status(L"当前模式：无");
        save_config();
    }
}

static void poll_power_profile(void) {
    int nowOnAc = query_ac_power();
    if (nowOnAc != (int)InterlockedCompareExchange(&g_onAcPower, 0, 0)) {
        log_event(nowOnAc ? "power profile: switched to AC" : "power profile: switched to battery");
        apply_power_profile(nowOnAc, 0);
    }
}

LRESULT CALLBACK SwatchProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        COLORREF col = (COLORREF)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        HBRUSH br = CreateSolidBrush(col);
        FillRect(hdc, &rc, br);
        DeleteObject(br);
        FrameRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

void set_swatch_color(HWND hwnd, COLORREF col) {
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)col);
    InvalidateRect(hwnd, NULL, TRUE);
}

static void update_white_calibration_ui(void) {
    if (g_hWhiteCalibrationSwatch)
        set_swatch_color(g_hWhiteCalibrationSwatch, g_observedWhiteColor);
    if (!g_hLblWhiteCalibration) return;
    wchar_t text[180];
    wsprintf(text,
        L"实测 #%02X%02X%02X　校正增益 R %ld%% / G %ld%% / B %ld%%",
        GetRValue(g_observedWhiteColor), GetGValue(g_observedWhiteColor),
        GetBValue(g_observedWhiteColor),
        g_whiteGainR / 10, g_whiteGainG / 10, g_whiteGainB / 10);
    SetWindowText(g_hLblWhiteCalibration, text);
}

static int choose_observed_white_color(HWND hwnd, COLORREF initial, COLORREF *selected) {
    static COLORREF calibrationCustom[16] = {0};
    CHOOSECOLOR cc = {0};
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = hwnd;
    cc.rgbResult = initial;
    cc.lpCustColors = calibrationCustom;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (!ChooseColor(&cc)) return 0;
    *selected = cc.rgbResult;
    return 1;
}

static void run_white_spectrum_calibration(HWND hwnd) {
    InterlockedExchange(&g_idleTransitionState, IDLE_TRANSITION_ACTIVE);
    InterlockedExchange64(&g_idleTransitionStartTick, 0);
    InterlockedExchange(&g_idleLightsOff, 0);
    InterlockedExchange(&g_whiteCalibrationActive, 1);
    Sleep(150);
    MessageBox(hwnd,
        L"键盘现在显示未经校正的纯白。\n\n"
        L"下一步请在完整光谱中选择你肉眼看到的键盘实际颜色，"
        L"不要选择你希望得到的白色。",
        L"白色光谱校准", MB_OK | MB_ICONINFORMATION);

    COLORREF observed = g_observedWhiteColor;
    int accepted = choose_observed_white_color(hwnd, observed, &observed);
    InterlockedExchange(&g_whiteCalibrationActive, 0);
    InterlockedExchange(&g_pacerResetRequested, 1);
    if (!accepted) {
        log_event("white calibration: cancelled by user");
        return;
    }

    LONG gainR, gainG, gainB;
    if (!calculate_white_calibration(observed, &gainR, &gainG, &gainB)) {
        MessageBox(hwnd,
            L"所选颜色过暗或某个通道接近为零，无法稳定计算白平衡。\n"
            L"请重新校准，并在光谱中选择更接近实际灯光亮度的颜色。",
            L"无法完成校准", MB_OK | MB_ICONWARNING);
        return;
    }

    g_observedWhiteColor = observed;
    InterlockedExchange(&g_whiteGainR, gainR);
    InterlockedExchange(&g_whiteGainG, gainG);
    InterlockedExchange(&g_whiteGainB, gainB);
    update_white_calibration_ui();
    save_config();
    char logText[180];
    sprintf(logText,
        "white calibration: observed=#%02X%02X%02X gains=R%ld G%ld B%ld",
        GetRValue(observed), GetGValue(observed), GetBValue(observed),
        gainR, gainG, gainB);
    log_event(logText);
}

static void reset_white_spectrum_calibration(void) {
    InterlockedExchange(&g_whiteCalibrationActive, 0);
    InterlockedExchange(&g_whiteGainR, 1000);
    InterlockedExchange(&g_whiteGainG, 1000);
    InterlockedExchange(&g_whiteGainB, 1000);
    g_observedWhiteColor = RGB(255,255,255);
    update_white_calibration_ui();
    save_config();
    log_event("white calibration: reset to neutral gains");
}

COLORREF pick_color(HWND hwnd, COLORREF initial) {
    static COLORREF custom[16] = {0};
    CHOOSECOLOR cc = {0};
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = hwnd;
    cc.rgbResult = initial;
    cc.lpCustColors = custom;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (ChooseColor(&cc)) return cc.rgbResult;
    return initial;
}

void draw_button_ex(LPDRAWITEMSTRUCT dis, BOOL useCustomColor, COLORREF customColor) {
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    wchar_t text[64];
    GetWindowText(dis->hwndItem, text, 64);

    BOOL pressed = (dis->itemState & ODS_SELECTED) != 0;
    COLORREF bg = useCustomColor ? customColor : (pressed ? CLR_PANEL_HOV : CLR_PANEL);

    HBRUSH br = CreateSolidBrush(bg);
    HPEN pen = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HGDIOBJ oldBr = SelectObject(hdc, br);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 10, 10);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(br);
    DeleteObject(pen);

    if (!useCustomColor) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, CLR_TEXT);
        DrawText(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

void draw_selector_button(LPDRAWITEMSTRUCT dis, BOOL isActive) {
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    wchar_t text[64];
    GetWindowText(dis->hwndItem, text, 64);

    COLORREF bg = isActive ? CLR_ACCENT : CLR_PANEL;
    COLORREF border = isActive ? CLR_ACCENT_LT : CLR_BORDER;

    HBRUSH br = CreateSolidBrush(bg);
    HPEN pen = CreatePen(PS_SOLID, isActive ? 2 : 1, border);
    HGDIOBJ oldBr = SelectObject(hdc, br);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(br);
    DeleteObject(pen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, CLR_TEXT);
    DrawText(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void draw_button(LPDRAWITEMSTRUCT dis) {
    draw_button_ex(dis, FALSE, 0);
}

void draw_labeled_color_button(LPDRAWITEMSTRUCT dis, COLORREF color) {
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    wchar_t text[64];
    GetWindowText(dis->hwndItem, text, 64);

    HBRUSH br = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HGDIOBJ oldBr = SelectObject(hdc, br);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 10, 10);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(br);
    DeleteObject(pen);

    double lum = (0.299*GetRValue(color) + 0.587*GetGValue(color) + 0.114*GetBValue(color));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, (lum > 140) ? RGB(0,0,0) : RGB(255,255,255));
    DrawText(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

BOOL CALLBACK SetFontOnChild(HWND hwnd, LPARAM lFont) {
    SendMessage(hwnd, WM_SETFONT, (WPARAM)lFont, TRUE);
    return TRUE;
}

HWND mk_label(HWND parent, const wchar_t *text, int x, int y, int w, int h) {
    return CreateWindow(L"STATIC", text, WS_CHILD|WS_VISIBLE, x,y,w,h, parent, NULL, NULL, NULL);
}

HWND mk_slider(HWND parent, int id, int x, int y, int w, int h, int lo, int hi, int pos) {
    HWND s = CreateWindow(TRACKBAR_CLASS, L"", WS_CHILD|WS_VISIBLE|TBS_AUTOTICKS, x,y,w,h, parent, (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessage(s, TBM_SETRANGE, TRUE, MAKELONG(lo,hi));
    SendMessage(s, TBM_SETPOS, TRUE, pos);
    return s;
}

HWND mk_button(HWND parent, const wchar_t *text, int id, int x, int y, int w, int h) {
    return CreateWindow(L"BUTTON", text, WS_CHILD|WS_VISIBLE|BS_OWNERDRAW, x,y,w,h, parent, (HMENU)(INT_PTR)id, NULL, NULL);
}

static void commit_touchbar_fps_limit(HWND edit, volatile LONG *storage) {
    wchar_t text[24];
    GetWindowText(edit, text, (int)(sizeof(text) / sizeof(text[0])));
    wchar_t *end = NULL;
    long value = wcstol(text, &end, 10);
    if (end == text) value = InterlockedCompareExchange(storage, 0, 0);
    LONG clamped = cfg_clamp(value, 15, 500);
    InterlockedExchange(storage, clamped);
    wsprintf(text, L"%ld", clamped);
    SetWindowText(edit, text);
}

void refresh_rain_color_visibility(void) {
    for (int i=0;i<5;i++) ShowWindow(g_rainColorBtn[i], (i < (int)g_rainColorCount) ? SW_SHOW : SW_HIDE);
    HWND top = GetAncestor(g_rainColorBtn[0], GA_ROOT);
    if (top) { InvalidateRect(top, NULL, TRUE); UpdateWindow(top); }
}
void refresh_matrix_color_visibility(void) {
    for (int i=0;i<3;i++) ShowWindow(g_matrixColorBtn[i], (i < (int)g_matrixColorCount) ? SW_SHOW : SW_HIDE);
    HWND top = GetAncestor(g_matrixColorBtn[0], GA_ROOT);
    if (top) { InvalidateRect(top, NULL, TRUE); UpdateWindow(top); }
}
void invalidate_group(HWND *arr, int count) {
    for (int i=0;i<count;i++) InvalidateRect(arr[i], NULL, TRUE);
}

void refresh_static_ui(void) {
    int layout = (int)g_staticLayout;
    int maxZones = (layout == 0) ? 6 : 10;
    int zc = (int)g_staticZoneCount;
    if (zc > maxZones) zc = maxZones;
    if (zc < 1) zc = 1;
    g_staticZoneCount = zc;

    SendMessage(g_hSliderStaticZones, TBM_SETRANGE, TRUE, MAKELONG(1, maxZones));
    SendMessage(g_hSliderStaticZones, TBM_SETPOS, TRUE, zc);

    SendMessage(g_hComboStaticZoneSel, CB_RESETCONTENT, 0, 0);
    for (int i=0;i<zc;i++) {
        wchar_t buf[16];
        wsprintf(buf, L"颜色 %d", i+1);
        SendMessage(g_hComboStaticZoneSel, CB_ADDSTRING, 0, (LPARAM)buf);
    }
    if (g_staticActiveZone >= zc) g_staticActiveZone = 0;
    SendMessage(g_hComboStaticZoneSel, CB_SETCURSEL, g_staticActiveZone, 0);
    ShowWindow(g_hComboStaticZoneSel, (zc > 1) ? SW_SHOW : SW_HIDE);

    InvalidateRect(g_btnStaticColor, NULL, TRUE);
    InvalidateRect(g_btnStaticHoriz, NULL, TRUE);
    InvalidateRect(g_btnStaticVert, NULL, TRUE);
    HWND top = GetAncestor(g_btnStaticColor, GA_ROOT);
    if (top) { InvalidateRect(top, NULL, TRUE); UpdateWindow(top); }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (g_msgTaskbarCreated && msg == g_msgTaskbarCreated) {
        if (Shell_NotifyIcon(NIM_ADD, &g_nid)) {
            KillTimer(hwnd, IDT_TRAY_RETRY);
            log_event("tray icon re-added after TaskbarCreated broadcast");
        }
        return 0;
    }
    if (g_msgShowInstance && msg == g_msgShowInstance) {
        ShowWindow(hwnd, SW_SHOW);
        ShowWindow(hwnd, SW_RESTORE);
        SetForegroundWindow(hwnd);
        log_event("second launch detected, window restored");
        return 0;
    }
    switch (msg) {
        case WM_CREATE: {
            g_hMainWindow = hwnd;
            HICON hAppIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_APPICON));
            if (!hAppIcon) hAppIcon = LoadIcon(NULL, IDI_APPLICATION);

            memset(&g_nid, 0, sizeof(g_nid));
            g_nid.cbSize = sizeof(g_nid);
            g_nid.hWnd = hwnd;
            g_nid.uID = 1;
            g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
            g_nid.uCallbackMessage = WM_TRAYICON;
            g_nid.hIcon = hAppIcon;
            wcscpy(g_nid.szTip, L"Better RGB 中文增强版");
            g_msgTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
            ChangeWindowMessageFilterEx(hwnd, g_msgTaskbarCreated, MSGFLT_ALLOW, NULL);
            g_msgShowInstance = RegisterWindowMessageW(L"BetterRGB_TheYamo_Show");
            ChangeWindowMessageFilterEx(hwnd, g_msgShowInstance, MSGFLT_ALLOW, NULL);
            if (!Shell_NotifyIcon(NIM_ADD, &g_nid)) {
                log_event("tray icon add failed (taskbar not ready yet), scheduling retries");
                g_trayRetries = 0;
                SetTimer(hwnd, IDT_TRAY_RETRY, 2000, NULL);
            }

            HFONT hFont = CreateFont(16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
                DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");

            const wchar_t *tabNames[] = {L"灯效", L"插电方案", L"电池方案", L"顶行 Bar", L"程序设置"};
            for (int i = 0; i < 5; i++)
                g_hPageButton[i] = mk_button(hwnd, tabNames[i], IDC_PAGE_EFFECTS+i,
                    20+i*140, 14, 132, 34);

            add_page_control(g_pageEffects, &g_pageEffectsCount,
                mk_label(hwnd, L"选择灯效：", 20, 66, 100, 20));
            g_hComboMode = CreateWindow(L"COMBOBOX", L"", WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,
                120, 63, 240, 260, hwnd, (HMENU)IDC_COMBO_MODE, NULL, NULL);
            add_page_control(g_pageEffects, &g_pageEffectsCount, g_hComboMode);
            populate_mode_combo(g_hComboMode, 0);

            add_page_control(g_pageEffects, &g_pageEffectsCount,
                mk_button(hwnd, L"停止主灯效（顶行信息可继续）", IDC_BTN_STOP, 390, 62, 310, 35));

            g_panelBreath[g_panelBreathCount++] = mk_label(hwnd, L"呼吸颜色：", 20, 105, 150, 20);
            g_hSwatchBreath = CreateWindow(L"SwatchClass", L"", WS_CHILD|WS_BORDER, 20,128,50,28, hwnd, (HMENU)(INT_PTR)999, NULL, NULL);
            g_panelBreath[g_panelBreathCount++] = g_hSwatchBreath;
            g_panelBreath[g_panelBreathCount++] = mk_button(hwnd, L"选择颜色", IDC_BTN_COLOR_BREATH, 80, 128, 120, 28);
            g_panelBreath[g_panelBreathCount++] = mk_label(hwnd, L"速度：", 20, 165, 150, 20);
            g_panelBreath[g_panelBreathCount++] = mk_slider(hwnd, IDC_SLIDER_BSPEED, 20, 185, 340, 30, 1, 40, g_breathSpeed);
            g_panelBreath[g_panelBreathCount++] = mk_label(hwnd, L"平滑度：", 20, 220, 150, 20);
            g_panelBreath[g_panelBreathCount++] = mk_slider(hwnd, IDC_SLIDER_BSMOOTH, 20, 240, 340, 30, 2, 150, g_breathSmooth);
            HWND breathRandom = CreateWindow(L"BUTTON", L"随机多色（每次呼吸换色）", WS_CHILD|BS_AUTOCHECKBOX,
                20, 280, 300, 24, hwnd, (HMENU)IDC_CHK_BREATH_RANDOM, NULL, NULL);
            g_panelBreath[g_panelBreathCount++] = breathRandom;
            CheckDlgButton(hwnd, IDC_CHK_BREATH_RANDOM, g_breathRandomMode ? BST_CHECKED : BST_UNCHECKED);

            g_hLblAngle = mk_label(hwnd, L"角度：0 度", 20, 105, 300, 20);
            g_panelWave[g_panelWaveCount++] = g_hLblAngle;
            HWND sAngle = mk_slider(hwnd, IDC_SLIDER_ANGLE, 20, 125, 340, 30, 0, 359, g_waveAngle);
            SendMessage(sAngle, TBM_SETTICFREQ, 45, 0);
            SetWindowLongPtr(sAngle, GWL_STYLE, GetWindowLongPtr(sAngle, GWL_STYLE) | TBS_TOOLTIPS);
            SendMessage(sAngle, TBM_SETTIPSIDE, TBTS_TOP, 0);
            g_panelWave[g_panelWaveCount++] = sAngle;
            g_panelWave[g_panelWaveCount++] = mk_label(hwnd, L"速度：", 20, 165, 150, 20);
            g_panelWave[g_panelWaveCount++] = mk_slider(hwnd, IDC_SLIDER_WSPEED, 20, 185, 340, 30, 1, 20, g_waveSpeed);

            g_panelSparkle[g_panelSparkleCount++] = mk_label(hwnd, L"速度：", 20, 105, 150, 20);
            g_panelSparkle[g_panelSparkleCount++] = mk_slider(hwnd, IDC_SLIDER_SPSPEED, 20, 125, 340, 30, 1, 10, g_sparkleSpeed);
            g_panelSparkle[g_panelSparkleCount++] = mk_label(hwnd, L"密度：", 20, 165, 150, 20);
            g_panelSparkle[g_panelSparkleCount++] = mk_slider(hwnd, IDC_SLIDER_SPDENS, 20, 185, 340, 30, 3, 20, g_sparkleDensity);

            g_panelReactive[g_panelReactiveCount++] = mk_label(hwnd, L"响应颜色：", 20, 105, 200, 20);
            g_hSwatchReact = CreateWindow(L"SwatchClass", L"", WS_CHILD|WS_BORDER, 20,128,50,28, hwnd, (HMENU)(INT_PTR)998, NULL, NULL);
            g_panelReactive[g_panelReactiveCount++] = g_hSwatchReact;
            g_panelReactive[g_panelReactiveCount++] = mk_button(hwnd, L"选择颜色", IDC_BTN_COLOR_REACT, 80, 128, 120, 28);
            g_panelReactive[g_panelReactiveCount++] = mk_button(hwnd, L"随机", IDC_BTN_REACT_RANDOM, 210, 128, 90, 28);
            g_panelReactive[g_panelReactiveCount++] = mk_label(hwnd, L"持续时间：", 20, 168, 150, 20);
            HWND rShort = CreateWindow(L"BUTTON", L"短", WS_CHILD|BS_AUTORADIOBUTTON|WS_GROUP,
                20, 190, 100, 24, hwnd, (HMENU)IDC_RADIO_SHORT, NULL, NULL);
            HWND rMedium = CreateWindow(L"BUTTON", L"中", WS_CHILD|BS_AUTORADIOBUTTON,
                130, 190, 100, 24, hwnd, (HMENU)IDC_RADIO_MEDIUM, NULL, NULL);
            HWND rLong = CreateWindow(L"BUTTON", L"长", WS_CHILD|BS_AUTORADIOBUTTON,
                240, 190, 100, 24, hwnd, (HMENU)IDC_RADIO_LONG, NULL, NULL);
            SendMessage(rMedium, BM_SETCHECK, BST_CHECKED, 0);
            g_panelReactive[g_panelReactiveCount++] = rShort;
            g_panelReactive[g_panelReactiveCount++] = rMedium;
            g_panelReactive[g_panelReactiveCount++] = rLong;

            HWND chkRev = CreateWindow(L"BUTTON", L"反向旋转", WS_CHILD|BS_AUTOCHECKBOX,
                20, 105, 200, 20, hwnd, (HMENU)IDC_CHK_WHEEL_REVERSE, NULL, NULL);
            g_panelWheel[g_panelWheelCount++] = chkRev;
            g_panelWheel[g_panelWheelCount++] = mk_label(hwnd, L"速度：", 20, 135, 150, 20);
            g_panelWheel[g_panelWheelCount++] = mk_slider(hwnd, IDC_SLIDER_WHSPEED, 20, 155, 340, 30, 1, 20, g_wheelSpeed);

            g_panelLightning[g_panelLightningCount++] = mk_label(hwnd, L"速度：", 20, 105, 200, 20);
            g_panelLightning[g_panelLightningCount++] = mk_slider(hwnd, IDC_SLIDER_LTSPEED, 20, 125, 340, 30, 1, 10, g_lightningSpeed);
            g_panelLightning[g_panelLightningCount++] = mk_label(hwnd, L"平滑度：", 20, 165, 200, 20);
            g_panelLightning[g_panelLightningCount++] = mk_slider(hwnd, IDC_SLIDER_LTSMOOTH, 20, 185, 340, 30, 1, 20, g_lightningSmooth);
            g_panelLightning[g_panelLightningCount++] = mk_label(hwnd, L"闪电宽度：", 20, 225, 200, 20);
            g_panelLightning[g_panelLightningCount++] = mk_slider(hwnd, IDC_SLIDER_LTWIDTH, 20, 245, 340, 30, 1, 4, g_lightningWidth);
            g_hLblConcurrent = mk_label(hwnd, L"同时出现数量：1", 20, 285, 250, 20);
            g_panelLightning[g_panelLightningCount++] = g_hLblConcurrent;
            g_panelLightning[g_panelLightningCount++] = mk_slider(hwnd, IDC_SLIDER_LTCONCUR, 20, 305, 340, 30, 1, 4, g_lightningConcurrent);

            g_panelFlame[g_panelFlameCount++] = mk_label(hwnd, L"闪烁速度：", 20, 105, 200, 20);
            g_panelFlame[g_panelFlameCount++] = mk_slider(hwnd, IDC_SLIDER_FLSPEED, 20, 125, 340, 30, 1, 10, g_flameSpeed);
            g_panelFlame[g_panelFlameCount++] = mk_label(hwnd, L"平滑度：", 20, 165, 200, 20);
            g_panelFlame[g_panelFlameCount++] = mk_slider(hwnd, IDC_SLIDER_FLSMOOTH, 20, 185, 340, 30, 1, 20, g_flameSmooth);

            g_panelRain[g_panelRainCount++] = mk_label(hwnd, L"下落速度：", 20, 105, 200, 20);
            g_panelRain[g_panelRainCount++] = mk_slider(hwnd, IDC_SLIDER_RNSPEED, 20, 125, 340, 30, 1, 10, g_rainSpeed);
            g_panelRain[g_panelRainCount++] = mk_label(hwnd, L"密度（同时出现的雨滴）：", 20, 165, 300, 20);
            for (int c=0;c<5;c++) {
                g_rainDensityBtn[c] = mk_button(hwnd, (const wchar_t*[]){L"1",L"2",L"3",L"4",L"5"}[c], IDC_RAIN_DENSITY_BASE+c, 20+c*68, 185, 60, 30);
                g_panelRain[g_panelRainCount++] = g_rainDensityBtn[c];
            }
            g_panelRain[g_panelRainCount++] = mk_label(hwnd, L"颜色数量：", 20, 225, 200, 20);
            for (int c=0;c<5;c++) {
                g_rainColorCountBtn[c] = mk_button(hwnd, (const wchar_t*[]){L"1",L"2",L"3",L"4",L"5"}[c], IDC_RAIN_COLORCNT_BASE+c, 20+c*68, 245, 60, 30);
                g_panelRain[g_panelRainCount++] = g_rainColorCountBtn[c];
            }
            for (int c=0;c<5;c++) {
                g_rainColorBtn[c] = mk_button(hwnd, L"", IDC_RAIN_COLOR_BASE+c, 20+c*68, 285, 60, 32);
                g_panelRain[g_panelRainCount++] = g_rainColorBtn[c];
            }

            g_panelMatrix[g_panelMatrixCount++] = mk_label(hwnd, L"光带速度：", 20, 105, 200, 20);
            g_panelMatrix[g_panelMatrixCount++] = mk_slider(hwnd, IDC_SLIDER_MXSPEED, 20, 125, 340, 30, 1, 10, g_matrixSpeed);
            g_panelMatrix[g_panelMatrixCount++] = mk_label(hwnd, L"样式：", 20, 165, 200, 20);
            g_hComboMatrixStyle = CreateWindow(L"COMBOBOX", L"", WS_CHILD|CBS_DROPDOWNLIST|WS_VSCROLL,
                20, 185, 340, 100, hwnd, (HMENU)IDC_COMBO_MXSTYLE, NULL, NULL);
            SendMessage(g_hComboMatrixStyle, CB_ADDSTRING, 0, (LPARAM)L"斜向光带");
            SendMessage(g_hComboMatrixStyle, CB_ADDSTRING, 0, (LPARAM)L"垂直数字雨");
            SendMessage(g_hComboMatrixStyle, CB_ADDSTRING, 0, (LPARAM)L"激光扫描");
            SendMessage(g_hComboMatrixStyle, CB_ADDSTRING, 0, (LPARAM)L"涟漪");
            SendMessage(g_hComboMatrixStyle, CB_SETCURSEL, 0, 0);
            g_panelMatrix[g_panelMatrixCount++] = g_hComboMatrixStyle;
            g_panelMatrix[g_panelMatrixCount++] = mk_label(hwnd, L"密度（斜向/激光模式）：", 20, 220, 300, 20);
            for (int c=0;c<5;c++) {
                g_matrixDensityBtn[c] = mk_button(hwnd, (const wchar_t*[]){L"1",L"2",L"3",L"4",L"5"}[c], IDC_MATRIX_DENSITY_BASE+c, 20+c*68, 240, 60, 30);
                g_panelMatrix[g_panelMatrixCount++] = g_matrixDensityBtn[c];
            }
            g_panelMatrix[g_panelMatrixCount++] = mk_label(hwnd, L"颜色数量：", 20, 280, 200, 20);
            for (int c=0;c<3;c++) {
                g_matrixColorCountBtn[c] = mk_button(hwnd, (const wchar_t*[]){L"1",L"2",L"3"}[c], IDC_MATRIX_COLORCNT_BASE+c, 20+c*113, 300, 105, 30);
                g_panelMatrix[g_panelMatrixCount++] = g_matrixColorCountBtn[c];
            }
            for (int c=0;c<3;c++) {
                g_matrixColorBtn[c] = mk_button(hwnd, L"", IDC_MATRIX_COLOR_BASE+c, 20+c*113, 340, 105, 32);
                g_panelMatrix[g_panelMatrixCount++] = g_matrixColorBtn[c];
            }

            g_panelStatic[g_panelStaticCount++] = mk_label(hwnd, L"布局：", 20, 105, 200, 20);
            g_btnStaticHoriz = mk_button(hwnd, L"横向分区", IDC_BTN_STATIC_HORIZ, 20, 125, 165, 32);
            HWND btnStaticVert = mk_button(hwnd, L"纵向分区", IDC_BTN_STATIC_VERT, 195, 125, 165, 32);
            g_btnStaticVert = btnStaticVert;
            g_panelStatic[g_panelStaticCount++] = g_btnStaticHoriz;
            g_panelStatic[g_panelStaticCount++] = g_btnStaticVert;
            g_panelStatic[g_panelStaticCount++] = mk_label(hwnd, L"分区数量：", 20, 165, 200, 20);
            g_hSliderStaticZones = mk_slider(hwnd, IDC_SLIDER_STATIC_ZONES, 20, 185, 340, 30, 1, 6, g_staticZoneCount);
            g_panelStatic[g_panelStaticCount++] = g_hSliderStaticZones;
            g_hComboStaticZoneSel = CreateWindow(L"COMBOBOX", L"", WS_CHILD|CBS_DROPDOWNLIST|WS_VSCROLL,
                20, 225, 340, 150, hwnd, (HMENU)IDC_COMBO_STATIC_ZONESEL, NULL, NULL);
            g_panelStatic[g_panelStaticCount++] = g_hComboStaticZoneSel;
            g_btnStaticColor = mk_button(hwnd, L"选择当前分区颜色", IDC_BTN_STATIC_COLOR, 20, 265, 340, 36);
            g_panelStatic[g_panelStaticCount++] = g_btnStaticColor;

            g_panelRipple[g_panelRippleCount++] = mk_label(hwnd, L"涟漪颜色：", 20, 105, 200, 20);
            g_hSwatchRipple = CreateWindow(L"SwatchClass", L"", WS_CHILD|WS_BORDER, 20,128,50,28, hwnd, (HMENU)(INT_PTR)997, NULL, NULL);
            g_panelRipple[g_panelRippleCount++] = g_hSwatchRipple;
            g_panelRipple[g_panelRippleCount++] = mk_button(hwnd, L"选择颜色", IDC_BTN_COLOR_RIPPLE, 80, 128, 120, 28);
            g_panelRipple[g_panelRippleCount++] = mk_button(hwnd, L"随机", IDC_BTN_RIPPLE_RANDOM, 210, 128, 90, 28);
            g_panelRipple[g_panelRippleCount++] = mk_label(hwnd, L"扩散速度：", 20, 168, 150, 20);
            g_panelRipple[g_panelRippleCount++] = mk_slider(hwnd, IDC_SLIDER_RIPSPEED, 20, 188, 340, 30, 1, 10, g_rippleSpeed);
            g_panelRipple[g_panelRippleCount++] = mk_label(hwnd, L"波纹宽度：", 20, 223, 150, 20);
            g_panelRipple[g_panelRippleCount++] = mk_slider(hwnd, IDC_SLIDER_RIPWIDTH, 20, 243, 340, 30, 1, 4, g_rippleWidth);

            g_panelRainbow[g_panelRainbowCount++] = mk_label(hwnd, L"整块键盘保持常亮，RGB 颜色连续平滑循环。", 20, 105, 340, 40);
            g_panelRainbow[g_panelRainbowCount++] = mk_label(hwnd, L"变色速度：", 20, 155, 150, 20);
            g_panelRainbow[g_panelRainbowCount++] = mk_slider(hwnd, IDC_SLIDER_RBWSPEED, 20, 175, 340, 30, 1, 20, g_rainbowSpeed);

            g_panelQuicksand[g_panelQuicksandCount++] = mk_label(hwnd, L"渐变颜色（首尾自动闭环）：", 20, 105, 300, 20);
            for (int c=0; c<3; c++) {
                g_quicksandColorBtn[c] = mk_button(hwnd, L"", IDC_QUICKSAND_COLOR_BASE+c, 20+c*113, 128, 105, 32);
                g_panelQuicksand[g_panelQuicksandCount++] = g_quicksandColorBtn[c];
            }
            g_panelQuicksand[g_panelQuicksandCount++] = mk_label(hwnd, L"下落速度：", 20, 170, 150, 20);
            g_panelQuicksand[g_panelQuicksandCount++] = mk_slider(hwnd, IDC_SLIDER_QSSPEED, 20, 190, 340, 30, 1, 10, g_quicksandSpeed);
            g_panelQuicksand[g_panelQuicksandCount++] = mk_label(hwnd, L"沙带宽度：", 20, 225, 150, 20);
            g_panelQuicksand[g_panelQuicksandCount++] = mk_slider(hwnd, IDC_SLIDER_QSSCALE, 20, 245, 340, 30, 1, 10, g_quicksandScale);
            g_panelQuicksand[g_panelQuicksandCount++] = mk_label(hwnd, L"连续 RGB 混色 + 每键 2×2 线性光采样", 20, 285, 340, 25);

            g_panelCurrent[g_panelCurrentCount++] = mk_label(hwnd, L"渐变颜色（首尾自动闭环）：", 20, 105, 300, 20);
            for (int c=0; c<3; c++) {
                g_currentColorBtn[c] = mk_button(hwnd, L"", IDC_CURRENT_COLOR_BASE+c, 20+c*113, 128, 105, 32);
                g_panelCurrent[g_panelCurrentCount++] = g_currentColorBtn[c];
            }
            g_panelCurrent[g_panelCurrentCount++] = mk_label(hwnd, L"流动速度：", 20, 170, 150, 20);
            g_panelCurrent[g_panelCurrentCount++] = mk_slider(hwnd, IDC_SLIDER_CURSPEED, 20, 190, 340, 30, 1, 10, g_currentSpeed);
            g_panelCurrent[g_panelCurrentCount++] = mk_label(hwnd, L"电流长度：", 20, 225, 150, 20);
            g_panelCurrent[g_panelCurrentCount++] = mk_slider(hwnd, IDC_SLIDER_CURWIDTH, 20, 245, 340, 30, 2, 8, g_currentWidth);
            g_panelCurrent[g_panelCurrentCount++] = mk_label(hwnd, L"相邻行反向流动，光头与拖尾连续衰减", 20, 285, 340, 25);

            g_panelTouchCurrent[g_panelTouchCurrentCount++] = mk_label(hwnd, L"渐变颜色（首尾自动闭环）：", 20, 105, 300, 20);
            for (int c=0; c<3; c++) {
                g_touchCurrentColorBtn[c] = mk_button(hwnd, L"", IDC_TOUCH_CURRENT_COLOR_BASE+c, 20+c*113, 128, 105, 32);
                g_panelTouchCurrent[g_panelTouchCurrentCount++] = g_touchCurrentColorBtn[c];
            }
            g_panelTouchCurrent[g_panelTouchCurrentCount++] = mk_label(hwnd, L"扩散速度：", 20, 170, 150, 20);
            g_panelTouchCurrent[g_panelTouchCurrentCount++] = mk_slider(hwnd, IDC_SLIDER_TCSPEED, 20, 190, 340, 30, 1, 10, g_touchCurrentSpeed);
            g_panelTouchCurrent[g_panelTouchCurrentCount++] = mk_label(hwnd, L"电流宽度：", 20, 225, 150, 20);
            g_panelTouchCurrent[g_panelTouchCurrentCount++] = mk_slider(hwnd, IDC_SLIDER_TCWIDTH, 20, 245, 340, 30, 1, 5, g_touchCurrentWidth);
            g_panelTouchCurrent[g_panelTouchCurrentCount++] = mk_label(hwnd, L"按键后仅沿该物理键行向左右扩散", 20, 285, 340, 25);

            g_panelScreenAmbient[g_panelScreenAmbientCount++] = mk_label(hwnd,
                L"把主屏幕低分辨率取样为整块键盘的模糊副屏。\n画面仅在内存中处理，不保存截图。",
                20, 105, 340, 48);
            g_panelScreenAmbient[g_panelScreenAmbientCount++] = mk_label(hwnd, L"空间模糊范围：", 20, 165, 200, 20);
            g_panelScreenAmbient[g_panelScreenAmbientCount++] = mk_slider(hwnd,
                IDC_SLIDER_AMBIENT_BLUR, 20, 185, 340, 30, 1, 8, g_ambientBlur);
            g_panelScreenAmbient[g_panelScreenAmbientCount++] = mk_label(hwnd, L"画面响应速度：", 20, 225, 200, 20);
            g_panelScreenAmbient[g_panelScreenAmbientCount++] = mk_slider(hwnd,
                IDC_SLIDER_AMBIENT_RESPONSE, 20, 245, 340, 30, 1, 10, g_ambientResponse);
            g_panelScreenAmbient[g_panelScreenAmbientCount++] = mk_label(hwnd,
                L"线性光混色 + 空间模糊 + 时间平滑", 20, 285, 340, 25);

            add_page_control(g_pageEffects, &g_pageEffectsCount,
                mk_label(hwnd, L"当前灯效亮度：", 390, 125, 250, 20));
            g_hSliderBright = mk_slider(hwnd, IDC_SLIDER_BRIGHT, 390, 145, 310, 30, 0, 100, g_brightness);
            add_page_control(g_pageEffects, &g_pageEffectsCount, g_hSliderBright);
            g_hLblFps = mk_label(hwnd, L"", 390, 190, 310, 20);
            add_page_control(g_pageEffects, &g_pageEffectsCount, g_hLblFps);
            g_hSliderFps = mk_slider(hwnd, IDC_SLIDER_FPS, 390, 215, 310, 30,
                TARGET_FPS_MIN, TARGET_FPS_MAX, g_targetFps);
            add_page_control(g_pageEffects, &g_pageEffectsCount, g_hSliderFps);
            update_fps_label();
            g_hStatus = CreateWindow(L"STATIC", L"当前模式：无", WS_CHILD|WS_VISIBLE|SS_LEFT,
                390, 270, 310, 25, hwnd, (HMENU)IDC_STATIC_STATUS, NULL, NULL);
            add_page_control(g_pageEffects, &g_pageEffectsCount, g_hStatus);
            add_page_control(g_pageEffects, &g_pageEffectsCount,
                mk_label(hwnd, L"左侧只显示当前灯效参数；电源方案、顶行信息和程序行为已拆分到独立页面。",
                    390, 315, 310, 70));

            g_hAutoPowerCheck = CreateWindow(L"BUTTON", L"按插电状态自动切换主灯效",
                WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
                30, 68, 650, 24, hwnd, (HMENU)IDC_CHK_POWER_AUTO, NULL, NULL);
            add_page_control(g_pagePowerShared, &g_pagePowerSharedCount, g_hAutoPowerCheck);
            g_hPowerStatus = mk_label(hwnd, L"", 30, 100, 650, 20);
            add_page_control(g_pagePowerShared, &g_pagePowerSharedCount, g_hPowerStatus);

            add_page_control(g_pageAc, &g_pageAcCount,
                mk_label(hwnd, L"插电主灯效", 30, 145, 200, 24));
            add_page_control(g_pageAc, &g_pageAcCount,
                mk_label(hwnd, L"灯效：", 30, 190, 90, 20));
            g_hComboAcMode = CreateWindow(L"COMBOBOX", L"", WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,
                130, 187, 300, 260, hwnd, (HMENU)IDC_COMBO_AC_MODE, NULL, NULL);
            add_page_control(g_pageAc, &g_pageAcCount, g_hComboAcMode);
            populate_mode_combo(g_hComboAcMode, 1);
            add_page_control(g_pageAc, &g_pageAcCount,
                mk_label(hwnd, L"亮度：", 30, 240, 100, 20));
            g_hSliderAcBright = mk_slider(hwnd, IDC_SLIDER_AC_BRIGHT, 130, 235, 300, 30, 0, 100, g_acBrightness);
            add_page_control(g_pageAc, &g_pageAcCount, g_hSliderAcBright);
            g_hLblAcFps = mk_label(hwnd, L"", 30, 295, 95, 20);
            add_page_control(g_pageAc, &g_pageAcCount, g_hLblAcFps);
            g_hSliderAcFps = mk_slider(hwnd, IDC_SLIDER_AC_FPS, 130, 290, 300, 30,
                TARGET_FPS_MIN, TARGET_FPS_MAX, g_acFps);
            add_page_control(g_pageAc, &g_pageAcCount, g_hSliderAcFps);
            add_page_control(g_pageAc, &g_pageAcCount,
                mk_label(hwnd, L"此页只编辑插电方案；插着电时改动会立即应用。", 30, 355, 500, 30));

            add_page_control(g_pageBattery, &g_pageBatteryCount,
                mk_label(hwnd, L"电池主灯效", 30, 145, 200, 24));
            add_page_control(g_pageBattery, &g_pageBatteryCount,
                mk_label(hwnd, L"灯效：", 30, 190, 90, 20));
            g_hComboBatteryMode = CreateWindow(L"COMBOBOX", L"", WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,
                130, 187, 300, 260, hwnd, (HMENU)IDC_COMBO_BAT_MODE, NULL, NULL);
            add_page_control(g_pageBattery, &g_pageBatteryCount, g_hComboBatteryMode);
            populate_mode_combo(g_hComboBatteryMode, 1);
            add_page_control(g_pageBattery, &g_pageBatteryCount,
                mk_label(hwnd, L"亮度：", 30, 240, 100, 20));
            g_hSliderBatteryBright = mk_slider(hwnd, IDC_SLIDER_BAT_BRIGHT, 130, 235, 300, 30, 0, 100, g_batteryBrightness);
            add_page_control(g_pageBattery, &g_pageBatteryCount, g_hSliderBatteryBright);
            g_hLblBatteryFps = mk_label(hwnd, L"", 30, 295, 95, 20);
            add_page_control(g_pageBattery, &g_pageBatteryCount, g_hLblBatteryFps);
            g_hSliderBatteryFps = mk_slider(hwnd, IDC_SLIDER_BAT_FPS, 130, 290, 300, 30,
                TARGET_FPS_MIN, TARGET_FPS_MAX, g_batteryFps);
            add_page_control(g_pageBattery, &g_pageBatteryCount, g_hSliderBatteryFps);
            add_page_control(g_pageBattery, &g_pageBatteryCount,
                mk_label(hwnd, L"此页只编辑电池方案；使用电池时改动会立即应用。", 30, 355, 500, 30));

            add_page_control(g_pageProgram, &g_pageProgramCount,
                mk_label(hwnd, L"程序设置", 30, 75, 250, 26));
            g_btnAutostart = mk_button(hwnd, L"", IDC_BTN_AUTOSTART, 30, 115, 660, 34);
            add_page_control(g_pageProgram, &g_pageProgramCount, g_btnAutostart);
            update_autostart_button();
            add_page_control(g_pageProgram, &g_pageProgramCount,
                mk_label(hwnd, L"无操作熄灯：", 30, 180, 120, 20));
            g_hComboIdle = CreateWindow(L"COMBOBOX", L"", WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,
                160, 177, 240, 180, hwnd, (HMENU)IDC_COMBO_IDLE, NULL, NULL);
            add_page_control(g_pageProgram, &g_pageProgramCount, g_hComboIdle);
            const wchar_t *idleLabels[] = {L"关闭", L"1 分钟", L"5 分钟", L"10 分钟", L"30 分钟"};
            for (int i=0; i<5; i++) SendMessage(g_hComboIdle, CB_ADDSTRING, 0, (LPARAM)idleLabels[i]);
            int idleSel = g_idleTimeoutMinutes == 1 ? 1 : g_idleTimeoutMinutes == 5 ? 2 :
                          g_idleTimeoutMinutes == 10 ? 3 : g_idleTimeoutMinutes == 30 ? 4 : 0;
            SendMessage(g_hComboIdle, CB_SETCURSEL, idleSel, 0);
            add_page_control(g_pageProgram, &g_pageProgramCount,
                mk_label(hwnd, L"过渡类型：", 30, 225, 120, 20));
            g_hComboTransitionFamily = CreateWindow(L"COMBOBOX", L"",
                WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,
                185, 222, 300, 180, hwnd, (HMENU)IDC_COMBO_TRANSITION_FAMILY, NULL, NULL);
            add_page_control(g_pageProgram, &g_pageProgramCount, g_hComboTransitionFamily);
            const wchar_t *transitionFamilies[] = {
                L"关闭过渡", L"全局渐变", L"涟漪", L"双蛇漩涡"
            };
            for (int i = 0; i < 4; i++)
                SendMessage(g_hComboTransitionFamily, CB_ADDSTRING, 0, (LPARAM)transitionFamilies[i]);

            g_hLblTransitionDetail = mk_label(hwnd, L"过渡细节：", 30, 275, 120, 20);
            add_page_control(g_pageProgram, &g_pageProgramCount, g_hLblTransitionDetail);
            g_hComboTransitionDetail = CreateWindow(L"COMBOBOX", L"",
                WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,
                185, 272, 505, 220, hwnd, (HMENU)IDC_COMBO_TRANSITION_DETAIL, NULL, NULL);
            add_page_control(g_pageProgram, &g_pageProgramCount, g_hComboTransitionDetail);

            g_hLblTransitionDuration = mk_label(hwnd, L"", 30, 330, 150, 20);
            add_page_control(g_pageProgram, &g_pageProgramCount, g_hLblTransitionDuration);
            g_hSliderTransitionDuration = mk_slider(hwnd, IDC_SLIDER_TRANSITION_DURATION,
                185, 322, 430, 32, 2, 30, (g_transitionDurationMs + 50) / 100);
            SendMessage(g_hSliderTransitionDuration, TBM_SETTICFREQ, 2, 0);
            add_page_control(g_pageProgram, &g_pageProgramCount, g_hSliderTransitionDuration);
            add_page_control(g_pageProgram, &g_pageProgramCount,
                mk_label(hwnd, L"0.2 - 3.0 秒", 620, 330, 90, 20));
            add_page_control(g_pageProgram, &g_pageProgramCount,
                mk_label(hwnd, L"只显示所选类型的细节；退出、停止和休眠会自动沿同一路径逆放。",
                    30, 365, 660, 30));
            g_btnWhiteCalibrate = mk_button(hwnd, L"开始白色光谱校准",
                IDC_BTN_WHITE_CALIBRATE, 30, 397, 250, 34);
            add_page_control(g_pageProgram, &g_pageProgramCount, g_btnWhiteCalibrate);
            g_btnWhiteReset = mk_button(hwnd, L"重置校准",
                IDC_BTN_WHITE_RESET, 295, 397, 135, 34);
            add_page_control(g_pageProgram, &g_pageProgramCount, g_btnWhiteReset);
            g_hWhiteCalibrationSwatch = CreateWindow(L"SwatchClass", L"",
                WS_CHILD|WS_BORDER, 445, 397, 50, 34, hwnd,
                (HMENU)(INT_PTR)996, NULL, NULL);
            add_page_control(g_pageProgram, &g_pageProgramCount, g_hWhiteCalibrationSwatch);
            g_hLblWhiteCalibration = mk_label(hwnd, L"", 30, 438, 660, 24);
            add_page_control(g_pageProgram, &g_pageProgramCount, g_hLblWhiteCalibration);
            update_white_calibration_ui();
            add_page_control(g_pageProgram, &g_pageProgramCount,
                mk_label(hwnd, L"安全策略：始终独占 RGB 接口；不会刷写 BIOS、EC 或键盘固件。",
                    30, 470, 660, 28));

            add_page_control(g_pageTouchbar, &g_pageTouchbarCount,
                mk_label(hwnd, L"顶行信息 Bar（独立于主灯效）", 30, 65, 660, 24));
            const wchar_t *touchbarModes[] = {
                L"关闭", L"电池电量", L"系统音量", L"麦克风输入",
                L"CPU 占用率", L"GPU 占用率", L"播放音频波形"
            };
            const wchar_t *touchbarDirections[] = {
                L"从左到右", L"从右到左", L"从两边到中间", L"从中间到两边"
            };

            add_page_control(g_pageTouchbar, &g_pageTouchbarCount,
                mk_label(hwnd, L"插电 Bar", 30, 105, 200, 24));
            add_page_control(g_pageTouchbar, &g_pageTouchbarCount,
                mk_label(hwnd, L"信息：", 30, 145, 90, 20));
            g_hComboAcTouchbarMode = CreateWindow(L"COMBOBOX", L"",
                WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,
                120, 142, 230, 210, hwnd, (HMENU)IDC_COMBO_AC_TOUCHBAR_MODE, NULL, NULL);
            add_page_control(g_pageTouchbar, &g_pageTouchbarCount, g_hComboAcTouchbarMode);
            g_hAcTouchbarDirectionLabel = mk_label(hwnd, L"方向：", 30, 190, 90, 20);
            add_page_control(g_pageTouchbar, &g_pageTouchbarCount, g_hAcTouchbarDirectionLabel);
            g_hComboAcTouchbarDirection = CreateWindow(L"COMBOBOX", L"",
                WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,
                120, 187, 230, 150, hwnd, (HMENU)IDC_COMBO_AC_TOUCHBAR_DIRECTION, NULL, NULL);
            add_page_control(g_pageTouchbar, &g_pageTouchbarCount, g_hComboAcTouchbarDirection);
            g_hAcTouchbarMetricLabel = mk_label(hwnd, L"指标颜色：", 30, 235, 90, 20);
            add_page_control(g_pageTouchbar, &g_pageTouchbarCount, g_hAcTouchbarMetricLabel);
            g_acTouchbarColorBtnStart = mk_button(hwnd, L"颜色 A", IDC_AC_TOUCHBAR_COLOR_START, 120, 230, 108, 30);
            g_acTouchbarColorBtnEnd = mk_button(hwnd, L"颜色 B", IDC_AC_TOUCHBAR_COLOR_END, 242, 230, 108, 30);
            add_page_control(g_pageTouchbar, &g_pageTouchbarCount, g_acTouchbarColorBtnStart);
            add_page_control(g_pageTouchbar, &g_pageTouchbarCount, g_acTouchbarColorBtnEnd);
            g_hAcTouchbarAudioLabel = mk_label(hwnd, L"音频 RGB：", 30, 280, 90, 20);
            add_page_control(g_pageTouchbar, &g_pageTouchbarCount, g_hAcTouchbarAudioLabel);
            for (int i = 0; i < 3; i++) {
                g_acTouchbarAudioColorBtn[i] = mk_button(hwnd, (const wchar_t *[]){L"RGB 1",L"RGB 2",L"RGB 3"}[i],
                    IDC_AC_TOUCHBAR_AUDIO_COLOR_BASE+i, 120+i*78, 275, 70, 30);
                add_page_control(g_pageTouchbar, &g_pageTouchbarCount, g_acTouchbarAudioColorBtn[i]);
            }

            add_page_control(g_pageTouchbar, &g_pageTouchbarCount,
                mk_label(hwnd, L"电池 Bar", 390, 105, 200, 24));
            add_page_control(g_pageTouchbar, &g_pageTouchbarCount,
                mk_label(hwnd, L"信息：", 390, 145, 90, 20));
            g_hComboBatteryTouchbarMode = CreateWindow(L"COMBOBOX", L"",
                WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,
                480, 142, 230, 210, hwnd, (HMENU)IDC_COMBO_BAT_TOUCHBAR_MODE, NULL, NULL);
            add_page_control(g_pageTouchbar, &g_pageTouchbarCount, g_hComboBatteryTouchbarMode);
            g_hBatteryTouchbarDirectionLabel = mk_label(hwnd, L"方向：", 390, 190, 90, 20);
            add_page_control(g_pageTouchbar, &g_pageTouchbarCount, g_hBatteryTouchbarDirectionLabel);
            g_hComboBatteryTouchbarDirection = CreateWindow(L"COMBOBOX", L"",
                WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,
                480, 187, 230, 150, hwnd, (HMENU)IDC_COMBO_BAT_TOUCHBAR_DIRECTION, NULL, NULL);
            add_page_control(g_pageTouchbar, &g_pageTouchbarCount, g_hComboBatteryTouchbarDirection);
            g_hBatteryTouchbarMetricLabel = mk_label(hwnd, L"指标颜色：", 390, 235, 90, 20);
            add_page_control(g_pageTouchbar, &g_pageTouchbarCount, g_hBatteryTouchbarMetricLabel);
            g_batteryTouchbarColorBtnStart = mk_button(hwnd, L"颜色 A", IDC_BAT_TOUCHBAR_COLOR_START, 480, 230, 108, 30);
            g_batteryTouchbarColorBtnEnd = mk_button(hwnd, L"颜色 B", IDC_BAT_TOUCHBAR_COLOR_END, 602, 230, 108, 30);
            add_page_control(g_pageTouchbar, &g_pageTouchbarCount, g_batteryTouchbarColorBtnStart);
            add_page_control(g_pageTouchbar, &g_pageTouchbarCount, g_batteryTouchbarColorBtnEnd);
            g_hBatteryTouchbarAudioLabel = mk_label(hwnd, L"音频 RGB：", 390, 280, 90, 20);
            add_page_control(g_pageTouchbar, &g_pageTouchbarCount, g_hBatteryTouchbarAudioLabel);
            for (int i = 0; i < 3; i++) {
                g_batteryTouchbarAudioColorBtn[i] = mk_button(hwnd, (const wchar_t *[]){L"RGB 1",L"RGB 2",L"RGB 3"}[i],
                    IDC_BAT_TOUCHBAR_AUDIO_COLOR_BASE+i, 480+i*78, 275, 70, 30);
                add_page_control(g_pageTouchbar, &g_pageTouchbarCount, g_batteryTouchbarAudioColorBtn[i]);
            }
            g_hAcTouchbarHint = mk_label(hwnd, L"", 30, 370, 320, 60);
            g_hBatteryTouchbarHint = mk_label(hwnd, L"", 390, 370, 320, 60);
            add_page_control(g_pageTouchbar, &g_pageTouchbarCount, g_hAcTouchbarHint);
            add_page_control(g_pageTouchbar, &g_pageTouchbarCount, g_hBatteryTouchbarHint);

            for (int i = 0; i < TOUCHBAR_MODE_COUNT; i++) {
                SendMessage(g_hComboAcTouchbarMode, CB_ADDSTRING, 0, (LPARAM)touchbarModes[i]);
                SendMessage(g_hComboBatteryTouchbarMode, CB_ADDSTRING, 0, (LPARAM)touchbarModes[i]);
            }
            for (int i = 0; i < TOUCHBAR_DIRECTION_COUNT; i++) {
                SendMessage(g_hComboAcTouchbarDirection, CB_ADDSTRING, 0, (LPARAM)touchbarDirections[i]);
                SendMessage(g_hComboBatteryTouchbarDirection, CB_ADDSTRING, 0, (LPARAM)touchbarDirections[i]);
            }

            set_swatch_color(g_hSwatchBreath, g_breathColor);
            set_swatch_color(g_hSwatchReact, g_reactiveColor);
            set_swatch_color(g_hSwatchRipple, g_rippleRandomMode ? RGB(200,200,200) : g_rippleColor);
            update_power_profile_ui();
            SetTimer(hwnd, IDT_POWER_POLL, 2000, NULL);

            refresh_rain_color_visibility();
            refresh_matrix_color_visibility();
            refresh_static_ui();
            show_ui_page(UI_PAGE_EFFECTS);

            EnumChildWindows(hwnd, SetFontOnChild, (LPARAM)hFont);
            return 0;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lp;
            int id = dis->CtlID;
            if (id >= IDC_PAGE_EFFECTS && id <= IDC_PAGE_PROGRAM) {
                draw_selector_button(dis, id - IDC_PAGE_EFFECTS == g_uiPage);
            } else if (id >= IDC_RAIN_COLOR_BASE && id < IDC_RAIN_COLOR_BASE+5) {
                draw_button_ex(dis, TRUE, g_rainColors[id-IDC_RAIN_COLOR_BASE]);
            } else if (id >= IDC_MATRIX_COLOR_BASE && id < IDC_MATRIX_COLOR_BASE+3) {
                draw_button_ex(dis, TRUE, g_matrixColors[id-IDC_MATRIX_COLOR_BASE]);
            } else if (id >= IDC_QUICKSAND_COLOR_BASE && id < IDC_QUICKSAND_COLOR_BASE+3) {
                draw_button_ex(dis, TRUE, g_quicksandColors[id-IDC_QUICKSAND_COLOR_BASE]);
            } else if (id >= IDC_CURRENT_COLOR_BASE && id < IDC_CURRENT_COLOR_BASE+3) {
                draw_button_ex(dis, TRUE, g_currentColors[id-IDC_CURRENT_COLOR_BASE]);
            } else if (id >= IDC_TOUCH_CURRENT_COLOR_BASE && id < IDC_TOUCH_CURRENT_COLOR_BASE+3) {
                draw_button_ex(dis, TRUE, g_touchCurrentColors[id-IDC_TOUCH_CURRENT_COLOR_BASE]);
            } else if (id == IDC_AC_TOUCHBAR_COLOR_START) {
                draw_labeled_color_button(dis, g_acTouchbarColorStart);
            } else if (id == IDC_AC_TOUCHBAR_COLOR_END) {
                draw_labeled_color_button(dis, g_acTouchbarColorEnd);
            } else if (id == IDC_BAT_TOUCHBAR_COLOR_START) {
                draw_labeled_color_button(dis, g_batteryTouchbarColorStart);
            } else if (id == IDC_BAT_TOUCHBAR_COLOR_END) {
                draw_labeled_color_button(dis, g_batteryTouchbarColorEnd);
            } else if (id >= IDC_AC_TOUCHBAR_AUDIO_COLOR_BASE && id < IDC_AC_TOUCHBAR_AUDIO_COLOR_BASE+3) {
                draw_labeled_color_button(dis, g_acTouchbarAudioColors[id-IDC_AC_TOUCHBAR_AUDIO_COLOR_BASE]);
            } else if (id >= IDC_BAT_TOUCHBAR_AUDIO_COLOR_BASE && id < IDC_BAT_TOUCHBAR_AUDIO_COLOR_BASE+3) {
                draw_labeled_color_button(dis, g_batteryTouchbarAudioColors[id-IDC_BAT_TOUCHBAR_AUDIO_COLOR_BASE]);
            } else if (id >= IDC_RAIN_DENSITY_BASE && id < IDC_RAIN_DENSITY_BASE+5) {
                draw_selector_button(dis, (id-IDC_RAIN_DENSITY_BASE+1) == (int)g_rainDensityTarget);
            } else if (id >= IDC_RAIN_COLORCNT_BASE && id < IDC_RAIN_COLORCNT_BASE+5) {
                draw_selector_button(dis, (id-IDC_RAIN_COLORCNT_BASE+1) == (int)g_rainColorCount);
            } else if (id >= IDC_MATRIX_DENSITY_BASE && id < IDC_MATRIX_DENSITY_BASE+5) {
                draw_selector_button(dis, (id-IDC_MATRIX_DENSITY_BASE+1) == (int)g_matrixDensityTarget);
            } else if (id >= IDC_MATRIX_COLORCNT_BASE && id < IDC_MATRIX_COLORCNT_BASE+3) {
                draw_selector_button(dis, (id-IDC_MATRIX_COLORCNT_BASE+1) == (int)g_matrixColorCount);
            } else if (id == IDC_BTN_STATIC_HORIZ) {
                draw_selector_button(dis, g_staticLayout == 0);
            } else if (id == IDC_BTN_STATIC_VERT) {
                draw_selector_button(dis, g_staticLayout == 1);
            } else if (id == IDC_BTN_STATIC_COLOR) {
                draw_labeled_color_button(dis, g_staticColors[g_staticActiveZone]);
            } else {
                draw_button(dis);
            }
            return TRUE;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wp;
            SetTextColor(hdc, CLR_TEXT);
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)g_bgBrush;
        }

        case WM_FPS_STATUS:
            update_fps_label();
            return 0;

        case WM_FN_HOTKEY:
            apply_fn_brightness((ULONG)wp);
            return 0;

        case WM_HSCROLL: {
            HWND ctrl = (HWND)lp;
            if (ctrl == g_hSliderTransitionDuration) {
                LONG value = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0) * 100;
                InterlockedExchange(&g_transitionDurationMs, cfg_clamp(value, 200, 3000));
                update_transition_duration_label();
                if (LOWORD(wp) != TB_THUMBTRACK) save_config();
                return 0;
            }
            if (ctrl == g_hSliderAcBright || ctrl == g_hSliderAcFps ||
                ctrl == g_hSliderBatteryBright || ctrl == g_hSliderBatteryFps) {
                LONG value = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
                if (ctrl == g_hSliderAcBright) {
                    InterlockedExchange(&g_acBrightness, value);
                    if (g_autoPowerProfiles && g_onAcPower) {
                        InterlockedExchange(&g_brightness, value);
                        SendMessage(g_hSliderBright, TBM_SETPOS, TRUE, value);
                    }
                } else if (ctrl == g_hSliderBatteryBright) {
                    InterlockedExchange(&g_batteryBrightness, value);
                    if (g_autoPowerProfiles && !g_onAcPower) {
                        InterlockedExchange(&g_brightness, value);
                        SendMessage(g_hSliderBright, TBM_SETPOS, TRUE, value);
                    }
                } else if (ctrl == g_hSliderAcFps) {
                    InterlockedExchange(&g_acFps, value);
                    if (g_autoPowerProfiles && g_onAcPower) {
                        InterlockedExchange(&g_targetFps, value);
                        set_effective_fps(value, "AC profile FPS changed by user");
                        SendMessage(g_hSliderFps, TBM_SETPOS, TRUE, value);
                        update_fps_label();
                    }
                } else {
                    InterlockedExchange(&g_batteryFps, value);
                    if (g_autoPowerProfiles && !g_onAcPower) {
                        InterlockedExchange(&g_targetFps, value);
                        set_effective_fps(value, "battery profile FPS changed by user");
                        SendMessage(g_hSliderFps, TBM_SETPOS, TRUE, value);
                        update_fps_label();
                    }
                }
                update_power_profile_ui();
                save_config();
                return 0;
            }
            if (ctrl == g_hSliderBright) g_brightness = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
            else if (ctrl == g_hSliderFps) {
                LONG fps = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
                InterlockedExchange(&g_targetFps, fps);
                set_effective_fps(fps, "target changed by user");
                update_fps_label();
            }
            else if (GetDlgCtrlID(ctrl) == IDC_SLIDER_BSPEED) g_breathSpeed = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
            else if (GetDlgCtrlID(ctrl) == IDC_SLIDER_BSMOOTH) g_breathSmooth = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
            else if (GetDlgCtrlID(ctrl) == IDC_SLIDER_RIPSPEED) g_rippleSpeed = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
            else if (GetDlgCtrlID(ctrl) == IDC_SLIDER_RIPWIDTH) g_rippleWidth = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
            else if (GetDlgCtrlID(ctrl) == IDC_SLIDER_RBWSPEED) g_rainbowSpeed = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
            else if (GetDlgCtrlID(ctrl) == IDC_SLIDER_QSSPEED) g_quicksandSpeed = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
            else if (GetDlgCtrlID(ctrl) == IDC_SLIDER_QSSCALE) g_quicksandScale = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
            else if (GetDlgCtrlID(ctrl) == IDC_SLIDER_CURSPEED) g_currentSpeed = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
            else if (GetDlgCtrlID(ctrl) == IDC_SLIDER_CURWIDTH) g_currentWidth = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
            else if (GetDlgCtrlID(ctrl) == IDC_SLIDER_TCSPEED) g_touchCurrentSpeed = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
            else if (GetDlgCtrlID(ctrl) == IDC_SLIDER_TCWIDTH) g_touchCurrentWidth = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
            else if (GetDlgCtrlID(ctrl) == IDC_SLIDER_AMBIENT_BLUR) g_ambientBlur = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
            else if (GetDlgCtrlID(ctrl) == IDC_SLIDER_AMBIENT_RESPONSE) g_ambientResponse = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
            else if (GetDlgCtrlID(ctrl) == IDC_SLIDER_ANGLE) {
                g_waveAngle = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
                wchar_t buf[64];
                wsprintf(buf, L"角度：%d 度", (int)g_waveAngle);
                SetWindowText(g_hLblAngle, buf);
            }
            else if (GetDlgCtrlID(ctrl) == IDC_SLIDER_WSPEED) g_waveSpeed = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
            else if (GetDlgCtrlID(ctrl) == IDC_SLIDER_SPSPEED) g_sparkleSpeed = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
            else if (GetDlgCtrlID(ctrl) == IDC_SLIDER_SPDENS) g_sparkleDensity = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
            else if (GetDlgCtrlID(ctrl) == IDC_SLIDER_WHSPEED) g_wheelSpeed = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
            else if (GetDlgCtrlID(ctrl) == IDC_SLIDER_LTSPEED) g_lightningSpeed = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
            else if (GetDlgCtrlID(ctrl) == IDC_SLIDER_LTSMOOTH) g_lightningSmooth = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
            else if (GetDlgCtrlID(ctrl) == IDC_SLIDER_LTWIDTH) g_lightningWidth = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
            else if (GetDlgCtrlID(ctrl) == IDC_SLIDER_LTCONCUR) {
                g_lightningConcurrent = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
                wchar_t buf[64];
                wsprintf(buf, L"同时出现数量：%d", (int)g_lightningConcurrent);
                SetWindowText(g_hLblConcurrent, buf);
            }
            else if (GetDlgCtrlID(ctrl) == IDC_SLIDER_FLSPEED) g_flameSpeed = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
            else if (GetDlgCtrlID(ctrl) == IDC_SLIDER_FLSMOOTH) g_flameSmooth = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
            else if (GetDlgCtrlID(ctrl) == IDC_SLIDER_RNSPEED) g_rainSpeed = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
            else if (GetDlgCtrlID(ctrl) == IDC_SLIDER_MXSPEED) g_matrixSpeed = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
            else if (GetDlgCtrlID(ctrl) == IDC_SLIDER_STATIC_ZONES) {
                g_staticZoneCount = (LONG)SendMessage(ctrl, TBM_GETPOS, 0, 0);
                refresh_static_ui();
            }
            capture_current_power_profile();
            return 0;
        }

        case WM_ACTIVATE:
            if (LOWORD(wp) != WA_INACTIVE) update_autostart_button();
            return 0;

        case WM_COMMAND:
            if (LOWORD(wp) >= IDC_PAGE_EFFECTS && LOWORD(wp) <= IDC_PAGE_PROGRAM) {
                show_ui_page(LOWORD(wp) - IDC_PAGE_EFFECTS);
                return 0;
            }
            if (HIWORD(wp) == EN_KILLFOCUS && (HWND)lp == g_hEditAcTouchbarFpsMax) {
                commit_touchbar_fps_limit(g_hEditAcTouchbarFpsMax, &g_acTouchbarFpsMax);
                save_config();
                return 0;
            }
            if (HIWORD(wp) == EN_KILLFOCUS && (HWND)lp == g_hEditBatteryTouchbarFpsMax) {
                commit_touchbar_fps_limit(g_hEditBatteryTouchbarFpsMax, &g_batteryTouchbarFpsMax);
                save_config();
                return 0;
            }
            if (HIWORD(wp) == CBN_SELCHANGE && (HWND)lp == g_hComboMode) {
                int sel = (int)SendMessage(g_hComboMode, CB_GETCURSEL, 0, 0);
                if (sel >= 0) {
                    show_panel_for_mode(sel);
                    if (sel == MODE_RAIN) refresh_rain_color_visibility();
                    if (sel == MODE_MATRIX) refresh_matrix_color_visibility();
                    if (sel == MODE_STATIC) refresh_static_ui();
                    start_mode(sel);
                }
                return 0;
            }
            if (HIWORD(wp) == CBN_SELCHANGE && (HWND)lp == g_hComboAcMode) {
                int sel = (int)SendMessage(g_hComboAcMode, CB_GETCURSEL, 0, 0);
                g_acMode = cfg_clamp(sel - 1, -1, MODE_COUNT - 1);
                if (g_autoPowerProfiles && g_onAcPower) apply_power_profile(1, 1);
                else save_config();
                return 0;
            }
            if (HIWORD(wp) == CBN_SELCHANGE && (HWND)lp == g_hComboBatteryMode) {
                int sel = (int)SendMessage(g_hComboBatteryMode, CB_GETCURSEL, 0, 0);
                g_batteryMode = cfg_clamp(sel - 1, -1, MODE_COUNT - 1);
                if (g_autoPowerProfiles && !g_onAcPower) apply_power_profile(0, 1);
                else save_config();
                return 0;
            }
            if (HIWORD(wp) == CBN_SELCHANGE && (HWND)lp == g_hComboAcTouchbarMode) {
                g_acTouchbarMode = cfg_clamp((LONG)SendMessage(g_hComboAcTouchbarMode, CB_GETCURSEL, 0, 0), 0, TOUCHBAR_MODE_COUNT - 1);
                if (g_onAcPower) InterlockedExchange(&g_touchbarMode, g_acTouchbarMode);
                refresh_touchbar_option_visibility();
                save_config();
                return 0;
            }
            if (HIWORD(wp) == CBN_SELCHANGE && (HWND)lp == g_hComboBatteryTouchbarMode) {
                g_batteryTouchbarMode = cfg_clamp((LONG)SendMessage(g_hComboBatteryTouchbarMode, CB_GETCURSEL, 0, 0), 0, TOUCHBAR_MODE_COUNT - 1);
                if (!g_onAcPower) InterlockedExchange(&g_touchbarMode, g_batteryTouchbarMode);
                refresh_touchbar_option_visibility();
                save_config();
                return 0;
            }
            if (HIWORD(wp) == CBN_SELCHANGE && (HWND)lp == g_hComboAcTouchbarDirection) {
                g_acTouchbarDirection = cfg_clamp((LONG)SendMessage(g_hComboAcTouchbarDirection, CB_GETCURSEL, 0, 0), 0, TOUCHBAR_DIRECTION_COUNT - 1);
                if (g_onAcPower) InterlockedExchange(&g_touchbarDirection, g_acTouchbarDirection);
                save_config();
                return 0;
            }
            if (HIWORD(wp) == CBN_SELCHANGE && (HWND)lp == g_hComboBatteryTouchbarDirection) {
                g_batteryTouchbarDirection = cfg_clamp((LONG)SendMessage(g_hComboBatteryTouchbarDirection, CB_GETCURSEL, 0, 0), 0, TOUCHBAR_DIRECTION_COUNT - 1);
                if (!g_onAcPower) InterlockedExchange(&g_touchbarDirection, g_batteryTouchbarDirection);
                save_config();
                return 0;
            }
            if (HIWORD(wp) == CBN_SELCHANGE && (HWND)lp == g_hComboIdle) {
                int sel = (int)SendMessage(g_hComboIdle, CB_GETCURSEL, 0, 0);
                const LONG values[] = {0, 1, 5, 10, 30};
                g_idleTimeoutMinutes = (sel >= 0 && sel < 5) ? values[sel] : 0;
                if (!g_idleTimeoutMinutes) {
                    InterlockedExchange(&g_idleTransitionState, IDLE_TRANSITION_ACTIVE);
                    InterlockedExchange64(&g_idleTransitionStartTick, 0);
                    InterlockedExchange(&g_idleLightsOff, 0);
                    InterlockedExchange(&g_pacerResetRequested, 1);
                }
                save_config();
                return 0;
            }
            if (HIWORD(wp) == CBN_SELCHANGE && (HWND)lp == g_hComboTransitionFamily) {
                int family = (int)SendMessage(g_hComboTransitionFamily, CB_GETCURSEL, 0, 0);
                LONG current = InterlockedCompareExchange(&g_powerTransitionMode, 0, 0);
                int detail = transition_family_from_mode(current) == family
                    ? transition_detail_from_mode(current) : 0;
                InterlockedExchange(&g_powerTransitionMode,
                    transition_mode_from_ui(family, detail));
                update_transition_detail_ui();
                save_config();
                return 0;
            }
            if (HIWORD(wp) == CBN_SELCHANGE && (HWND)lp == g_hComboTransitionDetail) {
                int family = (int)SendMessage(g_hComboTransitionFamily, CB_GETCURSEL, 0, 0);
                int detail = (int)SendMessage(g_hComboTransitionDetail, CB_GETCURSEL, 0, 0);
                InterlockedExchange(&g_powerTransitionMode,
                    transition_mode_from_ui(family, detail));
                save_config();
                return 0;
            }
            if (HIWORD(wp) == CBN_SELCHANGE && (HWND)lp == g_hComboMatrixStyle) {
                g_matrixStyle = (LONG)SendMessage(g_hComboMatrixStyle, CB_GETCURSEL, 0, 0);
                return 0;
            }
            if (HIWORD(wp) == CBN_SELCHANGE && (HWND)lp == g_hComboStaticZoneSel) {
                g_staticActiveZone = (LONG)SendMessage(g_hComboStaticZoneSel, CB_GETCURSEL, 0, 0);
                InvalidateRect(g_btnStaticColor, NULL, TRUE);
                return 0;
            }
            {
                int cid = LOWORD(wp);
                if (cid >= IDC_RAIN_COLOR_BASE && cid < IDC_RAIN_COLOR_BASE+5) {
                    int idx = cid - IDC_RAIN_COLOR_BASE;
                    g_rainColors[idx] = pick_color(hwnd, g_rainColors[idx]);
                    InvalidateRect(g_rainColorBtn[idx], NULL, TRUE);
                    return 0;
                }
                if (cid >= IDC_MATRIX_COLOR_BASE && cid < IDC_MATRIX_COLOR_BASE+3) {
                    int idx = cid - IDC_MATRIX_COLOR_BASE;
                    g_matrixColors[idx] = pick_color(hwnd, g_matrixColors[idx]);
                    InvalidateRect(g_matrixColorBtn[idx], NULL, TRUE);
                    return 0;
                }
                if (cid >= IDC_QUICKSAND_COLOR_BASE && cid < IDC_QUICKSAND_COLOR_BASE+3) {
                    int idx = cid - IDC_QUICKSAND_COLOR_BASE;
                    g_quicksandColors[idx] = pick_color(hwnd, g_quicksandColors[idx]);
                    InvalidateRect(g_quicksandColorBtn[idx], NULL, TRUE);
                    return 0;
                }
                if (cid >= IDC_CURRENT_COLOR_BASE && cid < IDC_CURRENT_COLOR_BASE+3) {
                    int idx = cid - IDC_CURRENT_COLOR_BASE;
                    g_currentColors[idx] = pick_color(hwnd, g_currentColors[idx]);
                    InvalidateRect(g_currentColorBtn[idx], NULL, TRUE);
                    return 0;
                }
                if (cid >= IDC_TOUCH_CURRENT_COLOR_BASE && cid < IDC_TOUCH_CURRENT_COLOR_BASE+3) {
                    int idx = cid - IDC_TOUCH_CURRENT_COLOR_BASE;
                    g_touchCurrentColors[idx] = pick_color(hwnd, g_touchCurrentColors[idx]);
                    InvalidateRect(g_touchCurrentColorBtn[idx], NULL, TRUE);
                    return 0;
                }
                if (cid == IDC_AC_TOUCHBAR_COLOR_START || cid == IDC_AC_TOUCHBAR_COLOR_END) {
                    COLORREF *color = cid == IDC_AC_TOUCHBAR_COLOR_START ? &g_acTouchbarColorStart : &g_acTouchbarColorEnd;
                    *color = pick_color(hwnd, *color);
                    if (g_onAcPower) {
                        g_touchbarColorStart = g_acTouchbarColorStart;
                        g_touchbarColorEnd = g_acTouchbarColorEnd;
                    }
                    InvalidateRect(cid == IDC_AC_TOUCHBAR_COLOR_START ? g_acTouchbarColorBtnStart : g_acTouchbarColorBtnEnd, NULL, TRUE);
                    save_config();
                    return 0;
                }
                if (cid == IDC_BAT_TOUCHBAR_COLOR_START || cid == IDC_BAT_TOUCHBAR_COLOR_END) {
                    COLORREF *color = cid == IDC_BAT_TOUCHBAR_COLOR_START ? &g_batteryTouchbarColorStart : &g_batteryTouchbarColorEnd;
                    *color = pick_color(hwnd, *color);
                    if (!g_onAcPower) {
                        g_touchbarColorStart = g_batteryTouchbarColorStart;
                        g_touchbarColorEnd = g_batteryTouchbarColorEnd;
                    }
                    InvalidateRect(cid == IDC_BAT_TOUCHBAR_COLOR_START ? g_batteryTouchbarColorBtnStart : g_batteryTouchbarColorBtnEnd, NULL, TRUE);
                    save_config();
                    return 0;
                }
                if (cid >= IDC_AC_TOUCHBAR_AUDIO_COLOR_BASE && cid < IDC_AC_TOUCHBAR_AUDIO_COLOR_BASE+3) {
                    int index = cid - IDC_AC_TOUCHBAR_AUDIO_COLOR_BASE;
                    g_acTouchbarAudioColors[index] = pick_color(hwnd, g_acTouchbarAudioColors[index]);
                    if (g_onAcPower) g_touchbarAudioColors[index] = g_acTouchbarAudioColors[index];
                    InvalidateRect(g_acTouchbarAudioColorBtn[index], NULL, TRUE);
                    save_config();
                    return 0;
                }
                if (cid >= IDC_BAT_TOUCHBAR_AUDIO_COLOR_BASE && cid < IDC_BAT_TOUCHBAR_AUDIO_COLOR_BASE+3) {
                    int index = cid - IDC_BAT_TOUCHBAR_AUDIO_COLOR_BASE;
                    g_batteryTouchbarAudioColors[index] = pick_color(hwnd, g_batteryTouchbarAudioColors[index]);
                    if (!g_onAcPower) g_touchbarAudioColors[index] = g_batteryTouchbarAudioColors[index];
                    InvalidateRect(g_batteryTouchbarAudioColorBtn[index], NULL, TRUE);
                    save_config();
                    return 0;
                }
                if (cid >= IDC_RAIN_DENSITY_BASE && cid < IDC_RAIN_DENSITY_BASE+5) {
                    g_rainDensityTarget = cid - IDC_RAIN_DENSITY_BASE + 1;
                    invalidate_group(g_rainDensityBtn, 5);
                    return 0;
                }
                if (cid >= IDC_RAIN_COLORCNT_BASE && cid < IDC_RAIN_COLORCNT_BASE+5) {
                    g_rainColorCount = cid - IDC_RAIN_COLORCNT_BASE + 1;
                    invalidate_group(g_rainColorCountBtn, 5);
                    refresh_rain_color_visibility();
                    return 0;
                }
                if (cid >= IDC_MATRIX_DENSITY_BASE && cid < IDC_MATRIX_DENSITY_BASE+5) {
                    g_matrixDensityTarget = cid - IDC_MATRIX_DENSITY_BASE + 1;
                    invalidate_group(g_matrixDensityBtn, 5);
                    return 0;
                }
                if (cid >= IDC_MATRIX_COLORCNT_BASE && cid < IDC_MATRIX_COLORCNT_BASE+3) {
                    g_matrixColorCount = cid - IDC_MATRIX_COLORCNT_BASE + 1;
                    invalidate_group(g_matrixColorCountBtn, 3);
                    refresh_matrix_color_visibility();
                    return 0;
                }
                if (cid == IDC_BTN_STATIC_HORIZ) { g_staticLayout = 0; refresh_static_ui(); return 0; }
                if (cid == IDC_BTN_STATIC_VERT)  { g_staticLayout = 1; refresh_static_ui(); return 0; }
                if (cid == IDC_BTN_STATIC_COLOR) {
                    g_staticColors[g_staticActiveZone] = pick_color(hwnd, g_staticColors[g_staticActiveZone]);
                    InvalidateRect(g_btnStaticColor, NULL, TRUE);
                    return 0;
                }
            }
            switch (LOWORD(wp)) {
                case IDC_BTN_WHITE_CALIBRATE:
                    run_white_spectrum_calibration(hwnd);
                    return 0;
                case IDC_BTN_WHITE_RESET:
                    reset_white_spectrum_calibration();
                    return 0;
                case IDC_BTN_AUTOSTART: {
                    int exists = autostart_task_exists();
                    if (is_elevated()) {
                        autostart_apply(exists ? 0 : 1);
                    } else {
                        relaunch_elevated(exists ? L"--remove-autostart" : L"--install-autostart", 0);
                    }
                    update_autostart_button();
                    return 0;
                }
                case IDC_BTN_STOP:
                    stop_current_effect();
                    capture_current_power_profile();
                    update_power_profile_ui();
                    set_status(L"当前模式：无");
                    show_panel_for_mode(-1);
                    SendMessage(g_hComboMode, CB_SETCURSEL, (WPARAM)-1, 0);
                    save_config();
                    break;
                case IDC_BTN_COLOR_BREATH:
                    g_breathColor = pick_color(hwnd, g_breathColor);
                    set_swatch_color(g_hSwatchBreath, g_breathColor);
                    break;
                case IDC_CHK_BREATH_RANDOM:
                    g_breathRandomMode = IsDlgButtonChecked(hwnd, IDC_CHK_BREATH_RANDOM) == BST_CHECKED;
                    break;
                case IDC_BTN_COLOR_REACT:
                    g_reactiveColor = pick_color(hwnd, g_reactiveColor);
                    set_swatch_color(g_hSwatchReact, g_reactiveColor);
                    g_reactiveRandomMode = 0;
                    break;
                case IDC_BTN_REACT_RANDOM:
                    g_reactiveRandomMode = 1;
                    set_swatch_color(g_hSwatchReact, RGB(200,200,200));
                    break;
                case IDC_BTN_COLOR_RIPPLE:
                    g_rippleColor = pick_color(hwnd, g_rippleColor);
                    g_rippleRandomMode = 0;
                    set_swatch_color(g_hSwatchRipple, g_rippleColor);
                    break;
                case IDC_BTN_RIPPLE_RANDOM:
                    g_rippleRandomMode = 1;
                    set_swatch_color(g_hSwatchRipple, RGB(200,200,200));
                    break;
                case IDC_CHK_POWER_AUTO:
                    if (g_onAcPower) {
                        g_acBrightness = g_brightness;
                        g_acFps = g_targetFps;
                    } else {
                        g_batteryBrightness = g_brightness;
                        g_batteryFps = g_targetFps;
                    }
                    g_autoPowerProfiles = IsDlgButtonChecked(hwnd, IDC_CHK_POWER_AUTO) == BST_CHECKED;
                    update_power_profile_ui();
                    if (g_autoPowerProfiles) apply_power_profile((int)g_onAcPower, 1);
                    else save_config();
                    break;
                case IDC_CHK_WHEEL_REVERSE:
                    g_wheelReverse = (IsDlgButtonChecked(hwnd, IDC_CHK_WHEEL_REVERSE) == BST_CHECKED) ? 1 : 0;
                    break;
                case IDC_RADIO_SHORT:  g_reactiveDuration = 10; break;
                case IDC_RADIO_MEDIUM: g_reactiveDuration = 20; break;
                case IDC_RADIO_LONG:   g_reactiveDuration = 45; break;
                case IDC_TRAY_RESTORE:
                    ShowWindow(hwnd, SW_SHOW);
                    ShowWindow(hwnd, SW_RESTORE);
                    SetForegroundWindow(hwnd);
                    break;
                case IDC_TRAY_EXIT:
                    DestroyWindow(hwnd);
                    break;
            }
            return 0;

        case WM_TIMER:
            if (wp == IDT_TRAY_RETRY) {
                if (Shell_NotifyIcon(NIM_ADD, &g_nid)) {
                    KillTimer(hwnd, IDT_TRAY_RETRY);
                    log_event("tray icon added on retry");
                } else if (++g_trayRetries >= 30) {
                    KillTimer(hwnd, IDT_TRAY_RETRY);
                    log_event("tray icon retries exhausted");
                }
            } else if (wp == IDT_POWER_POLL) {
                poll_power_profile();
            }
            return 0;

        case WM_SYSCOMMAND:
            if ((wp & 0xFFF0) == SC_MINIMIZE) {
                ShowWindow(hwnd, SW_HIDE);
                return 0;
            }
            return DefWindowProc(hwnd, msg, wp, lp);

        case WM_CLOSE:
            ShowWindow(hwnd, SW_HIDE);
            return 0;

        case WM_TRAYICON:
            if (lp == WM_LBUTTONDBLCLK || lp == WM_LBUTTONUP) {
                ShowWindow(hwnd, SW_SHOW);
                ShowWindow(hwnd, SW_RESTORE);
                SetForegroundWindow(hwnd);
            } else if (lp == WM_RBUTTONUP) {
                POINT pt; GetCursorPos(&pt);
                HMENU menu = CreatePopupMenu();
                AppendMenu(menu, MF_STRING, IDC_TRAY_RESTORE, L"打开控制面板");
                AppendMenu(menu, MF_SEPARATOR, 0, NULL);
                AppendMenu(menu, MF_STRING, IDC_TRAY_EXIT, L"退出");
                SetForegroundWindow(hwnd);
                TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
                DestroyMenu(menu);
            }
            return 0;

        case WM_DEVICECHANGE: {
            if (wp == DBT_DEVICEARRIVAL || wp == DBT_DEVICEREMOVECOMPLETE) {
                char logbuf[128];
                sprintf(logbuf, "WM_DEVICECHANGE received, wParam=%lu", (unsigned long)wp);
                log_event(logbuf);
                request_reconnect("device-change");
            }
            return TRUE;
        }

        case WM_POWERBROADCAST: {
            char logbuf[128];
            sprintf(logbuf, "WM_POWERBROADCAST received, wParam=%lu", (unsigned long)wp);
            log_event(logbuf);
            if (wp == PBT_APMPOWERSTATUSCHANGE) {
                poll_power_profile();
            } else if (wp == PBT_APMRESUMEAUTOMATIC || wp == PBT_APMRESUMESUSPEND) {
                request_reconnect("power-resume");
                poll_power_profile();
            }
            return TRUE;
        }

        case WM_QUERYENDSESSION:
            graceful_shutdown_fade();
            return TRUE;

        case WM_ENDSESSION:
            if (!wp) cancel_shutdown_fade();
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd, IDT_TRAY_RETRY);
            KillTimer(hwnd, IDT_POWER_POLL);
            InterlockedExchange(&g_hotkeyStop, 1);
            InterlockedExchange(&g_touchbarStop, 1);
            graceful_shutdown_fade();
            Shell_NotifyIcon(NIM_DELETE, &g_nid);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/*
 * 生命周期入口：处理自启参数，恢复配置，建立单实例，连接 HID，启动 WMI/指标
 * 线程并创建窗口。所有提前返回路径都应避免遗留设备句柄或重复计划任务。
 */
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show) {
    (void)hPrev; (void)cmd; (void)show;
    srand((unsigned int)time(NULL));

    LPWSTR cl = GetCommandLineW();
    if (wcsstr(cl, L"--install-autostart")) {
        autostart_apply(1);
        return 0;
    }
    if (wcsstr(cl, L"--remove-autostart")) {
        autostart_apply(0);
        return 0;
    }
    g_startupLaunch = (wcsstr(cl, L"--startup") != NULL);
    InterlockedExchange(&g_onAcPower, query_ac_power());
    load_config();
    int relockMode = 0;
    DWORD relockParentPid = 0;
    DWORD elevatedParentPid = 0;
    {
        wchar_t *rl = wcsstr(cl, L"--relock");
        if (rl) {
            relockMode = 1;
            relockParentPid = (DWORD)wcstoul(rl + 8, NULL, 10);
        }
        wchar_t *ep = wcsstr(cl, L"--elevated-relock");
        if (ep) {
            ep += wcslen(L"--elevated-relock");
            elevatedParentPid = (DWORD)wcstoul(ep, NULL, 10);
        }
    }
    if (elevatedParentPid) {
        HANDLE hParent = OpenProcess(SYNCHRONIZE, FALSE, elevatedParentPid);
        if (hParent) {
            log_event("elevated relock: waiting for non-elevated parent to release single-instance lock");
            WaitForSingleObject(hParent, 10000);
            CloseHandle(hParent);
        }
        Sleep(150);
    }
    if (relockMode && relockParentPid) {
        HANDLE hParent = OpenProcess(SYNCHRONIZE, FALSE, relockParentPid);
        if (hParent) {
            WaitForSingleObject(hParent, 5000);
            CloseHandle(hParent);
        }
        Sleep(150);
    }

    HANDLE hSingleInstance = CreateMutexW(NULL, TRUE, L"BetterRGB_TheYamo_SingleInstance");
    if (hSingleInstance && GetLastError() == ERROR_ALREADY_EXISTS) {
        UINT showMsg = RegisterWindowMessageW(L"BetterRGB_TheYamo_Show");
        HWND prev = FindWindowW(L"RGBMainWindowClass", NULL);
        if (prev) {
            PostMessageW(prev, showMsg, 0, 0);
            ShowWindow(prev, SW_SHOW);
            ShowWindow(prev, SW_RESTORE);
            SetForegroundWindow(prev);
        } else {
            PostMessageW(HWND_BROADCAST, showMsg, 0, 0);
        }
        CloseHandle(hSingleInstance);
        return 0;
    }
    cleanup_retired_fps_capture_tools();

    InitializeCriticalSection(&g_deviceLock);
    hid_init();
    SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
    if (SetProcessShutdownParameters(SHUTDOWN_PRIORITY_LAST_APP, SHUTDOWN_NORETRY))
        log_event("shutdown order: registered in the last-application range");
    else
        log_event("shutdown order: SetProcessShutdownParameters failed");
    if (!resolve_device_path()) {
        if (relockMode) restart_vendor_service_if_found();
        MessageBoxW(NULL,
            L"未找到兼容的键盘控制器。\n"
            L"本程序仅支持带单键 RGB 接口的 ITE8291 键盘（USB 厂商 ID：048D）。\n"
            L"检测详情已写入程序目录中的 rgb_engine_log.txt。",
            L"Better RGB - 错误", MB_OK|MB_ICONERROR);
        return 1;
    }
    g_h = hid_open_path(g_devicePath);
    if (!g_h) {
        if (relockMode) restart_vendor_service_if_found();
        MessageBoxW(NULL,
            L"已找到键盘 RGB 接口，但无法打开。\n"
            L"其他程序可能正在独占该接口，请查看程序目录中的 rgb_engine_log.txt。",
            L"Better RGB - 错误", MB_OK|MB_ICONERROR);
        return 1;
    }
    if (enter_custom_mode() < 0) {
        if (relockMode) restart_vendor_service_if_found();
        MessageBoxW(NULL,
            L"无法进入单键 RGB 控制模式。",
            L"Better RGB - 错误", MB_OK|MB_ICONERROR);
        hid_close(g_h); hid_exit();
        return 1;
    }
    int excl = log_exclusive_status();
    if (relockMode) {
        char lb[128];
        int attempt = 0;
        while (!excl && attempt < 5) {
            attempt++;
            Sleep((DWORD)attempt * 1000);
            EnterCriticalSection(&g_deviceLock);
            if (g_h) hid_close(g_h);
            g_h = hid_open_path(g_devicePath);
            LeaveCriticalSection(&g_deviceLock);
            if (g_h && enter_custom_mode() == 0) {
                excl = log_exclusive_status();
                if (excl) {
                    sprintf(lb, "relock: exclusive acquired on retry %d", attempt);
                    log_event(lb);
                }
            }
        }
        restart_vendor_service_if_found();
        if (!g_h) {
            MessageBoxW(NULL,
                L"尝试重新取得独占控制后，无法再次打开键盘设备。",
                L"Better RGB - 错误", MB_OK|MB_ICONERROR);
            hid_exit();
            DeleteCriticalSection(&g_deviceLock);
            return 1;
        }
        if (!excl) {
            sprintf(lb, "relock: still SHARED after %d retries, refusing to start in shared mode", attempt);
            log_event(lb);
            MessageBoxW(NULL,
                L"无法取得设备独占权：官方服务仍占用键盘 RGB 接口。\n"
                L"为避免冲突，程序不会以共享模式启动。请稍后重试。",
                L"Better RGB - 设备占用", MB_OK|MB_ICONERROR);
            hid_close(g_h);
            hid_exit();
            DeleteCriticalSection(&g_deviceLock);
            return 1;
        }
    } else if (!excl) {
        if (is_elevated()) {
            if (begin_relock() == 0) {
                hid_exit();
                DeleteCriticalSection(&g_deviceLock);
                return 0;
            }
            MessageBoxW(NULL,
                L"无法停止官方灯光服务或重新取得 RGB 接口独占权。\n"
                L"为保护键盘 EC，程序不会以共享模式运行，请关闭控制中心后重试。",
                L"Better RGB - 无法取得独占控制", MB_OK|MB_ICONERROR);
            hid_close(g_h);
            hid_exit();
            DeleteCriticalSection(&g_deviceLock);
            return 1;
        } else {
            int ans = MessageBoxW(NULL,
                L"官方服务（GCUBridge / 控制中心）正在占用键盘 RGB 接口，可能随时覆盖本程序的灯效。\n\n"
                L"是否立即处理？程序会短暂重启官方服务并重新启动，以取得 RGB 接口独占权。风扇、性能和电源功能不受影响，Windows 会请求管理员权限。\n\n"
                L"选择“否”将安全退出，本版本不允许共享模式。",
                L"Better RGB - 需要独占 RGB 接口", MB_YESNO|MB_ICONWARNING);
            if (ans == IDYES) {
                wchar_t relaunchArgs[64];
                _snwprintf(relaunchArgs, 63, L"--elevated-relock %lu", (unsigned long)GetCurrentProcessId());
                relaunchArgs[63] = 0;
                if (relaunch_elevated(relaunchArgs, 0) == 0) {
                    hid_close(g_h);
                    hid_exit();
                    DeleteCriticalSection(&g_deviceLock);
                    return 0;
                }
                log_event("elevation cancelled or failed, exiting without the lock");
                MessageBoxW(NULL,
                    L"未获得管理员权限，无法安全取得 RGB 接口独占权。\n"
                    L"程序即将关闭。请重新启动并允许管理员权限请求。",
                    L"Better RGB - 权限不足", MB_OK|MB_ICONERROR);
                hid_close(g_h);
                hid_exit();
                DeleteCriticalSection(&g_deviceLock);
                return 1;
            }
            if (ans == IDNO) {
                log_event("user declined exclusive relock; exiting because shared mode is disabled");
                hid_close(g_h);
                hid_exit();
                DeleteCriticalSection(&g_deviceLock);
                return 0;
            }
        }
    }

#define ENABLE_EC_MONITOR 0
#if ENABLE_EC_MONITOR
    HANDLE hMonitor = CreateThread(NULL, 0, ec_monitor_thread, NULL, 0, NULL);
#else
    HANDLE hMonitor = NULL;
#endif

    INITCOMMONCONTROLSEX icc; icc.dwSize = sizeof(icc); icc.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASS swc = {0};
    swc.lpfnWndProc = SwatchProc;
    swc.hInstance = hInst;
    swc.lpszClassName = L"SwatchClass";
    swc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    RegisterClass(&swc);

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"RGBMainWindowClass";
    g_bgBrush = CreateSolidBrush(CLR_BG);
    wc.hbrBackground = g_bgBrush;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON));
    if (!RegisterClass(&wc)) {
        MessageBox(NULL, L"窗口类注册失败。", L"Better RGB - 错误", MB_OK|MB_ICONERROR);
        hid_close(g_h); hid_exit();
        return 1;
    }

    HWND hwnd = CreateWindow(wc.lpszClassName, L"同模具 ITE8291 - Better RGB 中文增强版 3.6.1",
        WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 760, 540,
        NULL, NULL, hInst, NULL);
    if (!hwnd) {
        MessageBox(NULL, L"主窗口创建失败。", L"Better RGB - 错误", MB_OK|MB_ICONERROR);
        hid_close(g_h); hid_exit();
        return 1;
    }

    InterlockedExchange(&g_hotkeyStop, 0);
    g_hotkeyThread = CreateThread(NULL, 0, fn_hotkey_wmi_thread, NULL, 0, NULL);
    if (!g_hotkeyThread)
        log_event("Fn hotkey WMI: could not create listener thread");

    InterlockedExchange(&g_touchbarStop, 0);
    g_touchbarThread = CreateThread(NULL, 0, touchbar_metrics_thread, NULL, 0, NULL);
    if (!g_touchbarThread)
        log_event("touchbar metrics: could not create worker thread");

    if (g_startupLaunch) {
        log_event("startup launch: starting hidden in the system tray");
    } else {
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }

    if (g_wheelReverse) CheckDlgButton(hwnd, IDC_CHK_WHEEL_REVERSE, BST_CHECKED);
    CheckRadioButton(hwnd, IDC_RADIO_SHORT, IDC_RADIO_LONG,
        g_reactiveDuration == 10 ? IDC_RADIO_SHORT : (g_reactiveDuration == 45 ? IDC_RADIO_LONG : IDC_RADIO_MEDIUM));
    SendMessage(g_hComboMatrixStyle, CB_SETCURSEL, (WPARAM)g_matrixStyle, 0);
    if (g_cfgMode >= 0 && g_cfgMode < MODE_COUNT) {
        SendMessage(g_hComboMode, CB_SETCURSEL, (WPARAM)g_cfgMode, 0);
        show_panel_for_mode((int)g_cfgMode);
        if (g_cfgMode == MODE_RAIN) refresh_rain_color_visibility();
        if (g_cfgMode == MODE_MATRIX) refresh_matrix_color_visibility();
        if (g_cfgMode == MODE_STATIC) refresh_static_ui();
        start_mode((int)g_cfgMode);
    }

    HPOWERNOTIFY hPowerNotify = RegisterSuspendResumeNotification((HANDLE)hwnd, DEVICE_NOTIFY_WINDOW_HANDLE);
    if (!hPowerNotify) {
        log_event("RegisterSuspendResumeNotification FAILED");
    } else {
        log_event("RegisterSuspendResumeNotification succeeded");
    }

    DEV_BROADCAST_DEVICEINTERFACE devFilter = {0};
    devFilter.dbcc_size = sizeof(devFilter);
    devFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    devFilter.dbcc_classguid = GUID_DEVINTERFACE_HID_LOCAL;
    HDEVNOTIFY hDevNotify = RegisterDeviceNotification(hwnd, &devFilter, DEVICE_NOTIFY_WINDOW_HANDLE);
    if (!hDevNotify) {
        log_event("RegisterDeviceNotification FAILED");
    } else {
        log_event("RegisterDeviceNotification succeeded");
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (hPowerNotify) UnregisterSuspendResumeNotification(hPowerNotify);

    InterlockedExchange(&g_hotkeyStop, 1);
    if (g_hotkeyThread) {
        WaitForSingleObject(g_hotkeyThread, 2500);
        CloseHandle(g_hotkeyThread);
        g_hotkeyThread = NULL;
    }

    InterlockedExchange(&g_touchbarStop, 1);
    if (g_touchbarThread) {
        WaitForSingleObject(g_touchbarThread, 2500);
        CloseHandle(g_touchbarThread);
        g_touchbarThread = NULL;
    }

    InterlockedExchange(&g_monitorStop, 1);
    if (hMonitor) {
        WaitForSingleObject(hMonitor, 1500);
        CloseHandle(hMonitor);
    }

    hid_close(g_h);
    hid_exit();
    DeleteCriticalSection(&g_deviceLock);
    return 0;
}
