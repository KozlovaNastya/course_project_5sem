// GaussianBlurCore.h
//
// Платформенно-независимое ядро фильтра "Gaussian Blur 3x3".
// Не зависит ни от Photoshop SDK, ни от Windows/macOS: один и тот же файл
// используется в Windows- и macOS-проектах и тестируется отдельно от Photoshop.
//
// Ядро свёртки:
//            | 1 2 1 |
//   K = 1/16 | 2 4 2 |
//            | 1 2 1 |
//
// Все прямоугольники полуоткрытые: [top, bottom) x [left, right).
#pragma once

#include <cstddef>
#include <cstdint>

namespace gb {

// Тип отсчёта одного канала в памяти.
enum class SampleType {
    U8,   // 8 бит на канал
    U16,  // 16 бит на канал (в Photoshop диапазон 0..32768, ядру это безразлично)
    F32   // 32 бита с плавающей точкой
};

struct IRect {
    int32_t top;
    int32_t left;
    int32_t bottom;
    int32_t right;
};

inline int32_t RectWidth(const IRect& r)  { return r.right - r.left; }
inline int32_t RectHeight(const IRect& r) { return r.bottom - r.top; }
inline bool    RectEmpty(const IRect& r)  { return r.right <= r.left || r.bottom <= r.top; }

// Глубина канала Photoshop (8/16/32) -> тип отсчёта. false, если глубина не поддерживается
// (например, 1 бит для Bitmap).
bool SampleTypeFromDepth(int32_t depth, SampleType& out);

// Размер одного отсчёта в байтах.
int32_t SampleSize(SampleType type);

// --- Разбиение области фильтрации на тайлы ---------------------------------------------

struct TileGrid {
    IRect   area;   // вся область фильтрации (filterRect)
    int32_t tileW;
    int32_t tileH;
    int32_t cols;
    int32_t rows;
};

TileGrid MakeTileGrid(const IRect& area, int32_t tileW, int32_t tileH);

// Выходной прямоугольник тайла (col, row); последний столбец/строка могут быть уже.
IRect TileRect(const TileGrid& grid, int32_t col, int32_t row);

// --- Какой входной прямоугольник нужно запросить у Photoshop ---------------------------
//
// Для размытия 3x3 каждому выходному пикселю нужны соседи на расстоянии 1, поэтому входной
// прямоугольник = выходной, расширенный на 1 пиксель со всех сторон. Это решение проблемы
// стыков тайлов: соседние пиксели берутся из изображения, а не из "соседнего тайла".
//
// hostPadsOutsideImage == true  : Photoshop сам дополняет запрос за границами изображения
//                                 (inputPadding = plugInWantsEdgeReplication), запрос не обрезаем.
// hostPadsOutsideImage == false : запрос обрезается по границам изображения [0,w) x [0,h);
//                                 недостающих соседей ядро заменяет повтором краевого пикселя.
IRect InputRectFor(const IRect& outRect,
                   int32_t imageWidth, int32_t imageHeight,
                   bool hostPadsOutsideImage);

// --- Размытие одного канала ------------------------------------------------------------
//
// src          : левый верхний пиксель ВХОДНОГО прямоугольника (inData)
// srcRowBytes  : шаг между строками в байтах (rowBytes; может быть больше ширины*размер отсчёта!)
// srcColBytes  : шаг между пикселями в байтах (для одного запрошенного канала = размер отсчёта)
// srcW, srcH   : размер входного прямоугольника в пикселях
// offsetX/Y    : смещение левого верхнего пикселя ВЫХОДНОГО прямоугольника внутри входного
//                (обычно 1,1; 0 на краю изображения, если запрос был обрезан)
// dst...       : то же для выходного буфера (outData), размер outW x outH
//
// Выход за пределы входного буфера обрабатывается повтором краевого пикселя.
// Буферы src и dst не должны пересекаться.
// Возвращает false при некорректных аргументах (ничего не записывается).
bool BlurPlane(SampleType type,
               const void* src, std::ptrdiff_t srcRowBytes, std::ptrdiff_t srcColBytes,
               int32_t srcW, int32_t srcH,
               int32_t offsetX, int32_t offsetY,
               void* dst, std::ptrdiff_t dstRowBytes, std::ptrdiff_t dstColBytes,
               int32_t outW, int32_t outH);

}  // namespace gb
