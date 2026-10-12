# Stage 5 / #25: рабочий план музыкальной структуры

Актуально2026-10-12. Stage5/#25 выполняется: #79/#81/#82/#83/#85/#86 приняты и закрыты;
#83/0.3f принят2026-10-12: «#83 прошел приемку, что у нас дальше?». Full129 PASS.
#84/0.3g реализован: full130 PASS140.74s, packaged diagnostics/J3 exit0; приёмка ожидается.
#80/#84/#87 открыты; к #80/#87 автоматически не переходить. Current schema20/archive1.
Первоначальная спецификация подготовлена2026-10-09 на0.2g upd10/schema15/source15afe65;
требования и архитектурные ограничения ниже сохранены, исторические чекбоксы сверять
с текущими issues и тематическими отчётами. [Контекст](PROJECT_CONTEXT.md).

## Общие контракты

- Один Project Model, Application/Commands, Transport и AudioEngine для production
  и будущего Live; второй timeline или копия musical metadata не нужны.
- Chord Track хранит авторский текст. Распознавание гармонии, генерация MIDI,
  автоаккомпанемент и транспонирование содержимого вне этапа.
- Arranger сначала редактирует **секции и их метаданные**: изменение секции
  не переносит/обрезает/копирует clips. Content ripple/reorder — будущее расширение.
- Chords/sections/markers и MIDI привязаны к ticks (PPQ 960); WAV start/length/offset
  — samples. Tempo/meter edits не растягивают WAV.
- Tempo — ступенчатые BPM, anchor tick 0; meter — начала тактов, anchor bar 1.
  Изменение раннего meter пересчитывает ticks следующих bar-based meter points.
  Остальные musical objects сохраняют ticks; UI показывает итоговые позиции,
  не обещая сохранить прежние номера тактов.
- Current section/chord: [start,end); gap означает отсутствие current.
  Next имеет start строго позже tick. Marker current — последний <= tick
  со стабильным project tie order; next — первый strictly later.
- IDs глобально уникальны и стабильны при rename/move/Undo. Копия получает новый ID
  и не наследует Live bindings автоматически.
- Chords/sections не перекрываются; gaps разрешены. Markers с одинаковым tick
  разрешены. Невалидное/no-op/cancel изменение не даёт частичной публикации,
  нового Undo или dirty state.
- В первом объёме удаление section с SectionPatch либо marker с LiveAction
  отклоняется с понятной причиной. Не удалять связь молча и не создавать orphan.
  Полное связанное удаление требует отдельной атомарной команды/history всего
  документа и не обязательно для Stage 5.
- Loop — runtime sample range без section-ID binding. Перед изменением bounds,
  удалением section либо tempo/meter edit отключить loop. Application проверяет
  это до mutation. Rename/color не меняют loop.
- Tempo/meter и структурные изменения секций — Pause/Stop.
  Простые metadata edits допускаются при playback только через безопасный command
  path без DSP graph rebuild. Пока такого пути нет, первая сборка явно ограничивает
  edits Pause/Stop и фиксирует ограничение. Recording/pre-count блокирует структурные
  edits/navigation; Undo/Redo соблюдает те же ограничения.
- Timing edits сохраняют sample playhead. Timeline/MIDI/click/host context готовятся
  на control thread и публикуются согласованно. Prepare/publish failure оставляет
  согласованное исходное состояние, не выдаётся за успех или молчаливый audio disconnect.
- Callback не получает новых host allocations/locks/I/O/UI/ProjectStore reads.
  Current/next — control snapshot. Busy/queue-full/seek failure имеют явный результат;
  UI отображает подтверждённую transport position.
- .mrsproject — активная песня; будущий .mrlive — show-документ со ссылками
  на проекты и performance parameters. Активная песня использует тот же движок.
  Stage 5 готовит shared data/services, а не весь show UI или кнопку Live
  рядом с Arrange/Edit/Mix. Edit/load/seek не исполняют LiveAction.
- Selection/hover/popup временные. Существующие поля не требуют новой schema.
  Сохраняются unknown archive chunks, plugin opaque state, preset names и
  не затронутые musical/routing/MIDI/mixer данные.
- UI использует действующую тему, DPI и timeline transform; принятые clip
  drag/trim/copy/snap, mixer sends и recording не регрессируют.

## Порядок и чек-поинты

