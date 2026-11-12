# MR Audio-to-MIDI — Python Medium (#368 / 0.10.2)

## Compact conversion UX

Audio-to-MIDI uses Python MuScriptor Medium. Drag one audio clip onto an existing
Instrument Track to choose a model and exactly one instrument category; the
category conditions decoding, never remaps another party. Successful stem jobs
place one clip at the snapped drop position without a result picker. Audio clips
remain unchanged. Ctrl-drag inside an audio clip selects a sample-bounded range
without splitting; the range is used by the following drag or Convert to MIDI.
Right-click → Convert to MIDI processes a full mix without an instrument constraint;
only real returned groups appear as checkboxes. Select all/none and Create N tracks
commit the chosen parts in one existing MIDI project transaction.

Options → Preferences → Audio-to-MIDI owns bounded CPU threads, CPU device,
compatibility dispatch, optional grid/strength and fallback velocity (1–127).
Only CPU is offered by the installed CPU runtime. Quantization defaults Off and
runs in the shared tempo-aware MIDI adapter, not the model. These options are
captured when Start is pressed and cannot change a running job. Manage Models
contains explicit install/local install/verify/remove and runtime management;
there are no hidden downloads or alternative models/backends in conversion dialogs.

Both workflows share worker phases, actual chunks and monotonic elapsed time.
The progress bar remains indeterminate during work because chunk count alone is
not an end-to-end percentage. Close and Cancel stop the task; no partial placement.
Source/project revision and target identity are checked before commit. Existing
Save/Open/Save As, Undo/Redo and provenance storage are reused without migration.
The compact workflow extends the Python-only baseline described below.


**User decision 2026-11-11:** Python/PyTorch MuScriptor Medium is the only
transcription backend in MR Studio. User A/B tests found Python more accurate;
HOT-Step C++ is removed from the product, build and runtime artifacts. Old native
backend preferences migrate to Python. The shared JSONL/MIDI transport and
synthetic process fixtures remain for cancellation, integrity and placement tests;
they contain no model engine and are not distributed as recognition backends.
Additional model integrations are a subsequent, separate scope. Only Medium is supported. Source pin:
`muscriptor/muscriptor@8a1ef88672cc23c26619adaff37ec48d82c0b906`;
weights remain the existing official hash-pinned Medium installation.
**#368 accepted and closed (2026-11-11), PR #382 merged:** this describes the accepted Python-only Medium baseline. The compact conversion workflow above extends that baseline; additional models remain a separate scope.

## Python runtime and controls

Convert to MIDI offers explicit installation/verification/removal of a separate
Python 3.11.9 CPU runtime. Its pinned inference and beat packages, MIT source and
BeatThis final0 checkpoint are independent of Separation. Runtime acquisition
holds a cross-process shared lease; install/remove require an exclusive lease.
Managed inventories, source archive, Python/bootstrap and beat weights are checked.
No hidden inference downloads: the child strips HF credentials and blocks network
connections. Medium weights require the existing explicit HF terms/install flow
and are never packaged with Studio. CC BY-NC 4.0 rights are not commercial clearance.

CPU Auto reserves a logical CPU and caps at 16. Manual 1..32 is affinity-capped;
selection, backend and compatibility mode persist immediately in app preferences.
Python AVX2 dispatch is selected only after CPU/OS capability checks. Compatibility
requests baseline ATen dispatch; this is not proof that every third-party wheel
works on every pre-AVX CPU. The removed native engine is not a selectable fallback.
Diagnostics report actual threads/ISA, load, inference, end-to-end and peak process
memory. Python processes are fresh per run: warm means warm OS cache, not a resident
model. Additional threads do not promise speedup.

The stem workflow passes exactly one category through official Python `instruments`;
the full-mix workflow leaves decoding unrestricted. This is not separation or GM remapping.
Reference prelude forcing, sequential five-second chunks and strict EOS are used;
a missing EOS/token-limit error fails the job rather than publishing partial MIDI.
Original Python can emit paired zero-duration retriggers before its cleanup pass;
only its protocol opts into accepting them. They are omitted from editable notes;
historical protocol fixtures remain strict for compatibility testing; no native model executable ships.

