// test_core.cpp - независимые тесты ядра Gaussian Blur 3x3 (Photoshop не нужен).
//
// Проверяется:
//  1. известный результат на импульсе и на константном изображении;
//  2. совпадение с наивной эталонной реализацией (8/16 бит и float), включая границы;
//  3. БЕСШОВНОСТЬ: обработка по тайлам с имитацией Photoshop (запрос входного прямоугольника
//     с отступом 1 пиксель, нестандартные rowBytes/colBytes) даёт результат, побитово
//     совпадающий с обработкой всего кадра за один раз;
//  4. поведение при некорректных аргументах.
#include "../src/GaussianBlurCore.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace gb;

static int g_failed = 0, g_checks = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failed;                                                          \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
        }                                                                        \
    } while (0)

// Простейший детерминированный генератор.
static uint32_t g_seed = 12345;
static uint32_t Rnd() { g_seed = g_seed * 1664525u + 1013904223u; return g_seed >> 8; }

template <typename T> struct Image {
    int w, h;
    std::vector<T> px;
    Image(int w_, int h_) : w(w_), h(h_), px(static_cast<size_t>(w_) * h_) {}
    T& at(int x, int y) { return px[static_cast<size_t>(y) * w + x]; }
    const T& at(int x, int y) const { return px[static_cast<size_t>(y) * w + x]; }
};

static int Clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Эталон: прямая реализация формулы P' = 1/16 * sum K[i][j] * P[x+i][y+j], край = повтор.
template <typename T>
static Image<T> ReferenceBlurInt(const Image<T>& in) {
    static const int K[3][3] = {{1, 2, 1}, {2, 4, 2}, {1, 2, 1}};
    Image<T> out(in.w, in.h);
    for (int y = 0; y < in.h; ++y)
        for (int x = 0; x < in.w; ++x) {
            uint32_t s = 0;
            for (int j = -1; j <= 1; ++j)
                for (int i = -1; i <= 1; ++i)
                    s += K[j + 1][i + 1] * in.at(Clampi(x + i, 0, in.w - 1), Clampi(y + j, 0, in.h - 1));
            out.at(x, y) = static_cast<T>((s + 8) / 16);
        }
    return out;
}

static Image<float> ReferenceBlurFloat(const Image<float>& in) {
    static const float K[3][3] = {{1, 2, 1}, {2, 4, 2}, {1, 2, 1}};
    Image<float> out(in.w, in.h);
    for (int y = 0; y < in.h; ++y)
        for (int x = 0; x < in.w; ++x) {
            double s = 0;
            for (int j = -1; j <= 1; ++j)
                for (int i = -1; i <= 1; ++i)
                    s += K[j + 1][i + 1] * in.at(Clampi(x + i, 0, in.w - 1), Clampi(y + j, 0, in.h - 1));
            out.at(x, y) = static_cast<float>(s / 16.0);
        }
    return out;
}

static Image<uint8_t>  RefBlur(const Image<uint8_t>& i)  { return ReferenceBlurInt(i); }
static Image<uint16_t> RefBlur(const Image<uint16_t>& i) { return ReferenceBlurInt(i); }
static Image<float>    RefBlur(const Image<float>& i)    { return ReferenceBlurFloat(i); }

template <typename T> static SampleType TypeOf();
template <> SampleType TypeOf<uint8_t>() { return SampleType::U8; }
template <> SampleType TypeOf<uint16_t>() { return SampleType::U16; }
template <> SampleType TypeOf<float>() { return SampleType::F32; }

template <typename T> static T RandomSample();
template <> uint8_t RandomSample<uint8_t>() { return static_cast<uint8_t>(Rnd()); }
template <> uint16_t RandomSample<uint16_t>() { return static_cast<uint16_t>(Rnd() % 32769); }  // как в Photoshop: 0..32768
template <> float RandomSample<float>() { return (Rnd() % 100001) / 100000.0f; }

template <typename T> static bool Same(T a, T b) { return a == b; }
template <> bool Same<float>(float a, float b) { return std::fabs(a - b) < 1e-6f; }

// Целое изображение за один вызов (offset 0, вход = выход по размеру).
template <typename T>
static Image<T> BlurWhole(const Image<T>& in) {
    Image<T> out(in.w, in.h);
    bool ok = BlurPlane(TypeOf<T>(), in.px.data(), in.w * sizeof(T), sizeof(T), in.w, in.h, 0, 0,
                        out.px.data(), in.w * sizeof(T), sizeof(T), in.w, in.h);
    CHECK(ok);
    return out;
}

// Имитация Photoshop: выдаёт запрошенный прямоугольник в буфере с "неудобными" шагами.
// Если hostPads - запрос может выходить за границы, Photoshop дополняет краем (edge replication).
template <typename T>
struct HostBuffer {
    std::vector<unsigned char> mem;
    ptrdiff_t rowBytes, colBytes;
    int w, h;
    unsigned char* origin;  // указатель на левый верхний пиксель
};

