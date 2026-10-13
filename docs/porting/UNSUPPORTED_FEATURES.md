# UNSUPPORTED_FEATURES — реестр (Canada Hunt → Xbox 360)

Каждая запись: диагноз + чем проверено. Неизвестное не объявляется поддержанным.

## U1. CPU x86 → Xenon PPC — REQUIRES_SOURCE (блокер)
`Canada_Hunt.exe/OpenAL32/d3dx9_31/33 = x86 PE32`. PPC несовместим: формат, опкоды, ABI, syscalls (`ApiDb.cpp:139-143`). Переименование PE→XEX невозможно (`Backend: pe-to-xex-direct supported=false`).

## U2. Win32 UI/shell — UNSUPPORTED 8 шт (блокер)
`comdlg32/gdi32/ole32/oleaut32/shell32/user32` (+`gdi32` в d3dx9 DLL). На 360 нет Win32 message-pump, мыши, окон (`ApiDb.cpp:36-45`, `xime.h` только IME). Редизайн под геймпад/XUI, нужен source.

## U3. Ввод мышь-прицел — REQUIRES_REDESIGN (блокер)
`config.cs:14-17 mouse0 xaxis=yaw yaxis=pitch button0/1`. На 360 мыши нет; `XInput2Sample+XInput2Get*` не маппится 1:1 (`ApiDb.cpp:49-57`). `input analyze verdict: REQUIRES_REDESIGN`. Геймпад-профиль `canada_hunt_gamepad.json` и `canada_gamepad.cs` — драфты `UNVERIFIED/UNPROVEN`.

## U4. D3D8-путь — REQUIRES_SOURCE
Импорт `Direct3DCreate8` при активном `GFXD3D9Device` (лог). D3D8 на 360 нет (`ApiDb.cpp:58-63`). Нужен апгрейд GFX-устройства движка до D3D9-класса + source.

## U5. Шейдеры: 1 падение + SM1 fallback — PARTIAL
`shaders convert sm2: total=176 compiled=170 failed=1 unsupported=5` (`pc2c-shader-report.json`).
- Failed: `refractPixBumpV.hlsl: error X4509 invalid register semantic 'VC_TEX_TRANS2'` — реальная несовместимость с Xbox microcode compiler, нужна правка шейдера.
- Unsupported: 5 инклудов (`.h`) — честно не угаданы.
- `*1_1`-варианты — SM1.x фолбэки, движок выбирает в рантайме; на 360 нужен единый vs_2_0/ps_2_0 путь.

## U6. Текстуры PNG/JPEG 734 файла 133МБ — UNSUPPORTED (главный asset-блокер)
`assets list`: все `.png/.jpg` `convertible=no`. Декодеров нет (`SUPPORTED_FORMATS.md:15`). Конвертер трогает только `.bmp/.tga` (`Converter.cpp:108`), остальные молча пропускает (не считает unsupported — см. PORTING_DECISIONS D2). Без декодеров игра не найдёт UI/террейн/скайбоксы.

## U7. Форматы Torque — реестр пуст (блокер)
`.dts 159 (27МБ)/.dsq 88/.mis 7/.ter 7/.dso 177/.gui 15` — декодеров нет, граф зависимостей не построен, extract/repack невозможен. `inventory` только детектит.

## U8. Аудио OGG/WAV 92 файла 11МБ — REQUIRES_RESEARCH
Пайплайна нет (MVP). OpenAL→XAudio2/XACT требует консольной сборки + лицензии (`ApiDb.cpp:112-118`).

## U9. Сеть wsock32 — REQUIRES_SOURCE
Порт 28000/64 игрока → xnet/secure sockets, нужен source + тестовая служба.

## U10. Xenos tiling/swizzle — нет writer (ограничение)
`Backend: console-texture-output supported=false`. Оценки GPU — RGBA8+mips без tiling (`PORTING_LIMITATIONS.md:9`), т.е. снизу. DDS «как есть» класть нельзя.

## U11. Память 512МБ unified — PARTIAL (посчитано частично)
BMP-подмножество: `4981752 → 3938724B, gpu 8851340 → 7000048` (`assets convert xbox360-480p`), `verify 6/6`. Остальные 202МБ (включая 133МБ PNG/JPEG) не ужаты и не оценены на GPU. Бюджет не сходится без U6+U10.

## U12. Запуск игры — NOT_RUN
`minplay.xex`/`canadaview.xex` в Xenia — RUNTIME_VERIFIED (2026-10-13, D3D12, сотни свопов), но это не игра. Игра particular: runtime-проверки `NOT_RUN` (нет исполняемого игры под 360).
