#ifndef BETTERRGB_COLOR_MATH_H
#define BETTERRGB_COLOR_MATH_H
#include <windows.h>
double srgb_channel_to_linear(unsigned char value);
unsigned char linear_channel_to_srgb(double value);
int calculate_white_calibration(COLORREF observed, LONG *r, LONG *g, LONG *b);
#endif
