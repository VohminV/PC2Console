# PC2Console — архитектура

Инструментарий анализа Windows-игр и подготовки переноса на Xbox 360 (цель 1),
с архитектурным заделом под PS3 (цель 2, будущий backend). Язык: C++20,
сборка: CMake, зависимости: только стандартная библиотека (MVP собирается
без сети: MinGW 15 / MSVC 19+, CMake 3.20+).

## Принципы

1. **Ядро не зависит от консольных SDK.** Весь код `include/pc2c`,
   кроме `backends/*`, собирается современным хостовым компилятором.
   Компилятор XDK никогда не используется для сборки pc2c.
2. **Backend — явный контракт.** `backends::Backend`: `diagnose()`,
   `planBuild()`, таблица `Capability`. Xbox 360 и PS3 — отдельные классы,
   общий код запрещён.
3. **Никаких заглушек с фейковым успехом.** Неподдерживаемое завершается
   понятной ошибкой и кодом возврата 2 (`Status::Unsupported`).
4. **Факты отдельно от предположений.** Каждый отчёт несёт источник
   (файл, смещение) и confidence; импорт в таблице ≠ доказанный вызов.
5. **Вход никогда не изменяется и не запускается.** Все записи — в `--output`,
   атомарно (tmp + rename), с проверкой path traversal.

## Модули

| Модуль | Назначение |
|---|---|
| `core/` | Status, Logger, Config/Limits, Fs (безопасный обход, atomic write, joinSafe), Hash (SHA-256), Json (writer+reader) |
| `discovery/` | Сканирование каталога: виды файлов, magic-сигнатуры, engine-сигналы (только имена, низкая достоверность) |
| `analysis/` | PE/COFF-парсер без исполнения (PE32/PE32+, импорты/экспорты, arch, linker-hint, DirectX-hint); инвентаризация ресурсов с валидацией заголовков |
| `assets/` | Декодеры BMP/TGA → RGBA8, семантика (color/normal/mask), ресайз (bilinear/box+ренормализация), mipmaps, профили, конвертер каталога с манифестом SHA-256 и `verify` |
| `portability/` | База вердиктов DLL/CPU/DX, engine-guess, `plan` с категориями SUPPORTED…UNKNOWN (без фейковых процентов) |
| `intermediate/` | Версионируемая модель (`pc2c_model_version: 1`): facts/assumptions/limitations |
| `build/` | Диагностика XDK только по файловой системе (+ опциональный `--probe-tools`) |
| `backends/` | `Xbox360Backend`, `Ps3Backend` (future, честно не готов) |
| `cli/` | Единый `pc2c`, все команды из ТЗ |
| `test/` | Регрессии на синтетических фикстурах (генерируются в runtime, без бинарных блобов) |

## Потоки данных

```text
game_dir --scan--> FileRecords --pe--> PeInfo --inventory--> Inventory
    |--> plan (ApiDb + backend caps) --> PlanDoc JSON --> ModelDoc JSON
tex_dir --convert--> out_dir + pc2c-manifest.json --verify--> VerifyReport
```

## Коды возврата

`0` — успех; `1` — ошибка (плохие аргументы, I/O, verify mismatch);
`2` — честно не поддерживается (PS3 build, неизвестный формат в одиночной
операции там, где это осмысленно).