Уточнение 2026-10-10: пользователь прямо выбрал **#79 первым**. Необходимая
chord-only часть C0/#87 выполняется вместе с ним; далее следовать запросу
пользователя. Порядок ниже — исходная рекомендация, не запрет выбранного slice.
Пользователь подтвердил работоспособность Chord Track на **0.3a** и запросил
уточнения взаимодействий/оформления. **0.3a upd1** принят: «Отлично, все работает,
закрывай»; #79 закрыт completed. [Реализация и проверки](CHORD_TRACK_79.md).
Маркеры #82 приняты и закрыты как 0.3b (историческое имя пакета 0.3a upd2).
Current/next #85 и все дополнительные UI пункты 0.3c приняты:
«Закрывай, все пункты этого билда работают»; #85 закрыт completed.
#86 принят и закрыт 2026-10-12 на0.3d: «Билд проверку прошел».
Отдельные UI/browser/MIDI дополнения #148/#149 приняты на0.3d upd1/upd3.
Текущая принятая0.3e upd2; #81 закрыт2026-10-12 («Отлично принимаю! Что дальше?»).
Далее рекомендован #80 /0.3f; автоматически не начинать.
Текущая схема19/archive1, реализация и проверки: [NAVIGATION_86.md](NAVIGATION_86.md).
Каждый новый sub-issue увеличивает букву версии. Остальные sub-issues открыты.

1. **C0 / #87:** metadata-only command/history, references и отказ до mutation;
   fixtures shared model/schema 15/архивной совместимости. Проверить ветку до кода.
2. **C1 / #82:** marker editor → Commands → Undo → Save/Open.
3. **C2 / #85/#86:** shared current/next и подтверждённая navigation.
4. **C3 / #79:** Chord Track.
5. **C4 / #80/#81:** sections/bounds/colors/IDs/links/loop policy.
6. **C5 / #83/#84:** stopped preparation, tempo/meter, MIDI/click/plugin context.
7. **C6 / #87/#25:** сквозной проект, compatibility, регрессии, локальный пакет
   и пользовательская приёмка. Номер новой сборки определяется при выпуске.

#87 проверяется в каждом срезе, не откладывается на последний cleanup.
#83/#84 могут делить preparation-код, но принимаются отдельно.
Чек-поинты включены в существующие issues; создавать дополнительные задачи не нужно.

Уточнение 2026-10-12: пользователь выбрал **#81 следующим**, до #80.
0.3e реализует section colors/bounds и минимальное создание секции для доступа
к функции. В upd1 по прямому запросу добавлены rename/delete и меню ПКМ;
в upd2 — body move, inline rename, opacity/layout и marker colors (schema20).
#80 остаётся open: copy и целостная приёмка редактора — отдельный срез.
No content ripple, stable IDs/history/SaveOpen. [SECTION_COLORS_BOUNDS_81.md](SECTION_COLORS_BOUNDS_81.md).

## Общая матрица приёмки

| Проверка | Ожидаемое поведение |
|---|---|
| Edit → Undo → Redo → Save → Open | Точные values/order/IDs/references |
| Invalid/no-op/cancel | Без partial state, history/dirty и сдвига transport |
| Boundary/gap/marker ties | Однозначный current/next |
| Zoom/scroll/DPI/русские названия | Hit-test, полосы и playhead согласованы |
| Удаление привязанного объекта | Отказ, документ валиден и сохраняем |
| Recording/pre-count/active loop | Guards на уровне Application |
| Tempo/meter edit | WAV samples прежние, MIDI/click/host context согласованы |
| Legacy Save/Open | Supported snapshots 1–19, без потери поддерживаемых данных |
| Future Live consumer | Те же IDs/time/context через shared service |

Проверки выбирать по слою: model/commands/persistence, timeline/transport,
engine/MIDI/click, synthetic VST3 context. После кода — локальная сборка и
затронутые тесты; перед приёмкой этапа — необходимая общая регрессия и GUI/ASIO smoke.
Документальная подготовка не означает PASS; звук, визуальная и пользовательская
приёмка отмечаются отдельно. Не изменять пользовательские проекты/настройки.

## Основания

[Риски](STAGE_5_RISK_REVIEW.md), [musical contracts](MUSICAL_TIMELINE.md),
[persistence](PERSISTENCE.md), [Live workflow](LIVE_WORKFLOW.md),
[UI](UI_UX_CONCEPT.md), [roadmap](ROADMAP.md),
[metronome](METRONOME_PRECOUNT.md), [MIDI](MIDI_MUSICAL_EDIT.md),
[time stretch — отдельный этап](TIME_STRETCH.md).

Новая неоднозначность оформляется в соответствующем issue до расширения объёма.

## Описания sub-issues

### #79 — [Stage 5] Редактор Chord Track

Parent: #25

## Цель

Редактировать введённые пользователем аккорды на общей музыкальной шкале проекта.