template <typename T>
static HostBuffer<T> HostFetch(const Image<T>& img, const IRect& r, bool pad, ptrdiff_t extraCol, ptrdiff_t extraRow) {
    HostBuffer<T> b;
    b.w = RectWidth(r);
    b.h = RectHeight(r);
    b.colBytes = sizeof(T) + extraCol;
    b.rowBytes = b.w * b.colBytes + extraRow;
    b.mem.assign(static_cast<size_t>(b.rowBytes) * b.h + 16, 0xCD);  // мусор вне данных
    b.origin = b.mem.data() + 3;                                      // невыровненное начало
    b.mem.resize(b.mem.size() + 3);
    b.origin = b.mem.data() + 3;
    for (int y = 0; y < b.h; ++y)
        for (int x = 0; x < b.w; ++x) {
            int sx = r.left + x, sy = r.top + y;
            if (pad) { sx = Clampi(sx, 0, img.w - 1); sy = Clampi(sy, 0, img.h - 1); }
            else { CHECK(sx >= 0 && sx < img.w && sy >= 0 && sy < img.h); }  // запрос обязан быть в границах
            T v = img.at(Clampi(sx, 0, img.w - 1), Clampi(sy, 0, img.h - 1));
            std::memcpy(b.origin + y * b.rowBytes + x * b.colBytes, &v, sizeof(T));
        }
    return b;
}

// Полная схема плагина: тайлы -> InputRectFor -> HostFetch -> BlurPlane -> запись в выход.
template <typename T>
static Image<T> BlurTiled(const Image<T>& img, const IRect& filterRect, int tileW, int tileH, bool hostPads) {
    Image<T> out = img;  // вне filterRect пиксели остаются исходными
    TileGrid grid = MakeTileGrid(filterRect, tileW, tileH);
    for (int row = 0; row < grid.rows; ++row)
        for (int col = 0; col < grid.cols; ++col) {
            IRect outR = TileRect(grid, col, row);
            IRect inR = InputRectFor(outR, img.w, img.h, hostPads);
            HostBuffer<T> in = HostFetch(img, inR, hostPads, /*extraCol*/ 0, /*extraRow*/ 5);
            const int ow = RectWidth(outR), oh = RectHeight(outR);
            const ptrdiff_t oCol = sizeof(T) + 0, oRow = ow * oCol + 7;
            std::vector<unsigned char> obuf(static_cast<size_t>(oRow) * oh + 16, 0xEE);
            bool ok = BlurPlane(TypeOf<T>(), in.origin, in.rowBytes, in.colBytes, in.w, in.h,
                                outR.left - inR.left, outR.top - inR.top,
                                obuf.data(), oRow, oCol, ow, oh);
            CHECK(ok);
            for (int y = 0; y < oh; ++y)
                for (int x = 0; x < ow; ++x) {
                    T v;
                    std::memcpy(&v, obuf.data() + y * oRow + x * oCol, sizeof(T));
                    out.at(outR.left + x, outR.top + y) = v;
                }
        }
    return out;
}

template <typename T>
static void TestAgainstReference(int w, int h) {
    Image<T> img(w, h);
    for (auto& p : img.px) p = RandomSample<T>();
    Image<T> got = BlurWhole(img);
    Image<T> ref = RefBlur(img);
    bool same = true;
    for (size_t i = 0; i < got.px.size(); ++i) same = same && Same(got.px[i], ref.px[i]);
    CHECK(same);
}

template <typename T>
static void TestTiling(int w, int h) {
    Image<T> img(w, h);
    for (auto& p : img.px) p = RandomSample<T>();
    Image<T> whole = BlurWhole(img);

    const int tiles[][2] = {{1, 1}, {2, 3}, {3, 2}, {7, 5}, {16, 16}, {64, 64}, {1000, 1000}};
    for (bool pads : {false, true})
        for (auto& t : tiles) {
            IRect full = {0, 0, h, w};
            Image<T> tiled = BlurTiled(img, full, t[0], t[1], pads);
            bool same = true;
            for (size_t i = 0; i < whole.px.size(); ++i) same = same && Same(whole.px[i], tiled.px[i]);
            CHECK(same);
        }
}

template <typename T>
static void TestSelectionRect(int w, int h) {
    // Область фильтрации (выделение) внутри изображения: соседи за её границей берутся из
    // реального изображения, а пиксели вне выделения не меняются.
    Image<T> img(w, h);
    for (auto& p : img.px) p = RandomSample<T>();
    Image<T> whole = BlurWhole(img);
    IRect sel = {3, 4, h - 2, w - 5};
    for (bool pads : {false, true}) {
        Image<T> tiled = BlurTiled(img, sel, 6, 4, pads);
        bool same = true;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                bool inside = y >= sel.top && y < sel.bottom && x >= sel.left && x < sel.right;
                same = same && Same(tiled.at(x, y), inside ? whole.at(x, y) : img.at(x, y));
            }
        CHECK(same);
    }
}

