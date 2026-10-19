#include <math.h>
#include "color_math.h"

double srgb_channel_to_linear(unsigned char value) {
    double c = (double)value / 255.0;
    return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

unsigned char linear_channel_to_srgb(double value) {
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
    return gain < 50 ? 50 : gain > 1000 ? 1000 : gain;
}

/*
 * 用户在光谱中选择“纯白指令下肉眼看到的实际颜色”。先转到线性光空间，
 * 再以最弱通道为基准反算三个独立增益；只衰减过强通道，避免任何通道溢出。
 */
int calculate_white_calibration(COLORREF observed,
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