## Объём и поведение

- Создание, изменение symbol/start/end, перемещение, изменение длительности и удаление события через UI и Commands. Редактор полей обеспечивает точный ввод; drag/resize используют общий timeline transform и snap.
- Обозначение сохраняется как авторский текст, без обязательного harmonic parser. Непустой symbol, текущий лимит модели 256 UTF-8 bytes; start < end, неотрицательные ticks и отсутствие пересечений.
- Границы [start, end), gaps допустимы. Редактирование не меняет ноты MIDI, WAV или соседние аккорды. Сохранить ID при edit; копирование, если реализуется, даёт новый ID.
- Полоса показывает длительности и current highlight; подробности current/next через #85. Selection/popup не сохраняются. Не требовать крупных карточек current/next.
- Одна завершённая операция/gesture — одна Undo entry; Cancel/Escape не меняет проект. Модель, UI и shared context обновляются из одной версии.

## Зависимости и архитектура

C0 #87; общий UI/context #85. Использовать существующие chords/SetMusicalData, избегать записи устаревших vectors поверх новых данных.

Общие контракты Stage 5: tick/sample anchoring, stable IDs, atomic validation, recording/loop guards и единый shared engine. Полный рабочий план — локальный `docs/STAGE_5_PLAN.md`; исходные основания — `docs/MUSICAL_TIMELINE.md`, `docs/PERSISTENCE.md`, `docs/LIVE_WORKFLOW.md`. UI идёт через Application/Commands; callbacks не получают UI/IO/locks/allocations.

## Контрольные точки

- [ ] C0: command/validation/Undo contract и layout полосы.
- [ ] C1: CRUD и точное редактирование времени, drag/resize/snap.
- [ ] C2: shared context, save/open, границы и UI smoke.

## Условия приёмки

- [ ] Создать Am, C/G и авторское русское обозначение; поменять текст и длительность, выполнить Undo/Redo и Save/Open — значения и IDs совпадают.
- [ ] Попытка overlap/нулевой длительности/пустого symbol/превышения лимита отклоняется целиком; gap не получает ложного current chord.
- [ ] На начале/конце события и после seek highlight соответствует #85; scroll/zoom/DPI не меняют hit-test.
- [ ] Audio/MIDI clips и транспорт не меняются от metadata edit; соблюдаются общие playback/recording ограничения.

## Проверки и завершение

Model CRUD/overlap/UTF-8, command history, serialization round-trip; GUI жесты/границы/длинный текст. Не проверять здесь распознавание или генерацию гармонии.

Код, build/tests, документация и пакет — локально. На GitHub фиксируются результаты и пользовательская приёмка; без push/merge/Actions. Чекбоксы пока не пройдены: это требования, а не отчёт о реализации.

---

### #80 — [Stage 5] Редактор Arranger Track

Parent: #25

## Цель

Сделать секции проекта редактируемыми; первый объём — музыкальная структура, без перестановки содержимого.

## Объём и поведение

- Создание, переименование, выбор, изменение start/end, перемещение и удаление секции. Границы и цвет детализированы в #81; общий редактор доступен из Arrange.
- Секции sorted/non-overlapping, [start,end), gaps разрешены. Не перемещать/обрезать/копировать clips, MIDI events, chords или markers вслед за секцией.
- Rename/move сохраняют ID и SectionPatch связь. Новая секция/копия получает новый ID и не наследует Live binding автоматически.
- Удаление секции с SectionPatch отклоняется до публикации с объяснением. Нет скрытого удаления patch либо orphan; #87 контролирует save validity.
- Bounds/move/create/delete выполняются на Pause/Stop; при active loop изменение bounds/delete требует отключить loop. Name/color не изменяют transport/loop.
- Выделение временное, completed gesture — одна Undo entry. Rename/color идут через metadata-only path, без перезапуска DSP.

## Зависимости и архитектура

C0 #87; #81 реализуется совместно; #86 навигирует по тем же IDs.

Общие контракты Stage 5: tick/sample anchoring, stable IDs, atomic validation, recording/loop guards и единый shared engine. Полный рабочий план — локальный `docs/STAGE_5_PLAN.md`; исходные основания — `docs/MUSICAL_TIMELINE.md`, `docs/PERSISTENCE.md`, `docs/LIVE_WORKFLOW.md`. UI идёт через Application/Commands; callbacks не получают UI/IO/locks/allocations.

## Контрольные точки

- [ ] C0: выбрать точный UI редактирования и безопасный reference/loop contract.
- [ ] C1: section CRUD/Undo и общий layout.
- [ ] C2: #81, #86, ссылки, save/open и GUI smoke.

