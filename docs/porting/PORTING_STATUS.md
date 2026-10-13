# PORTING_STATUS — Canada Hunt → Xbox 360 (mission Promt.md)

Обновлено: 2026-10-13 (Universal Framework итерация). Вход: `C:\Realistic Hunting Games\canada_hunt\game`. Тулчейн: `pc2c 0.1.0 (157/157 test PASSED, ctest 2/2)`, XDK 21256 + VS2010 (`doctor`: все ok). Выходы: `%TEMP%\opencode\canada_*`.

## Легенда
`PASSED` — проверено артефактом; `PARTIAL` — часть готова; `FAILED` — подтверждена ошибка; `NOT_RUN` — не запускалось; `RUNTIME_VERIFIED` — запуск доказан логом.

## ZERO — известные данные — PASSED
- Корень/README/CLI/backend/тесты/сэмплы изучены; `canada_assets_480p` не считается портом.
- Созданы: `GAME_PROFILE.md`, `PORTING_STATUS.md` (этот), `UNSUPPORTED_FEATURES.md`, `PORTING_DECISIONS.md`, `UNIVERSAL_FRAMEWORK.md`.
- Новое в Universal-итерации: `formats/Registry`, `engines/Torque`, `graph/Deps`, CLI `formats list` + `graph`.

## ONE — инвентаризация — PASSED
- `inspect`: 1607 файлов 206976779B, engine `torque? [medium]`.
- `inventory`: convertible_bmp_tga=4981752.
- `report → canada_model.json (506054B, facts 1634)`.
- `formats list`: 20 форматов (supported=8: bmp/tga/hlsl/pe-exe/pe-dll/torque-script/gui/mission; unsupported=12).
- `graph`: **nodes=1607 edges=7108 text_files=101 resolved=7049 unresolved=59** (`canada_graph.json`).
- Расширения: png 524/jpg 210/dso 177/hlsl 171/dts 159/ogg 91/cfg и т.д. (см. GAME_PROFILE). Архивов 0, скрытых 0, без расширения 0, >50МБ 0.
- Команда: `pc2c inspect|inventory|report|formats|graph` — воспроизводимо.

## TWO — движок/технологии — PASSED, путь E
- TGEA (medium), x86 VS2008, D3D9-runtime + D3D8-импорт, OpenAL, DirectInput, TorqueScript moveMap, wsock32:28000, C++ source отсутствует.
- Выбор: **E** (полный порт невозможен) + D частично (ассеты/шейдеры/ввод). См. DECISIONS D0.

## THREE — аудит PC2Console — PASSED (найдена 1 ложноположительная метрика)
- Трасса: `game → scanGameDir (Scanner.cpp) → parsePeFile (Pe.cpp) → buildInventory → buildPlan (Plan.cpp) → ModelDoc → convertDir (Converter.cpp) → verifyDir → shaders convertDir (ShaderConvert.cpp, runFxc) → buildXboxSample (Toolchain.cpp via MSBuild) → Xenia`.
- `Converter.cpp:105-108` молча пропускает не-bmp/tga (D2) — отчётность исправлена здесь, код ждёт ревью.
- `ShaderConvert.cpp:247-248` честно Fail при failed>0 — PASSED.
- `plan` без процентов + caveat `import presence does not prove a call` — PASSED.
- `build` разделяет backend-диагностику и реальный MSBuild — PASSED.

## FOUR — asset pipeline
- Архивы: n/a (0) — NOT_RUN (extract/repack не требуется).
- Текстуры: PARTIAL — `convert xbox360-480p: total_bmp_tga=6 converted=6 verify 6/6, 4981752→3938724B gpu 8851340→7000048`; 734 PNG/JPEG — UNSUPPORTED (U6).
- Геометрия/модели: NOT_RUN (декодеров DTS/DSQ/MIS/TER нет, U7).
- Материалы/шейдеры: PARTIAL — `170/176 compiled, 1 failed X4509, 5 unsupported` + `.vso/.pso` + report.
- Звук/видео/шрифты/UI: NOT_RUN (U8).
- Проверка: manifest SHA-256 + `verify` + roundtrip-декод — PASSED для BMP-подмножества.

## FIVE — игровые системы/WinAPI — PASSED (диагноз)
- 4 PE: Canada_Hunt (17 имп/12 DLL), OpenAL32 (83/2), d3dx9_31 (177/4), d3dx9_33 (197/4).
- `plan`: BLOCKED 8x UNSUPPORTED, остальное REQUIRES_SOURCE, CRT SUPPORTED. Без source адаптеры не реализуются (запрет фиктивных DLL).

## SIX — Xbox 360 backend — PASSED (для сэмплов) / NOT_RUN (для игры)
- Toolchain: `cl 16.00.11886 + link 10.00.11886 + imagexex 2.0.21256.0 + fxc`, VS2010 present, XEX2 magic подтверждён.
- `build canadaview --target xbox360: ok:true, canadaview.xex 2891776B` (пересборка 2026-10-13).
- `pe-to-xex-direct=false, console-texture-output=false` — честно.
- Сборка игрового EXE: невозможна (нет source) — E.

## SEVEN — интеграция — NOT_RUN
- Схема каталогов/поиск ресурсов/манифесты `.mis` не реализованы (нужны декодеры Torque). Копирование `canada_assets_480p` в runtime не делалось (запрещено Promt §9).

## EIGHT — сборка и проверка — PARTIAL
- Входы зафиксированы, конфиг `Release|Xbox 360`, лог MSBuild через `sample_build`, артефакты + SHA в manifest/report.
- Xenia: предыдущий прогон minplay — RUNTIME_VERIFIED (entry 8224D788, 1280x720 свопы, no fatal, 2026-10-13); на этой итерации Xenia не найдена — повтор NOT_RUN. Причина: `where xenia.exe` пуст, `C:\Tools` пуст.
- Игра в эмуляторе: NOT_RUN (нет игрового XEX).

## NINE — геймплей — NOT_RUN
Критерии (exe→runtime→меню→ресурсы→ввод→звук→сцена→механики): все NOT_RUN для игры. Для сэмплов: exe PASSED, ввод/кадры RUNTIME_VERIFIED (2026-10-13), звук/сцены игры n/a.

## TEN — оптимизация — PARTIAL (измерения частичные)
- Бюджет: 512МБ unified. Измерено: BMP 7.0МБ GPU-est (нижняя граница, без tiling). Не измерено: 133МБ PNG/JPEG, DTS/OGG, draw calls, шейдерная стоимость, CPU/GPU, загрузки. Вслепую не ужималось (D3).

## Блокеры (остались)
1. Исходники C++ игры + Xbox360-сборка TGEA + лицензия (E).
2. PNG/JPEG-декодеры (133МБ).
3. Декодеры Torque (DTS/DSQ/MIS/TER/GUI/DSO).
4. Правка `refractPixBumpV.hlsl X4509`.
5. Редизайн ввода мышь→геймпад + XAudio2.
6. Xenos writer + devkit-прогон + Xenia-повтор (нужен `xenia.exe`).

## Следующий шаг
1. Поставить `xenia.exe` (официальный release-builds-windows) и повторить `canadaview.xex` с логом.
2. Затем: PNG/JPEG-декодер → пересчёт бюджета → декодеры Torque (по одному формату с векторами).
3. Не повторять: inspect/analyze/inventory/plan/convert/verify/shaders/build — завершены.
