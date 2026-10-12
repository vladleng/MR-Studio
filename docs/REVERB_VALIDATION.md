# MR Reverb — QA #132, 2026-10-10

Принятая сборка **0.3c upd4** с новым Input gate и ходом Mix:
[реализация, совместимость и проверка](REVERB_GATE_MIX.md).
Release/CTest126/126/editor packaged smoke PASS. **2026-10-11** пользователь
подтвердил «Это сработало, закрывай»; #132 и общий #128 закрыты completed.
Точный новый CPU% и отдельные Mix/mono/mixed-DPI результаты не сообщены;
полная actual ASIO stress матрица, mono audition и physical mixed-DPI остаются
NOT RUN. Пакет и manifest сохранены. Далее — исторический отчёт QA на upd2.

Работа локальная; accepted runtime **0.3c upd2**, DSP source `00bad07`.
QA source **`d674fbdfc87cd55a31bb00466dcfc28fbc6e7114`** на текущей
ветке `mrs/0.2g-fix1-editor-space-local`.
#130/#131 закрыты по прямой приёмке пользователя. #132 остаётся открытым до
финальной оценки результатов; #128 требует отдельной итоговой приёмки.
Код приложения и существующий пакет не меняются. Документ не публикуется.

## Слуховая оценка и границы

Пользователь: «Он и без проверки звучит убедительно». По этому решению внешний
A/B не является блокирующим условием этой приёмки. Внешний reference processor
не подтверждён как доступный, A/B **NOT RUN**. Не заявляется эквивалентность
аппаратному Bricasti или конкретному коммерческому процессору.

Пользователь принял функциональность и CPU upd2: примерно 1.5% в простое,
12–14% при работе (48 kHz / ASIO 256 / Workers 8 / Process Buffer 1024).
Это пользовательский замер одного экземпляра, не полная ASIO матрица.
Отдельное прослушивание mono fold-down и физический mixed-DPI **NOT RUN**.

## Воспроизводимые проверки

Новый `mrs_reverb_qa` / CTest `processing_reverb_reference`:

- double direct-convolution oracle для logarithmic swept-sine с независимыми
  L/R входами: mono/stereo/True Stereo, 44.1/48/96 kHz, блоки 1/63/128/257/511;
- четыре независимых FIR пути, численный mono fold-down;
- floating-point headroom выше 0 dBFS без скрытого hard clip;
- NaN/Inf/subnormal sanitation и повторная подготовка одного 48 kHz IR
  на 44.1/96/48 kHz, latency 0, длительность и сброс истории;
- параллельный реальный delay 7 frames + Reverb: PDC и последний tap полного
  20001-frame IR без двойной задержки/обрезки;
- host new/delete probe на измеряемых callback путях.

`desktop_reverb` дополнен Application round-trip одного внешнего 48 kHz IR
в проектах 44.1/48/96 kHz: load/monitor/save/reopen/restore; несовпадающая
частота устройства отклоняется с сохранением текущего engine rate.

Existing regression suites проверяют полную длину до 20 s, tiny input без gate,
silence/resume, wet Pre-delay/Tone tail, смену IR/parameters и transport,
плохие/слишком большие WAV, missing/hash mismatch/root relocation,
scan cancellation, track/bus/Master, Undo/presets и editor lifetime.
Export проверяется на существующем engine-render + WAV Recorder: полный
заданный диапазон с хвостом. Пользовательский UI Bounce/Export этим не заявляется.
Cab IR regressions сохраняются.

## CPU/RAM методика

`mrs_reverb_qa --matrix <mrs_vst3_fixture.vst3> <matrix.csv>` — **offline**,
не ASIO. Ryzen 7 PRO 4750U, 8 physical/16 logical, 16 GB RAM, Windows x64 Release.
108 сочетаний: 44.1/48/96 kHz × True Stereo IR 8/20 s × 2/8/16 экземпляров ×
1/8 workers × blocks 128/256/512. Каждый новый Reverb предварительно обрабатывает
полную длину IR + 2048 frames, чтобы измерять заполненную FDL, затем engine
64 warm callbacks и 256 измеренных callbacks на размер. Включены transport Play
и синтетический hosted VST3 instrument; live path сохраняет прямое исполнение.
Adaptive parallel выключен для воспроизводимости, engine watchdog не увеличен.
Дисковый I/O CSV/console и создание графа идут вне callback.

CSV: mean/p95/p99/max microseconds, deadline, late count, worker timeouts/fault,
суммарный prepared_bytes только нужного количества Reverb, working set/private
bytes всего процесса, PDC и host allocation/free counts. RAM включает retained
engine/assets/VSTi; prepared_bytes не равно полному процессному RAM. DLL плагина
имеет собственный allocator: host probe не доказывает отсутствие allocations
в сторонних DLL. Замер времени callback включает probe overhead.

Превышение deadline — наблюдение предела, а не успешная realtime приёмка.
Не переносить процент offline deadline utilization на ASIO meter или Windows CPU.
Actual ASIO stress с 2/8/16 экземплярами и коммерческим VSTi **NOT RUN**.

