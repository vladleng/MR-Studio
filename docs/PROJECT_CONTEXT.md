# MR Studio: текущий контекст разработки

Обновлено: **2026-10-06**, Asia/Krasnoyarsk. Это основной краткий снимок для нового
чата. Постоянные правила: [AGENTS.md](../AGENTS.md). Подробная история остаётся
в тематических документах; старые верхние записи README/START_HERE не являются
текущим статусом. Проверяй ветку, файлы и более новые инструкции пользователя.

## Где работать

| Назначение | Локальный путь |
|---|---|
| Исходники, Git, CMake, tests, docs | `C:/Users/Vladislav/Documents/GitHub/MR-Studio` |
| Папка чатов, Builds, Saves, навыки | `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio` |
| Навыки проекта | Рабочая папка чатов: `.codex/skills/<имя>/SKILL.md` |
| Активная ветка | `mrs/0.1q-fix3-processing-local` |
| Существующая сборочная папка | Репозиторий: `build/asio-local` |
| GitHub для issues/docs | `vladleng/MR-Studio` |

GitHub source может отставать от локальной разработки. Не заменяй локальный код
клоном/main/pull ради синхронизации документации. Машинные пути выше — текущая
конфигурация; на другой машине сначала найди существующий checkout.

## Продукт и активное направление

Одна DAW с режимами Arrange/Edit/Mix и будущим встроенным Live Mode, единым
Project Model/Transport/Core. Активная разработка — Windows JUCE + ASIO.
Следующий фокус разработки — MIDI по решению пользователя 2026-10-06. P4
пользователь продолжает тестировать в процессе; его оставшиеся проверки не
блокируют начало MIDI. Собственные эффекты/инструменты/Amp-Preamp отложены. Пользователь стремится к уровню
эффективности Fender Studio Pro. Универсальность важнее настройки под один проект.

В пользовательском примере TH-U и Xvox находятся на разных каналах, ONE — на
Master. Не превращай их в одну последовательную цепь при оценке параллелизма.

## Точное состояние этапов

