# #85 — текущий и следующий музыкальный контекст / 0.3c

## Контракт

Один shared MusicalTimeline и immutable ProjectStore snapshot для Arrange/Edit/Mix
и будущего Live consumer. Desktop берёт одну копию MusicalContext на message thread
после Application::poll; labels и подсветка lanes используют её project/revision,
подтверждённый transport, tick и current/next. Seek не отображается оптимистически.
Busy mailbox удерживает последний coherent transport; queue-full не меняет позицию.

Current chord/section: [start,end), в gap — «-». Next имеет start строго позже tick.
Current marker — последний <=tick, равные ticks сохраняют project order; next —
первый strictly later. После конца chord/section нет current/next; последний marker
остаётся current по своему контракту. При пустом/новом проекте поля очищаются.

Компактная строка над Arrange: Chord / Section / Marker, формат Current → Next.
Длинные значения сокращаются в строке, полный текст в tooltip/accessibility title.
Current chord выделяется общим accent; section имеет тонкий accent border; current
marker group — прозрачную accent полосу. Selection — независимое временное состояние.
При смене current IDs сбрасывается интерполяция visual playhead, чтобы линия не
задерживалась на старой стороне границы при уже обновлённой подсветке.

Refresh не меняет проект/history/DSP. Timeline компилируется только при time/rate
изменениях. Marker index сортируется только при изменении marker vector; обновление
clip/mix/chord не пересортировывает markers. Labels форматируются при изменении
контекста/проекта, а не при каждом новом sample. RT callback не получает UI/IO/locks,
новых allocations или musical model. Event trigger executor не добавляется.

## Дополнительные запросы пользователя

- Кнопки Arrange и Audio settings удалены. Edit/Mix закрываются своим переключателем;
  Audio settings доступны через меню. Focus order обновлён.
- Повторное имя дорожки над clips убрано; имя остаётся в левой панели.
  Clip rectangle: top=trackTop+1, height=trackHeight-3. Между соседними clips —
  ровно 3 logical px при любых trackHeight/vertical scroll. Paint, hit-test,
  selection, drag/trim, MIDI/audio recording preview используют одну clipRect.
- Строка context занимает 24 px; Arrange/browser начинается y94 вместо y70.
  Максимальная высота mixer dock уменьшена на 24 px, чтобы сохранить минимальную
  полезную высоту Arrange и фиксированные фейдеры на окне 1200×700.
- Нумерация: #79=0.3a, #82=0.3b (исторически выдан 0.3a upd2), #85=0.3c.
  Существующий принятый пакет не переписан.

## Затронутые слои

UI и NON-RT shared context refresh затронуты; engine/transport используются через
принятый bounded mailbox. Durable model, schema15/envelope1, commands и Undo не
меняются. Новый context UI transient и не сохраняется; Save/Open обновляет projection.
Shared consumer fixtures используют тот же service без копии проекта. Полный
navigation UI #86 и section editor #80/#81 не входят в этот срез.

## Проверки

musical_context/edits/navigation/engine: boundaries/gap/end/ties, subscriptions двух
consumers, immutable old snapshot, revision, Undo/Redo, tempo, loop, acknowledged
seek, queue-full и busy mailbox. GUI musicalContextSmoke: labels, current chord,
exact gap/tie, delayed seek, busy, edit/history, DSP generation, loop wrap, Save/Open,
новый/пустой проект, отсутствие старых кнопок, clip gap на высотах 56/128/200 со scroll,
fresh software screenshots 100/150%. Сохранены существующие ruler/clip/mixer tests.

Первый compile не PASS: ошибочные add_track/path в новой test fixture исправлены
на add_instrument_track и filesystem::path. Первый GUI run не PASS: maximum dock
съел минимальный Arrange после новой context строки; production bounds исправлены.
Следующий run обнаружил старую fixture, требующую удалённую Arrange; она обновлена.
Финальные результаты и source/package provenance — в PROJECT_CONTEXT.

Пользователь принял все пункты 0.3c 2026-10-10: «Закрывай, все пункты этого
билда работают». #85 закрыт completed; дополнительные UI изменения также приняты.
Реальная ASIO и physical mixed-DPI отдельно инструментально не проверялись. Старые installed TH-U/Nuro проверки не повторялись.

## Результат локальной проверки

Feature source 71e232b9798338c3110a531ffe7d7e87d679d716: Release all targets PASS,
120/120 CTest PASS (98.34s), log build/asio-local/03c-final-ctest.log.
Final source be083addc91c66480dd0ba74f3ba39c7da991804: corrected placeholder glyph,
tooltip hit region, current representative for tied marker group, fresh playhead
in preview. Release GUI PASS; packaged J3/diagnostics PASS, exit0, version0.3c,
clean_shutdown. Final 100/150 previews viewed, matching PDB saved separately.
До пользовательской проверки: FEATURE READY WITH MANUAL CHECK / RT SAFE WITH MANUAL CHECK.
Теперь пользовательская приёмка завершена; исходные свидетельства проверок сохранены.
Package/version/hash and diagnostics log path are recorded in PROJECT_CONTEXT.
