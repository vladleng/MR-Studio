# MR Reverb: локальная оптимизация 0.3c upd2

2026-10-10. Продолжение #128/#131 по прямому запросу пользователя.
Канонический MR_REVERB.md не копируется; код, тесты и этот документ локальны.
#132, #86 и #25 автоматически не начинаются.

## Сценарий и граница измерения

Пользователь подтвердил функциональность 0.3c upd1, но сообщил о высокой нагрузке:
ASIO 48 kHz / Device Buffer 256 / Workers 8 / Process Buffer 1024; одна пустая
аудиодорожка, транспорт остановлен, один MR Reverb. Quad True Stereo IR
«1 Halls 01 Large Hall, 48K», 189378 frames (3.945375 s), параметры 0 ms / 0 dB /
25%. С Reverb на скриншотах CPU 16.9–21.6%, без него пользователь сообщил 0.4%.
Footer показывает callback time / buffer deadline, а не Windows CPU.

Offline профиль воспроизводит IR, частоту, два канала и параметры. Он измеряет
только ReverbProcessor::process: не ASIO, engine scheduling, GUI или общую CPU.
Один DSP имеет одного владельца; Workers 8 не распараллеливает один экземпляр.
Проценты улучшения DSP нельзя напрямую вычитать из пользовательского ASIO meter.
Приватный IR только прочитан, не изменён, не скопирован в пакет или репозиторий.

## Изменения и realtime граница

- Runtime свёртки считает точную конечную поддержку последнего ненулевого входа:
  полное число frames подготовленного IR. Только после последнего возможного
  wet sample и при дальнейшем нулевом входе останавливаются head/FFT/MAC.
  Порогового noise gate/обрезания хвоста нет; используется прежняя input sanitation.
- При переходе в покой один раз сбрасываются короткие head histories, позиции,
  FDL counters и generation tags. Большие FDL/output buffers не очищаются.
  Первый новый сигнал начинает чистую локальную фазу без добавленной задержки.
  Непрерывный сигнал через односэмпловый IR не вызывает reset на каждом сэмпле.
- Pre-delay, Tone, smoothing, MIDI/events и 80 ms crossfade остаются в processor
  и продолжают исполняться при спящей свёртке. Delay/Tone хвост доигрывает;
  публикация/retirement завершается и на тишине. Старые kernels освобождает NON-RT.
- FFT butterflies вычисляют два комплексных значения одним SSE2 vector;
  scalar remainder/fallback сохранён, AVX или отдельная CPU configuration не нужны.
- Для реальных входов/IR spectral MAC считает только P+1 независимых bins из 2P.
  Остальные восстанавливаются conjugation. На больших tiers mirroring входит
  в bounded incremental job, без большого одиночного callback burst. Quota учитывает
  новую стоимость MAC/mirror. Полные spectra/plans остаются в прежних буферах.

RT-CRITICAL: Runtime/process, butterflies, MAC и incremental mirror; только DSP owner.
RT-ADJACENT: прежний atomic pending/retired handoff, ownership не меняется.
NON-RT: чтение WAV, resampling, планы/kernels, capture diagnostics и освобождение.
Новых locks, I/O, heap work, shared ownership churn и worker waits в callback нет.

Неравномерные partitions 128/1024/8192, shared input FFT для LL/LR и RL/RR,
нулевая algorithmic latency, source/prepared memory budgets существовали до upd2.
Расположение partitions и полный IR не меняются. Incremental tier требует
как минимум 2P offset: P для input block, P для выполнения job.

UI, project schema v16/Reverb v1, commands/Undo, presets, external identity,
missing/changed IR и host scheduling policy изменений не требуют и не изменены.
Shared processing backend получает исправление; Live acceptance не заявляется.

## Воспроизводимость и проверки

