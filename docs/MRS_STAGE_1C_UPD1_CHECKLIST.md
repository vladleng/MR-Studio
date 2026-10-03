# 0.1d upd1 fix1 — интерфейс и моно
Распаковать MR-Studio-0.1d-upd1-fix1-ASIO-Windows, запустить MoonRiverStudio.exe.

1. Фон, панели и кнопки серые. Выбранная вкладка тоже серая.
   Цвета аккордов, секций, аудиоклипов, waveform, playhead и clip selection прежние.
2. Тонкая верхняя строка меню: Files. В нём:
   New project, Open project, Save, Save as, Import WAVs, Open WAV as new project,
   Open demo project и Exit. Этих кнопок больше нет в рабочей области.
3. Проверить меню и горячие клавиши: Ctrl+N/O/S, Ctrl+Shift+S, Ctrl+I.
   Import добавляет WAV в текущий проект; Open WAV заменяет проект.
   При замене/выходе сохраняются существующие Save/Discard/Cancel предупреждения.
4. В верхней навигации только Arrange, Edit, Mix. Кнопки Live нет.
   Старое сохранённое предпочтение Live открывает Arrange.
5. Моно WAV звучит одинаково в первых двух выбранных выходах (обычно L/R).
   Стерео сохраняет L/R. При одном выбранном выходе моно тоже играет;
   дополнительные cue-выходы не получают моно автоматически.
6. Проверить Audio settings и ранее принятые Pause/edit/Undo, воспроизведение
   длинных WAV, сохранение проекта. Resize и масштаб Windows 100–150%.

upd1 — обновление интерфейса; fix1 — центрирование моно, объединены в одну сборку.
Базовая 0.1d проверена пользователем. Пользователь принял доработку 2026-10-03; PR #45 слит в main.
Live определён как отдельный show-режим внутри MRS с документом .mrlive, ссылками
на проекты и тем же SHARED Engine. Создание/открытие .mrlive и show-экран будут
реализованы в LIVE этапах; текущий Files работает с .mrsproject и WAV.

Все шесть CI jobs пройдены: Linux/ASIO 54/54, Windows offline 55/55 с GUI smoke.
Сборка для проверки: https://github.com/vladleng/MR-Studio/actions/runs/37128824438/artifacts/11276401042


## Acceptance — 2026-10-03
User confirmed all base 0.1d and UI/mono follow-up functions work. Stage 1c /
0.1d upd1 fix1 accepted; PR #45 merged into main. Validated code:
2177219103682c08bc011b9c5e9baee8f8f00aa5 (all six CI jobs passed; Linux/ASIO 54/54,
Windows offline 55/55). Whole #21 remains open. Next: Stage 1d / 0.1e recording,
monitor foundation and integrated save/load acceptance; development not started.
