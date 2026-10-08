#include "core.hpp"
#include <cstdarg>
#include <cstdio>

Color hsv(float h, float s, float v) {
    h = std::fmod(h, 360.0f);
    if (h < 0) h += 360.0f;
    float c = v * s, x = c * (1 - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1)), m = v - c;
    float r = 0, g = 0, b = 0;
    if (h < 60) r = c, g = x;
    else if (h < 120) r = x, g = c;
    else if (h < 180) g = c, b = x;
    else if (h < 240) g = x, b = c;
    else if (h < 300) r = x, b = c;
    else r = c, b = x;
    return rgba((int)((r + m) * 255), (int)((g + m) * 255), (int)((b + m) * 255));
}

Mat4 identity() {
    Mat4 r{};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1;
    return r;
}

Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int c = 0; c < 4; c++)
        for (int row = 0; row < 4; row++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = s;
        }
    return r;
}

Mat4 translate(float x, float y, float z) {
    Mat4 r = identity();
    r.m[12] = x;
    r.m[13] = y;
    r.m[14] = z;
    return r;
}

Mat4 rotX(float deg) {
    float c = std::cos(deg * kDeg), s = std::sin(deg * kDeg);
    Mat4 r = identity();
    r.m[5] = c;
    r.m[6] = s;
    r.m[9] = -s;
    r.m[10] = c;
    return r;
}

Mat4 rotY(float deg) {
    float c = std::cos(deg * kDeg), s = std::sin(deg * kDeg);
    Mat4 r = identity();
    r.m[0] = c;
    r.m[2] = -s;
    r.m[8] = s;
    r.m[10] = c;
    return r;
}

Mat4 rotZ(float deg) {
    float c = std::cos(deg * kDeg), s = std::sin(deg * kDeg);
    Mat4 r = identity();
    r.m[0] = c;
    r.m[1] = s;
    r.m[4] = -s;
    r.m[5] = c;
    return r;
}

Mat4 scale(float x, float y, float z) {
    Mat4 r = identity();
    r.m[0] = x;
    r.m[5] = y;
    r.m[10] = z;
    return r;
}

Mat4 perspective(float fovy, float aspect, float n, float f) {
    float t = 1.0f / std::tan(fovy * kDeg * 0.5f);
    Mat4 r{};
    r.m[0] = t / aspect;
    r.m[5] = t;
    r.m[10] = (f + n) / (n - f);
    r.m[11] = -1;
    r.m[14] = 2 * f * n / (n - f);
    return r;
}

Mat4 ortho(float l, float r, float b, float t, float n, float f) {
    Mat4 o = identity();
    o.m[0] = 2 / (r - l);
    o.m[5] = 2 / (t - b);
    o.m[10] = -2 / (f - n);
    o.m[12] = -(r + l) / (r - l);
    o.m[13] = -(t + b) / (t - b);
    o.m[14] = -(f + n) / (f - n);
    return o;
}

Mat4 lookAt(Vec3 eye, Vec3 target, Vec3 up) {
    Vec3 f = normalize(target - eye);
    Vec3 s = normalize(cross(f, up));
    Vec3 u = cross(s, f);
    Mat4 r = identity();
    r.m[0] = s.x;
    r.m[4] = s.y;
    r.m[8] = s.z;
    r.m[1] = u.x;
    r.m[5] = u.y;
    r.m[9] = u.z;
    r.m[2] = -f.x;
    r.m[6] = -f.y;
    r.m[10] = -f.z;
    r.m[12] = -dot(s, eye);
    r.m[13] = -dot(u, eye);
    r.m[14] = dot(f, eye);
    return r;
}

Vec3 xformPoint(const Mat4& m, Vec3 p) {
    return {m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z + m.m[12],
            m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z + m.m[13],
            m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z + m.m[14]};
}

Vec3 xformDir(const Mat4& m, Vec3 d) {
    return {m.m[0] * d.x + m.m[4] * d.y + m.m[8] * d.z, m.m[1] * d.x + m.m[5] * d.y + m.m[9] * d.z,
            m.m[2] * d.x + m.m[6] * d.y + m.m[10] * d.z};
}

bool rayBox(const Ray& r, const BBox& b, float& tOut) {
    float tmin = 0.0f, tmax = 1e30f;
    const float o[3] = {r.pos.x, r.pos.y, r.pos.z}, d[3] = {r.dir.x, r.dir.y, r.dir.z};
    const float lo[3] = {b.min.x, b.min.y, b.min.z}, hi[3] = {b.max.x, b.max.y, b.max.z};
    for (int i = 0; i < 3; i++) {
        if (std::fabs(d[i]) < 1e-9f) {
            if (o[i] < lo[i] || o[i] > hi[i]) return false;
        } else {
            float t1 = (lo[i] - o[i]) / d[i], t2 = (hi[i] - o[i]) / d[i];
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return false;
        }
    }
    tOut = tmin;
    return true;
}

static uint32_t g_seed = 0x9E3779B9u;
uint32_t rnd() {
    g_seed ^= g_seed << 13;
    g_seed ^= g_seed >> 17;
    g_seed ^= g_seed << 5;
    return g_seed;
}
float rndf() { return (rnd() & 0xFFFFFF) / float(0x1000000); }
int rndi(int lo, int hi) { return lo + (int)(rnd() % (uint32_t)(hi - lo + 1)); }

std::string fmt(const char* f, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, f);
    vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return buf;
}

std::string money(double v) {
    long long cents = (long long)std::llround(v * 100);
    bool neg = cents < 0;
    if (neg) cents = -cents;
    std::string w = std::to_string(cents / 100), out;
    int cnt = 0;
    for (int i = (int)w.size() - 1; i >= 0; i--) {
        out.insert(out.begin(), w[i]);
        if (++cnt % 3 == 0 && i > 0) out.insert(out.begin(), ' ');
    }
    return std::string(neg ? "-$" : "$") + out;
}
