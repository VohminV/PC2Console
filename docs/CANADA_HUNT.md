# Canada Hunt (2009, PSR Outdoors) — досье переноса на Xbox 360

Дата анализа: 2026-10-13. Игра запускается на данном ПК (console.log от сегодня).
Исходники C++ отсутствуют; TorqueScript (.cs) — в наличии.

## 1. Что это (подтверждено файлами)

- **Движок: Torque Game Engine Advanced (TGEA)** — `main.cs` (Torque Demo CLI,
  GuiCanvas, `$platform`), `common/main.cs`, 177 `.dso`, 159 `.dts`, 7 `.mis` /
  `.ter`, 15 `.gui`, `prefs.cs`: *"This is a Torque Game Engine Advanced server"*.
  pc2c: `engine guess: torque? [medium]`.
- **EXE**: `Canada_Hunt.exe`, x86 PE32, линкер 9.0 (**VS2008**), всего 17 импортов:
  `Direct3DCreate8` (!), LoadLibraryA/GetProcAddress (D3D9 грузится динамически —
  лог: `GFXD3D9Device - using 'd3dx9_37.dll'`), mixerOpen, GetSaveFileNameW,
  ImmGetContext, SHGetSpecialFolderPathW.
- **Графика**: D3D9, SM 3.0 (Pix/Vert 3.0 по логу), 800×600 fullscreen; 171 HLSL
  (пары `*P/*V`, есть `*1_1`-варианты → SM1.x-фолбэки, модель выбирает движок
  в рантайме — строки моделей в исходниках отсутствуют).
- **Звук**: OpenAL (`OpenAL32.dll`), SFX-профили в скриптах.
- **Сеть**: wsock32 (UDP, порт 28000, до 64 игроков по prefs).
- **Ввод**: DirectInput + TorqueScript `moveMap` (`client/config.cs`, 17 строк —
  полная карта!): `w/s` = движение, **мышь: xaxis=yaw, yaxis=pitch,
  button0=altTrigger, button1=mouseFire**; `a/c/d/z/f1/p/r/tab` — игровые функции.
  Джойстик в движке есть, но выключен (`$pref::Input::JoystickEnabled = 0`).
  pc2c-вердикт ввода: `REQUIRES_REDESIGN` (мышь-прицел без эквивалента).
- **main.cs:271**: `$platform !$= "xbox" && $platform !$= "xenon"` — ветвь уже
  предусматривала консоли этой линейки в данной ветке движка.

## 2. Ресурсы (1607 файлов, 207 МБ)

| Формат | Файлов | Объём | Статус |
|---|---|---|---|
| PNG + JPEG | 734 | **133 МБ** | ⚠️ детект, декодеров нет — главный asset-блокер |
| BMP | 6 | 5 МБ | ✅ сконвертировано в 480p (5.0→3.9 МБ, verify 6/6) |
| HLSL | 171+5 | 0.3 МБ | ✅ **170 скомпилировано в Xbox-микрокод** (см. §6) |
| OGG/WAV | 92 | 11 МБ | аудио-пайплайн вне MVP |
| DTS/DSQ/MIS/TER/GUI/CS/DSO | ~350 | ~49 МБ | форматы Torque — реестр декодеров пуст |

## 3. Вердикт плана (`pc2c plan --target xbox360`)

**BLOCKED, 8× UNSUPPORTED** (comdlg32, gdi32, ole32, oleaut32, shell32, user32…),
остальное `REQUIRES_SOURCE` (CPU x86, kernel/advapi/winmm/wsock32/imm32/d3d8),
`SUPPORTED` только CRT. Бинарник портировать нельзя — требуется исходный код.

## 4. Что реально сделано сейчас

1. Полный отчёт: `canada-report.json` / `canada-plan.json` / `canada-inv.json` (temp).
2. Игровой геймпад-профиль **`profiles/canada_hunt_gamepad.json`** (v1, 14 действий,
   0 конфликтов): левый стик=движение, правый=обзор (sens 1.4), RT=огонь,
   LT=прицел, X/B/Y/LB/RB/Back/RThumb/Start=функции PSR — из `config.cs`.
3. BMP-пайплайн доказан на игре (6/6, verify ok).
4. Правила классификатора расширены по фактам игры: d3d8/imm32/wsock32,
   Torque-сигналы (`.dso/.mis/.ter/.dif/.dts`), Torque-токены анализа
   (`moveMap.bind`, `ActionMap`, `mouse0`, `$platform`) + 4 регрессионных теста.

## 6. Конвертация исходников (новое, 2026-10-13)

### 6a. HLSL → Xbox 360 микрокод: 170/176 ✅

Команда: `pc2c shaders convert <shaders> --output <dir> --profile sm2`
(настоящий XDK fxc 2.0.21256.0, профили vs_2_0/ps_2_0, entry main).
Результат на игре: **compiled=170, failed=1, unsupported=5** (отчёт
`pc2c-shader-report.json`, выходы `.vso/.pso`).
Единственное настоящее падение: `refractPixBumpV.hlsl` —
`error X4509: invalid register semantic 'VC_TEX_TRANS2'` (реальная
несовместимость с Xbox microcode compiler, требует правки шейдера).
5 unsupported: `.h`-инклуды + файлы без стадии (честно, не угаданы).
По пути найдены и починены два бага конвертера: отсутствие родительских
каталогов (fxc1001) и смешанные `/` в путях вывода (fxc1001).

### 6b. TorqueScript config → gamepad ActionMap ✅

Команда: `pc2c input torquescript config.cs --output gamepad.cs --bind fn=CONTROL ...`
Результат на игре: **12 активных привязок, 2 честных драфта** (клавиши w/s на ось —
value semantics требуют ревью, закомментированы, не активный код).
Функции игры сохранены дословно (`PSR_aim()`, `yaw`, `mouseFire`…),
имена joystick-устройств помечены UNVERIFIED, вердикт механизма — UNPROVEN
до подтверждения движкового пути. Файл: temp `canada-gamepad.cs`.

## 5. Путь к играбельному порту (честно, по порядку)

1. **Движок**: выяснить доступность Xbox 360-сборки Torque-линейки с xenon-ветвью
   (см. main.cs:271) + лицензия; без неё — только реверс/реимплементация.
2. **Исходники C++ игры** (Canada_Hunt.exe закрыт) — без них только пп. 3–4.
3. **Ввод**: включить джойстик (`JoystickEnabled`), перенести `config.cs` на
   `canada_hunt_gamepad.json`, редизайн прицеливания мышь→стик.
4. **Ресурсы**: PNG/JPEG-декодеры в pc2c (133 МБ — следующий asset-этап),
   шейдеры — Xbox fxc после движка, аудио — XAudio2.
5. **Сборка/запуск**: XDK toolchain (доказан на minplay) → XEX → Xenia/devkit.
