#include "core/util.h"
#include <cmath>
#include <cstdio>
#include <ctime>

std::string fmt(const char* f, double a, double b, double c) {
    char buf[128]; std::snprintf(buf, sizeof buf, f, a, b, c); return buf;
}
std::string fmtDuration(double s) {
    if (s < 1.0)    return fmt("%.2f s", s);
    if (s < 60.0)   return fmt("%.1f s", s);
    if (s < 3600.0) return fmt("%.0f min %02.0f s", std::floor(s / 60), std::fmod(s, 60.0));
    if (s < 86400.0 * 2) return fmt("%.0f h %02.0f min", std::floor(s / 3600), std::floor(std::fmod(s, 3600.0) / 60));
    return fmt("%.1f days", s / 86400.0);
}
std::string mmss(float t) {
    t = std::max(0.0f, t);
    char b[32]; std::snprintf(b, sizeof b, "%d:%04.1f", (int)(t / 60), std::fmod(t, 60.0f));
    return b;
}

std::wstring widen(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}
std::string timestamp() {
    char b[64]; time_t t = time(nullptr); tm lt; localtime_s(&lt, &t);
    std::strftime(b, sizeof b, "%Y%m%d_%H%M%S", &lt);
    return b;
}

double nowSeconds() {
    LARGE_INTEGER f, n; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&n);
    return double(n.QuadPart) / double(f.QuadPart);
}

double secondsSince(LARGE_INTEGER t0) {
    LARGE_INTEGER f, n; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&n);
    return double(n.QuadPart - t0.QuadPart) / double(f.QuadPart);
}