## Условия приёмки

- [ ] Создать Intro/Verse/Chorus с gap, переименовать и переместить секцию: clips и остальные lanes остаются на месте, ID сохраняется.
- [ ] Overlap, отрицательная/нулевая длина и удаление привязанной секции дают понятный отказ без dirty/history изменений.
- [ ] Undo/Redo, Save/Open сохраняют порядок, bounds, имя, цвет, IDs и валидные связи.
- [ ] При active loop/recording/pre-count ограничения действуют и для команд; navigation использует отредактированные данные.

## Проверки и завершение

Section model/history/reference validation, persistence и GUI. Проверить принятые clip gestures; content rearrangement/ripple вне задачи.

Код, build/tests, документация и пакет — локально. На GitHub фиксируются результаты и пользовательская приёмка; без push/merge/Actions. Чекбоксы пока не пройдены: это требования, а не отчёт о реализации.

---

### #81 — [Stage 5] Цвета и границы секций

Parent: #25

## Цель

Дать секциям понятные визуальные границы и сохраняемый цвет без изменения содержимого проекта.

## Объём и поведение

- Редактируемый цвет section colorRGB 0xRRGGBB; палитра в принятой теме, читаемый текст/selection, корректный контраст. Цвет не заменяет системные error/status состояния.
- Точный ввод start/end и drag границ через общий time transform/snap. Показывать итоговую musical position и избегать неоднозначного hit-test с clip/ruler gestures.
- Никакого автоматического ripple или обрезки clips. Не допускать overlap и end <= start; gap допустим. Последний визуальный пиксель не меняет [start,end) semantics.
- Preview gesture временный, commit атомарный; отмена восстанавливает исходное значение без Undo entry. Completed gesture — одна entry.
- Сохранение color/bounds и ID; изменение bounds на Pause/Stop, active loop предварительно отключён. Смена цвета не сдвигает loop/playhead и не пересоздаёт DSP.

## Зависимости и архитектура

#80, C0 #87; общая полоса и #85 текущая секция.

Общие контракты Stage 5: tick/sample anchoring, stable IDs, atomic validation, recording/loop guards и единый shared engine. Полный рабочий план — локальный `docs/STAGE_5_PLAN.md`; исходные основания — `docs/MUSICAL_TIMELINE.md`, `docs/PERSISTENCE.md`, `docs/LIVE_WORKFLOW.md`. UI идёт через Application/Commands; callbacks не получают UI/IO/locks/allocations.

## Контрольные точки

- [ ] C0: palette/hit areas/точный ввод и конфликт жестов.
- [ ] C1: color/bounds Commands и Undo.
- [ ] C2: zoom/DPI/scroll, границы, сохранение.

## Условия приёмки

- [ ] Изменить цвет и обе границы, Undo/Redo и Save/Open дают точные исходные/новые значения и тот же ID.
- [ ] Overlap/нулевая длина/cancel не повреждают секции; active loop не остаётся со скрыто устаревшими границами.
- [ ] При нескольких zoom и 100/150/200% DPI границы/selection/подписи соответствуют реальным ticks, соседние clip gestures работают.
- [ ] Цвет читается в действующей теме и не маскирует error/recording state; clips не меняются.

## Проверки и завершение

Bounds validation/history/serialization и GUI color/hit-test/контраст; не вводить schema bump для существующего colorRGB.

Код, build/tests, документация и пакет — локально. На GitHub фиксируются результаты и пользовательская приёмка; без push/merge/Actions. Чекбоксы пока не пройдены: это требования, а не отчёт о реализации.

---

### #82 — [Stage 5] Редактор маркеров

Parent: #25

## Цель

Создавать, редактировать и удалять маркеры на timeline, сохраняя stable IDs и связь с общей моделью.

## Объём и поведение

- UI создания, выбора, переименования, точного ввода/перемещения tick, выбора существующего MarkerKind и удаления. Доступны generic/cue/warning/lyric/action/navigation.
- Markers с одинаковым tick допустимы; сохранять стабильный порядок проекта. Визуально выбирать каждый ID через список/меню, не терять маркеры под одной меткой.
- Tick >= 0 и в допустимом диапазоне модели; использовать общую validation для name/kind/ID. Move/rename сохраняют ID, создание выдаёт новый уникальный ID.
- Action/cue здесь только metadata: edit/load/seek не исполняют MIDI/Live actions. Не добавлять runtime trigger executor.
- Удаление marker с LiveAction отклоняется с понятной причиной до Project mutation. Не терять action link и не портить save.
- Atomic Commands, dirty, одна entry на gesture, cancel/no-op без history; playback metadata path и recording ограничения общие.

