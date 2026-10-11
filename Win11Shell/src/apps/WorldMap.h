#pragma once
// Упрощённая карта мира (равнопромежуточная проекция), нарисованная из полигонов материков,
// и граница дня и ночи по положению Солнца. Используется приложением «Часы».

#include "../Renderer.h"

namespace WorldMap
{
constexpr float kTopLat = 84.0f;
constexpr float kBottomLat = -58.0f;
constexpr float kAspect = 360.0f / (kTopLat - kBottomLat);

// Пары «долгота, широта».
const float kNorthAmerica[] = {
    -168, 66, -162, 70, -156, 71.5f, -140, 70, -128, 70.5f, -115, 68, -95, 72, -82, 73, -80, 68, -90, 64, -94, 59,
    -87, 55.5f, -82, 52.5f, -79, 55, -77, 60, -72, 61.5f, -65, 60, -61, 56, -56, 52, -59, 48, -65, 49, -70, 47,
    -66, 44.5f, -70, 42, -74, 40.5f, -76, 37, -76, 35, -81, 31.5f, -80, 27, -80.5f, 25.2f, -82, 26.5f, -83, 29.5f,
    -89, 30.2f, -94, 29.5f, -97, 27.5f, -97.5f, 22, -95.5f, 18.8f, -91, 18.5f, -90.5f, 21, -87, 21.5f, -88, 16,
    -84, 15, -83.5f, 11, -79.5f, 9, -77.5f, 8, -80.5f, 7.5f, -85, 10, -87.5f, 13, -92, 14.5f, -96, 15.7f,
    -105, 19.5f, -106, 23, -110, 24, -112.5f, 29, -114.5f, 31.5f, -111, 26, -109.5f, 23, -112, 25, -114, 28,
    -117, 32.5f, -120.5f, 34.5f, -124, 40.5f, -124, 46, -124.7f, 48.4f, -123, 49, -128, 51, -131, 54, -134, 57.5f,
    -140, 59.8f, -146, 60.5f, -152, 59, -157, 57, -162, 55, -158, 58.5f, -162, 60, -165, 62.5f, -164, 64.5f,
};
const float kSouthAmerica[] = {
    -77.5f, 8, -72, 12, -63, 10.5f, -60, 8.5f, -52, 5, -50, 0, -44, -2.5f, -35, -5.5f, -35, -9, -39, -13.5f, -39, -18,
    -41, -22, -48, -25.5f, -48.5f, -28.5f, -53, -33.5f, -58, -34.5f, -57, -38, -62, -39, -65, -41, -65, -45,
    -67.5f, -46.5f, -69, -51, -68.5f, -53, -71.5f, -54, -74.5f, -51, -74, -46, -73.5f, -41, -73.5f, -37, -71.5f, -30,
    -70.5f, -23, -70.3f, -18.5f, -76, -14, -79, -8, -81, -5, -80, -1, -80, 1, -78, 3, -77.5f, 7,
};
const float kEurasia[] = {
    -9.5f, 37, -9, 43, -1.5f, 43.5f, -1.5f, 46.5f, -4.5f, 48, -1.5f, 49.5f, 2, 51, 4.5f, 52.5f, 8, 53.5f, 8.5f, 55.5f,
    10, 54.5f, 14, 54, 19, 54.5f, 21, 57, 24, 59.5f, 29.5f, 60, 23, 60.5f, 21.5f, 63, 25, 65.5f, 21, 65, 17.5f, 62.5f,
    19, 60, 16.5f, 57.5f, 13, 55.5f, 11, 58.5f, 8, 58, 5, 59, 5, 62, 10, 64, 14, 67.5f, 17, 69.5f, 23, 70.8f, 28, 71,
    31, 70, 33, 69, 41, 67.5f, 40, 65, 44, 66.3f, 44, 68.5f, 54, 68.5f, 60, 69.5f, 68, 69, 73, 72.5f, 80, 73, 87, 75,
    98, 76, 105, 77.5f, 113, 73.5f, 125, 73.5f, 140, 72.5f, 150, 71.5f, 160, 70, 170, 69.5f, 180, 68.5f, 180, 65,
    178, 64.5f, 174, 61.5f, 164, 60, 163, 57.5f, 156, 51, 156, 57, 162, 61.5f, 156, 61.5f, 151, 59, 142, 59.3f,
    135, 54.5f, 141, 52, 140, 48, 133, 43, 130, 42.5f, 129, 35.5f, 126.5f, 34.5f, 126, 37.5f, 125, 40, 121.5f, 39,
    121.5f, 40.5f, 118, 39, 119, 37, 122.5f, 37, 119.5f, 35, 121.5f, 31.5f, 122, 29, 119.5f, 25.5f, 116, 22.8f,
    110.5f, 21, 109, 21.5f, 106.5f, 20, 105.5f, 18.5f, 109, 15, 109, 11.5f, 105, 8.6f, 103.5f, 10.5f, 100.5f, 13.5f,
    99, 10, 100.5f, 7, 103.5f, 1.5f, 101, 2.8f, 98.5f, 8, 98.5f, 13, 97.5f, 16.5f, 94.5f, 16, 94, 19, 92, 21.5f,
    89, 22, 87, 21.5f, 85, 19.5f, 80.5f, 15.5f, 80, 10, 77.5f, 8, 76.5f, 9.5f, 73, 17, 72.5f, 21, 69, 22.5f,
    67, 24.8f, 61.5f, 25.2f, 57, 25.7f, 56.5f, 27, 52, 27.8f, 50, 30, 48, 30, 48.5f, 28.5f, 50.8f, 24.8f, 51.5f, 24.2f,
    56, 26, 56.5f, 24.5f, 59.8f, 22.5f, 55.5f, 17.8f, 52, 16.5f, 45, 13, 43.3f, 12.8f, 42.5f, 15.5f, 39, 21.5f,
    35, 28, 34.5f, 29.5f, 32.5f, 30, 35, 32.5f, 36, 35.5f, 36, 36.8f, 32, 36.2f, 28, 36.7f, 26.5f, 39.5f, 26, 40.7f,
    23, 40.5f, 24, 38, 22.5f, 36.5f, 21, 38.5f, 19.5f, 40.5f, 19.5f, 42, 16, 43.5f, 13.5f, 45.5f, 12.3f, 45.2f,
    12.5f, 44, 16, 41.5f, 18.5f, 40, 16, 38, 15.7f, 40, 12.5f, 41.7f, 10.5f, 43, 8.5f, 44.3f, 6, 43, 3.5f, 43.3f,
    3, 41.8f, 0.5f, 40.5f, -0.5f, 38.5f, -2, 36.8f, -5.5f, 36, -7, 37,
};
const float kAfrica[] = {
    -17, 21, -16, 24, -13, 27.5f, -10, 29.5f, -9.5f, 32, -6.5f, 34, -5.5f, 35.8f, -2, 35.1f, 3, 36.8f, 10, 37.3f,
    11, 35.5f, 10, 34, 11, 33.2f, 15, 32.3f, 19, 30.3f, 20, 32, 23, 32.6f, 25, 31.6f, 29, 30.9f, 32.5f, 31.2f, 33, 28,
    35.5f, 24, 37, 21, 38.5f, 18, 39.5f, 15.5f, 43.3f, 12.5f, 44, 10.5f, 51, 11.8f, 51, 10.5f, 49, 6, 46, 2,
    41.5f, -2, 39.5f, -5, 39, -8, 40.5f, -10.5f, 40.5f, -15, 37, -17.5f, 35.5f, -22, 32.5f, -25.5f, 32.8f, -28.5f,
    31, -30.5f, 27.5f, -33.5f, 22.5f, -34, 18.5f, -34.2f, 18, -32, 15, -27, 14.5f, -22.5f, 11.8f, -17.5f, 13.5f, -11.5f,
    12.2f, -6, 9, -1, 9.8f, 3.5f, 8.5f, 4.5f, 6, 4.3f, 2, 6.3f, -2, 4.8f, -7.5f, 4.4f, -11.5f, 6.8f, -13.5f, 9.5f,
    -16.8f, 12.5f, -17.5f, 14.7f, -16.5f, 16.5f, -16, 19,
};
const float kAustralia[] = {
    113, -22, 114, -26, 115, -33.5f, 117.5f, -35, 123.5f, -34, 129, -31.5f, 134, -32.5f, 138, -35.5f, 140, -38,
    144, -38.5f, 147, -38.5f, 150, -37.5f, 151.5f, -33, 153, -30, 153, -25.5f, 150.5f, -22.5f, 146, -18.5f,
    145.3f, -14.5f, 143.5f, -14, 142.5f, -10.7f, 141.5f, -13, 141.5f, -17, 139.5f, -17.5f, 136, -15.5f, 137, -12,
    132.5f, -11.3f, 130, -12.5f, 129.5f, -15, 126, -14, 122, -17.5f, 122, -18.5f, 118.5f, -20.3f,
};
const float kGreenland[] = {-73, 78, -60, 82, -40, 83.5f, -22, 82.5f, -18, 76, -22, 70.5f, -32, 68, -40, 65,
                            -44, 60, -50, 62, -53, 66.5f, -54, 70, -58, 75.5f, -68, 77};
const float kBritain[] = {-5.7f, 50, 1.4f, 51.2f, 1.7f, 52.7f, 0.2f, 53.5f, -1.5f, 55, -2, 56, -1.8f, 57.6f, -4, 58.6f,
                          -5, 58.6f, -6, 57, -5.5f, 55.8f, -4.8f, 54.8f, -3, 54, -4.5f, 53.3f, -4.2f, 52.1f, -5.2f, 51.7f, -3, 51.4f};
const float kIreland[] = {-6, 52, -6, 53.5f, -5.5f, 54.5f, -7.2f, 55.3f, -8.5f, 54.5f, -10, 53.5f, -10.3f, 51.8f, -8, 51.6f};
const float kIceland[] = {-24, 65.5f, -22, 66.4f, -15, 66.3f, -13.5f, 65, -18.5f, 63.4f, -22.5f, 63.8f};
const float kHonshu[] = {130, 31, 131.5f, 31.5f, 132, 33.8f, 135, 33.5f, 136.5f, 34.3f, 139, 34.8f, 140.8f, 35.7f, 141, 38.3f,
                         142, 39.5f, 141.5f, 41.2f, 140, 41.2f, 140, 40, 139.7f, 38.2f, 138.5f, 37.5f, 137, 37, 136, 35.7f,
                         133, 35.5f, 131, 34.4f, 130.4f, 33.6f, 129.7f, 32.8f};
const float kHokkaido[] = {140, 42, 141.5f, 42.5f, 143.5f, 42, 145.5f, 43.3f, 142, 45.5f, 141.5f, 43.5f, 140.4f, 43.2f};
const float kSumatra[] = {95.3f, 5.6f, 98, 4, 103.5f, -1, 106, -3, 106, -5.8f, 104.5f, -5.9f, 101, -2.5f, 98.7f, 1.5f};
const float kJava[] = {105.2f, -6.8f, 108, -6.3f, 111, -6.5f, 114.5f, -7.8f, 114.5f, -8.7f, 110, -8.2f, 106.5f, -7.4f};
const float kBorneo[] = {109, 1.5f, 110, -1.5f, 111, -3, 114.5f, -4, 116.5f, -2.5f, 118, 1, 119, 5, 117, 7, 115.5f, 5.5f,
                         113, 3.2f, 111, 2};
const float kSulawesi[] = {119.5f, -5.5f, 120.5f, -2.5f, 121, 1, 125, 1.5f, 123, 0.5f, 121.5f, -1, 122.5f, -4.5f, 120.5f, -5.5f};
const float kNewGuinea[] = {131, -1, 134, -1, 138, -1.7f, 141, -2.6f, 145, -4.2f, 147.5f, -6, 147.5f, -8, 150, -10.5f,
                            146, -8.6f, 144, -7.7f, 141, -9, 138.5f, -8.3f, 138, -5.5f, 135, -4.4f, 132.5f, -4, 132, -2.8f};
const float kLuzon[] = {120, 18.5f, 122.3f, 18.5f, 122, 16, 124, 13, 121.5f, 13.8f, 120.6f, 14.5f, 120, 16};
const float kMindanao[] = {122, 7, 125.5f, 9.5f, 126.5f, 7.3f, 125.5f, 5.8f, 123.5f, 7.8f};
const float kSriLanka[] = {79.8f, 9.5f, 81.8f, 7.5f, 81.2f, 6.2f, 80, 6.1f};
const float kTaiwan[] = {120.1f, 23, 121, 25.2f, 122, 25, 121, 22};
const float kMadagascar[] = {49.3f, -12, 50.5f, -15.5f, 49.5f, -17, 47, -25, 45, -25.5f, 43.5f, -22, 44, -17, 47, -15.5f};
const float kTasmania[] = {144.7f, -40.7f, 148.3f, -40.9f, 148, -43.2f, 146, -43.6f};
const float kNzNorth[] = {172.7f, -34.4f, 174.5f, -36.5f, 178.5f, -37.7f, 177, -39.3f, 176, -41.3f, 174.8f, -41.3f, 175, -40,
                          173.8f, -39.2f, 174.5f, -37.5f};
const float kNzSouth[] = {172.7f, -40.5f, 174.3f, -41.7f, 173, -43.5f, 171, -44.9f, 169, -46.6f, 166.5f, -46, 168, -44, 171.5f, -41.7f};
const float kCuba[] = {-85, 21.9f, -80, 23.2f, -77, 21.8f, -74.2f, 20.2f, -77.5f, 19.9f, -81.5f, 22.2f};
const float kHispaniola[] = {-74.4f, 19.8f, -71, 19.9f, -68.4f, 18.6f, -71.5f, 17.6f, -74.4f, 18.3f};
const float kNovayaZemlya[] = {52, 71.5f, 56, 73.5f, 62, 76.5f, 68, 76.8f, 60, 74, 56, 71};
const float kSvalbard[] = {11, 78.5f, 16, 80, 27, 80.2f, 22, 78, 16, 76.8f};
// Внутренние моря (рисуются цветом воды поверх суши).
const float kBlackSea[] = {28, 41.5f, 29, 41.1f, 31, 41.1f, 33.5f, 42, 36, 41.6f, 38.5f, 40.9f, 41.5f, 41.5f, 41.5f, 42.5f,
                           39, 44.3f, 37.5f, 44.8f, 38, 46.5f, 35, 45.5f, 33.5f, 44.5f, 32.5f, 45.5f, 30.5f, 46.5f, 29.7f, 45, 28.5f, 43.5f};
const float kCaspian[] = {49, 46.5f, 47, 45, 47.5f, 43, 49.5f, 40.5f, 49, 38, 51, 36.8f, 54, 37, 53.8f, 40, 52.8f, 41.5f,
                          53.5f, 42.5f, 51.5f, 44.5f, 53, 45.5f, 51.5f, 47};

struct Shape
{
    const float* pts;
    size_t count; // число чисел (пар * 2)
};

template <size_t N>
constexpr Shape MakeShape(const float (&a)[N])
{
    return Shape{a, N};
}

inline const std::vector<Shape>& Land()
{
    static const std::vector<Shape> v = {
        MakeShape(kNorthAmerica), MakeShape(kSouthAmerica), MakeShape(kEurasia),  MakeShape(kAfrica),
        MakeShape(kAustralia),    MakeShape(kGreenland),    MakeShape(kBritain),  MakeShape(kIreland),
        MakeShape(kIceland),      MakeShape(kHonshu),       MakeShape(kHokkaido), MakeShape(kSumatra),
        MakeShape(kJava),         MakeShape(kBorneo),       MakeShape(kSulawesi), MakeShape(kNewGuinea),
        MakeShape(kLuzon),        MakeShape(kMindanao),     MakeShape(kSriLanka), MakeShape(kTaiwan),
        MakeShape(kMadagascar),   MakeShape(kTasmania),     MakeShape(kNzNorth),  MakeShape(kNzSouth),
        MakeShape(kCuba),         MakeShape(kHispaniola),   MakeShape(kNovayaZemlya), MakeShape(kSvalbard),
    };
    return v;
}
inline const std::vector<Shape>& Seas()
{
    static const std::vector<Shape> v = {MakeShape(kBlackSea), MakeShape(kCaspian)};
    return v;
}

inline Point Project(const Rect& m, float lon, float lat)
{
    return {m.x + (lon + 180.0f) / 360.0f * m.w, m.y + (kTopLat - lat) / (kTopLat - kBottomLat) * m.h};
}

inline void FillShapes(Renderer& r, const Rect& m, const std::vector<Shape>& shapes, Color c)
{
    for (const Shape& s : shapes)
    {
        std::vector<Point> pts;
        pts.reserve(s.count / 2);
        for (size_t i = 0; i + 1 < s.count; i += 2)
            pts.push_back(Project(m, s.pts[i], s.pts[i + 1]));
        r.FillPolygon(pts, c);
    }
}

// Склонение Солнца (градусы) и подсолнечная долгота для текущего момента UTC.
inline void SunPosition(const SYSTEMTIME& utc, float& declination, float& subsolarLon)
{
    static const int kCum[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    const int month = Clamp(static_cast<int>(utc.wMonth), 1, 12);
    const int dayOfYear = kCum[month - 1] + static_cast<int>(utc.wDay) - 1;
    declination = -23.44f * std::cos(2.0f * kPi / 365.0f * static_cast<float>(dayOfYear + 10));
    const float hours = static_cast<float>(utc.wHour) + static_cast<float>(utc.wMinute) / 60.0f + static_cast<float>(utc.wSecond) / 3600.0f;
    subsolarLon = -15.0f * (hours - 12.0f);
}

// Многоугольник ночной стороны (граница дня и ночи, замкнутая к полюсу, где ночь).
inline std::vector<Point> NightPolygon(const Rect& m, const SYSTEMTIME& utc, float latShift = 0.0f)
{
    float decl = 0.0f, sunLon = 0.0f;
    SunPosition(utc, decl, sunLon);
    if (std::fabs(decl) < 0.1f)
        decl = decl < 0.0f ? -0.1f : 0.1f;
    const float tanDecl = std::tan(decl * kPi / 180.0f);
    std::vector<Point> pts;
    const int steps = 96;
    for (int i = 0; i <= steps; ++i)
    {
        const float lon = -180.0f + 360.0f * static_cast<float>(i) / static_cast<float>(steps);
        const float h = (lon - sunLon) * kPi / 180.0f;
        const float lat = std::atan(-std::cos(h) / tanDecl) * 180.0f / kPi + latShift;
        pts.push_back(Project(m, lon, lat));
    }
    // Летом в северном полушарии ночь — к югу от линии, зимой — к северу.
    const float edgeY = decl > 0.0f ? m.Bottom() + 2.0f : m.y - 2.0f;
    pts.push_back({m.Right() + 2.0f, edgeY});
    pts.push_back({m.x - 2.0f, edgeY});
    return pts;
}
} // namespace WorldMap
