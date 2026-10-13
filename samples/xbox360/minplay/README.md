# minplay — minimal PLAYABLE Xbox 360 sample (Phase 5)

Оригинальный код проекта (не копия сэмплов XDK). Доказывает сквозной путь
backend: PPC compile + link + imagexex-упаковка + геймпад + графика.

## Игра

Двигай квадрат **левым стиком** (с дедзоной). **A** — очко: квадрат меняет
цвет/растёт, пад вибрирует. **START/BACK** — выход. Мыши/клавиатуры нет.

## Сборка (проверенная команда, VS2010 + XDK 21256)

```bat
C:\Windows\Microsoft.NET\Framework\v4.0.30319\MSBuild.exe minplay2010.sln /p:Configuration=Release /p:Platform="Xbox 360"
```

Результат: `Release\minplay.xex` (imagexex запускается целями MSBuild автоматически).

## Запуск

- Devkit: скопировать `minplay.xex` через `xbcp`, запустить (см. XBOX360_SDK.md).
- xbEmulate/Xenia: verified-статус см. в `docs/XBOX360_BACKEND.md` (Phase 5.4).
