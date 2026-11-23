// GaussianBlurPlugin.cpp
//
// Классический фильтр Photoshop (Filter Plug-in API): Gaussian Blur 3x3.
// Точка входа PluginMain вызывается Photoshop с селекторами
// About / Parameters / Prepare / Start / Continue / Finish.
//
// Алгоритм (свёртка, обработка границ, шаги rowBytes) - в GaussianBlurCore.*,
// он не зависит от SDK. Здесь - только взаимодействие с FilterRecord.
//
// Схема обработки (всё делается в Start через advanceState):
//   для каждого тайла области фильтрации
//     для каждого канала (plane)
//       запросить у Photoshop входной прямоугольник (тайл + 1 пиксель со всех сторон)
//       и выходной прямоугольник (тайл)  -> advanceState()
//       размыть канал: inData -> outData
//   Continue только обнуляет прямоугольники (работа уже сделана), Finish ничего не держит.

#include "PIDefines.h"
#include "PITypes.h"
#include "PIAbout.h"
#include "PIFilter.h"

#include "GaussianBlurCore.h"

#include <cstdint>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

#define PLUGIN_TITLE "Gaussian Blur 3x3"

namespace {

FilterRecordPtr gFilterRecord = nullptr;
int16*          gResult       = nullptr;

// ---- Доступ к прямоугольникам: 32-битные координаты (большие документы), с запасным вариантом ----

gb::IRect FromVRect(const VRect& r) { return gb::IRect{r.top, r.left, r.bottom, r.right}; }
gb::IRect FromRect(const Rect& r)   { return gb::IRect{r.top, r.left, r.bottom, r.right}; }

VRect ToVRect(const gb::IRect& r) {
    VRect v;
    v.top = r.top; v.left = r.left; v.bottom = r.bottom; v.right = r.right;
    return v;
}

Rect ToRect(const gb::IRect& r) {
    Rect v;
    v.top    = static_cast<int16>(r.top);
    v.left   = static_cast<int16>(r.left);
    v.bottom = static_cast<int16>(r.bottom);
    v.right  = static_cast<int16>(r.right);
    return v;
}

bool Uses32Bit() { return gFilterRecord->bigDocumentData != nullptr; }

gb::IRect GetFilterRect() {
    return Uses32Bit() ? FromVRect(gFilterRecord->bigDocumentData->filterRect32)
                       : FromRect(gFilterRecord->filterRect);
}

void GetImageSize(int32_t& w, int32_t& h) {
    if (Uses32Bit()) {
        w = gFilterRecord->bigDocumentData->imageSize32.h;
        h = gFilterRecord->bigDocumentData->imageSize32.v;
    } else {
        w = gFilterRecord->imageSize.h;
        h = gFilterRecord->imageSize.v;
    }
}

void SetInRect(const gb::IRect& r) {
    if (Uses32Bit()) gFilterRecord->bigDocumentData->inRect32 = ToVRect(r);
    else             gFilterRecord->inRect = ToRect(r);
}

void SetOutRect(const gb::IRect& r) {
    if (Uses32Bit()) gFilterRecord->bigDocumentData->outRect32 = ToVRect(r);
    else             gFilterRecord->outRect = ToRect(r);
}

void SetMaskRect(const gb::IRect& r) {
    if (Uses32Bit()) gFilterRecord->bigDocumentData->maskRect32 = ToVRect(r);
    else             gFilterRecord->maskRect = ToRect(r);
}

// ---- Вспомогательное --------------------------------------------------------------------------

// Сколько каналов нужно обработать. Выходные каналы - префикс входных (слой: цвет, затем прозрачность,
// маски...). Если хост не заполнил out*-счётчики, берём общее число каналов.
int16 OutputPlaneCount() {
    const int32 n = gFilterRecord->outLayerPlanes + gFilterRecord->outTransparencyMask +
                    gFilterRecord->outLayerMasks + gFilterRecord->outInvertedLayerMasks +
                    gFilterRecord->outNonLayerPlanes;
    if (n <= 0 || n > gFilterRecord->planes) return gFilterRecord->planes;
    return static_cast<int16>(n);
}

bool IsSupportedImage(gb::SampleType& type) {
    const int16 mode = gFilterRecord->imageMode;
    if (mode == plugInModeBitmap || mode == plugInModeIndexedColor) return false;
    return gb::SampleTypeFromDepth(gFilterRecord->depth, type);
}

// ---- Селекторы ----------------------------------------------------------------------------------

void DoAbout() {
#if defined(_WIN32)
    AboutRecordPtr about = reinterpret_cast<AboutRecordPtr>(gFilterRecord);
    HWND hwnd = nullptr;
    if (about->platformData != nullptr)
        hwnd = reinterpret_cast<HWND>(static_cast<PlatformData*>(about->platformData)->hwnd);
    MessageBoxA(hwnd,
                PLUGIN_TITLE "\n\nGaussian blur with a 3x3 kernel (1 2 1 / 2 4 2 / 1 2 1) / 16.\n"
                "Coursework: classic Photoshop filter plug-in.",
                "About " PLUGIN_TITLE, MB_OK);
#elif defined(__APPLE__)
    CFUserNotificationDisplayNotice(
        0, kCFUserNotificationNoteAlertLevel, nullptr, nullptr, nullptr,
        CFSTR("About " PLUGIN_TITLE),
        CFSTR(PLUGIN_TITLE "\n\nGaussian blur with a 3x3 kernel (1 2 1 / 2 4 2 / 1 2 1) / 16.\n"
              "Coursework: classic Photoshop filter plug-in."),
        CFSTR("OK"));
#endif
}

// У фильтра нет настраиваемых параметров и диалога, поэтому хранить нечего.
// (Этот селектор Photoshop может и не вызвать - например, при Ctrl+F.)
void DoParameters() {}

// Сообщаем, что дополнительная память под буферы нам не нужна:
// входные/выходные данные выделяет и держит Photoshop.
void DoPrepare() {
    gFilterRecord->bufferSpace   = 0;
    gFilterRecord->bufferSpace64 = 0;
}

void DoStart() {
    gb::SampleType sampleType;
    if (!IsSupportedImage(sampleType)) {
        *gResult = filterBadMode;
        return;
    }

    const gb::IRect area = GetFilterRect();
    if (gb::RectEmpty(area)) return;  // нечего обрабатывать

    int32_t imageW = 0, imageH = 0;
    GetImageSize(imageW, imageH);

    // Просим Photoshop дополнять запросы за границами изображения повтором краевого пикселя.
    // Если хост этого не умеет, InputRectFor обрежет запрос по границам изображения,
    // а ядро само повторит краевой пиксель - результат тот же.
    const bool hostPads = gFilterRecord->supportsPadding != 0;
    if (hostPads) gFilterRecord->inputPadding = plugInWantsEdgeReplication;

    // Масштаб 1:1 (Fixed 16.16).
    gFilterRecord->inputRate = static_cast<Fixed>(1) << 16;
    gFilterRecord->maskRate  = static_cast<Fixed>(1) << 16;

    // Размер тайла: подсказка хоста, иначе 256. Ограничиваем сверху ради памяти.
    int32_t tileW = gFilterRecord->outTileWidth;
    int32_t tileH = gFilterRecord->outTileHeight;
    if (tileW <= 0) tileW = 256;
    if (tileH <= 0) tileH = 256;
    if (tileW > 1024) tileW = 1024;
    if (tileH > 1024) tileH = 1024;

    const gb::TileGrid grid = gb::MakeTileGrid(area, tileW, tileH);
    const int16 planes = OutputPlaneCount();
    const int32_t progressTotal = grid.cols * grid.rows * planes;
    int32_t progressDone = 0;

    const int32_t bytesPerSample = gb::SampleSize(sampleType);
    const gb::IRect noMask = {0, 0, 0, 0};

    for (int32_t row = 0; row < grid.rows; ++row) {
        for (int32_t col = 0; col < grid.cols; ++col) {
            const gb::IRect outR = gb::TileRect(grid, col, row);
            const gb::IRect inR  = gb::InputRectFor(outR, imageW, imageH, hostPads);

            SetInRect(inR);
            SetOutRect(outR);
            SetMaskRect(noMask);  // выделение Photoshop применяет сам (WriteOutsideSelection = false)

            for (int16 plane = 0; plane < planes; ++plane) {
                // Один канал за раз - небольшой расход памяти.
                gFilterRecord->inLoPlane  = gFilterRecord->inHiPlane  = plane;
                gFilterRecord->outLoPlane = gFilterRecord->outHiPlane = plane;

                *gResult = gFilterRecord->advanceState();
                if (*gResult != noErr) return;

                // Шаг между пикселями: если хост не заполнил, это размер отсчёта (канал один).
                const std::ptrdiff_t inCol  = gFilterRecord->inColumnBytes  > 0 ? gFilterRecord->inColumnBytes  : bytesPerSample;
                const std::ptrdiff_t outCol = gFilterRecord->outColumnBytes > 0 ? gFilterRecord->outColumnBytes : bytesPerSample;

                const bool ok = gb::BlurPlane(
                    sampleType,
                    gFilterRecord->inData, gFilterRecord->inRowBytes, inCol,
                    gb::RectWidth(inR), gb::RectHeight(inR),
                    outR.left - inR.left, outR.top - inR.top,
                    gFilterRecord->outData, gFilterRecord->outRowBytes, outCol,
                    gb::RectWidth(outR), gb::RectHeight(outR));
                if (!ok) {
                    *gResult = filterBadParameters;
                    return;
                }

                gFilterRecord->progressProc(++progressDone, progressTotal);
            }

            if (gFilterRecord->abortProc()) {
                *gResult = userCanceledErr;
                return;
            }
        }
    }
}

// Вся работа выполнена в Start через advanceState. Нулевые прямоугольники сообщают Photoshop,
// что больше данных не нужно.
void DoContinue() {
    const gb::IRect zero = {0, 0, 0, 0};
    SetInRect(zero);
    SetOutRect(zero);
    SetMaskRect(zero);
}

// Ресурсов, которые нужно освобождать, нет: память под данные принадлежит Photoshop,
// собственных handle/буферов плагин не создаёт.
void DoFinish() {}

}  // namespace

DLLExport MACPASCAL void PluginMain(const int16 selector,
                                    FilterRecordPtr filterRecord,
                                    intptr_t* data,
                                    int16* result) {
    (void)data;
    try {
        gFilterRecord = filterRecord;
        gResult       = result;
        *result       = noErr;

        if (selector != filterSelectorAbout && gFilterRecord->bigDocumentData != nullptr)
            gFilterRecord->bigDocumentData->PluginUsing32BitCoordinates = true;

        switch (selector) {
            case filterSelectorAbout:      DoAbout();      break;
            case filterSelectorParameters: DoParameters(); break;
            case filterSelectorPrepare:    DoPrepare();    break;
            case filterSelectorStart:      DoStart();      break;
            case filterSelectorContinue:   DoContinue();   break;
            case filterSelectorFinish:     DoFinish();     break;
            default: break;
        }
    } catch (...) {
        if (result != nullptr) *result = memFullErr;  // исключения за пределы плагина выпускать нельзя
    }
}
