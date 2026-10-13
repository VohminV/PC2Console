# Xbox 360 SDK 21256.3 — аудит состава

Дата проверки: 2026-10-13. Метод: чтение каталогов, заголовков, версий файлов.
Неизвестные EXE не запускались (кроме опционального `doctor --probe-tools`,
который опрашивает только известные бинари SDK флагами версии).

## Корень

`C:\Program Files (x86)\Microsoft Xbox 360 SDK` (`%XEDK%` совпадает).
Подтверждена версия **21256** (`include/xbox/xdk.h`: `_XDK_VER 21256`;
`imagexex.exe` 2.0.21256.0). Каталоги: `bin/{win32,x64,xbox}`,
`include/{win32,xbox}`, `lib/{win32,x64,xbox}`, `doc`, `Source/{crt,Samples,Tools}`,
`system`, `redist`, `art`, `xboxfont`, `TechPreview`.

## Ключевые инструменты (`bin/win32`)

| Компонент | Статус | Версия/примечание |
|---|---|---|
| `cl.exe` (PPC кросс) | ✅ | 16.00.11886.00 |
| `link.exe` | ✅ | 10.00.11886.00 |
| `lib.exe`, `dumpbin/editbin`, `ml.exe` | ✅ | тот же набор 10.00.11886 |
| `imagexex.exe` | ✅ | 2.0.21256.0 — упаковщик XEX |
| `fxc.exe` | ✅ | 5.2.3790.1830 — шейдеры Xbox |
| `xbcp/xbdir/xbmemdump/xbreboot`, `xbdm.dll` | ✅ | нужен devkit для реального использования |
| `pix.exe`, `xbperfview`, `xstudio`, `xact*`, `xma2encode`, `xwmaencode` | ✅ | аудио/отладка/перф |
| `xdkvars.bat`/`xdkenv.bat` | ✅ | выставляют `PATH/INCLUDE/LIB/_NT_SYMBOL_PATH` |

## Заголовки и библиотеки

- `include/xbox`: 306 файлов — `xtl.h`, `xbox.h`, `xgraphics.h`, `d3d9*.h`,
  `d3dx9*.h`, `xaudio2*.h`, `xact3.h`, `xinput2.h`, `xam.h`, `xbdm.h`,
  CRT-заголовки, `ppcintrinsics.h`, `xnamath*.h/inl`.
- `lib/xbox`: 134 файла — `xboxkrnl.lib`, `d3d9*.lib`, `d3dx9*.lib`,
  `xgraphics*.lib`, `xaudio2.lib`, `xapilib*.lib`, `xact*.lib`, `xinput*.lib`,
  `xnet.lib`, `xonline.lib`, `xam`-смежные, `libcMT/libcpMT(+d)`, `xboxmath`.
- Сэмплы `Source/Samples` + `Common`: Audio/Graphics/Media/NaturalInput/
  Online/System/UI.

## Интеграция

- **VS2010 Ultimate ENU** установлен, `VS100COMNTOOLS` задан — требование
  XDK-сборки выполнено на уровне присутствия.
- `doc/1033`, `relnotes.htm` (ссылка на GDN), `readme1st.txt` (подключение devkit).

## Честные ограничения

1. Присутствие ≠ исправность: `canBuildMinSample=false`, пока Phase 5
   (реальный compile+link+imagexex минимального сэмпла) не пройден.
2. Запуск/отладка XEX требуют devkit или легитимную тестовую среду —
   в наличии не подтверждены.
3. Лицензирование: закрытые файлы SDK не копируются в репозиторий,
   не распространяются, установщики не запускаются на основной системе.
4. `pc2c doctor` по умолчанию только читает ФС; `--probe-tools` — единственный
   режим, выполняющий известные бинари SDK (версионные запросы).
