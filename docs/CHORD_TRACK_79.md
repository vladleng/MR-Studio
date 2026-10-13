# #79 — редактор Chord Track

Принято2026-10-13: **0.3h upd2/#80**, «Все работает - закрывай!».
#80 closed completed; parent#25 актуально8/9, остаётся#87 (не начат).
Приёмка docs/issues-only, source/пакет/manifest/ярлык и прежние проверки неизменны;
новая ASIO/physical DPI матрица не предоставлена. [Контекст](PROJECT_CONTEXT.md).

0.3h upd2/#80, 2026-10-13: uniform marker/chord group gestures, chord color palette/radius3.
Schema21 (legacy1–20), archive1. Full130 PASS148.13s, packaged diagnostics/J3 exit0;
GUI100/150 inspected. Upd2 accepted2026-10-13 («Все работает - закрывай!»); #80 closed completed.
[Контракт/проверки](MUSICAL_LANE_GESTURES_UPD2.md). #87 не начат; code/build/package local.

Принято и закрыто completed 2026-10-10 на 0.3a upd1: пользователь
«Отлично, все работает, закрывай». В следующей сборке вместе с #82 убрать
обводку всей полосы при клике; выделение отдельного аккорда сохранить.

## Уточнение взаимодействий — 2026-10-10 / 0.3a upd1

Пользователь подтвердил «Chord track работает» на 0.3a и запросил дальнейшие
правки. Двойной клик по пустой полосе создаёт Am сразу, без формы: начало по
Arrange Snap, длительность до четырёх четвертей либо ближайшего следующего
аккорда. Двойной клик по аккорду / Enter открывает редактор symbol/start/end;
ПКМ оставляет контекстное меню. + Chord также создаёт сразу.

Backspace (и прежний Delete) удаляет выбранный аккорд. Как у клипов, Alt-drag
копирует выбранный объект, а Duplicate создаёт копию непосредственно после его
конца. Toolbar Duplicate/удаление выбирают цель по текущему выделению; выбор
клипа очищает выделение аккорда и наоборот. Источник копии остаётся на месте,
копия получает новый ID и собственную одну Undo entry. Preview копии временный;
Escape/focus loss отменяют. Перекрытия в единственной chord lane запрещены:
конфликтный preview отмечается цветом, release/duplicate отклоняется атомарно,
без новой записи истории. CopyChord проверяет expected source в shared core,
Application использует прежний metadata-only edit/history; schema не меняется.

Полоса использует тот же arrangementCanvas и общий paintGrid, что Arrange.
Прозрачность заливки 0.72 совпадает с клипами; обводка аккордов уменьшена ровно
вдвое (обычная 0.5, выбранная 0.75 logical px). Ширина области захвата края прежняя.
Source `68ff0cdf217c7e32c798044ae826af97e9f0eaba` (feature `04f14af`).
Release GUI/desktop/core/musical/timeline build PASS; CTest **23/23 PASS (44.04s)**
(core, musical, desktop_chords, J1/J3/diagnostics). Новые регрессии: прямое
создание/Undo/Redo, double click editor, Alt-copy/fresh ID/source preservation,
overlap отказ без истории, toolbar target, Backspace, exact canvas/grid-through-alpha;
desktop copy во время playback не меняет sample/device/plugin generations.
После harness-only исправления PNG append выполнены GUI rebuild и packaged
J3/diagnostics PASS, exit0, version=0.3a upd1, clean_shutdown.
Свежие software screenshots 100/150 просмотрены; native pointer/ASIO/mixed-DPI
для уточнений приняты пользователем 2026-10-10. Issue #79 закрыт completed.

Компактный пакет:
`C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.3a-upd1-Chord-Track-JUCE-ASIO-Windows-local`.
EXE SHA256 `A2A791FE4EB651F7E4EE198C473F85E99D16AD904521CB34C2756E80424B4FE9`.
Validation/PDB/PNG/logs: `build/asio-local/deliveries/0.3a-upd1`;
session `mr-session-1791599144316-12096-1.log`. Старые пакеты не изменены.

## История первого кандидата 0.3a

