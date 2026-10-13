# Universal Framework — аудит и план миграции (Promt Universal §12)

Дата: 2026-10-13. База: `pc2c 0.1.0`, тесты 157/157, Canada Hunt 1607 файлов.

## Карта существующее → требуемое

| Требуется | Есть | Статус |
|---|---|---|
| `GameDiscovery` | `discovery/Scanner.cpp` (walk + magic + engine-сигналы) | PARTIAL — нет архивов-контейнеров, нет критического пути запуска |
| `BinaryAnalyzer` | `analysis/Pe.cpp` (arch/imports/exports/linker-hint) | PARTIAL — нет дизассемблера/IR, по контракту §4 (только адаптеры, без DRM-обхода) |
| `EngineDetector` | `portability/ApiDb.cpp guessEngine` | PARTIAL — только эвристика имён, нет версионирования |
| `FormatRegistry` | **НОВОЕ `formats/Registry.cpp`** | PASSED — 20 форматов, caps d/v/x/c/r, `pc2c formats list` |
| `DependencyGraph` | **НОВОЕ `graph/Deps.cpp`** | PASSED — 1607 nodes / 7108 edges на Canada Hunt |
| `ReconstructionPipeline` | `analysis/Inventory.cpp` + graph | PARTIAL — текст-рефы да, бинарная реконструкция нет |
| `AssetPipeline` | `assets/Converter.cpp` (BMP/TGA) + `shaders/ShaderConvert.cpp` (XDK fxc) | PARTIAL — PNG/JPEG/DDS/Audio/Torque-bin нет |
| `CodeAnalysisPipeline` | только PE-парсинг | NOT_RUN — дизассемблер только через контролируемый адаптер, не встроен |
| `EngineAdapter` | **НОВЫЙ `engines/Torque.cpp`** | PASSED (текст) — `.cs/.gui/.mis` extractRefs доказан; `.dts/.dsq/.ter/.dif/.dso` detect-only |
| `PlatformAdapter` | `backends/*` + `build/Toolchain.cpp` | PASSED для сэмплов (PPC+imagexex+Xenia); для игры — BLOCKED без source |
| `BuildBackend` | `pc2c build` (реальный MSBuild) | PASSED (`canadaview.xex 2891776B ok:true`) |
| `RuntimeVerifier` | ручной Xenia-прогон | PARTIAL — предыдущий minplay RUNTIME_VERIFIED; повтор NOT_RUN (нет xenia.exe) |
| `EvidenceReport` | `intermediate/Model.cpp` + `plan/report` + `docs/porting/*` | PASSED — факты/assumptions/limitations разделены |

## Почему не переписывалось ядро
`Scanner/Inventory/Pe/Converter/fxc/Backend` уже реализуют свои контракты и покрыты тестами. Доказано прогоном: `inspect 1607`, `convert 6/6 verify 6/6`, `shaders 170/176`, `build canadaview ok`. Переписывание не нужно; добавлены только недостающие `formats/engines/graph` без изменения сигнатур ядра.

## Первый реализованный компонент (§11.2–11.4)
`TorqueAdapter + FormatRegistry + DependencyGraph`:
- Входы: `root + ScanResult + Inventory + Limits`. Выходы: `Graph{node/edge/resolved/unresolved/warnings}` + JSON. Ошибки: oversize/unreadable/binary → warnings, never fail-hard; unresolved edges сохраняются как `resolved=false`.
- Доказательство декодирования реального формата: текстовые `.cs/.gui/.mis` Canada Hunt — `101 text files → 7108 edges (7049 resolved / 59 unresolved)`, пример `sgShaders.cs → shaders/...hlsl [resolved]`, `59 unresolved` честно сохранены (в т.ч. отсутствующий `tools/main.cs` из `console.log`).
- Обратная сборка: resolution `exact → suffix → basename(unique)`; таблицы в JSON (`edges_sample 5000` + note).
- CLI: `pc2c formats list`, `pc2c graph <dir> [--output]`.
- Тесты: 15 новых (registry honesty, torque refs, synthetic graph resolve/unresolved), всего 157/157 + `ctest 2/2`.

## Что дальше (по приоритету игры)
1. PNG/JPEG-декодер (133МБ блокер) с циклом detect→validate→extract→convert→reread.
2. Torque-bin исследование по одному формату (начать с `.dso` заголовка) с векторами, без фейкового декода.
3. `RuntimeVerifier`: автовызов Xenia с логом + статусы NOT_RUN/FAILED/PARTIAL/PASSED/RUNTIME_VERIFIED.
4. `CodeAnalysisPipeline`: x86-import→PPC-stub карта как IR-отчёт (без заявления о восстановлении исходников).