## Зависимости и архитектура

Первый вертикальный срез после C0 #87; результаты потребляют #85/#86.

Общие контракты Stage 5: tick/sample anchoring, stable IDs, atomic validation, recording/loop guards и единый shared engine. Полный рабочий план — локальный `docs/STAGE_5_PLAN.md`; исходные основания — `docs/MUSICAL_TIMELINE.md`, `docs/PERSISTENCE.md`, `docs/LIVE_WORKFLOW.md`. UI идёт через Application/Commands; callbacks не получают UI/IO/locks/allocations.

## Контрольные точки

- [x] C0: marker command/reference contract, список совпадающих позиций.
- [x] C1: UI CRUD и Undo/Redo.
- [x] C2: save/open, context/navigation и регрессия ruler.

## Условия приёмки

- [x] Создать все поддерживаемые виды, два marker на одном tick и русское имя; выбрать и изменить каждый независимо.
- [x] Move/rename + Undo/Redo + Save/Open сохраняют ID, kind, tick и tie order.
- [x] Удаление привязанного action marker отклоняется без изменений; обычный marker удаляется и восстанавливается Undo.
- [x] Edit/load/seek не отправляют MIDI actions; hit-test/scroll/zoom не ломают принятые ruler/clip gestures.

## Проверки и завершение

Marker model/history/duplicate ticks/reference persistence; GUI overlapping labels/list selection. Новые action semantics не входят в задачу.

Код, build/tests, документация и пакет — локально. На GitHub фиксируются результаты и пользовательская приёмка; без push/merge/Actions. Чекбоксы пока не пройдены: это требования, а не отчёт о реализации.

---

### #83 — [Stage 5] Редактирование карты темпа

Parent: #25

## Цель

Редактировать ступенчатую tempo map и согласованно применять её к timeline, MIDI, metronome и plugin host context.

## Объём и поведение

- Создание/перемещение/изменение BPM/удаление tempo points через UI и Commands; точный ввод tick/BPM. Anchor tick 0 остаётся, BPM anchor редактируется.
- Строго возрастающие уникальные ticks; finite BPM в существующем диапазоне 1–1000. Никаких ramps, авто stretch WAV или tempo detection.
- Musical objects/MIDI сохраняют ticks, WAV samples остаются прежними. Playhead сохраняет sample position; отображаемые ticks/bars могут измениться.
- Только Pause/Stop; recording/pre-count запрещают изменение; active loop сначала отключается. Ограничения проверяются Application.
- Prepare candidate timeline/MIDI/click/host context на control thread до согласованного commit. При prepare/publish failure — явный отказ и согласованное исходное состояние, без скрытого отключения аудио.
- Учесть tempo boundary внутри callback/process chunk, loop/seek и master/insert/instrument context. Новая карта не должна вступать в разных потребителях в разные моменты.
- Undo/Redo повторяет preparation и ограничения; невалидный/no-op edit не создаёт history. Не переносить capture/plugin ownership в callback.

## Зависимости и архитектура

C0 #87, общая stopped preparation с #84; metronome/pre-count #96 уже принят.

Общие контракты Stage 5: tick/sample anchoring, stable IDs, atomic validation, recording/loop guards и единый shared engine. Полный рабочий план — локальный `docs/STAGE_5_PLAN.md`; исходные основания — `docs/MUSICAL_TIMELINE.md`, `docs/PERSISTENCE.md`, `docs/LIVE_WORKFLOW.md`. UI идёт через Application/Commands; callbacks не получают UI/IO/locks/allocations.

## Контрольные точки

- [x] C0: time anchoring и prepare/commit/failure contract.
- [x] C1: UI/map validation, history и save/open.
- [x] C2: boundary engine/MIDI/click/VST3 tests и локальная ASIO приёмка.

## Условия приёмки

- [x] Карта 120→90→140 BPM: точное редактирование, Undo/Redo, Save/Open сохраняют points и IDs остальных musical objects.
- [x] Tempo boundary внутри блока: MIDI/click и synthetic VST3 context соответствуют одной карте на корректном участке; master/insert не расходятся.
- [x] WAV samples и sample playhead неизменны; новая musical позиция и текущие контексты пересчитаны. Нет заявленного time stretch.
- [x] Удаление tick 0, duplicate tick, NaN/invalid BPM, active loop/recording либо failure подготовки отклоняются атомарно.
- [x] Playback/seek/record после успешного commit работают; pre-count следует своему принятому контракту темпа в точке начала.

## Проверки и завершение