| Область | Состояние |
|---|---|
| P1 / [#51](https://github.com/vladleng/MR-Studio/issues/51), 0.1r | Принят 2026-10-05, issue закрыт. При 128 frames регулярные щелчки исчезли, редкие остаются |
| P2 / [#52](https://github.com/vladleng/MR-Studio/issues/52), 0.1s | Принят 2026-10-06, issue закрыт. Первый native whole-graph anticipation slice |
| P3 / [#53](https://github.com/vladleng/MR-Studio/issues/53), 0.1t | Принят 2026-10-06: «P3 вроде стабилен. Переходим дальше». Issue закрыт для поставленного mixed slice; длительная matrix остаётся P4/#16 |
| 0.1t fix1, редакторы плагинов | Принят пользователем 2026-10-06: «теперь с плагинами все как надо». Bypass, порядок окон и Pin подтверждены |
| P4 / [#54](https://github.com/vladleng/MR-Studio/issues/54) | 0.1u software slice принят пользователем 2026-10-06. #54 открыт для оставшихся проверок; пользователь тестирует P4 параллельно MIDI |
| Parent Stage 3 / #23, длительная matrix #16 | Не закрыты этой приёмкой |
| 3e2 / 3e3 | Безопасные переходы/dynamic latency и recovery/isolation остаются отдельными задачами |

В пользовательской переписке «0.1c показывает хорошие результаты» было принято
как подтверждение тогдашней 0.1s/P2; не возвращай roadmap к старой 0.1c.

## Последняя поставка

- Пакет: `Builds/MR-Studio-0.1u-P4-profiling-JUCE-ASIO-Windows-local` в папке чатов.
- Запуск: `Moon River Studio JUCE.exe`; `mrs_vst3_scan.exe` рядом.
- Source commit: `ef11071bd221e30f822c2fd09e4689bfbf0b98ad` (только локально).
- SHA256 EXE: `3B75C8CF1886CC983544254171D6B9499D515E7CA0500F193BE50CF874DBCBA9`.
- Cached configure/full Release прошли. **110/110 CTest, 57.29 s**;
  финальные profiling/J3 проверки — 2/2, 1.71 s. Packaged hidden J3 — exit 0.
- 112 канал-конфигураций OFF + 112 ON: совпадают checksum; worker timeout 0,
  Late 14/13. Playback/mixed — 210 OFF + 210 ON, 67 200 точных PCM-блоков;
  producer/disk/worker misses 0, Late 13/13. Все выбросы сохранены в CSV.
- По минуте streamed mixed raw recording и playback при 48 kHz / Device 128,
  Workers 4, profiling ON: по 22 540 точных блоков, Late/U/D/timeouts 0.
  Запись: 2 885 120 mono raw frames, каждый равен исходному входу, fault 0.
  Это paced synthetic callbacks; физический ASIO не открывался.
- Панель/CSV проверены, software snapshots 100%/150% просмотрены. Пакет имеет
  SHA256 manifest; EXE совпадает с Release. Старые 0.1t/fix1 и другие версии сохранены.
- [Методика](ENGINE_PROFILING.md), [замеры и ограничения](ENGINE_PROFILING_BENCHMARK.md).
  Результаты не доказывают Fender parity, общий CPU speedup или отсутствие щелчков.

## Существенные границы реализации

P1 обрабатывает независимые каналы параллельно и сводит результаты детерминированно;
зависимые buses/Master ждут входы. P2 заранее готовит подходящий native playback.
P3 разделяет независимый native playback и live/shared DSP через ограниченную
PCM-очередь; фактические DSP/PDC/source-cursor имеют по одному владельцу.

Process Buffer по умолчанию Off. Для mixed нужны Workers >= 2 и подходящие
prerecorded native-каналы; Process 1024/4096 не меньше Device Buffer. VST3,
live/shared buses и Master обрабатываются напрямую. VST3 anticipation не реализован;
TH-U/Xvox/ONE-only граф не получает обещания нового ускорения сверх P1.
Mon — оценка driver I/O + fixed PDC, не измеренная акустическая задержка.
Недостающий playback PCM не останавливает live/capture/device time; восстановление
может обрезать tails. Seamless history/crossfade и native crash/hang isolation не готовы.

0.1t fix1: редактор — owned top-level окно Windows над MR Studio; Pin сохраняет
его при открытии другого плагина, закрытие одного не закрывает остальные.
Host bypass по-прежнему после Pause/Stop: PDC-граф пересобирается, native child
редактора переподключается в том же JUCE-окне. Pin — состояние открытого окна,
не проекта. Structural/device/project changes и Undo/Redo могут закрывать редакторы.

0.1u P4: Transport -> Engine profiling... включает timing каналов/insert/Master,
worker job balance и отдельные device/ahead banks, экспорт CSV из UI. По умолчанию
OFF; закрытие отключает сбор. Timing добавляет overhead. Dependency cost estimate
исключает source/reduction/queue/scheduler wait и не равен полной critical-path
wall time. Сумма пересекающихся jobs не равна process CPU. Engine counters сбрасываются
при stopped prepare, processor counters — при пересоздании PreparedGraph. Снимки
приблизительны, project schema/Undo не менялись. Полная семантика — в ENGINE_PROFILING.

## Что читать по задаче

- Общий смысл продукта: [PROJECT_VISION](PROJECT_VISION.md), [ARCHITECTURE](ARCHITECTURE.md),
  [DEVELOPMENT_TRACKS](DEVELOPMENT_TRACKS.md). Их исторические статусы не заменяют снимок выше.
- Движок: [план](ENGINE_PERFORMANCE_PLAN.md), [reliability](PROCESSING_RELIABILITY.md),
  [P1](ENGINE_PARALLEL_PROCESSING.md), [P2](ENGINE_ANTICIPATIVE_PROCESSING.md),
  [P3](ENGINE_MIXED_PROCESSING.md), [P4](ENGINE_PROFILING.md).
- Плагины: [редакторы/fix1](ENGINE_PLUGIN_EDITORS.md), [VST3](VST3_HOSTING.md),
  [пресеты](PLUGIN_PRESETS.md).
- UI: `mr-studio-ui-design-system/SKILL.md`, [JUCE J3](JUCE_J3.md).
- Проверки: `mr-studio-build-test/SKILL.md` и [локальные команды](LOCAL_DEVELOPMENT.md).

Карта исходников: `apps/studio-juce/src/` — актуальный UI; `apps/studio-desktop/`
— Application/services и исторический Win32 UI; `core/audio/` — engine/worker/ahead/mixed;
`core/processing/` — processor graph, VST3, native DSP, PDC; `core/src/` — model,
commands, serialization; `tests/` — регрессии. JUCE GUI smoke находится также
в `apps/studio-juce/src/Smoke.cpp`.

## Пользовательский ASIO CSV: Test 01

2026-10-06: получен `C:/Users/Vladislav/Documents/MR Studio/Projects/Test 01.csv`.
Только анализ, не пользовательская приёмка P4 и не подтверждение записи.
Komplete Audio ASIO Driver, 48 kHz, B 256, Workers 4, Process 256, profiling ON.
Callback load p50/p95/p99 17/24/28%, max 1.8651 ms при deadline 5.3333 ms;
Late/XR/disk/worker timeout 0. Но Ahead underruns **17 212**, invalidations 5,
buffered 0, producer max 0.4337 ms. Проверить starvation/учёт очереди отдельно:
metrics накоплены за engine session, timing cells — только пока profiling ON;
по CSV нельзя вычислить текущую частоту underrun или акустические последствия.
7 709 device timing calls соответствуют 41.1147 s обработанных frames; это не
известная длительность всей сессии/не гарантия непрерывной игры или записи.

Средние device channel jobs: Voc 0.8046 ms, GTR 0.5794 ms, PB 0.4983 ms,
Keys 0.0067 ms; времена пересекаются, не складывать в callback load.
Xvox Pro 0.6044 ms, TH-U 0.3725 ms, Xrack Pro 0.4860 ms;
две Ambiente 0.1867/0.1917 ms. ONE имеет 0 measured calls; пользователь
после Test 01 подтвердил, что ONE был в bypass.
Следующий полезный контроль — свежий session CSV с Process Off / 1024 при
неизменной остальной нагрузке; текущие данные не являются 128-frame проверкой.
Исходный CSV и пользовательский проект не изменялись. Code/build/package без
изменений; #54 не закрывать по этому снимку.

## Пользовательский ASIO CSV: Test 02

2026-10-06: получен `C:/Users/Vladislav/Documents/MR Studio/Projects/Test 02.csv`.
Пользователь называет режим «выключенным DSP»; CSV показывает Process Buffer Off,
Workers 4 и активный processor DSP, включая ONE. Komplete Audio ASIO Driver,
48 kHz / Device 256. Callback load p50/p95/p99 24/34/37%, max 2.3907 ms
при deadline 5.3333 ms. Late/XR/disk/worker timeout 0. Ahead counters 0 ожидаемы
при отключённом producer и не подтверждают исправление Test 01 queue starvation.

ONE: 385 вызовов, mean 0.4243 ms, max 0.8427 ms. Prepared graph PDC 2465 samples
= 51.3542 ms; это fixed compensation, отдельно от driver I/O и Process Buffer,
не акустическая latency и не пропущенный deadline. Device Voc/GTR/PB mean
0.6197/0.5394/0.4274 ms. Timing frames 98 560 = 2.0533 s обработки при profiling;
полная длительность session из CSV неизвестна. Это короткий profiling sample,
не длительная ASIO приёмка. Прямое сравнение с Test 01 не изолирует Process:
одновременно изменились bypass ONE и Process, окна сбора тоже различаются.

Для чистого сравнения держать ONE и остальные plugin states одинаковыми,
сравнить Process Off / 1024 свежими counters, дать более длинный profiling window.
P4 остаётся открыт. Исходные CSV/проект/код/билд/пакет не изменены; только анализ
и локальная запись контекста.

## Пользовательский ASIO CSV: Process 1024

2026-10-06: `C:/Users/Vladislav/Documents/MR Studio/Projects/Test dsp 1024.csv`.
Komplete Audio ASIO Driver, 48 kHz / Device 256 / Workers 4 / Process 1024,
profiling ON. ONE активен: 9 410 measured calls. PDC прежний: 2465 samples.
Callback p50/p95/p99 25/33/37%, max 2.7451 ms (51.47% deadline 5.3333 ms).
Late/XR/disk/worker timeouts и Ahead underruns/invalidations **0**;
queued 1024 frames в момент snapshot, producer max 0.4354 ms.
Timing содержит 2 408 960 frames = 50.1867 s обработки при profiling; это не
доказательство wall duration всей сессии, записи или непрерывного транспорта.

С Test 02 (Process Off, ONE active): p50 24->25%, p95 34->33%, p99 37->37%,
max 2.3907->2.7451 ms. Ускорение callback не установлено; окна сбора различаются
(~2.05 vs ~50.19 processed s), max нельзя трактовать как доказанное ухудшение.
ONE mean 0.4676 ms, max 1.2369 ms. Основные VST3 inserts остаются device-owned;
producer timings каналов малы, поэтому выраженного выигрыша anticipation тут
не видно. Test 01 Process 256 имел 17 212 накопленных underruns и ONE bypass;
при 1024 текущие counters чистые, но это не доказательство найденной/исправленной
причины прежних misses. Снимок поддерживает работоспособность очереди 1024 на
этой нагрузке. #54 остаётся открыт без явной приёмки и более широкой matrix.
CSV/проект/код/билд/пакет не изменены; результаты сохранены только локально.

## Пользовательский ASIO CSV: Device 128 / Process 1024

2026-10-06: `C:/Users/Vladislav/Documents/MR Studio/Projects/Test 128 dsp 1024.csv`.
Komplete Audio ASIO Driver, 48 kHz / Device 128 / Workers 4 / Process 1024,
profiling ON. ONE 0 measured calls, PDC 0; соответствует выбранному live bypass.
Callback p50/p95/p99 16/27/32%, max 1.4593 ms при deadline 2.6667 ms (54.72%).
Late/XR/disk/worker timeouts 0. Ahead buffered 1024, producer max 0.6036 ms;
**Ahead underruns 43 и invalidations 43**. Совпадение totals не устанавливает
причину или связь каждого события. В mixed code rebase вызывают как command/mix
revision, так и head.sequence > producer sequence после отставания; поэтому
нельзя автоматически объявить конкретную причину. Пользователь ответил «нет»
на вопрос о щелчках/потере playback и Pause/Seek/изменениях параметров: слышимых
проблем и указанных действий не было. Не списывать 43 события на ручной транспорт.
Для диагностики проверить producer scheduling/queue starvation и корректность
счётчиков, в том числе вне активного playback; причины CSV сам по себе не даёт.

Timing содержит 80 135 device calls / 10 257 280 frames = 213.6933 s обработки
(~3 min 34 s), не полную wall duration сессии или доказательство 5-min continuous
playing/записи. В producer 80 167 calls: может содержать заранее queued frames.
Host plugin PDC теперь 0; driver/converter latency остаётся, acoustic не измерена.
Субъективный live/playback контроль без слышимых проблем подтверждён пользователем.
Пользователь затем уточнил: в самом мультитреке была небольшая пауза.
Вероятно, речь о тишине/разрыве в материале; это не подтверждение нажатия Pause.
Не приписывать пропуски этому месту: CSV содержит только totals, без времени
событий. Тишина может скрыть слышимые последствия недостающего playback PCM,
но сама по себе при непрерывном транспорте не доказывает нормальность underrun.
43 очередных события остаются диагностическим пунктом P4; record acceptance
и полный scope #54/#16 этим CSV/ответом не подтверждены.
User CSV/проект/код/билд/пакет не изменены; локально сохранён только анализ.

## Приёмка 0.1u и переход к MIDI — 2026-10-06

Пользователь согласился принять поставленный software slice 0.1u, оставить
#54 открытым для оставшихся проверок и перейти к MIDI в следующем чате:
«Да, давай, хочу в следующем чате уже заняться миди, а P4 буду в процессе тестировать».
Это приёмка сборки/профилирования и разрешение сменить фокус разработки;
не подтверждение полной sustained recording/transport/reconnect matrix.
43 Ahead underrun/invalidation остаются диагностическим пунктом, причины не
установлены. Live 128 / Workers 4 / Process 1024 / ONE bypass без слышимых
проблем принят в сообщённом пользователем объёме; сведения о паузе в материале
сохранены выше. Более широкие #16, 3e2/3e3 не закрыты. Выпущенный пакет и его
manifest не переписываются после новой пользовательской приёмки.

## Продолжение и обновление этого файла

2026-10-06: по запросу пользователя Stage 4 / MIDI / #24 разбит на **4a–4h**:
0.2a MIDI-вход/VST3-инструменты; 0.2b клипы/playback; 0.2c запись;
0.2d piano roll; 0.2e quantize/transpose; 0.2f CC/PC;
0.2g внешний MIDI; **4h / 0.2h — метроном и precount для audio/MIDI записи**.
Итоговая проверка Stage 4 — после 4h. По последнему уточнению пользователя
Arranger Track, Chord Track и остальная музыкальная структура остаются в Stage 5 / #25.
Доработки — updN, исправления — fixN;
независимые счётчики сбрасываются при переходе к новой букве.
План и критерии: [MRS_STAGE_4_PLAN](MRS_STAGE_4_PLAN.md), схема — VERSIONING.
GitHub: parent #24 обновлён; созданы issues #63–#70 для 4a–4h соответственно.
Stage 5 / #25 сохраняет Chord/Arranger и музыкальную структуру; его scope не менялся.
План, roadmap, VERSIONING и этот контекст опубликованы отдельным docs-only
коммитом `dcd2dad1b5dbd5dd95199d8071af0daf18955e69` в GitHub main с `[skip ci]`;
remote SHA проверен. Локальная source-ветка не отправлялась, код/пакет не менялись.
Это планирование, не реализация/приёмка. Следующий конкретный подэтап — 4a / 0.2a
после запроса на начало реализации. Source/build/package без изменений;
ветка `mrs/0.1q-fix3-processing-local`, source `ef11071bd221e30f822c2fd09e4689bfbf0b98ad`.
Проверка документации: diff и согласованность версий; сборка не требуется.

Следующий чат: **MIDI**. Сначала прочитай AGENTS.md, этот контекст и существующие
[MIDI processor graph](MIDI_PROCESSOR_GRAPH.md), [musical timeline](MUSICAL_TIMELINE.md),
затем проверь текущую MIDI реализацию и выбери конкретный объём с пользователем.
Не считать словом «MIDI» уже согласованную реализацию всех инструментов, piano
roll, hardware routing или иной конкретной функции. Новая MIDI задача не начата
в этом чате: source edits отсутствуют, последнее изменение — docs/приёмка.
Сохраняй принятые аудио/редакторные границы, применяй соответствующие навыки.

P4 теперь остаётся проверкой пользователя параллельно разработке MIDI. Не
навязывай завершение #54 как условие начала MIDI и не продолжай engine fixes
самостоятельно вместо MIDI. При новом CSV/аудио-дефекте анализируй сообщение
и фиксируй результаты; по запросу исправления возвращайся к нужной части P4.
Осталось: выяснить 43 queue events, подтвердить запись и sustained transport/
reconnect/state checks. #54/#16 не закрывать автоматически; фиксировать точный
объём будущей приёмки и реальные durations, без vendor/project optimizations.

После содержательной работы обновляй этот снимок: дату, задачу, подтверждённый
результат, незавершённое, ветку/source commit, пакет/проверки, приёмку и следующий шаг.
При передаче незавершённого кода укажи файлы/ошибку/последнюю команду и безопасное
продолжение. Не копируй весь журнал чата и не запускай старые patch-скрипты.
