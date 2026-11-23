# Gaussian Blur 3×3 — фильтр Photoshop, сборка для macOS

Записка к архиву. Это тот же классический фильтр Photoshop на C++, что и в `GaussianBlur_Windows.zip`
(размытие по Гауссу ядром 3×3, веса 1 2 1 / 2 4 2 / 1 2 1, делённые на 16), только с проектом Xcode
и итоговым форматом **`.plugin`** (bundle). В меню: **Filter → Course Project → Gaussian Blur 3x3**.
Параметров и диалога нет.

## Что в сборке

| Файл / папка | Назначение |
|---|---|
| `GaussianBlur.xcodeproj` | Проект Xcode (цель `GaussianBlur`, конфигурации Debug и Release) |
| `PhotoshopSdk.xcconfig` | Путь к SDK (см. ниже) |
| `mac/Info.plist` | Описание bundle (тип `8BFM` — фильтр Photoshop) |
| `pipl/PiPLs.json` | PiPL: имя, категория, режимы, точки входа для Windows и macOS (arm64/x86_64) |
| `src/GaussianBlurCore.h/.cpp` | Алгоритм: свёртка, края, тайлы. Не зависит от SDK |
| `src/GaussianBlurPlugin.cpp` | `PluginMain`, селекторы, работа с `FilterRecord` |
| `tests/` | Тесты ядра (Photoshop не нужен) |

**Общая логика с Windows:** `src/` и `pipl/` — те же файлы, что в Windows-архиве; платформенное различие
только в окне «About» (`#if _WIN32` / `#elif __APPLE__` в `GaussianBlurPlugin.cpp`).

Сам **Photoshop SDK в архив не входит** — это ваш `photoshop-cpp-sdk-main.zip`, его нужно распаковать отдельно.

## Требования

- macOS и Xcode с Command Line Tools (`xcode-select --install`). Заголовки SDK подключают системные заголовки
  Apple (`MacTypes.h`, `CoreServices`), поэтому нужен Xcode с macOS SDK. Проект рассчитан на современный Xcode.
- Распакованный `photoshop-cpp-sdk-main` (нужна только папка `pluginsdk`).
- Photoshop, который читает **JSON PiPL**: SDK из вашего архива описывает плагины именно так, без `.r`-файлов.
  Если плагин не появился в меню, первым делом проверьте версию Photoshop.

## Сборка

1. Распакуйте архивы рядом:
   ```
   любая_папка/
     photoshop-cpp-sdk-main/pluginsdk/...
     GaussianBlur_macOS/GaussianBlur.xcodeproj
   ```
   Если SDK лежит в другом месте, создайте рядом с проектом файл `PhotoshopSdk.local.xcconfig`:
   ```
   PHOTOSHOP_SDK_DIR = /полный/путь/к/photoshop-cpp-sdk-main/pluginsdk
   ```
   (или передайте `PHOTOSHOP_SDK_DIR=...` в командной строке `xcodebuild`).
2. Откройте `GaussianBlur.xcodeproj`, выберите схему **GaussianBlur**, конфигурацию **Release**, **Product → Build**.
   Из терминала:
   ```
   xcodebuild -project GaussianBlur.xcodeproj -target GaussianBlur -configuration Release
   ```
3. Результат: `build/Release/GaussianBlur.plugin`.

Если SDK не найден, первая фаза сборки («Check Photoshop SDK») остановит её понятным сообщением.

### Настройки сборки, которые выбрал я (проверьте под свою среду)

| Настройка | Значение | Комментарий |
|---|---|---|
| `ARCHS` | `$(ARCHS_STANDARD)` | universal: arm64 + x86_64 (в PiPL заданы обе точки входа) |
| `MACOSX_DEPLOYMENT_TARGET` | 11.0 | в примерах SDK 10.13; для arm64 всё равно нужна ≥ 11.0 |
| `CLANG_CXX_LANGUAGE_STANDARD` | gnu++17 | |
| `CODE_SIGN_IDENTITY` | `-` (ad-hoc) | позволяет собрать без аккаунта разработчика |
| `PRODUCT_BUNDLE_IDENTIFIER` | `com.example.GaussianBlur3x3` | замените на свой |
| `WRAPPER_EXTENSION` / `PLUGIN_TYPE` | `plugin` / `8BFM` | как в примерах SDK |
| `GCC_TREAT_WARNINGS_AS_ERRORS` | NO | в примерах SDK YES; заголовки SDK дают предупреждения на современных компиляторах |