TimeMap conversion/boundaries/round-trip; failure-path history; MIDI/click/transport и synthetic VST3 tempo context; GUI + ASIO smoke. Callback allocations/locks не добавлены.

Код, build/tests, документация и пакет — локально. На GitHub фиксируются результаты и пользовательская приёмка; без push/merge/Actions. 2026-10-12: локальная0.3f реализована, Release/full129 PASS146.00s; packaged diagnostics/J3 PASSexit0, snapshots100/150 inspected. C0/C1 пройдены; C2 автоматическая часть PASS, пользовательская приёмка подтверждена2026-10-12. Подробная ASIO/DPI матрица не предоставлена. Детали — docs/TEMPO_MAP_83.md; source eb2f30e. Пользователь подтвердил общий PASS; новые измерения не приписываются. #83 закрыт completed.

---

### #84 — [Stage 5] Редактирование карты размера

Parent: #25

## Цель

Редактировать meter map по началам тактов и обеспечить согласованный счёт долей, сетку, click и host context.

## Объём и поведение

- Создание/редактирование/удаление meter point по номеру такта, numerator/denominator. Anchor bar 1 не удаляется; strictly increasing unique bars.
- Диапазоны существующей модели: numerator 1–64, denominator — степень 2 до 64. Meter point только на границе такта; PPQ/quarter остаётся 960.
- Доли 6/8 считаются eighth-note beats, не путать с quarter ticks. Проверить ruler, snap/BBT и piano-roll labels; не переопределять принятый MIDI quantize.
- Изменение раннего meter меняет tick позиции следующих bar-based meter points; chords/sections/markers/MIDI сохраняют ticks, WAV — samples. Показать новые положения без скрытого переноса контента.
- Pause/Stop, recording/pre-count и active loop policy как #83. Сохранять sample playhead, готовить timeline/click/host context до commit; failure атомарен.
- Проверить предоставляемые plugin context meter/bar fields и valid flags. Если существующий bridge их не передаёт, добавить согласованный prepared context, а не заявлять meter support по одному BPM.
- Undo/Redo/save/open и current/next проходят через ту же shared модель; без второго Live clock.

## Зависимости и архитектура

#83 общий timing preparation, C0 #87; #85 musical context; принятый metronome/pre-count.

Общие контракты Stage 5: tick/sample anchoring, stable IDs, atomic validation, recording/loop guards и единый shared engine. Полный рабочий план — локальный `docs/STAGE_5_PLAN.md`; исходные основания — `docs/MUSICAL_TIMELINE.md`, `docs/PERSISTENCE.md`, `docs/LIVE_WORKFLOW.md`. UI идёт через Application/Commands; callbacks не получают UI/IO/locks/allocations.

## Контрольные точки

- [x] C0: bar→tick contract, отображение beats и VST3 context.
- [x] C1: UI/validation/history/persistence.
- [ ] C2: 4/4→6/8→3/4, boundary/click/grid/plugin и ASIO smoke.

## Условия приёмки

- [ ] Создать 4/4→6/8→3/4: BBT/ruler/grid и click accent/beat count соответствуют map; проверены границы тактов.
- [ ] Изменить ранний размер: более поздние meter points остаются на заданных bar numbers; остальные musical ticks и WAV samples неизменны.
- [ ] Invalid denominator/duplicate bar/удаление bar 1/recording/active loop и prepare failure не дают частичной карты.
- [ ] Undo/Redo и Save/Open точны; plugin time signature/bar context и valid flags соответствуют новой карте.
- [ ] 6/8 beat labels и snap согласованы, существующее MIDI quantize не меняет семантику.

## Проверки и завершение

Meter/BBT conversions, denominator beats, map validation/history/persistence; MIDI/click/VST3 bar/meter boundary; UI/ASIO. Pre-count не запускает project content раньше принятого момента.

Код, build/tests, документация и пакет — локально. На GitHub фиксируются результаты и пользовательская приёмка; без push/merge/Actions. 2026-10-12: C0/C1 и автоматическая частьC2 реализованы и проверены на0.3g; full130 PASS140.74s, packaged diagnostics/J3 exit0. Подробности docs/METER_MAP_84.md, source7d90ad2. Пользовательская ASIO/DPI приёмка ожидается; acceptance чекбоксы не закрывать автоматически.

---

### #85 — [Stage 5] Отображение текущего и следующего музыкального контекста

Parent: #25

## Цель

Показывать согласованные current/next chord, section и marker из общего MusicalTimeline и подтверждённой позиции transport.

## Объём и поведение

