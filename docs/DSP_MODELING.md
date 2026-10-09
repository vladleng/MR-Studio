# Moon River Studio — Native DSP, Cab IR and Neural Modeling

## 1. Цель

Moon River Studio в будущем может включать собственные встроенные эффекты и гитарный processing stack, чтобы базовый production и Live Mode workflow не зависел только от сторонних VST3.

Возможные категории:

- EQ;
- compressor;
- gate;
- saturation;
- delay/reverb;
- cab/room convolution;
- preamp/amp/pedal models;
- neural capture player.

Все эти processors подключаются к общему SHARED processor graph и доступны всем workspaces MRS.

## 2. Cab / Room IR

Кабинеты, микрофонные позиции и помещения удобно поддерживать через impulse responses.

```text
Input -> Amp/Preamp -> Convolution -> Cab IR -> Room/FX
```

Базовый Cab IR module:

- WAV IR import;
- mono/stereo IR;
- IR browser;
- low/high cut;
- phase/polarity;
- mix;
- gain normalization;
- latency reporting;
- efficient partitioned convolution.

IR — линейная модель и подходит прежде всего для cabinet/mic/room responses, но не заменяет полноценную нелинейную модель distortion/preamp/power amp.

## 3. Amp / Preamp / Pedal modeling

Для нелинейных устройств рассматриваются два подхода.

### 3.1 Classical DSP

```text
Input
 -> preamp stage / waveshaping
 -> tone stack
 -> saturation
 -> power amp model
 -> cab IR
```

Преимущества:

- контролируемые Gain/Bass/Mid/Treble;
- малая и предсказуемая latency;
- возможность оптимизации под realtime;
- независимость от neural model file.

### 3.2 Neural capture/model player

MRS может поддерживать engine для воспроизведения заранее обученных моделей усилителей, преампов и педалей.

```text
Reference stimulus -> hardware chain -> recorded response -> training -> model
```

Runtime:

```text
Guitar -> Neural Model -> Cab IR -> FX
```

На первом этапе разумнее интегрировать существующий open neural modeling ecosystem/player, чем разрабатывать training framework с нуля.

## 4. Hybrid processor

Перспективная схема собственного Moon River Amp:

```text
Neural / DSP Preamp
        |
Parametric Tone Stack
        |
Power Amp DSP
        |
Cab IR
        |
Room / Delay / Reverb
```

Это позволяет сочетать характер capture с управляемыми параметрами.

## 5. Factory Content vs User Library

Нужно жёстко разделять:

### Factory Content

Только:

- собственные Moon River captures/IR;
- контент с явным разрешением на распространение;
- лицензированные сторонние модели.

### User Library

Пользователь самостоятельно импортирует:

- WAV IR;
- поддерживаемые neural model files;
- собственные captures;
- сторонний контент, права на использование которого лежат на пользователе.

Лицензия движка/формата не означает автоматического права распространять конкретные models/IR.

## 6. Собственная библиотека Moon River

В будущем можно создать собственные модели, например:

```text
Moon River Amp Collection
├── MR Jazz Clean
├── MR Warm Tube
├── MR Crunch
├── MR Lounge Clean
└── MR Bass Vintage
```

Они могут быть сделаны:

- на собственном hardware;
- через собственный DSP;
- как собственные neural captures;
- как hybrid models.

## 7. Realtime requirements

Все native processors подчиняются общим правилам SHARED Audio Engine:

- no allocation in audio callback;
- no file I/O in callback;
- no network access;
- predictable latency;
- latency reporting;
- safe preset/patch switching;
- preload/warmup тяжёлых models;
- denormal protection;
- bounded CPU usage.

## 8. Live-safe classification

Для встроенного Live Mode полезно классифицировать processors/plugins:

- `LIVE SAFE` — пригоден для low-latency monitoring path;
- `PLAYBACK / MIX` — допустим в process path;
- `HIGH LATENCY` — требует предупреждения или исключается из live monitoring path.

MRS должна учитывать reported latency, oversampling и собственные ограничения processor при построении low-latency path.

Классификация является metadata общего processor graph, а не отдельной Live plugin system.

## 9. Patch model

Live patch — state/snapshot того же processor graph, который настраивается в Mix/Arrange.

```text
MRS Processor Graph
      |
      +-> Production state
      +-> Saved Patch A
      +-> Saved Patch B
```

Live Mode переключает подготовленные states через SHARED engine.

## 10. AI integration

AI layer может управлять встроенным DSP через тот же structured Tool API.

Пример:

```text
"Сделай гитару мягче и темнее, в jazz clean направлении"
```

AI получает текущий processor state и предлагает/применяет разрешённые параметры:

```text
set_processor_parameter(track, "MR Amp", "gain", 2.4)
set_processor_parameter(track, "MR Amp", "treble", 4.8)
set_processor_model(track, "MR Cab", "MR 1x12 Warm")
```

Все изменения undoable и не выполняются непосредственно из realtime thread.

## 11. Порядок реализации

Не требуется реализовывать всё на ранней версии DAW.

Рациональный порядок:

1. общий plugin/processor graph;
2. простые native utility processors;
3. convolution / Cab IR;
4. stable preset/state system;
5. Live-safe classification and patch snapshots;
6. neural model player;
7. собственные Moon River models/captures;
8. advanced hybrid amp system.

## 12. Planned MR Saturator — Studio vs Live quality separation (2026-10-09)

Спецификация отдельного будущего нативного эффекта: **[MR Saturator](MR_SATURATOR.md)**; parent issue [#105](https://github.com/vladleng/MR-Studio/issues/105), feature track [#78](https://github.com/vladleng/MR-Studio/issues/78).

- На **Studio Mix** приоритет — качество DSP при корректно компенсированной задержке: oversampling, нелинейные многокаскадные/динамические модели и исследование ADAA допустимы по результатам измерений. Типичный буфер сведения может быть больше 128 frames.
- На **Studio Record/Monitor** низкий буфер (например 128 frames) нужен для записи/мониторинга; profile/latency/PDC учитываются отдельно, без молчаливого обхода эффектов и без изменения raw recording.
- **Live Mode** остаётся режимом той же DAW на едином SHARED backend; live-safe классификация относится к конкретной модели/профилю и не ограничивает все Studio-модели.
- **Realtime safety** остаётся общей для всех DSP — больший Studio buffer не оправдывает allocations, locks и I/O в callback.

Это backlog, не команда начинать реализацию и не новое завершение Stage 3. Возможный внешний VST3/встроенный channel-strip рассматривается позже, но не входит в MVP.