Начато 2026-10-10, ветка `mrs/0.2g-fix1-editor-space-local`, чистый HEAD a5ae86b.
База: принятая 0.2g upd10/source15afe65. Кандидат: **0.3a**.
Пользователь назначил 0.3a вместо ошибочного 0.2g upd11 и указал на лишние картинки
в папке пакета. Номер исправлен в приложении; runtime package содержит только
EXE/helpers, licenses, README и SHA256. Previews/tests/docs/logs/Symbols отдельно.
Пользователь выбрал #79 первым; необходимая часть C0/#87 включена в эту задачу.
Другие редакторы Stage 5 и полный #85/#87 не объявляются реализованными.

## Поведение и входы

- Полоса Chord Track под секциями Arrange показывает длительности событий.
  Текущий аккорд выделен синим по подтверждённому shared MusicalTimeline context;
  gaps и exclusive end не продлевают аккорд.
- `+ Chord` создаёт Am в позиции playhead либо следующем свободном месте.
  Двойной щелчок по пустой полосе создаёт Am без формы, по событию открывает редактор.
  ПКМ — Add/Edit/Duplicate/Remove; Enter редактирует, Backspace/Delete удаляют.
  Alt-drag копирует с preview, toolbar Duplicate копирует после конца выбранного аккорда.
- Поля: авторский symbol, Start и End в ticks, End exclusive; PPQ 960.
  Символ сохраняет введённое обозначение, включая кириллицу и slash chords.
  Нет распознавания гармонии, генерации MIDI или транспонирования.
- Drag тела переносит событие, drag левого/правого края меняет длительность.
  Используется Arrange transform и Snap (1/16, ppq/4).
  Соседние события ограничивают жест; точный ввод overlap отклоняется.
- Preview живёт только в UI; отпускание — одна command/history entry.
  Escape/focus loss отменяют. Смена проекта/объекта/time map/view transform
  делает старый gesture недействительным. Нельзя менять проект toolbar shortcut
  посреди preview.
- Update/delete применяются к конкретному ID с проверкой expected baseline.
  Устаревшая форма/меню не перезаписывает более новый edit и другой проект.
  Cancel/invalid/no-op не меняют revision/dirty/history.
- Можно редактировать и выполнять chord Undo/Redo во время playback и loop.
  Recording/pre-count блокирует команды и UI. Audio/MIDI clips, time map,
  sections/markers, transport, plugin runtime и routes не изменяются.
- Нет новой Project schema: существующие Chord поля в schema 15, envelope 1.
  Selection, focus, popup и preview временные и не сохраняются.

## Слои и state flow

| Слой | Статус / решение |
|---|---|
| UI | AFFECTED: отдельный ChordTrack, существующая тема и координаты Arrange |
| Shared model/domain | AFFECTED: ID-targeted AddChord/UpdateChord/RemoveChord |
| Application/Commands | AFFECTED: validated chord-only publication |
| Undo/Redo | AFFECTED: существующий ProjectStore, chord-only history без rebuild |
| Audio/RT | RT-ADJACENT audit; callback/DSP/graph preparation NOT AFFECTED |
| Notifications/context | Existing shared ProjectStore/MusicalTimeline; без второго clock |
| Serialization/compatibility | Код NOT AFFECTED: существующие schema 15/readers 1–15 |
| Project files | Формат NOT AFFECTED; обычное save управление media сохраняется |
| Studio/Live | SHARED MODEL/COMMANDS; новый editor только production Arrange |
| Tests/docs | AFFECTED: desktop_chords + real GUI smoke и локальные документы |

UI → Application chord command → candidate validation → ProjectStore execute →
shared context/UI snapshot → Undo/Redo/save/open. Chord command не использует
общий structural `Application::edit`, потому что он rebuilds DSP graph.
History выбирает chord-only path только если ВСЕ остальные поля проекта равны.
Time/routing/clip/inserts history сохраняет прежний путь и ограничения.

## RT boundary и ограничения

Все vector/string allocations, sorting, expected checks, dialogs и gestures идут
на message/control thread. Callback не получает ProjectStore, locks/I/O/UI calls
или новые allocations. DSP/плагины сохраняют прежнего владельца.
Metadata edits не вызывают plugin capture, device restart, timeline rebind или
engine prepare; карта времени не меняется. Existing poll может отправить прежние
bounded parameter/mix values, но не пересоздаёт graph и не меняет MIDI generation.
MIDI route publication сравнивает routes/generation перед реальным изменением.

