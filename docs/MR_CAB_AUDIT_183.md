# MR Cab P1 / #183 — аудит, 2026-10-14

**Статус: AUDIT COMPLETE / ожидает пользовательской приёмки.** #184 не начат.
Проверены GitHub #182–188, канон/roadmap/handoff и фактический локальный код.
Baseline: принятая 0.4 Editing; branch `mrs/0.2g-fix1-editor-space-local`,
HEAD `26c9dac`, source `1b69cf1`. До аудита checkout чистый.
JUCE **9.0.3** подтверждён cmake/Juce.cmake, CMakeCache и SDK CMakeLists.
GitHub source main отстаёт; три новых MR_CAB документа получены из main.
Закрытые #23/#128 не переоткрыты; #182–188 остаются открытыми.

## Фактическая коллекция

Пользователь предоставил **G12M-50_Hempback_Open_1x12**, один кабинет.
Vox V412 из первоначального примера не находится в указанном корне: НЕ ПРОВЕРЕН.
Инвентаризация ограничена предоставленным корнем, другие коллекции/архивы не сканировались.
В нём только 48.0 kHz; отсутствие 44.1/88.2/96 здесь не блокирует MVP.
**68 WAV, 3 732 592 bytes, все RIFF PCM24 / 48000 Hz**, ошибок чтения/нечисловых samples нет.
200ms: 9600 frames; 500ms: 24000 frames. 34 точных filename-пары между длинами, непарных нет.
Всего 62 mono + 6 stereo. В корне не найдено license/readme: права перераспространения
не подтверждены; используем внешние пользовательские файлы, ничего не включаем в поставку.

| Группа | 200ms | 500ms | Каналы / фактические варианты |
|---|---:|---:|---|
| Factory mixes | 12 | 12 | 10 mono + 2 stereo на длину |
| Royer R-121 | 6 | 6 | mono: Balanced, Bright, Dark, Dark 2, Fat, Thin |
| Sennheiser MD421 | 6 | 6 | mono: Balanced, Bright, Dark, Dark 2, Fat, Thin |
| Shure SM57 | 7 | 7 | mono: те же шесть + Rear |
| Neumann TLM 107 Room | 3 | 3 | mono RoomL, mono RoomR, один Room Stereo |

Factory на каждой длине: Hi-Gn и Lo-Gn, каждый имеет 121+57, 421+121,
421+57, All, All+Room Mono, All+Room Stereo. Это 12 готовых WAV, не stems.
Hi/Lo — vendor voicing без автоматической DSP gain-коррекции.
У отдельных микрофонов нет Hi/Lo вариантов. Dark 2 — отдельный token, не Dark.
Balanced/Bright/Dark/Fat/Thin — исходные vendor labels; физические координаты
не доказаны. Rear присутствует только SM57; не создавать Rear для остальных.

RoomL/RoomR **моно**, а не stereo halves true-stereo quad Reverb.
Они не имеют распознаваемого Reverb suffix в конце stem (после RoomL идёт Celestion).
Нельзя автоматически преобразовать их в LL/LR/RL/RR. В Simple безопасен явно выбранный
готовый Stereo WAV с diagonal L/R семантикой Cab; RoomL/R сохраняются отдельными variants.
Multi-mic компоненты присутствуют, но phase/latency/mic compatibility для P4 ещё не доказана.

## Численный анализ хвостов (без прослушивания)

Off-thread read-only анализ PCM, без экспорта samples.
Доля энергии после 200ms в 500ms WAV: **0.0008058–0.0762694%** всего отклика.
Отдельные Room: **0.0611456–0.0762694%**; готовые All+Room: **0.0011141–0.0018084%**.
Это ненулевой хвост, но его субъективная полезность/слышимость не установлена.
**0 из 34** коротких WAV идентичны первым 9600 frames длинной версии;
максимальный sample difference **0.00600791** в шкале full-scale.
Следовательно 200/500 нельзя считать просто одинаковым IR с нулевым padding
или автоматически обрезать 500 до 200. Сохраняем оба исходника.
Baseline 48k/200 dry, 48k/500 Room/A-B остаётся; никаких silent length swaps.

Подробная metadata inventory и воспроизводимый одноразовый audit script:
`build/asio-local/validation/cab183/inventory.json` и `inventory.py`, локально.
В них нет PCM; root с личным абсолютным путём хранится только локально в script.

## Аудит и reuse mapping

