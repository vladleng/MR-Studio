# SHARED Stage 0 — проверка

Issue #15. В этом этапе проверяется общий backend. Окно DAW, ASIO и воспроизведение
аудио появятся в следующих этапах.

## Быстрая проверка на Windows

1. Скачать artifact `MR-Studio-SHARED-Stage-0-Windows` из успешного
   запуска GitHub Actions **Core contracts** для ветки
   `shared/stage-0-core-contracts`.
2. Распаковать архив. Запустить PowerShell в папке с exe.
3. Выполнить `.\mrs_core_check.exe`.
4. Проверить четыре строки PASS и итог
   `SHARED Stage 0 check passed`. Код завершения должен быть 0.
5. При желании выполнить `.\mrs_core_tests.exe model`, затем так же
   `timeline`, `transport`, `commands`, `serialization`, `integration`.
   Каждая команда должна вывести PASS и число проверок.

Если запускать exe двойным щелчком, консоль может сразу закрыться.
Проверять лучше из PowerShell.

## Что подтверждает проверка

- Arrange/Edit/Mix/Live используют общие Project/Transport handles.
- Действия одного режима видны другому.
- Подписки получают согласованное состояние.
- Undo/Redo сохраняет идентичность треков.
- Отклонённые команды не оставляют частичные изменения.
- Project snapshot v1 проходит save/load round-trip со всеми ID.
- Tempo/meter conversion и loop проходят граничные проверки.

## Границы этапа

Эта проверка не подтверждает качество аудио, ASIO latency, обработку MIDI,
плагинов или готовность к выступлениям. Это критерии следующих этапов.

Подробности контрактов и оставшихся задач: [CORE_CONTRACTS.md](CORE_CONTRACTS.md).