Линкуется только `CoreFoundation` (для окна About). Устаревший Carbon, который подключают примеры SDK, нужен им
для диалоговых утилит; этому фильтру — нет.

## Установка и запуск

1. Закройте Photoshop.
2. Скопируйте `GaussianBlur.plugin` в `Plug-ins` вашего Photoshop, например
   `/Applications/Adobe Photoshop <версия>/Plug-ins/` (лучше в отдельную подпапку).
3. Если файл пришёл с другой машины или из загрузки, снимите карантин: `xattr -dr com.apple.quarantine GaussianBlur.plugin`.
   Для передачи другим людям плагин нужно подписать Developer ID и, как правило, нотаризовать — ad-hoc подписи достаточно только для локальных проверок.
4. Запустите Photoshop → **Filter → Course Project → Gaussian Blur 3x3**.

Быстрая проверка результата сборки:
```
ls build/Release/GaussianBlur.plugin/Contents/Resources           # должен быть PiPLs.json
lipo -info build/Release/GaussianBlur.plugin/Contents/MacOS/GaussianBlur   # arm64 x86_64
nm -gU build/Release/GaussianBlur.plugin/Contents/MacOS/GaussianBlur | grep PluginMain   # _PluginMain
codesign -dv build/Release/GaussianBlur.plugin
```

## Как это работает

То же, что в Windows-версии: для каждого тайла запрашивается входной прямоугольник на 1 пиксель больше выходного
(поэтому на стыках тайлов нет швов), края изображения — повтор крайнего пикселя, каналы обрабатываются по одному,
учитываются `rowBytes`/`colBytes`, поддерживаются 8/16/32 бита на канал, Bitmap и Indexed не поддерживаются.
Выделение применяет сам Photoshop. Подробности — в комментариях `src/GaussianBlurCore.h` и `src/GaussianBlurPlugin.cpp`.

## Что проверено, а что нет

Проверено (на Linux, без Mac и без Photoshop):

- **Ядро** — 63 956 проверок, 0 ошибок (с `-fsanitize=address,undefined`): совпадение с прямой формулой для 8/16/32 бит,
  края, швы между тайлами (имитация запросов Photoshop с нестандартными `rowBytes`), выделение внутри изображения.
- `pbxproj` разобран независимым парсером: структура корректна, все ссылки на объекты существуют, каждый файл в группе,
  в цели есть фазы Sources / Frameworks / Resources / проверка SDK. `Info.plist` и `PiPLs.json` корректны (`PiPLs.json` проверен по `pipl-schema.json` из SDK).
- macOS-ветка `PluginMain` прошла проверку синтаксиса против настоящих заголовков SDK из вашего архива. Системные заголовки Apple
  при этом были **заменены заглушками**, так что это проверка только моего кода и его стыка с SDK.

**Не проверено** (требует Mac / Photoshop):

- Сборка в Xcode: проект написан вручную по образцу `hidden.xcodeproj` из SDK, но ни разу не открывался в Xcode.
  Возможны мелочи (версия Xcode, предупреждения заголовков SDK, подпись).
- Загрузка и работа в Photoshop на macOS: появление в меню, 16/32 бита, слои с прозрачностью, выделения, отмена.

Тесты ядра на Mac: `tests/run_tests.sh`. Ожидаемый итог: `63956 checks, 0 failed`.

## Известные ограничения

- Фиксированное ядро 3×3, без радиуса и диалога; нет скриптинга и записи в Actions.
- Подпись только ad-hoc; для распространения нужен Developer ID и нотаризация.
- Версии компонента в `PiPLs.json` (26.1.0) скопированы из примеров SDK.