Логи: `build/asio-local/validation/reverb-132/`. Предыдущие package GUI smoke,
скриншоты и matching symbols: `build/asio-local/deliveries/0.3c-upd2-reverb-optimization/`.
Старый принятый пакет с SHA256 manifest сохраняется без изменений.

## Результаты

Reference PASS: максимальная swept-sine ошибка **4.28445e−8**, mono fold-down
**2.78625e−8**. Headroom/sanitation/rate reprepare и реальный PDC/full-tail PASS;
host allocation/free **0/0**.

Матрица завершена: **108 строк / 27648 callbacks**, fault/timeouts **0/0**,
host allocations/deallocations **0/0**. В **69 режимах** есть хотя бы одно
превышение deadline; всего **8644** late callbacks. Это измеренные пределы
нагрузки, не общий realtime PASS. Один последовательный прогон, порядок фиксирован;
редкие OS scheduling spikes и тепловое состояние CPU могут влиять на максимум.

48 kHz / 256 frames / Workers 8: deadline **5.333 ms**:

| IR | Instances | Mean ms | p95 ms | Max ms | Late / 256 | Reverb prepared MiB | Process private MiB |
|---|---:|---:|---:|---:|---:|---:|---:|
| 8 s | 2 | 0.926 | 1.644 | 4.265 | 0 | 78.83 | 97.50 |
| 8 s | 8 | 1.739 | 2.762 | 7.042 | 1 | 315.32 | 336.18 |
| 8 s | 16 | 2.570 | 3.503 | 4.082 | 0 | 630.64 | 655.77 |
| 20 s | 2 | 0.948 | 1.770 | 2.198 | 0 | 185.33 | 213.21 |
| 20 s | 8 | 1.640 | 2.222 | 2.786 | 0 | 741.32 | 772.09 |
| 20 s | 16 | 3.775 | 6.047 | 9.871 | 27 | 1482.64 | 1518.00 |

Пиковый process private **3094298624 bytes (2950.95 MiB)**,
Reverb prepared **3031059840 bytes (2890.64 MiB)**: 16 × 20 s × 96 kHz.
96 kHz / 20 s / 16 / Workers 8 / 256: mean **4.040 ms** при deadline
**2.667 ms**, late **207/256** — режим перегружен. Reverb latency 0;
matrix graph PDC **7 frames** принадлежит synthetic VST3 fixture.
Длительность 20 s поддерживается полностью, но не обещает realtime обработку
любого количества экземпляров. Shared FX return предпочтителен, если требуется
один общий room/hall для нескольких каналов; индивидуальные разные IR требуют
собственных ресурсов. Actual ASIO stress остаётся manual до релизного заявления
о гарантированных рабочих лимитах; не меняет принятую пользователем single-instance
нагрузку и слуховую оценку.

Полная Release сборка **PASS**, CTest **126/126 PASS (137.83 s)**, включая
DSP (6.72 s), reference (0.35 s), Application Reverb (0.30 s), GUI editor
(1.18 s), библиотеку и все Cab IR regressions. Private user library
**134/134 quad и соответствующие L/R пары PASS**: каналы quad/pair идентичны,
DSP max error **1.19209e−7**, alloc/free **0/0**, полный longest tail **320001
frames / 6.6667 s**. Библиотека только прочитана, не упакована.

GUI SHA256 после полной сборки совпадает с принятым runtime:
`6164D000F0725A9AA05C779ACAEBD87CE12D5175874D2777618226047F12C074`.
Новый пользовательский пакет не требуется; предыдущие matching screenshots/PDB
сохраняют применимость к идентичному EXE. Source/docs push/merge/Actions не выполняются.

## Чек-лист приёмки

- [x] Функциональность #130/#131 и single-instance ASIO CPU приняты пользователем.
- [x] Звучание убедительно по пользовательской оценке; внешний A/B снят с gates.
- [x] Offline reference/PDC и полная CPU/RAM матрица получены без runtime fault.
- [x] Финальная полная CTest и private quad/pair library проверка.
- [ ] Actual ASIO multi-instance stress на рабочем проекте с VSTi, без переноса
  offline late counts на физическое устройство. Нужен перед обещанием гарантированных
  multi-instance лимитов/Live suitability; пока таких обещаний нет.
- [ ] Отдельное слуховое mono fold-down, если нужна именно эта часть QA;
  численная проверка уже PASS. Physical mixed-DPI остаётся необязательной manual
  проверкой перед заявлением о разных физических мониторах.
- [ ] Итоговая приёмка #132/#128 пользователем с известными измеренными пределами.

Внешний A/B можно выполнить позже для исследования с тем же законным user IR,
48 kHz, wet 100%, Pre-delay 0, Tone 0, уровнем и одинаковой раскладкой LL/LR/RL/RR.
Опциональная команда `--ab-files <quad WAV> <new folder>` создаёт dry impulse/sweep/
tones и два MR wet render с L-only/R-only возбуждением, полным хвостом. Команда
в этой QA не запускалась, пользовательские IR не копировались. Это тестовые
материалы, не часть пользовательского пакета и не результаты внешнего сравнения.