Current chord highlight использует acknowledged shared position. Полный current/next
display #85, Live/show UI, keyboard selection traversal и action execution не входят
в этот slice. Форма времени использует точные ticks; дополнительные BBT поля
можно добавить отдельно без изменения durable model.

## Проверки

Feature source: `40f856a5985f35760f9233074487c6ad93577299`.
Current version source: `c018d2e6ee5975977dd12c39bd62ed045da1616f`.
Cached configure Windows x64/MSVC/JUCE/ASIO/VST3: PASS.
Локальная Release сборка всех targets: PASS, без compiler warnings/errors.
Полный CTest: **119/119 PASS, 102.23 s**, включая desktop_chords,
musical/context/engine/persistence и JUCE J1/J2/J3/diagnostics.
Логи: `build/asio-local/02g-upd11-build.log`,
`02g-upd11-gui-build.log`, `02g-upd11-final-ctest.log`.
Software 100/150 previews просмотрены: длительности/current/selection/полоса
и соседние элементы читаемы. Физический mixed-DPI/ASIO/pointer не проверен.

- desktop_chords: sorted insertion; ID-targeted rename/move/delete/history;
  invalid/overlap/limits/stale/no-op atomicity; playback/sample/loop/MIDI generation/
  insert generation/device open count; gaps; UTF-8; archive unknown chunk;
  save/open; сохранность clips/time; recording/pre-count guards.
- GUI: actual modal form create/edit/cancel/invalid/stale/project replacement;
  mouse body/оба края/snap/off-grid; one Undo/Redo; Escape/focus loss; selected Delete;
  boundary/gap highlight; zoom/scroll hit-test; 100/150 software snapshots.
- Не запускать специальные installed TH-U/Nuro проверки без новой причины.

Первый focused run: исправлены две ошибки fixtures (expected media-path rewrite
при managed save и явный UTF-8 ввод в JUCE String); результаты этого прогона
не являются PASS. Также обновлён прежний playhead hit-test: новая редактируемая
полоса принимает мышь, playhead остаётся прозрачным для неё и ruler.
Финальный прогон выше выполнен после этих исправлений.

## Пакет

Локальная папка:
`C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.3a-Chord-Track-JUCE-ASIO-Windows-local`.
Компактный runtime EXE, scanner/crash reporter, licenses, README и SHA256.json.
Embedded JUCE resources не требуют loose assets. Previews, логи, fixtures/tests,
документация и matching PDB остаются в local build/validation area.
0.3a validation working directory: `build/asio-local/deliveries/0.3a`;
Release version rebuild log: `build/asio-local/03a-version-build.log`.
Предыдущие пакеты сохранены; новая папка не содержит PNG/test logs/docs/Symbols.
EXE SHA256: `0817575211B3323104B950D29138A500FF398D47F8B158BA30FB936D7BAF15FD`.
Исходный packaged smoke под ошибочным номером 0.2g upd11: PASS/exit0/clean_shutdown.
Повторный packaged 0.3a J3/diagnostics smoke: PASS, exit 0; session log
`diagnostics-gui-smoke/mr-session-1791597966624-13644-1.log` в validation directory
подтверждает `version=0.3a` и `clean_shutdown`.
FEATURE READY WITH MANUAL CHECK / RT SAFE WITH MANUAL CHECK.

## Пользовательская проверка

1. Создать Am и C/G двойным щелчком либо кнопкой + Chord.
2. Изменить авторское название и Start/End, проверить ошибочный overlap.
3. Перенести аккорд и обе границы, включить Snap; Escape отменяет drag.
4. Undo/Redo по одной операции; Save/Open сохраняет названия, позиции и длительности.
5. Во время ASIO playback изменить аккорд/границы и Undo: звук и loop продолжаются,
   clips не перемещаются; граница/gap корректно меняют highlight.
6. Во время записи/pre-count изменения запрещены; проверить pointer и DPI своей системы.

Issue остаётся открытым до пользовательской приёмки. Документация/код/сборки локально;
GitHub используется для issue progress. Без push/merge/Actions. Прежние пакеты сохранены.
