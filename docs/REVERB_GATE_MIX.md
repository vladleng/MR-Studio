# MR Reverb — Input gate и ход Mix, 0.3c upd4

2026-10-10, дополнение по прямому запросу пользователя в #132/#128.
Source `f97fba54af24b768b44fbbde123912c93eefa22c`, текущая локальная ветка.
**Принято 2026-10-11:** пользователь после upd4 подтвердил «Это сработало,
закрывай». #132 и общий #128 закрыты completed; #129–#131 закрыты ранее.
Новые точные CPU%, threshold/IR и отдельные результаты Mix не сообщены;
полная ASIO stress матрица/mono audition/physical mixed-DPI не объявляются PASS.
Приёмка зафиксирована без изменения исходников, пакета и его manifest.
Shared return тест подтвердил, что ненулевой вход от цепей TH-U поддерживает
активную свёртку в Stop. Точный уровень, DC/noise и блок-источник не измерены.
Гейт универсален: поведение не зависит от бренда, трека или состояния транспорта.

## Поведение

В редакторе **Options... → Input gate**. По умолчанию OFF; порог −90 dBFS,
диапазон **−120..−60 dBFS**, шаг **0,1 dB**, Ctrl для тонкой регулировки.
Options раскрывает строку в текущем редакторе; само раскрытие временное и не
сохраняется. Выключенный gate показывает недоступный threshold.

Общий peak detector для L/R открывает wet input сразу, без добавленной задержки
и ослабления начала транзиента. Закрывающий порог на 6 dB ниже открывающего,
hold 50 ms, линейное закрытие 10 ms до точного нуля. Гейт стоит перед IR,
не меняет dry, не сбрасывает ядро/хвост и не прекращает готовый Pre-delay/Tone
хвост. После закрытия convolution продолжает рассчитывать полный конечный IR;
CPU падает после его завершения, а не сразу после Stop. Если upstream шум выше
порога, он продолжит открывать gate: пользователь выбирает порог над шумом.

Mix: нелинейный ход с **25% wet на середине шкалы** (квадратическая зависимость
wet от позиции). Например, 10% хода = 1% wet. Цифры показывают настоящий wet
процент; DSP dry/wet blend и сохранённые значения остаются линейными и прежними.
Mix 25% в старом проекте даёт прежний звук. 100% остаётся полностью wet для return.
Существующее сглаживание изменения Mix 20 ms сохранено.

## Интеграция и совместимость

`ReverbState` — источник истины: `gate_enabled`, `gate_threshold`.
Application/SetInserts и существующий preview/commit обеспечивают Undo/Redo
(один Undo на жест), параметрическую передачу в работающий граф без пересборки
или повторного открытия device. Новые DSP параметры: 3 enable(0/1), 4 threshold.
RT detector/ramp принадлежит одному DSP owner; кеш порога обновляется при
изменении значения. Нет новых alloc/free, locks, I/O, потоков или UI вызовов.
Время работы detector bounded по числу сэмплов; IR loader/retirement прежние.

Snapshot **schema17 / REVERB state2** пишет gate; state1 читается как OFF/−90.
Проекты v1–16 и старые MR presets читаются; новое состояние предназначено для
upd4 и следующих версий. Проекты/пресеты сохраняют gate даже при missing IR,
повторной загрузке и выборе нового IR. Пользовательские проекты и IR не менялись.
UI/model/commands/Undo/serialization/engine/tests/docs затронуты; IR assets и
routing не затронуты. DSP общий; готовность Live не расширяется этим дополнением.

## Приёмка и проверки

Release build **PASS**, **CTest126/126 PASS(141.65s)**: reference/silence/tail,
новые gate tests 44.1/48/96k × 128/256/512, mono/stereo link, hold/hysteresis,
exact closure, dry, Pre-delay/Tone, automation offset, invalid controls и
alloc/free; state1 migration, project/preset/reopen/Undo; GUI gesture и
Options layout 760×390/900×450, software scale100/150/200% PASS. Packaged
`--reverb-editor-smoke` exit0; narrow/150% previews визуально inspected.
RT alloc/free0/0, software transport/handoff/restore regressions PASS.

Private quad Large Hall48k/256, same quiet−80dBFS signal,1024 measured blocks:
gate OFF mean237.029/p95375.7us; gate ON после хвоста mean8.968/p959.3us;
drained silence mean7.670us. Full impulse/tail oracle maxerror8.9407e−8,
alloc/free0/0. Один offline прогон, не обещание общей ASIO CPU/Live deadline.
Gate OFF active signal mean239.385us; условия finite-support idle прежние.
IR только читался, не копировался в пакет. Логи и previews отдельно
`build/asio-local/validation/reverb-gate`, symbols отдельно
`build/asio-local/deliveries/0.3c-upd4-reverb-gate`.

Пакет chat workspace Builds:
`MR-Studio-0.3c-upd4-MR-Reverb-Gate-Mix-JUCE-ASIO-Windows-local`,13files,
12payload hashes verified. GUI SHA256
`7605FD36F459F001DF4423B25891F07BA171005309C97ED56678EC3BE52AFD1B`.
Предыдущие пакеты сохранены. Code/docs локально, GitHub только issue updates;
push/merge/Actions отсутствуют. При доставке: READY WITH MANUAL CHECK /
RT SAFE WITH MANUAL CHECK. Итоговая пользовательская приёмка получена 2026-10-11.

Исторический manual checklist при доставке (до приёмки): проверить тот же
return/insert проект на ASIO48k/256. Включить
gate и поднимать порог от −90 dBFS, пока тихий upstream перестанет возбуждать
Reverb. После Stop дождаться IR+Pre-delay хвоста; CPU должен уменьшиться.
Прослушать тихие ноты/затухания, не теряя музыкального входа; при необходимости
снизить порог. Gate OFF должен возвращать прежнюю обработку. Проверить Mix
мышью, особенно нижнюю часть, и сохранение/открытие проекта. Пользователь затем
подтвердил успешную работу и поручил закрыть. #132/#128 закрыты; отдельная
физическая mixed-DPI/полная hardware stress matrix остаётся NOT RUN.
