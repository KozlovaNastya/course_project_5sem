// GaussianBlurCore.cpp - см. GaussianBlurCore.h
#include "GaussianBlurCore.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace gb {

bool SampleTypeFromDepth(int32_t depth, SampleType& out) {
    switch (depth) {
        case 8:  out = SampleType::U8;  return true;
        case 16: out = SampleType::U16; return true;
        case 32: out = SampleType::F32; return true;
        default: return false;
    }
}

int32_t SampleSize(SampleType type) {
    switch (type) {
        case SampleType::U8:  return 1;
        case SampleType::U16: return 2;
        case SampleType::F32: return 4;
    }
    return 0;
}

TileGrid MakeTileGrid(const IRect& area, int32_t tileW, int32_t tileH) {
    TileGrid g;
    g.area  = area;
    g.tileW = tileW > 0 ? tileW : 1;
    g.tileH = tileH > 0 ? tileH : 1;
    if (RectEmpty(area)) {
        g.cols = g.rows = 0;
    } else {
        g.cols = (RectWidth(area)  + g.tileW - 1) / g.tileW;
        g.rows = (RectHeight(area) + g.tileH - 1) / g.tileH;
    }
    return g;
}

IRect TileRect(const TileGrid& grid, int32_t col, int32_t row) {
    IRect r;
    r.left   = grid.area.left + col * grid.tileW;
    r.top    = grid.area.top  + row * grid.tileH;
    r.right  = std::min(r.left + grid.tileW, grid.area.right);
    r.bottom = std::min(r.top  + grid.tileH, grid.area.bottom);
    return r;
}

IRect InputRectFor(const IRect& outRect, int32_t imageWidth, int32_t imageHeight,
                   bool hostPadsOutsideImage) {
    IRect in;
    in.top    = outRect.top    - 1;
    in.left   = outRect.left   - 1;
    in.bottom = outRect.bottom + 1;
    in.right  = outRect.right  + 1;
    if (!hostPadsOutsideImage) {
        in.top    = std::max<int32_t>(in.top, 0);
        in.left   = std::max<int32_t>(in.left, 0);
        in.bottom = std::min<int32_t>(in.bottom, imageHeight);
        in.right  = std::min<int32_t>(in.right, imageWidth);
    }
    return in;
}