`mrs_reverb_tests --profile-ir <quad-WAV>` — opt-in offline профиль; не CTest
с приватными данными. 1024 callbacks на сценарий, signal filling вне таймера,
предварительный полный прогрев IR для steady signal, полный drain перед тишиной.
Buffers 128/256/512, callback allocation/free probe, полный quad impulse oracle.
Исходный profiler сохранён до изменения DSP; парные baseline/new прогоны
последовательны, без одновременной сборки, чтобы уменьшить влияние нагрузки.
Raw logs, baseline/new EXE и JSON summary остаются в
`build/asio-local/deliveries/0.3c-upd2-reverb-optimization`, не в пользовательском пакете.

Автоматические регрессии: mono/stereo/True Stereo, mono host, arbitrary silent
onsets/gaps и callback 127/251/256/257, tiny 1e-18 input, последний IR tap,
wet delay 200 ms/Tone +6 dB после idle, silent IR handoff, rapid concurrent swaps,
20 s True Stereo 48/96 kHz с обеими input paths и максимальными FDL histories.
Существуют также 20 s mono 44.1/48/96 kHz, dense direct convolution oracle,
controls/events, bypass, track/bus/Master, Stop/Seek/loop и offline WAV tail.
Полный пользовательский quad oracle: max error 8.9407e-8; RT alloc/free 0/0.

Actual ASIO meter/dropouts/listening нового пакета — пользовательская проверка.
Offline timing не подменяет аппаратную приёмку или весь #132.

## Финальные парные измерения

Windows x64 Release, MSVC/VS 18, AMD Ryzen 7 PRO 4750U (8 cores / 16 threads).
Три парных прогона окончательного DSP, медиана среднего времени 1024 callbacks:

| Buffer | Steady signal: до → после, µs | Снижение | Drained silence: до → после, µs | Снижение |
|---|---|---|---|---|
| 128 | 190.249 → 139.553 | 26.65% | 191.749 → 3.74561 | 98.05% |
| 256 | 378.544 → 277.961 | 26.57% | 382.989 → 7.56855 | 98.02% |
| 512 | 787.907 → 559.519 | 28.99% | 779.330 → 15.0084 | 98.07% |

256 signal p95 range: 522.0–532.9 → 439.5–453.7 µs; worst observed across
three runs: 682.9 → 645.6 µs. 256 drained silence p95: 528.1–535.0 →
7.6–8.4 µs. Наблюдаемый max включает OS/preemption, не является worst-case bound.
Свежая тишина 256: 350.512 → 7.53467 µs. Полный private quad oracle по-прежнему
8.9407e-8, callback alloc/free 0/0. Raw results: final-baseline-1..3.log,
final-optimized-1..3.log, final-paired-summary.json. Ранние замеры давали около
31% signal improvement; окончательная запись использует отдельные final logs.

## Локальная поставка

Source `00bad07210df94328eb92d6e1c9565bf6c052bfe`, ветка `mrs/0.2g-fix1-editor-space-local`.
Final Release build PASS, CTest 125/125 PASS (114.65 s), packaged editor smoke PASS.
DSP suite 6.61 s, library 12.54 s, desktop Reverb 0.21 s, editor 1.11 s.
Перед финальным уточнением one-tap reset полный CTest также 125/125 PASS (115.62 s).
Пакет `MR-Studio-0.3c-upd2-MR-Reverb-Optimized-JUCE-ASIO-Windows-local`:
13 files, runtime/helpers/licenses/README/manifest; без assets, IR, tests, logs/PDB.
GUI SHA256 `6164D000F0725A9AA05C779ACAEBD87CE12D5175874D2777618226047F12C074`.
Matching GUI/crash reporter symbols и packaged smoke рядом с profile logs.

RT SAFE WITH MANUAL CHECK / READY WITH MANUAL CHECK: software numerical,
allocation, handoff, transport/lifetime and GUI regressions PASS; intended ASIO
CPU/dropouts/listening нового пакета NOT RUN. Issue #131 сохраняет функциональную
приёмку upd1 и остаётся открыт до оценки нагрузки. Source/docs не публиковались.
