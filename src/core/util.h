#pragma once
// Formatting, text and timing helpers.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>

std::string fmt(const char* f, double a, double b = 0, double c = 0);   // printf with up to three numbers
std::string fmtDuration(double seconds);                                 // "0.80 s", "11 min 23 s", "2 h 03 min"
std::string mmss(float seconds);                                         // "1:05.3"
std::wstring widen(const std::string& utf8);
std::string timestamp();                                                 // "20261102_141530"

double nowSeconds();
double secondsSince(LARGE_INTEGER t0);