namespace {

template <typename T>
inline T LoadSample(const unsigned char* p) {
    T v;
    std::memcpy(&v, p, sizeof(T));  // безопасно для невыровненных адресов
    return v;
}

template <typename T>
inline void StoreSample(unsigned char* p, T v) {
    std::memcpy(p, &v, sizeof(T));
}

inline int32_t Clamp(int32_t v, int32_t lo, int32_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Смещения (в байтах) столбцов x-1, x, x+1 для каждого выходного x, с повтором краевого пикселя.
std::vector<std::ptrdiff_t> ColumnOffsets(int32_t outW, int32_t srcW, int32_t offX, std::ptrdiff_t srcCol) {
    std::vector<std::ptrdiff_t> cx(static_cast<size_t>(outW) * 3);
    for (int32_t x = 0; x < outW; ++x)
        for (int k = 0; k < 3; ++k)
            cx[static_cast<size_t>(x) * 3 + k] = Clamp(x + offX + k - 1, 0, srcW - 1) * srcCol;
    return cx;
}

// Целочисленный вариант: веса 1 2 1 / 2 4 2 / 1 2 1, сумма весов 16.
// Округление к ближайшему: (sum + 8) >> 4. Для 16 бит сумма <= 65535*16 < 2^32.
template <typename T>
void BlurIntegral(const unsigned char* src, std::ptrdiff_t srcRow, std::ptrdiff_t srcCol,
                  int32_t srcW, int32_t srcH, int32_t offX, int32_t offY,
                  unsigned char* dst, std::ptrdiff_t dstRow, std::ptrdiff_t dstCol,
                  int32_t outW, int32_t outH) {
    const std::vector<std::ptrdiff_t> cx = ColumnOffsets(outW, srcW, offX, srcCol);
    for (int32_t y = 0; y < outH; ++y) {
        const unsigned char* r0 = src + Clamp(y + offY - 1, 0, srcH - 1) * srcRow;
        const unsigned char* r1 = src + Clamp(y + offY,     0, srcH - 1) * srcRow;
        const unsigned char* r2 = src + Clamp(y + offY + 1, 0, srcH - 1) * srcRow;
        unsigned char* d = dst + y * dstRow;
        for (int32_t x = 0; x < outW; ++x) {
            const std::ptrdiff_t* c = &cx[static_cast<size_t>(x) * 3];
            const uint32_t sum =
                1u * LoadSample<T>(r0 + c[0]) + 2u * LoadSample<T>(r0 + c[1]) + 1u * LoadSample<T>(r0 + c[2]) +
                2u * LoadSample<T>(r1 + c[0]) + 4u * LoadSample<T>(r1 + c[1]) + 2u * LoadSample<T>(r1 + c[2]) +
                1u * LoadSample<T>(r2 + c[0]) + 2u * LoadSample<T>(r2 + c[1]) + 1u * LoadSample<T>(r2 + c[2]);
            StoreSample<T>(d + x * dstCol, static_cast<T>((sum + 8u) >> 4));
        }
    }
}

void BlurFloat(const unsigned char* src, std::ptrdiff_t srcRow, std::ptrdiff_t srcCol,
               int32_t srcW, int32_t srcH, int32_t offX, int32_t offY,
               unsigned char* dst, std::ptrdiff_t dstRow, std::ptrdiff_t dstCol,
               int32_t outW, int32_t outH) {
    const std::vector<std::ptrdiff_t> cx = ColumnOffsets(outW, srcW, offX, srcCol);
    for (int32_t y = 0; y < outH; ++y) {
        const unsigned char* r0 = src + Clamp(y + offY - 1, 0, srcH - 1) * srcRow;
        const unsigned char* r1 = src + Clamp(y + offY,     0, srcH - 1) * srcRow;
        const unsigned char* r2 = src + Clamp(y + offY + 1, 0, srcH - 1) * srcRow;
        unsigned char* d = dst + y * dstRow;
        for (int32_t x = 0; x < outW; ++x) {
            const std::ptrdiff_t* c = &cx[static_cast<size_t>(x) * 3];
            const float sum =
                1.0f * LoadSample<float>(r0 + c[0]) + 2.0f * LoadSample<float>(r0 + c[1]) + 1.0f * LoadSample<float>(r0 + c[2]) +
                2.0f * LoadSample<float>(r1 + c[0]) + 4.0f * LoadSample<float>(r1 + c[1]) + 2.0f * LoadSample<float>(r1 + c[2]) +
                1.0f * LoadSample<float>(r2 + c[0]) + 2.0f * LoadSample<float>(r2 + c[1]) + 1.0f * LoadSample<float>(r2 + c[2]);
            StoreSample<float>(d + x * dstCol, sum * (1.0f / 16.0f));
        }
    }
}

}  // namespace

bool BlurPlane(SampleType type,
               const void* src, std::ptrdiff_t srcRowBytes, std::ptrdiff_t srcColBytes,
               int32_t srcW, int32_t srcH,
               int32_t offsetX, int32_t offsetY,
               void* dst, std::ptrdiff_t dstRowBytes, std::ptrdiff_t dstColBytes,
               int32_t outW, int32_t outH) {
    if (outW < 0 || outH < 0) return false;
    if (outW == 0 || outH == 0) return true;  // нечего делать
    if (src == nullptr || dst == nullptr) return false;
    if (srcW <= 0 || srcH <= 0) return false;
    const std::ptrdiff_t sz = SampleSize(type);
    if (srcColBytes < sz || dstColBytes < sz) return false;

    const unsigned char* s = static_cast<const unsigned char*>(src);
    unsigned char* d = static_cast<unsigned char*>(dst);
    switch (type) {
        case SampleType::U8:
            BlurIntegral<uint8_t>(s, srcRowBytes, srcColBytes, srcW, srcH, offsetX, offsetY,
                                  d, dstRowBytes, dstColBytes, outW, outH);
            return true;
        case SampleType::U16:
            BlurIntegral<uint16_t>(s, srcRowBytes, srcColBytes, srcW, srcH, offsetX, offsetY,
                                   d, dstRowBytes, dstColBytes, outW, outH);
            return true;
        case SampleType::F32:
            BlurFloat(s, srcRowBytes, srcColBytes, srcW, srcH, offsetX, offsetY,
                      d, dstRowBytes, dstColBytes, outW, outH);
            return true;
    }
    return false;
}

}  // namespace gb
