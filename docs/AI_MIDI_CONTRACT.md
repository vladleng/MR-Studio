# AI MIDI draft — #165 / 0.3l, 2026-10-13

Запрос: «Приступай к #165 и в этом билде сделай фон маркеров на 50 процентов
прозрачным, не затрагивая текст и сам флажок». База — принятая0.3k/#164.

## Пользовательский путь

Создать/выбрать instrument track с нужным инструментом или выделить MIDI clip.
Browser→AI→Permissions→EDIT до запроса. Указать musical range, стиль/плотность
и требуемую задачу: новая партия, замена диапазона либо изменение существующих нот.
Tempo/meter/chords/sections, selected track/clip и его notes/events входят в bounded
snapshot. Отсутствующий Chord Track/key signature не заменяется выдуманным контекстом.
Style/density и drum pitches/maps можно явно указать в сообщении. В текущей модели
MR Studio отдельного сохранённого Drum Map/key signature нет; ассистенту предписано
уточнять drum pitches/map при отсутствии данных, не предполагать GM по умолчанию.
Новый track, instrument load или изменение device/routing этим tool не создаются.

Для длинного draft в Connection можно выбрать Output:2048/4096; default1024 прежний,
получение большего ответа может стоить больше. Один запрос/message, без retries и
query loop. После ответа review локального before/after и Apply; для MIDI нужны
остановленный transport, loop off, отсутствие recording/count-in. READ/SUGGEST
не пишут. Draft insert/replace/edit требует EDIT; AUTO mask config8 сохраняет
старые5bits, разрешает только выбранные marker/transpose/velocity tools. Даже AUTO
использует ручной Apply. Selection preview остаётся unsupported.

## JSON contract

Tool `midi_draft` добавлен в envelope v1:7 Core descriptors,5 provider suggestion
tools (query/selection не объявлены provider). Closed objects/strict integer JSON;
unknown/duplicate keys, floats, missing fields, invalid UTF-8, trailing/truncated
data отклоняются. До32KiB/envelope, depth8/nodes4096,256 total notes+CC per draft;
group1–8plans, preview1024fields/64KiB, existing session budgets прежние. Даже
корректный draft может превысить общий diff/playback/project budget и будет отклонён.

```json
{
  "mode": "insert",
  "track_id": "existing-instrument-track-id",
  "name": "Bass",
  "tick_start": 0,
  "tick_end": 3840,
  "notes": [{"start": 0, "duration": 960, "pitch": 36, "velocity": 100, "channel": 0}],
  "cc": [{"start": 0, "controller": 11, "value": 80, "channel": 0}]
}
```

`tick_start/tick_end` — absolute project ticks, half-open range,960ticks/quarter;
не seconds/samples и не fixed4/4 bars. `notes.start/cc.start` — относительно начала
этого range. Duration>0, нота целиком внутри range. Pitch0–127, velocity1–127,
channel0–15 (UI показывает1–16), CC controller/value0–127. CC — existing MidiKind2;
program/pressure/pitch bend events сохраняются, этим draft не заменяются.

- INSERT: required track_id/name, clip_id запрещён. Новые host IDs, один новый MIDI
  clip [tick_start,tick_end) на existing instrument track. Existing clips не удаляются;
  временное перекрытие clips возможно, как при обычном human insert. С0.3l upd2
  notes=[]/cc=[] допустимы для явно запрошенного пустого клипа.
- REPLACE: required clip_id, track_id/name запрещены. Только существующий MIDI clip
  и range внутри его visible project range. Удаляются ноты в подтверждённом range
  и только CC этого range; остальные notes/events/source trim/clip geometry/IDs
  сохраняются. Нота, пересекающая границу range, вызывает отказ всей операции.
  Пустые notes/cc допустимы как явная очистка подтверждённого диапазона.
- EDIT: required clip_id, track_id/name запрещены. Для каждой note/CC required id
  существующего объекта этого clip и range; полные новые значения. ID сохраняется.
  Остальные events/notes не меняются; пустой edit отвергается.

IDs в INSERT/REPLACE events запрещены; host генерирует их при подготовке, сохраняет
в Preview и повторяет точно через Redo. Source-offset conversion выполняется host,
скрытые части trimmed source сохраняются. Polyphony по разным pitches/channels
разрешена; same-pitch/channel overlap в draft target отвергается вместо неявного
слияния note-off intervals. Coincident duplicate CC/controller/channel отвергаются.
Это ограничения AI draft, не изменение human MIDI editor semantics.

Existing transpose_notes/set_note_velocity теперь применяются через trusted Preview
и prepared MIDI transaction. Их explicit note IDs и whole-source-note semantics
как у существующего Core editor; visible range показывает project MIDI clip start.
Model/project limits:32768 notes/project,8192channel events/clip,65536events/project,
existing clip/voice limits и compiled MIDI playback budgets проверяются до Apply.
Модельный `finish_reason=length` блокирует весь group, даже если отдельная tool JSON
выглядит завершённой. Неверный group не применяется частично.

## Runtime, history и persistence

Core commands AddMidiClip/SetMidiNotes/SetMidiEvents/EditMidiNotes reused. Draft
bound к expected clip; ProjectEdit preflights group и immutable before/after.
Diff содержит target IDs, clip/range/mode, note pitch/velocity/channel/start/duration
и CC controller/value/start. Midi plans дополнительно проходят compile budget
validation без device/DSP/plugin capture или model publication.

Application использует existing TimingPlan (он содержит также MIDI playback data),
без пересоздания processor graph. Control thread готовит note/CC playback, затем
device stop→engine quiesce/join→symmetric swap→ProjectStore commit→device start→
transaction finish. Отказ restart/publication возвращает old MIDI plan и всю историю/
revision/label/model; observers не видят незавершённый commit. Plugin/runtime/route/
MIDI generation сохраняются. Timing/head неизменны, MIDI path не вызывает throwing
projection refresh после публикации до установки applied ledger; обычные Store
observers/UI poll обновляют проекции.

