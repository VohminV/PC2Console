# PC2Console — исследование окружения (факты на 2026-10-13)

Проверено чтением каталогов/файлов и версий, без запуска неизвестных EXE.
Файловые версии считаны через свойства файлов (PowerShell VersionInfo).

## Подтверждено

- **Xbox 360 XDK 21256** в `C:\Program Files (x86)\Microsoft Xbox 360 SDK`
  (`XEDK` указывает туда же): `include/xbox/xdk.h` содержит
  `#define _XDK_VER 21256`; `imagexex.exe`/`xbcp.exe` — FileVersion 2.0.21256.0.
- **Кросс-компилятор PowerPC**: `bin/win32/cl.exe` 16.00.11886.00,
  `link.exe` 10.00.11886.00 (эпоха VS2010), `lib.exe`, `ml.exe` на месте.
- **Упаковка/шейдеры**: `imagexex.exe`, `fxc.exe` (5.2.3790.1830) на месте.
- **Заголовки**: `include/xbox` — 306 файлов (`xgraphics.h`, `d3d9*.h`,
  `xaudio2.h`, `xinput2.h`, `xtl.h`, `xbox.h`, …). **Библиотеки**: `lib/xbox` —
  134 файла (`d3d9.lib`, `d3dx9.lib`, `xgraphics.lib`, `xaudio2.lib`,
  `xapilib.lib`, `xinput2.lib`, `xboxkrnl.lib`, `libcMT/libcpMT`, …).
- **VS2010 Ultimate ENU** установлен (`VS100COMNTOOLS` задан) — требуется
  интеграцией XDK.
- **Сэмплы**: `Source/Samples/{Audio,Graphics,Media,NaturalInput,Online,System,ui}`,
  `Common`.
- **Хостовый тулчейн для сборки pc2c**: MinGW GCC 15.2 (`C:\winlibs\mingw64`),
  MSVC 14.51 (VS18 Community), CMake 4.4.4, Ninja + mingw32-make.
- Сборка pc2c проверена на MinGW 15.2 + Ninja; `ctest`: 2/2 pass;
  встроенный сьют: 68/68 pass.

## Предположения (не подтверждены)

- Что XDK-комплект полностью исправен: присутствие файлов ≠ гарантия сборки.
  Флаг `canBuildMinSample=false`, пока не выполнен реальный compile+link+imagexex
  минимального сэмпла (Phase 5).
- Наличие/доступность devkit-консоли для запуска XEX: не проверялось
  (нет легитимной тестовой среды в наличии).

## Не подтверждено / отсутствует

- PS3 toolchain: в окружении не найден (есть посторонний `SCE_ROOT_DIR` от PS5 —
  к PS3 PPA SDK отношения не имеет, не использовать).
- Совместимость XDK-установщика/лицензии для перераспространения: не исследуется,
  закрытые компоненты не копируются в репозиторий.

## Вывод для проекта

Независимые компоненты (ядро, анализ, Asset Pipeline) реализованы и работают
без SDK. Backend Xbox 360 реализует диагностику и план сборки, но заявляет
`ready=false` до прохождения Phase 5. Это зафиксировано и в коде, и в отчётах.
