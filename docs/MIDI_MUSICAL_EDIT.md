# 0.2e / Stage 4e — Musical note editing

Issue #67, parent #24. 0.2d принята пользователем: «Да, все работает».
После уточнения пользователя выполняется 0.2e, 0.2f ещё не начата.
Все дополнительные доработки отложены до реализации всех пунктов Stage 4.

## Контракт

Piano roll → Musical edit... открывает Quantize/Transpose/Velocity/Length.
Scope: Selected notes либо Whole clip (ноты, пересекающие слышимый диапазон).
Пустое выделение сообщает ошибку; оно не означает весь клип.

Quantize: 1/4, 1/8, 1/16, 1/32, 1/8 triplet, 1/16 triplet, strength 0–100%.
Start only / Length only / Start and length — независимая политика. Сетка начала
отсчитывается от source_offset (начала видимой части клипа); duration от нуля.
Ближайшая позиция, tie округляется от нуля; strength линейно приближает к сетке
с округлением до tick (PPQ960). Strength 0 не изменяет ноты.
Transpose: целые -127..127 semitones. Общий сдвиг ограничивается диапазоном
всей группы, сохраняя интервалы при pitch 0/127.
Velocity: Set/Add/Scale %, результат округляется и ограничивается 1–127.
Length: Set/Add в quarter-note beats, Scale %. Минимум 1 tick; выход за клип
ограничивается. Для исходных нот, пересекающих trim edge, сохраняется допустимая
исходная граница; скрытые части не обрезаются автоматически.

Одна Apply — один общий EditMidiNotes/Undo; no-op не создаёт history entry.
ID/order/channel и события контроллеров сохраняются. Редактирование после
Pause/Stop, во время recording запрещено. Save/open используют schema 13 без
новых полей. Clipboard/selection и настройки dialog — transient UI state.

## Слои и realtime

Shared core transform/command, Application, JUCE dialog, tests/docs: AFFECTED.
Project note model/persistence/engine playback: существующие paths переиспользованы.
Audio callback/RT ingress, routing и scheduling: NOT AFFECTED. Core доступен
Studio/встроенному Live; dialog — Studio UI. 0.2f/CC lanes: DEFERRED.
Новые вычисления и allocations происходят на control/message thread; существующая
проверка проекта/подготовка графа предшествует commit, rollback сохраняется.

Source `abd77b09f3edb2045873cf33e19d291095eda26b` локально. Full Release без warnings, 113/113 CTest PASS (58.96 s).
Packaged recording/clips/live/J3 PASS; dialog 100%/150% просмотрен.
Core transform, selection/isolation, borders, IDs/controller preservation,
Undo/Redo/serialization и GUI regressions PASS. FEATURE READY WITH MANUAL CHECK.
ASIO/VST3/mixed DPI для новой поставки не запускались. Пакет/хеш — PROJECT_CONTEXT.
