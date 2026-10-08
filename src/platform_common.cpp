#include "platform.hpp"
#include <cstring>

namespace plat {

bool saveScreenshotBMP(const std::string& path, int w, int h, const unsigned char* rgba) {
    // rgba — снизу вверх, как отдаёт glReadPixels; BMP тоже хранит строки снизу вверх
    int rowSize = (w * 3 + 3) & ~3;
    std::string d(54 + rowSize * h, '\0');
    auto put32 = [&](int off, unsigned v) { memcpy(&d[off], &v, 4); };
    auto put16 = [&](int off, unsigned short v) { memcpy(&d[off], &v, 2); };
    d[0] = 'B';
    d[1] = 'M';
    put32(2, (unsigned)d.size());
    put32(10, 54);
    put32(14, 40);
    put32(18, (unsigned)w);
    put32(22, (unsigned)h);
    put16(26, 1);
    put16(28, 24);
    put32(34, (unsigned)(rowSize * h));
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const unsigned char* p = rgba + (y * w + x) * 4;
            char* q = &d[54 + y * rowSize + x * 3];
            q[0] = (char)p[2];
            q[1] = (char)p[1];
            q[2] = (char)p[0];
        }
    return writeFile(path, d);
}

}  // namespace plat
