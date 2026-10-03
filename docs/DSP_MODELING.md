# Moon River Studio — Native DSP, Cab IR and Neural Modeling

## 1. Цель

Moon River Studio в будущем может включать собственные встроенные эффекты и гитарный processing stack, чтобы базовый live/production workflow не зависел только от сторонних VST3.

Возможные категории:

- EQ;
- compressor;
- gate;
- saturation;
- delay/reverb;
- cab/room convolution;
- preamp/amp/pedal models;
- neural capture player.

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

Пример цепочки:

```text
Input
 -> preamp stage / waveshaping
 -> tone stack
 -> saturation
 -> power amp model
 -> cab IR
```

Преимущества:

- контролируемые параметры Gain/Bass/Mid/Treble;
- малая и предсказуемая latency;
- возможность оптимизации под realtime;
- независимость от neural model file.

### 3.2 Neural capture/model player

MRS может поддерживать формат/engine для воспроизведения заранее обученных моделей усилителей, преампов и педалей.

Концепция:

```text
Reference stimulus -> hardware chain -> recorded response -> training -> model
```

В runtime:

```text
Guitar -> Neural Model -> Cab IR -> FX
```

На первом этапе разумнее интегрировать существующий open neural modeling ecosystem/player, чем разрабатывать собственный training framework с нуля.

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

Необходимо жёстко разделять:

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

Лицензия движка/формата не означает автоматического права распространять конкретные модели или IR.

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

Все native processors должны подчиняться общим правилам Audio Engine:

- no allocation in audio callback;
- no file I/O in callback;
- no network access;
- predictable latency;
- latency reporting;
- safe preset/patch switching;
- preload/warmup тяжёлых моделей;
- denormal protection;
- bounded CPU usage.

## 8. Live-safe classification

Для MRL полезно классифицировать processors/plugins:

- `LIVE SAFE` — пригоден для low-latency path;
- `PLAYBACK / MIX` — допустим в process path;
- `HIGH LATENCY` — требует предупреждения или исключается из live monitoring path.

MRS должен учитывать reported latency, oversampling и собственные ограничения processor при построении live path.

## 9. AI integration

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

Все изменения должны быть undoable и не выполняться непосредственно из realtime thread.

## 10. Порядок реализации

Не требуется реализовывать всё на ранней версии DAW.

Рациональный порядок:

1. базовый plugin/processor graph;
2. простые native utility processors;
3. convolution / Cab IR;
4. stable preset/state system;
5. neural model player;
6. собственные Moon River models/captures;
7. advanced hybrid amp system.
