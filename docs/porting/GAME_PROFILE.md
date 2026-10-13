# GAME_PROFILE — Canada Hunt (C:\Realistic Hunting Games\canada_hunt\game)

Дата: 2026-10-13. Источник: рекурсивный обход + `pc2c inspect/analyze/inventory/report` + чтение ключевых файлов. Копия игры не делалась (только чтение); оригиналы не изменялись.

## 1. Сводка

- Файлов: **1607**, байт: **206976779** (`inspect`: `files=1607 bytes=206976779`).
- `inventory`: `resources=1607 total_bytes=206976779 convertible_bmp_tga=4981752`.
- `report` (`canada_model.json`): `facts=1634, assumptions=4, limitations=29`.
- Больших архивов >50МБ: нет. Файлов без расширения: нет. Скрытых: нет.

## 2. По расширениям (факт обхода 2026-10-13)

| ext | файлов | объём |
|---|---|---|
| .png | 524 | 83778KB |
| .jpg | 210 | 46091KB |
| .dso | 177 | 656KB |
| .hlsl | 171 | 287KB |
| .dts | 159 | 27355KB |
| .ogg | 91 | 10931KB |
| .cs | 79 (+TourqeScript) | 162KB |
| .dsq | 88 | 5644KB |
| .gui | 15 | 56KB |
| .mis | 7 | 3883KB |
| .ter | 7 | 4042KB |
| .ml | 7 | 3323KB |
| .bmp | 6 | 4864KB |
| .dll | 3 | 5879KB |
| .exe | 1 | 1877KB |
| .h | 6 | 13KB |
| .uft/.dml/.u3d/.hfl/.ms3d/.max/.fil/.cfg/.ico/.log/.wav/.psr/.dtx/.mtl/.obj | остатки | ~3МБ |

`pc2c` категории: `image=740 (137968648B), audio=92 (11203440B), shader=171 (294816B), model=1 (129268B, эвристика), executable=1, library=3, config=3, other=596`.

## 3. Движок и технологии (факты vs гипотезы)

Подтверждено:
- **Torque Game Engine Advanced (TGEA)**, confidence medium (`engine guess: torque? [medium] via torque artifacts`).
  - `main.cs:271`: `if( $platform !$= "xbox" && $platform !$= "xenon" )` — xenon-ветвь в этой линейке движка.
  - `common/prefs.cs`: `This is a Torque Game Engine Advanced server`, `MaxPlayers=64 Port=28000`.
  - `common/main.cs`, 177 `.dso` (компилированные скрипты), 159 `.dts` (модели), 88 `.dsq` (анимации), 7 `.mis`/`.ter` (миссии/террейн), 15 `.gui`.
  - `console.log` (2026-10-13, реальный запуск на этом ПК): `DirectInput enabled`, `GFXD3D9Device - using 'd3dx9_37.dll'`, `Pix/Vert 3.0`, `800x600 fullscreen 32bpp`, `UDP port 0`, `SFXProfile preload failed` (2 шт), `Missing loadingGui.gui`.
- EXE: `Canada_Hunt.exe x86 PE32, linker 9.0 VS2008, 17 импортов, 12 DLL` (`analyze`).
- Графика: D3D9 runtime (лог), при импорте `Direct3DCreate8` — движковый GFX-слой выбирает D3D9 (`ApiDb`: d3d8 → REQUIRES_SOURCE).
- Аудио: `OpenAL32.dll` (экспорты `al*/alc*`, linker 8.0 VS2005) + 91 `.ogg` + 1 `.wav`.
- Сеть: `wsock32.dll`, UDP 28000, до 64 игроков.
- Ввод: `scriptsAndAssets/client/config.cs` (17 строк, полная карта): `keyboard w/s/a/c/d/z/f1/p/r/tab`, `mouse0 xaxis=yaw yaxis=pitch button0=altTrigger button1=mouseFire`; `defaults.cs:25` джойстик есть но `$pref::Input::JoystickEnabled=0`. `input analyze`: `has_sources=yes findings=25, verdict REQUIRES_REDESIGN`.
- Исходников C++ игры/движка: **нет** (только `.h` шейдерные + `shdrConsts.h`). `.vcproj/.sln`: нет.
- DRM/защита: не обнаружена (нет признаков; обход не выполнялся).
- Символы отладки: нет данных (PE без проверки PDB; `.pdb` в игре нет).

Гипотеза: версия TGEA 2008-era (по датам файлов 2008-2009 + d3dx9_31/33 + VS2008). Не подтверждена точная ревизия.

## 4. Роль файлов и зависимости

- Обязательны для старта (по логу): `Canada_Hunt.exe + main.cs + common/main.cs + scriptsAndAssets/{client,server}/main + d3dx9_*.dll + OpenAL32.dll + profile/D3D9.cs + террейн/миссия из .mis/.ter`.
- Контейнеры: **нет** (archives=0 в report). Все ресурсы — россыпью, пути захардкожены в скриптах (`C:/Realistic Hunting Games/...` в логе).
- Таблицы/манифесты движка: `.mis` ссылаются на `.ter/.dts/.cs`; `.gui` на `.png/.jpg`; `.cs` на `.dso` (компилят при загрузке). Полный граф не построен — форматы Torque не парсятся pc2c (реестр декодеров пуст).
- Генерируемое при запуске: `console.log`, `prefs.cs` (даты 2026-10-13 — перезаписаны игрой).
- Опциональное: сетевой сервер (можно offline), `tools/` (отсутствует — `Missing file .../tools/main.cs` в логе, не критично).

## 5. Профиль совместимости Xbox 360

- Подтверждено совместимое: только CRT (`msvcrt.dll` → SUPPORTED).
- CONVERTIBLE (нужен source): D3D9-класс, XInput-часть (мышь — нет), `d3d8→D3D9`, сокеты→xnet, OpenAL→XAudio2 (нужна лицензия).
- UNSUPPORTED (8 шт, verdict BLOCKED): `comdlg32/gdi32/ole32/oleaut32/shell32/user32` (+gdi32 в d3dx9 DLL).
- REQUIRES_SOURCE: CPU x86, kernel/advapi/winmm/wsock32/imm32.
- UNKNOWN: graphics `none-detected` (импортов D3D9 в таблице нет — грузится через GetProcAddress, caveat `import presence does not prove a call`).
