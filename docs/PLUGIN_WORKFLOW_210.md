# Plugin workflow — #210

Контракт доработки [#210](https://github.com/vladleng/MR-Studio/issues/210),
2026-10-17. Пользовательская приёмка ожидается; результаты локальных проверок
фиксируются в Issue отдельно от этого контракта.

## Insert drag/drop

В Mixer захват названия FX с ЛКМ и движение более 5 logical px начинает drag.
Внутри канала это reorder; на другой канал ЛКМ копирует, Shift + ЛКМ переносит.
Линия показывает границу вставки, подпись — Reorder/Copy/Move. Недопустимая
цель отмечается Invalid drop. Список прокручивается при drag. При drop заново
проверяются document identity/revision, source slot, цель, transport и лимит 8.
Escape отменяет стандартный JUCE drag; no-op не создаёт запись Undo.

Инструмент остаётся первым узлом instrument channel и не переносится как FX.
FX можно вставить только после него. MIDI-only каналы не принимают audio FX.
Track/Mixer channel reorder и отдельная instrument button принадлежат #213.

Reorder сохраняет PreparedGraph, индексы узлов, processor/controller/editor
instances, параметры и IR loaders. После Stop/Pause и quiesce всех DSP owners
готовится новый serial edge/order/PDC plan, затем публикуется обычный engine
graph с прежними insert graphs. Ни factory/prepare/restore, ни detach/attach
editor при reorder не вызываются. Undo/Redo используют тот же путь.
Новый план сначала целиком проверяется/выделяется на control thread.

Copy получает новый slot ID и независимое состояние, захваченное у живого
VST3 до клонирования. Move сохраняет slot ID. Межканальная операция — одна
команда Undo; затронутые цепи готовятся до model commit и используются как
runtime без второго throwaway preflight. Остальные каналы сохраняют графы.
Межканальный перенос может пересоздать затронутые DSP/editor instances;
гарантия сохранения экземпляров относится к reorder внутри канала.
Структурные операции по-прежнему требуют Stop/Pause.

## Общий power

Кнопка около имени канала управляет только текущими insert FX; первый
instrument node исключён. Это bulk bypass, не mute и не выгрузка цепи.
Одна команда SetInserts выключает FX, повторное включение восстанавливает
индивидуальную маску до выключения. В смешанном состоянии виден отдельный
маркер и tooltip. Уже полностью bypassed цепь без запомненной маски включается.

Текущее bypass-состояние — Project Model: Undo/Redo и project save/reopen.
Историческая маска восстановления — ограниченная текущим проектом/сессией
UI utility; она не включается в preset и не переносится между компьютерами.
Новые/скопированные FX сохраняют своё собственное bypass-состояние: bulk power
действует на существовавшие в момент нажатия FX. Это может сделать цепь mixed.
Принятый bounded bypass/PDC contract #134 сохраняется.

## Host power и pin

Слева в MR-owned toolbar находится power SVG, справа pin SVG. Используется
существующий Theme; vendor-owned VST3 GUI не изменяется. Power проецирует тот
же slot bypass, что Mixer. Редакторы синхронизируют состояние из модели.
Pin означает сохранение окна при открытии другого редактора; глобальный
Always On Top не включается, прежний native owner и Space bridge сохраняются.

По решению пользователя pin хранится в ViewSettings.nativeEditorViews по
стабильному slot ID, вместе с остальными UI-настройками. Он переживает
close/reopen и перезапуск приложения; старые настройки без поля pinned дают
false. Копия с новым slot ID не наследует pin оригинала. Project schema и
archive format не изменены. Существующий лимит UI-cache 256 entries сохранён.

## Проверка

Адресные processing latency/realtime, desktop inserts/Undo/persistence,
synthetic VST3 state/creation/editor, JUCE J3/native editors и затронутые GUI
smoke. Software preview не заменяет ASIO/xrun/listening/physical DPI.
Точные source/EXE, PASS/FAIL/NOT TESTED и пользовательская приёмка — в #210.