**User decision 2026-11-11:** product conversion emits original
note timing from the model. The converter does not offer BPM/meter detection,
tempo-map following/import, fill gaps or leading-bar trim. Optional post-import
quantization is configured separately in Audio-to-MIDI Preferences and defaults
Off; it runs in the shared MIDI placement adapter. Stale musical options are cleared at
`Application::queue_transcription` for the Python product path. Conversion does not change
the project's tempo/meter map or create muted processed/original reference pairs.
Notes are mapped from their original seconds into the existing project timeline;
this coordinate conversion is not grid quantization or tempo detection. Legacy
lower-level musical import/export helpers remain covered for existing callers.

Opening the converter reads installation metadata rather than hashing the model.
Full model verification remains the explicit Verify action. Python runtime
inventory verification/probe is likewise explicit through Verify runtime, and is
not repeated automatically before each conversion. Installation validation,
lease/lifetime ownership, worker/model/source integrity and cancellation guards
remain. This does not eliminate normal Python startup or model loading cost.

Velocity is **not** recognized by this pinned MuScriptor model. Its official
[token vocabulary](https://github.com/muscriptor/muscriptor/blob/8a1ef88672cc23c26619adaff37ec48d82c0b906/muscriptor/tokenizer/notes.py)
uses velocity only as 0/1 note-off/note-on; public NoteStartEvent has no velocity,
and official notes_to_midi defaults to a shared velocity 100. The Studio converter therefore uses a fallback velocity (100 by default), configured in Audio-to-MIDI Preferences. Audio-amplitude-based velocity estimation would be
a separate feature, not recovered MuScriptor model output.

Source audio samples remain unchanged. Existing project MIDI follows its normal
tick-based tempo-map semantics. A hidden transcription record preserves model/source/crop provenance and created
clip IDs. Projects with these records use snapshot schema35; unrelated projects
retain their existing schema32/33/34 wire format. Older projects still open.
No technical markers are inserted into the musical timeline. Undo/Redo and Save/Open preserve all of it.

#368 acceptance covered MR Studio 0.10.2-fix5 and its recorded CI/Windows user validation (see [#368](https://github.com/vladleng/MR-Studio/issues/368) and [PR #382](https://github.com/vladleng/MR-Studio/pull/382)). No new physical ASIO/DPI measurements or universal model-quality guarantee are claimed by this document. Any additional models, velocity estimation or revised import UI require separate verification.


## Общие границы конвертации и размещения

- Audio-to-MIDI — **внешняя фонова́я операция**, не часть ASIO/ARA/JUCE audio callback. Контроллер отделяет живой ProjectStore/GUI/AudioEngine от worker; cancellation/timeout/crash не создают частично размещённого MIDI.
- Для выбранного **обрезанного аудиоклипа или готового stem** берётся фактически слышимый диапазон PCM, а не скрытый полный исходный WAV. Оригинал не изменяется. Source offset, clip/sample rate и позиция на таймлайне должны оставаться согласованными после resampling/чанков.
- Проект, клип, revision, источник и выбранный PCM проверяются до публикации результата и ещё раз перед размещением; устаревшие результаты отклоняются. Повторная конвертация не стирает ранее размещённые партии.
- Распознанные партии становятся **обычными MIDI tracks/clips** с устойчивыми ID. Размещение через один undoable Command; Save/Open/Undo/Redo сохраняют обычные гарантийные механизмы проекта. Не привязывать автоматически VSTi/внешний MIDI-порт при отсутствии явного запроса.
- Исходный MIDI timing сохраняется без скрытого квантайза. Модель не распознаёт выразительную velocity: используется настраиваемая fallback velocity, по умолчанию 100. Пустой результат валиден как отсутствие нот, а не как повод создать пустой клип.
- Установку, права на веса, verify/remove, сохранность модельных файлов и приоритет фоновой задачи обеспечивают отдельные model/worker компоненты. Не копировать закрытые веса, секреты или пользовательские WAV в артефакты.
- Реальный доступный набор UI действий и режимы выбора партий сверять с актуальным кодом и [#342](https://github.com/vladleng/MR-Studio/issues/342). Drag & drop и выбор партий описаны в разделе Compact conversion UX выше; другие модели не входят в этот контракт.

## Исторический C++ HOT-Step

Удалённый native backend и результаты прежних A/B сравнений документированы в [Issue #346](https://github.com/vladleng/MR-Studio/issues/346), [Issue #368](https://github.com/vladleng/MR-Studio/issues/368) и истории Git этого файла. **Не возвращать HOT-Step в продукт по этой справке.**
