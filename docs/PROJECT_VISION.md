# MR Studio / MR Live — долгосрочное видение

## Продукты

**MR Studio** — независимая Windows/JUCE DAW для создания музыки: аудио и MIDI, Arrange/Edit/Mix, VST3/ARA2, native DSP, музыкальный таймлайн, сохранение и восстановление проектов.

**MR Live** — независимое приложение для выступления: show/setlist, items, playback/stems, live inputs, patch/sub-patch, MIDI и управление на сцене. Его `.mrlive` — собственный show-документ. Оно может запускаться из Home MR Studio, но **не является встроенным Live Mode**, не открывает Studio song напрямую как Live item и не требует запущенной DAW. Экспорт Studio → Live Playback — отдельная будущая функция, не существующий импорт.

Обе программы разделяют **Shared Core** (transport, audio/MIDI, project/routing/processing infrastructure, plugin host) без дублирования backend. Это повторное использование кода, а не тождество форматов документов или UI.

## Принципы

- Надёжность real-time звука и предсказуемость сценического исполнения важнее косметических расширений.
- Выделенный аудиопоток не блокируется GUI, AI, network, disk I/O или тяжёлой подготовкой DSP.
- Использовать общие Commands/Undo/Redo, стабильные ID, миграции форматов и совместимость проектов.
- Единый [MR UDS](MR_UNIFIED_DESIGN_SYSTEM.md) для обеих программ и собственных плагинов при разной организации экранов.
- Система допускает независимые выпуски Studio и Live, с отдельными версиями и пользовательской приёмкой.

## Источники решений

[AGENTS.md](../AGENTS.md) — обязательные правила; [ARCHITECTURE.md](ARCHITECTURE.md) — технические границы; [MR Live native integration](MR_LIVE_NATIVE_INTEGRATION.md) — show workflow; [GitHub Issues](https://github.com/vladleng/MR-Studio/issues) — приоритеты, scope, статус. Долгосрочное видение не подтверждает готовность функции.