Общая история помечает entry `AI proposal (N commands)`; `history_name` читает имя
целевой операции центрального ProjectStore. Подготовленный MIDI Undo/Redo выбирается
для AI entry; прежний human MIDI history path сохраняется. Mixed marker/MIDI proposal
атомарен. Replay/stale/policy/session/Live guards от #163/#164 сохранены; Undo/Redo
не обращаются к provider. Save/Open использует прежнюю schema21/archive1/config8;
preview/chat/credentials/drafts не записываются в project.

NON-RT: JSON/network/Core preflight/diff/compile/transaction/UI. RT-ADJACENT: existing
stopped/joined TimingPlan swaps. RT-CRITICAL callback code: NOT CHANGED. Нет новых
callback allocation/locks/I/O/plugin-state calls. Engine DSP/processing ownership,
audio clips/routing/devices/plugin-editor lifetime: REUSED/UNCHANGED. UI/Core commands/
Application runtime integration/history/tests/docs: AFFECTED. Durable model/schema:
NOT AFFECTED. Music quality и actual ASIO/instrument audition: MANUAL CHECK.

## Marker fill

Только normal marker badge fill меняется с общей clip alpha0.72 на0.5. Caption,
flag/vertical line и selection outline рисуются прежними непрозрачными цветами;
не применяется общий Component alpha. Other lanes/clip fills и Alt-copy ghost0.35
сохраняются. Цвет/RGB persistence не меняется.

## Проверки

ai_midi_draft: strict schema/ranges/overflow, empty harmony, tempo+meter changes,
insert/replace/edit/CC, exact outer/hidden note IDs и retained non-CC, boundary
crossing rejection, empty scoped replace, invalid target/overlap/257events,32768note
project limit, playback note/CC emission, backend restart failure/rollback, Redo
failure/history branch, retained plugin/MIDI generation, stopped guard, Save/Open.
GUI: captured selection/timing/selected MIDI notes, explicit output budget, MIDI
before/after+Apply/Undo/Redo, complete-looking truncated group blocked, previous
chat/Enter/ShiftEnter/bottom bounds and permission checks. MIDI/Browser100/150
images inspected. Реальный provider bass/drums/voicing quality→playback→Undo/Redo→
Save/Open и physical ASIO/DPI ещё требуют пользовательской приёмки.
Итоговые source/build/tests/package/status: [PROJECT_CONTEXT](PROJECT_CONTEXT.md).
Source/build/package local, docs/issues GitHub; без source push/PR/merge/Actions.

## Provider schema compatibility / 0.3l upd1 — 2026-10-14

Actual HTTP400 rejected top-level oneOf in midi_draft chat/completions parameters.
Provider projection now removes only that root combinator and describes mode/target/ID
rules in the function description. Closed properties/types/required fields/limits
remain in wire schema; full Core schema and decoder still enforce mode constraints.
No project/history/engine/RT changes. Serialized request regression and existing
MIDI/GUI checks: affected14/14 PASS4.63s, Release configure/build PASS; packaged AI
smoke exit0. Actual provider paid request is NOT RUN; user retry/acceptance pending.
Empty INSERT remains rejected by the existing contract; use at least one note/CC.
Exact source/package/hash: PROJECT_CONTEXT.md. #165 remains open.

## Empty INSERT and dialogue / 0.3l upd2 — 2026-10-14

Empty insert now uses existing AddMidiClip, exact range and one shared Undo/Redo;
no notes/CC or musical style required. Positive range and valid target/name remain
mandatory; empty EDIT still rejected. GUI+Core preview/apply/history/SaveOpen and
backend rollback PASS. Last8 completed dialogue turns/max16KiB are sent with one
fresh snapshot; previous proposals are historical, never authority for Apply.
Current selection/meter map governs target/time.4/4 at960PPQ=3840ticks/bar.
Provider schema projection fix from upd1 retained. Final affected14/14 PASS4.68s;
source/package/hash in PROJECT_CONTEXT. Real provider follow-ups and music quality
manual check; #165 remains open. Earlier upd1 empty-INSERT limitation is superseded.

## User empty-clip verification — 2026-10-14

Actual0.3l upd2 screenshot: Piano empty INSERT from bar2 for2bars, ticks[3840,11520),
provider validated, Apply accepted and clip visible. User: «undo/redo и сохранения работают».
Confirms empty clip creation/history/saving scenario; no separate explicit reopen
confirmation or musical notes/CC generation/replace/edit/instrument audition report.
#165 remains open for broader musical acceptance; source/package/tests unchanged.
Docs/issues-only, no rebuild or new provider calls. Current details in PROJECT_CONTEXT.

## User acceptance #165 — 2026-10-14

User: «Все звучит», after actual provider generated/applied C major voicing on Piano:
bar2 for2bars, ticks[3840,11520), notes48/52/55/60, duration7680; visible Piano Roll.
Earlier user confirmed empty clip and «undo/redo и сохранения работают». Accept and
close #165 for the demonstrated generation/voicing/common history contract. Other
modes/CC/rollback/SaveOpen retain synthetic test evidence; no invented manual bass/
drum/style matrix, explicit reopen report or ASIO/DPI measurements. Other child
issues not implicitly accepted. Parent26 remains open, native2/11 completed.
Docs/issues-only acceptance; existing source/package/hash/tests/shortcut unchanged.
Next suggested #166 Analysis hooks, not started. Current facts in PROJECT_CONTEXT.
