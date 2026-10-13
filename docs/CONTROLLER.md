# Controller Compatibility — подсистема ввода PC2Console

Независимая подсистема (ядро не связано с платформенными API): анализ,
перенос и тестирование управления Xbox 360 Controller без нарушения
существующей игровой логики.

## Компоненты (`include/pc2c/input/`, `src/input/`)

| Компонент | Роль |
|---|---|
| `Types.h` | `ControllerState`, кнопки (биты = `XINPUT_GAMEPAD_*`), оси, `BackendCaps`, `RumbleRequest`. C++03-clean: shared с XDK TU |
| `Backend.h` | `IInputBackend`: `poll()`, `setRumble()`, `caps()` — единственный контракт |
| `Axis.h` | `StickFilter`: deadzone radial/axial, чувствительность, инверсия, кривая gamma, опциональное сглаживание (с латентностью!); диагонали клампятся к 1.0 |
| `Mapping.h` | `InputMapping`: действия→кнопки/оси/триггеры, конфликты, completeness |
| `Profiles.h` | Версионируемый JSON (`input_profile_version: 1`): маппинг, стики, камера, rumble, платформа, источник; `validate()` |
| `Manager.h` | `ControllerManager`: фреймы, edges, reconnect-счётчик, без протухшего состояния |
| `Rumble.h` | `RumbleAdapter`: импульсы с таймером; без поддержки — честный no-op |
| `Diagnostics.h` | Сэмплинг, замер Гц, ASCII-визуализация deadzone, JSON с `tested_on` |
| `Analyze.h` | Статический анализ ввода игры (исходники + импорты), черновик карты привязок |
| `backends/WindowsBackend.cpp` | Настольный backend: динамический XInput (1_4→1_3→9_1_0), свой код, только этот TU знает Win32 |
| `xbox360/Xbox360Backend.cpp` | Консольный backend, **только XDK toolchain** (`#error` без `_XBOX`); использует лишь подтверждённое: `XInputGetState/SetState` (xbox.h), `XINPUT_STATE` (xinputdefs.h), `ERROR_DEVICE_NOT_CONNECTED` (winerror.h), `XUSER_MAX_COUNT` |

## CLI

```bat
pc2c input backends
pc2c input diag [--user N] [--seconds S] [--rumble-test] [--format json]
pc2c input analyze <dir> [--format json]
pc2c input profile validate <file>
pc2c input profile make-default --output <file> [--target xbox360]
```

## Профиль (v1)

Действия, привязки кнопок/осей/триггеров, deadzone, sensitivity, inversion,
`curve_gamma`, камера (`sens_x/y`, `invert_y`), rumble (`enabled`, `strength`),
`platform`, `source`, `required_actions`. Дефолт: левый=move, правый=camera,
RT=fire, LT=aim, A=jump, X=action, LB/RB=оружие, D-pad, Start/Back.

## Матрица тестирования: что где проверено (§9)

| Проверка | PC (desktop) | Xbox 360 |
|---|---|---|
| Чтение контроллера (Windows XInput, DLL 1_4) | ✅ реально (нет пада → честный disconnect; Гц замерен) | — |
| Нормализация/дедзоны/кривые/триггеры/диагонали | ✅ 37 автотестов | — (та же математика, C++03-clean TU) |
| Профили save/load/validate | ✅ автотесты + CLI | формат переносим |
| Reconnect, missing pad, rumble-unsupported | ✅ автотесты (FakeBackend) | — |
| Консольный backend | — (XDK-only TU, by design) | ✅ компиляция XDK cl `/c`, 0 ошибок/варнингов |
| Сквозной ввод в игре | — | ✅ minplay в Xenia: опрос `XInputGetState`, Present-loop; интерактив с падом — сторона пользователя |
| Rumble на консоли | — | код `XInputSetState` в minplay + backend; эффект — на железе/пользователе |

## Правила

- Нет пада → `connected=false`, нули не подменяются.
- Нет rumble → `setRumble=false`, адаптер молчит.
- Нет исходников → вердикт `REQUIRES_SOURCE`/`UNKNOWN`, никаких обещаний;
  код в чужой процесс не внедряется.
- Дефолтное сглаживание ВЫКЛ (латентность документируется при включении).
- Диагонали не дают буст скорости (кламп магнитуды).