- Использовать shared service/immutable control snapshot; не вычислять отдельную Live musical модель и не читать ProjectStore в audio callback.
- Current chord/section: [start,end); gaps показывают отсутствие, next — start строго позже tick. Marker current — последний <=tick с project tie order; next — первый strictly later.
- Отображать контекст в существующих Arrange lanes/компактных подписях. Будущий Live Chord Track — горизонтальная полоса с текущим highlight и видимыми соседями, без обязательных крупных карточек.
- Одна версия проекта/transport для всех полей; seek отображается после подтверждения движка. Сглаженный visual playhead не должен давать противоречивый highlight на границе.
- UI polling/coalesced refresh на control thread: не пересобирать timeline/DSP на каждый metadata revision и не блокировать UI ожиданием audio state.
- Edit/Undo/Open/seek/loop обновляют отображение; отсутствующие данные и конец проекта корректны. Никаких runtime actions при перескоке.

## Зависимости и архитектура

C0 #87 и #82; используются существующие sections/chords даже до editor #79/#80; #86 shared navigation.

Общие контракты Stage 5: tick/sample anchoring, stable IDs, atomic validation, recording/loop guards и единый shared engine. Полный рабочий план — локальный `docs/STAGE_5_PLAN.md`; исходные основания — `docs/MUSICAL_TIMELINE.md`, `docs/PERSISTENCE.md`, `docs/LIVE_WORKFLOW.md`. UI идёт через Application/Commands; callbacks не получают UI/IO/locks/allocations.

## Контрольные точки

- [x] C0: snapshot/revision и правила границ.
- [x] C1: UI current/next при playback и edits.
- [x] C2: seek/loop/gaps/ties и shared consumer fixture.

## Условия приёмки

- [x] В exact start/end и gap UI показывает однозначный current/next; конец проекта и пустые lanes не дают старого контекста.
- [x] Одинаковые marker ticks соблюдают стабильный порядок; next не застревает на текущем tick.
- [x] Seek/loop/Undo/Open не смешивают старые и новые IDs/tempo; queue-full/busy не показывает неподтверждённую позицию.
- [x] При playback UI остаётся отзывчивым; refresh не вызывает DSP rebuild и новых callback allocations/locks.
- [x] Shared consumer получает тот же контекст без копирования модели; полной реализации Live/show UI не требуется.

## Проверки и завершение

Boundary/gap/tie/snapshot service tests, transport acknowledgement/failure; GUI playback/seek/loop/edit refresh.

Код, build/tests, документация и пакет — локально. На GitHub фиксируются результаты и пользовательская приёмка; без push/merge/Actions. Чекбоксы пока не пройдены: это требования, а не отчёт о реализации.

---

### #86 — [Stage 5] Навигация по секциям и маркерам

Parent: #25

## Цель

Обеспечить предсказуемый переход по musical structure через единый transport без скрытых действий.

## Объём и поведение

- Предыдущая/следующая секция и marker, переход к выбранному ID из списка/полосы. Использовать существующие MusicalTimeline navigation commands.
- Previous внутри секции возвращает её начало; на начале — предыдущее начало. Next — строго следующее начало. Не wrap при достижении края.
- Markers с одинаковым tick дают одну transport position; выбор конкретного ID остаётся возможным. Для unknown/deleted ID — понятный отказ без seek.
- Сохранять play/pause state; recording/pre-count запрещает navigation. Не выполнять LiveAction/MIDI trigger при seek и не включать playback автоматически.
- Seek через подготовку/очередь движка; current/next и playhead после ack. Queue-full/busy/failure — явный результат, без оптимистического UI.
- Loop section — явная отдельная команда, если включается в UI: корректный sample range по текущей карте, runtime state без Project Undo. Bounds/map edit требует отключённого loop; переименование/цвет его не меняют.

## Зависимости и архитектура

#85 ack/context, #82 markers, C0 #87; #80 редактор секций может следовать позже.

Общие контракты Stage 5: tick/sample anchoring, stable IDs, atomic validation, recording/loop guards и единый shared engine. Полный рабочий план — локальный `docs/STAGE_5_PLAN.md`; исходные основания — `docs/MUSICAL_TIMELINE.md`, `docs/PERSISTENCE.md`, `docs/LIVE_WORKFLOW.md`. UI идёт через Application/Commands; callbacks не получают UI/IO/locks/allocations.

## Контрольные точки

- [ ] C0: edge/tie/recording/loop/ack contract.
- [ ] C1: кнопки/список/переход к ID.
- [ ] C2: playback seek, failure и изменённые/удалённые объекты.

## Условия приёмки

