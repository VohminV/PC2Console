# PC2Console

Инструментарий исследования, анализа и подготовки переноса Windows-игр на
**Xbox 360** (цель 1) с архитектурным заделом под **PS3** (цель 2, будущий backend).
MVP 0.1.0. Язык: C++20, сборка: CMake, внешние зависимости: **нет**
(только стандартная библиотека — сборка без сети).

> Уменьшение текстур — лишь один этап. Игра считается портированной только
> после адаптации компонентов и подтверждённого запуска результата.

## Быстрый старт

```bat
set PATH=C:\winlibs\mingw64\bin;%PATH%
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cd build
ctest                       & REM 2/2 pass
pc2c.exe test               & REM 68/68 pass
pc2c.exe doctor             & REM диагностика XDK 21256 + VS2010
```

## CLI

```bat
pc2c doctor [--probe-tools] [--format json]
pc2c backends list
pc2c inspect <game_dir> [--format json]
pc2c analyze <game_dir> [--output analysis.json]
pc2c inventory <game_dir> [--format json]
pc2c assets list <game_dir>
pc2c assets extract <game_dir> --output <dir> [--overwrite]
pc2c assets convert <input_dir> --output <dir> --profile xbox360-480p [--max-texture-size N] [--overwrite] [--dry-run]
pc2c assets verify <dir>
pc2c plan <game_dir> --target xbox360 [--output plan.json]
pc2c build <project_dir> --target xbox360 [--dry-run]
pc2c report <game_dir> [--target xbox360] [--output model.json]
pc2c test [--verbose]
```

Профили: `conservative`, `xbox360-480p`, `xbox360-360p`, `xbox360-240p`,
`xbox360-120p`, `balanced`. Поддерживаемая конвертация: BMP 24/32, TGA 24/32
(+RLE); PNG/JPEG/DDS — детект без конвертации (честно). Безопасность: входы не
изменяются и не запускаются; запись атомарная; `--overwrite` required.

## Статус окружения (проверено 2026-10-13)

XDK 21256 полный (`%XEDK%`, `_XDK_VER=21256`, cl/link 10.00.11886, imagexex,
fxc, 306 заголовков, 134 библиотеки, VS2010, сэмплы). Backend заявляет
`ready=false` до прохождения Phase 5 (реальная сборка минимального сэмпла).
Подробности: `docs/XBOX360_SDK.md`, `docs/RESEARCH.md`.

## Документация

`ARCHITECTURE.md` · `RESEARCH.md` · `SUPPORTED_FORMATS.md` · `XBOX360_SDK.md` ·
`XBOX360_BACKEND.md` · `PS3_BACKEND.md` · `PORTING_LIMITATIONS.md` ·
`SECURITY.md` · `TEST_PLAN.md` · `ROADMAP.md`