| Компонент / файл | Подтверждённый контракт | Решение |
|---|---|---|
| core/audio/src/wav.cpp | inspect_wav, WavFile::read; PCM16/24/32, float32; bounded chunked Reverb decode | Общий decoder/header layer; добавить Cab worker adapter с stop_token, без второго decoder |
| core/include/mrs/core.hpp, core/src/serialization.cpp | schema22; CabIr embedded original; Reverb external identity, REVERB 2 (gate) | Legacy unchanged; новые catalog metadata отдельно; не копировать устаревший REVERB 1 из старого handoff |
| core/processing/src/cab_ir.cpp | mrs.cab-ir / CIR1; <=1s, mono/diagonal stereo; 128 direct + 128 FFT tail; 64MiB spectra budget; latency0 | Сохранить ID, embedded restore, controls, leading delay и без normalization |
| core/processing/src/ir_resample.hpp | shared windowed-sinc preparation | Переиспользовать off-thread для того же source IR/rate/length |
| core/audio/src/reverb_library.cpp | 10000-file cap, immutable snapshots, generation/cancel, identity size/stamp/FNV, root-relative refs | Общие scanner/identity helpers; отдельный Cab semantic manifest, не Reverb-specific pair classification |
| core/audio/include/mrs/reverb_library.hpp | per-node ReverbLoader graph lease, shutdown/join, active_revision, last good kernel | Повторить lifecycle/publication pattern; нынешний loader жёстко связан с ReverbKernel, не подключается напрямую к Cab |
| core/processing/src/reverb.cpp, processing.hpp | pending/retired ownership, off-thread collect, 80ms overlap, warm when quiescent | Извлечь/адаптировать kernel handoff для Cab; сохранить Cab DSP semantics, без Reverb tone/gate/predelay |
| apps/studio-desktop/src/application.cpp | poll_reverbs→SetInserts; shutdown before graph replace; stopped acknowledge; param mailbox/history | Интеграция Cab теми же commands/ownership; active asset revision отдельно от новой Loading |
| apps/studio-juce/src/Editors.cpp, ReverbBrowser.* | Legacy synchronous file-picker load; native preset shell; browser/root/search/status | Общий browser presentation/theme; Cab facet view отдельный, без жёстких vendor buttons |

Legacy Cab restore **не имеет async kernel switching**: restore allocates/resamples
только при quiescent graph; process использует заранее выделенные буферы.
Изменение scalar controls сглажено 20ms; смена самого WAV — структурная Pause/Stop операция.
Reverb уже даёт blueprint background prep/80ms crossfade, но не drop-in Cab processor.
Reverb DSP поддерживает mono/stereo runtime config и отдельную convolution routing;
его tone/gate/pre-delay нельзя незаметно внести в Cab.

## Рекомендуемые изменения следующих этапов (НЕ реализованы)

P2/#184: metadata-only Collection/Cabinet/IRVariant + PhysicalAsset schema v1,
stable IDs независимо от absolute root; semantic key без rate/length, physical key с ними.
Optional mic/position/mix/voicing/room/channel facets + raw tokens, source/confidence,
manual overrides, size/mtime scan и worker digest verification.
Premixed vs component и room_included независимы от stereo.
Наличие комбинаций брать из реальных записей, не Cartesian product.
Exact project rate или off-thread resample того же WAV; selected length сохраняется.

P3/#185: **предлагается** новый InsertKind, добавленный в конец enum,
processor ID `mrs.cab-v2` и versioned external CabState; старый `mrs.cab-ir`
и embedded fields остаются. Это выбранное направление, ID пока не зарегистрирован.
Schema bump потребуется при записи нового project insert (текущее значение22
не менять в P1/P2 metadata-only); старые1–22 читаются с прежней семантикой.
Preset native shell использует тот же versioned state. Никакой автоматической legacy migration;
upgrade лишь явной Undoable командой с подтверждённой parity.
App-global roots/manifest/manual overrides — preferences; project — variant reference,
source identity и DSP; runtime cache не сериализовать.
Missing/changed не удаляет durable selection, не подставляет похожий файл;
failed replacement сохраняет старый kernel, fresh missing instance dry + visible status.

RT boundary: scan/decode/resample/FFT/hash/catalog/retire — NON-RT;
publish/control/lifecycle — RT-ADJACENT; processing/handoff/ramps — RT-CRITICAL.
Один producer на node; поколения исключают stale publication.
Quiesce/join перед graph replacement; audio не ждёт worker, без I/O/locks/heap/free.
Фактический switch safety и bounded peak RAM нужно подтвердить реализацией и тестами.

## План проверок

P2 synthetic fixtures only: точные приведённые vendor tokens, case/separators,
Dark vs Dark 2, Rear-only, RoomL/R mono, All+Room Stereo diagonal; unmatched/duplicate
variants, unknown→Unclassified, manual overrides, rates/length grouping,
Unicode/long paths, cancellation/root generations, symlink policy, malformed WAV,
changed/missing/relink, deterministic IDs/reindex. Не импортировать vendor PCM в tests.

P3: direct+partition impulse parity/leading delay/no normalization, mono/stereo
independence, resampling44.1/48/96, rapid switching/play/stopped acknowledge,
last-good on failure, per-node retirement, no RT allocation/free/lock/I/O,
SaveOpen/preset/UndoRedo/missing references, legacy schema1–22 migrations,
Reverb library/true-stereo regression, track/bus/Master/raw recording/PDC/bypass.
Opt-in реальная коллекция: обе длины одного mic и Room; fixed project rates,
128/256/512/1024 blocks, 1/2/4/8 instances, callback max/xruns/CPU/peak RAM/PDC
и listening A/B без normalizing/trimming. Это будущая матрица, результатов пока нет.

## Завершение P1

Архитектура/model/UI/commands/persistence/Undo/RT: **AUDITED, runtime unchanged**.
Studio/Live: общий будущий core; пригодность Live не измерена.
Изменены только docs; audit tooling/logs в ignored build. WAV открывались read-only.
Новая сборка, CTest, GUI, физический ASIO, CPU/RAM benchmark и listening: **NOT RUN** —
P1 audit-only, продуктовых изменений нет. Предыдущие145/145 относятся к0.4,
не к MR Cab2.0. Приёмка P1 пользователем пока не получена.
Следующий шаг: пользовательская приёмка #183; #184 только после отдельного разрешения.