- [ ] Previous/next изнутри секции, с границы, из gap и на краях соответствуют правилам; нет wrap.
- [ ] Два marker на одном tick не создают бесконечного next; jump по каждому ID достигает общей позиции.
- [ ] После rename/move/Undo/Open выбираются актуальные IDs; stale ID отклоняется без transport mutation.
- [ ] Playback сохраняется, Pause не запускается; recording/pre-count защищены и на уровне commands.
- [ ] Queue failure не сдвигает UI; seek не исполняет action metadata. Если есть loop section — range соответствует карте и явному выбору.

## Проверки и завершение

Navigation edge/tie/unknown ID и transport ack/failure; GUI playing/paused/recording guards; loop mapping при наличии команды.

Код, build/tests, документация и пакет — локально. На GitHub фиксируются результаты и пользовательская приёмка; без push/merge/Actions. Чекбоксы пока не пройдены: это требования, а не отчёт о реализации.

---

### #87 — [Stage 5] Общая модель, Undo/Redo и сохранение музыкальной структуры

Parent: #25

## Цель

Обеспечить целостность всех редакторов Stage 5 через shared Project/ProjectDocument; выполнять проверки в каждом feature slice.

## Объём и поведение

- Начать с C0 до первого editor: command validation/dirty/history и metadata-only application path. Не отправлять устаревшие полные musical vectors из открытого popup поверх актуальной модели.
- Использовать текущую schema 20 и archive envelope 1, existing readers 1–20. Исходный план был на15; marker RGB добавлен на принятом #81. Не вводить новую схему для UI selection или существующих musical полей.
- Стабильные global IDs, точные time/color/symbol/kind/order values; SectionPatch/LiveAction references проверяются перед commit/save.
- Первый объём: запрещать удаление связанной section/marker, не терять внешние ProjectDocument bindings. Если реализуется связанное удаление, command/history должен охватывать весь документ атомарно.
- Rename/move не ломают bindings; копия получает новый ID без автоматического клонирования bindings. Невалидный/no-op/cancel не создаёт dirty/history.
- Save/Open сохраняет unknown archive chunks/order/bytes, plugin opaque state и preset names, routing/MIDI/mixer/accepted sends. Миграция legacy versions без потери поддерживаемых данных.
- Production/future Live читают ту же модель и shared service. .mrlive хранит show refs, а не копию музыкальной структуры активного .mrsproject. Никаких actions при load/seek.
- Metadata history не пересоздаёт DSP; timing history соблюдает stopped preparation и error atomicity #83/#84. Заключительная проверка всех девяти задач на одном проекте.

## Зависимости и архитектура

Сквозная задача для #79–86, первая C0 и последняя C6; исходные #15/#17 закрыты. Полный Live/show UI вне Stage 5.

Общие контракты Stage 5: tick/sample anchoring, stable IDs, atomic validation, recording/loop guards и единый shared engine. Полный рабочий план — локальный `docs/STAGE_5_PLAN.md`; исходные основания — `docs/MUSICAL_TIMELINE.md`, `docs/PERSISTENCE.md`, `docs/LIVE_WORKFLOW.md`. UI идёт через Application/Commands; callbacks не получают UI/IO/locks/allocations.

## Контрольные точки

- [ ] C0: command/history/reference/compatibility contract и fixtures.
- [ ] C1–C5: в каждом slice edit→Undo→Redo→Save→Open и invalid/failure checks.
- [ ] C6: единый mixed project, необходимые регрессии, локальный пакет и пользовательская приёмка.

## Условия приёмки

- [ ] Проект с chords/sections/colors/markers/tempo/meter проходит весь цикл с точными values/IDs/order и валидными bindings.
- [ ] Нельзя получить orphan или несохраняемый документ удалением, Undo/Redo, cancel либо stale UI edit.
- [ ] Schema 15 с preset names и поддерживаемые legacy fixtures открываются/сохраняются; неизвестные chunks и plugin state не теряются.
- [ ] Production и shared consumer видят одну ревизию/контекст; второго engine/timeline нет.
- [ ] Локальная сборка, затронутые проверки и общая регрессия этапа подтверждены отдельно от GUI/ASIO и пользовательской приёмки. Issue не закрывать по одной только сериализации.

## Проверки и завершение

Project/ProjectDocument commands/history/validation/archive migrations/unknown chunk retention; mixed fixture всех lanes; engine/GUI regression по изменённым слоям. Не менять пользовательские проекты/настройки.

Код, build/tests, документация и пакет — локально. На GitHub фиксируются результаты и пользовательская приёмка; без push/merge/Actions. Чекбоксы пока не пройдены: это требования, а не отчёт о реализации.