int main() {
    // 1. Импульс 160 в центре 5x5 -> 40 / 20 / 10 / 0 (веса 4/16, 2/16, 1/16)
    {
        Image<uint8_t> img(5, 5);
        for (auto& p : img.px) p = 0;
        img.at(2, 2) = 160;
        Image<uint8_t> out = BlurWhole(img);
        CHECK(out.at(2, 2) == 40);
        CHECK(out.at(1, 2) == 20 && out.at(3, 2) == 20 && out.at(2, 1) == 20 && out.at(2, 3) == 20);
        CHECK(out.at(1, 1) == 10 && out.at(3, 3) == 10 && out.at(1, 3) == 10 && out.at(3, 1) == 10);
        CHECK(out.at(0, 0) == 0 && out.at(4, 4) == 0 && out.at(0, 2) == 0);
    }
    // Константное изображение не меняется (8/16/float), максимумы не переполняются.
    {
        Image<uint8_t> a(9, 7);  for (auto& p : a.px) p = 255;
        Image<uint8_t> ao = BlurWhole(a);  for (auto p : ao.px) CHECK(p == 255);
        Image<uint16_t> b(9, 7); for (auto& p : b.px) p = 65535;
        Image<uint16_t> bo = BlurWhole(b); for (auto p : bo.px) CHECK(p == 65535);
        Image<float> c(9, 7);    for (auto& p : c.px) p = 0.37f;
        Image<float> co = BlurWhole(c);    for (auto p : co.px) CHECK(std::fabs(p - 0.37f) < 1e-6f);
    }
    // Изображение 1x1, 1xN, Nx1 (край со всех сторон).
    {
        Image<uint8_t> a(1, 1); a.at(0, 0) = 77;
        CHECK(BlurWhole(a).at(0, 0) == 77);
        TestAgainstReference<uint8_t>(1, 9);
        TestAgainstReference<uint8_t>(9, 1);
    }

    // 2. Эталон
    TestAgainstReference<uint8_t>(37, 23);
    TestAgainstReference<uint16_t>(37, 23);
    TestAgainstReference<float>(37, 23);

    // 3. Тайлы
    TestTiling<uint8_t>(37, 23);
    TestTiling<uint16_t>(31, 29);
    TestTiling<float>(25, 33);
    TestSelectionRect<uint8_t>(40, 30);
    TestSelectionRect<uint16_t>(40, 30);
    TestSelectionRect<float>(40, 30);

    // Сетка тайлов покрывает область ровно один раз.
    {
        IRect area = {2, 3, 20, 31};
        TileGrid g = MakeTileGrid(area, 7, 5);
        std::vector<int> cover(32 * 32, 0);
        for (int r = 0; r < g.rows; ++r)
            for (int c = 0; c < g.cols; ++c) {
                IRect t = TileRect(g, c, r);
                for (int y = t.top; y < t.bottom; ++y)
                    for (int x = t.left; x < t.right; ++x) cover[y * 32 + x]++;
            }
        bool ok = true;
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 32; ++x) {
                bool in = y >= area.top && y < area.bottom && x >= area.left && x < area.right;
                ok = ok && cover[y * 32 + x] == (in ? 1 : 0);
            }
        CHECK(ok);
        CHECK(MakeTileGrid({0, 0, 0, 0}, 8, 8).cols == 0);
    }

    // 4. Некорректные аргументы
    {
        uint8_t s[4] = {1, 2, 3, 4}, d[4] = {9, 9, 9, 9};
        CHECK(!BlurPlane(SampleType::U8, nullptr, 2, 1, 2, 2, 0, 0, d, 2, 1, 2, 2));
        CHECK(!BlurPlane(SampleType::U8, s, 2, 1, 2, 2, 0, 0, nullptr, 2, 1, 2, 2));
        CHECK(!BlurPlane(SampleType::U8, s, 2, 1, 0, 2, 0, 0, d, 2, 1, 2, 2));
        CHECK(!BlurPlane(SampleType::U16, s, 2, 1, 2, 2, 0, 0, d, 2, 1, 2, 2));  // шаг меньше размера отсчёта
        CHECK(BlurPlane(SampleType::U8, s, 2, 1, 2, 2, 0, 0, d, 2, 1, 0, 0));    // пустой выход - не ошибка
        SampleType t;
        CHECK(SampleTypeFromDepth(8, t) && t == SampleType::U8);
        CHECK(SampleTypeFromDepth(16, t) && t == SampleType::U16);
        CHECK(SampleTypeFromDepth(32, t) && t == SampleType::F32);
        CHECK(!SampleTypeFromDepth(1, t));
    }

    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
