# Xbox 360 Backend — статус и план

Целевая платформа: оригинальная Xbox 360 (Xenon PPC, Xenos, 512 МБ unified).
Современный Xbox GDK **не** используется и заменой не считается.

## Текущий статус (MVP 0.1.0 → Phase 5 DONE 2026-10-13)

- `diagnose()`: SDK обнаружен (XDK 21256, все ключевые инструменты, VS2010),
  `ready=true` (scope: путь тулчейна доказан на сэмпле minplay; порты конкретных
  игр по-прежнему требуют исходников).
- `planBuild()`: возвращает JSON-план + `next_step`.
- `pc2c build <sample> --target xbox360`: выполняет РЕАЛЬНУЮ пересборку сэмпла
  (MSBuild 4.0, Release|Xbox 360), проверяет XEX2-magic, отчитывается JSON.

## Phase 5 — доказательство (факты, 2026-10-13)

1. **Исходник**: `samples/xbox360/minplay/minplay.cpp` — оригинальный код
   (квадрат на стике, A = очко + вибрация, START = выход; XYZ + HLSL vs/ps_2_0,
   ортопроекция; претрансформированных вершин на 360 нет — проверено ошибкой
   компиляции `D3DFVF_XYZRHW`).
2. **Проект**: `minplay2010.sln/vcxproj` (конфигурация сверена с XDK-сэмплом
   TriangleHLSL: defines `NDEBUG;_XBOX;XBOX;...`, MT, либы
   `d3d9/d3dx9/xgraphics/xapilib/xaudio2/x3daudio/xmcore/xboxkrnl/xbdm`).
3. **Сборка**: `MSBuild.exe minplay2010.sln /p:Configuration=Release
   /p:Platform="Xbox 360"` → PPC compile + link + imagexex OK, 0 ошибок.
   Выход: `Release/minplay.xex` (2 629 632 байта) + `minplay.exe` (PE,
   machine **0x01F2** — pc2c классифицирует как `arch=xbox360`).
4. **Запуск** (Xenia master v1.0.2844, D3D12, официальный
   `release-builds-windows`, вне репозитория): `Launching module
   game:\minplay.xex`, entry `8224D788`, сотни кадров `XE_SWAP/VdSwap`
   1280x720 A8R8G8B8, PPC-трансляция активна, fatal-ошибок нет, процесс жил
   до принудительной остановки (цикл игры бесконечен по дизайну).
5. **Играть сейчас**: `xenia.exe Release\minplay.xex` — геймпад XInput (стик,
   A, START); клавиатурный маппинг — по документации Xenia.

Остаточный блокер: прогон на физическом devkit (нужна консоль).

## Этапы (по ТЗ, статус)

1. ✅ Диагностика SDK и инструментов (файловая система).
2. ⏳ Минимальная тестовая программа (следующий этап).
3. ⏳ Проверка компиляции/линковки/целевого формата (XEX через imagexex).
4. ⏳ Способы запуска/отладки (нужен devkit или легитимная тест-среда).
5. ⏳ Модуль консольных ресурсов.
6. ✅ Система описания платформенных API (ApiDb: D3D9/XInput/XAudio2 →
   CONVERTIBLE; D3D10+ → UNSUPPORTED; Win32-shell → UNSUPPORTED…).
7. ⏳ Планирование адаптации исходного кода (plan уже классифицирует).
8. ✅ Интеграция с Asset Pipeline (хост-сайд BMP/TGA работает без SDK).
9. ⏳ Сэмпл с графическим выводом.
10. ⏳ Проверка на целевой платформе.

## Консольные форматы текстур — исследование, не реализация

Известно (требует верификации тестовыми векторами до любого кода):
Xenos использует tiled/swizzled layout, отличный pitch/alignment/mipmap layout
от PC-DDS; «положить DDS как есть» нельзя. Поэтому конвертера Xenos-форматов
в MVP **нет**, ограничение явно задокументировано и отражено capability
`console-texture-output=false`. Следующий шаг: спека + тестовые векторы +
условия корректности, затем реализация.

## Ввод: геймпад (подтверждено по файлам SDK, реализации пока нет)

- `include/xbox/xbox.h:227-259`: классический опрос `XInputGetState/SetState/
  GetCapabilities` **есть** на 360; `XINPUT_STATE/GAMEPAD/VIBRATION` —
  `xinputdefs.h:158-179`; кнопки `XINPUT_GAMEPAD_*`, дедзоны стиков (7849/8689),
  порог триггеров (30).
- `include/xbox/xinput2.h` (512 строк): дополнительно сэмпл/контекстная модель
  (`XInput2Sample` + `XInput2Get*`) для экзотики (гитары/рули/штурвалы).
- `lib/xbox`: `xinput2.lib`, `xinputremap.lib` (+debug-варианты).
- `include/xbox/xinputdefs.h`: кнопки `XINPUT_GAMEPAD_*`, дедзоны стиков
  (7849/8689), порог триггеров (30).
- `lib/xbox`: `xinput2.lib`, `xinputremap.lib` (+debug-варианты).
- `include/xbox/xime.h`: IME текстового ввода (JP/KR/CN) — путь текстового
  ввода, не игровая клавиатура. Мыши и Win32 message-pump клавиатуры нет:
  весь mouse/keyboard-ввод — редизайн под геймпад (классификатор: user32 →
  UNSUPPORTED, dinput → REQUIRES_SOURCE).
- Реализация (слой опроса XInput2, дедзоны, маппинг, тест на железе) —
  Phase 5/6, после исходников и тест-среды. Сейчас — только классификация.
- UPDATE (Controller Compatibility): консольный backend реализован как
  `src/input/xbox360/Xbox360Backend.cpp` — один интерфейс `IInputBackend`,
  только подтверждённые вызовы (`XInputGetState/SetState`, disconnect=1167);
  скомпилирован XDK cl `/c` с 0 ошибок/варнингов (объект в temp, вне репозитория).
  Shared-заголовки (`Types.h`, `Backend.h`) — C++03-clean subset, собираются и
  хостом, и XDK. Детали и матрица тестов: `docs/CONTROLLER.md`.

## Запреты

- PE переименованием в XEX не превращается (формат, опкоды, ABI, syscalls).
- Автопереписывание DirectX текстовыми заменами не делается; только
  промежуточный графический интерфейс + проверенные преобразования.
- Закрытая игра без исходников → отчёт о препятствиях, не обещание автопорта.
