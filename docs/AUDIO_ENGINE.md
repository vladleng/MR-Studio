# Moon River Studio — Audio Engine / ASIO / Performance

## 1. Главный приоритет

Производительность и стабильность Audio Engine являются критическим требованием **всей Moon River Studio**, а не только Live Mode.

DAW не считается пригодной для серьёзной работы, если при реальной нагрузке появляются:

- audio dropouts / xruns;
- треск;
- нестабильная задержка;
- заметные CPU-spikes при переключении patches/projects;
- зависимость playback от UI, waveform, metadata, AI или network thread.

Ключевой ориентир — **Fender Studio Pro как performance benchmark** на том же компьютере, интерфейсе, драйвере, sample rate, buffer size и сопоставимой plugin-chain.

Audio Engine принадлежит SHARED Core и используется Arrange/Edit/Mix и Live Mode.

---

## 2. Основной Windows audio path

Приоритетный professional path:

```text
Moon River Studio
      ↓
SHARED Audio Engine
      ↓
ASIO Host Layer
      ↓
Vendor ASIO Driver
      ↓
Audio Interface
```

MRS должна использовать родной ASIO-драйвер аудиоинтерфейса, если он установлен.

Примеры:

- RME ASIO;
- Focusrite USB ASIO;
- MOTU ASIO;
- Yamaha/Steinberg ASIO;
- другие 64-bit vendor ASIO drivers.

MRS не должна требовать собственного виртуального ASIO-драйвера для обычной работы.

WASAPI Exclusive / Shared можно оставить как fallback для встроенной аудиокарты, preview и non-critical сценариев.

---

## 3. Audio Device Settings

Минимальная панель настроек:

```text
Driver Type
ASIO Device
Sample Rate
Buffer Size
Inputs
Outputs
Device Control Panel
```

### ASIO Device

MRS получает список доступных vendor drivers и позволяет выбрать нужный.

### Sample Rate

Для Live Mode желательно использовать единый sample rate для всего show/setlist, например 48 kHz.

В production mode project/device mismatch также должен обрабатываться явно.

### Buffer Size

Нельзя полагаться на жёсткий список buffer sizes. Нужно учитывать возможности конкретного драйвера.

Если buffer меняется только через vendor control panel, MRS должна уметь открыть эту панель.

### Channel Names

Если driver предоставляет реальные названия I/O, использовать их вместо абстрактных `Input 1 / Output 1`.

---

## 4. Audio Device Profiles

Нужно поддержать сохранённые профили оборудования.

```text
Profile: Live Main Rig
Device: RME Fireface UCX II
Sample Rate: 48 kHz
Buffer: 64

Inputs
1 -> Guitar
2 -> Vocal
3-4 -> Keys

Outputs
1-2 -> FOH
3-4 -> Monitor
5 -> Click
6 -> Cue
```

Другие profiles могут использоваться для production, rehearsal, другого интерфейса или резервного ноутбука.

---

## 5. Dual-path processing

Архитектура должна позволять разделять low-latency monitoring и тяжёлую playback/mix обработку.

```text
                    SHARED Audio Engine
                             |
             +---------------+---------------+
             |                               |
       LIVE / LOW LATENCY              PROCESS PATH
             |                               |
   Guitar / Vocal / Keys           Tracks / Mix / Playback
             |                         Click / Cues / FX
       low-latency DSP                       |
             +---------------+---------------+
                             |
                           Mixer
                             |
                         Audio Out
```

Это нужно не только на сцене. Большая production-сессия не должна автоматически разрушать low-latency monitoring записываемого инструмента.

---

## 6. Realtime rules

Audio callback выполняет только realtime-safe операции.

В audio thread запрещаются:

- file I/O;
- network I/O;
- UI rendering;
- disk logging;
- dynamic plugin loading;
- тяжёлое перестроение routing graph;
- metadata/AI parsing;
- blocking mutex;
- лишние memory allocations.

Worker threads обслуживают:

- disk streaming;
- plugin loading/scanning;
- waveform generation;
- project serialization;
- import/metadata;
- remote/network;
- AI;
- UI;
- logging.

Если UI или AI зависнут, audio должен продолжать работать.

---

## 7. Один engine для всех режимов

Критическое правило:

```text
Arrange/Edit/Mix
       |
       +------> SHARED Audio Engine <------ Live Mode
```

Live Mode не имеет:

- собственного ASIO layer;
- отдельного playback engine;
- отдельного mixer backend;
- отдельного plugin host.

Любая backend-оптимизация должна улучшать всю DAW.

---

## 8. Plugin strategy

VST3 hosting — потенциально одна из самых тяжёлых частей engine.

Plugins полезно классифицировать:

### Live-safe / low-latency

- amp sim;
- EQ;
- compressor;
- basic modulation;
- low-latency reverb/delay.

### High-latency / process-oriented

- heavy lookahead;
- linear-phase processing;
- large oversampling;
- plugins с существенной собственной latency.

MRS должна показывать reported plugin latency. Live Mode может дополнительно предупреждать, если chain плохо подходит для low-latency monitoring.

---

## 9. Patch switching

Нельзя загружать тяжёлый plugin/preset непосредственно в момент section transition.

```text
Current Patch -> ACTIVE
Next Patch    -> PRELOADED / WARM

Section Boundary
      ↓
fast switch / safe crossfade
```

Patch является state/snapshot общего processor graph.

---

## 10. Project / song preload

Live Mode может заранее подготовить следующий project/song state:

- audio assets;
- project metadata;
- routing;
- plugins/presets;
- MIDI state.

Цель — убрать тяжёлую инициализацию из момента `Next Song`.

Механизмы preload/read-ahead принадлежат SHARED Core и могут использоваться production mode там, где это полезно.

---

## 11. Playback optimization

Для long-file/stems playback:

- read-ahead buffers;
- заранее открытые assets;
- sample-locked synchronization;
- минимальный realtime resampling;
- preload следующего state;
- predictable disk access.

Live Mode может использовать упрощённый UI/workflow, но не отдельный playback backend.

---

## 12. ASIO compatibility cases

Нужно корректно обрабатывать:

### Device already in use

Некоторые ASIO drivers не разрешают simultaneous access нескольким приложениям. Показывать понятную ошибку, не зависать.

### Sample-rate conflict

Явно показывать project/show sample rate и device sample rate.

### Device disconnect/reconnect

Сначала безопасный отказ, позднее controlled reconnect workflow.

### Driver control panel

Должна быть команда открытия vendor panel.

### Driver-specific behavior

Compatibility matrix должна накапливаться по реальным интерфейсам и driver versions.

---

## 13. Performance metrics

Нельзя оценивать engine только по среднему CPU.

Минимум:

- xruns/dropouts;
- maximum callback time;
- callback load percentiles;
- reported input/output latency;
- round-trip latency, если измеряется;
- plugin latency;
- CPU spike при patch switch;
- CPU spike при project/song switch;
- long-session stability;
- device reconnect behavior;
- disk streaming headroom.

---

## 14. Studio Pro Performance Benchmark

Benchmark выполняется для **Moon River Studio Audio Engine**.

Одинаковые условия:

```text
Same computer
Same audio interface
Same vendor ASIO driver
Same sample rate
Same buffer size
Comparable plugin chain
Comparable live inputs
Comparable playback load
```

| Test | Studio Pro | Moon River Studio |
|---|---:|---:|
| 48 kHz / 128 samples | baseline | target |
| 48 kHz / 64 samples | baseline | target |
| Guitar live chain | baseline | target |
| Keys VST | baseline | target |
| multitrack playback | baseline | target |
| Click + cue | baseline | target |
| Patch switching | baseline | target |
| long-session stress | baseline | target |
| full-show stress | baseline | target |

Главный критерий:

> **На конфигурации, где Studio Pro стабильно работает, MRS должна стремиться к сопоставимой live-stability без systematic xruns/dropouts.**

Если этот критерий не достигается, performance problem становится blocking issue для Live readiness и требует анализа в SHARED Core.

---

## 15. SHARED Audio Performance Gate — issue #16

До тяжёлого наращивания host-функций необходимо подтвердить:

- ASIO device enumeration;
- direct vendor driver open;
- input/output callback;
- sample rate / buffer handling;
- channel enumeration;
- vendor control panel;
- stereo/multitrack playback prototype;
- low-latency input passthrough;
- callback stability metrics;
- prototype stress test;
- benchmark со Studio Pro.

### Acceptance

1. Vendor ASIO driver работает напрямую.
2. I/O routing стабилен.
3. Audio callback независим от UI/AI/network.
4. Базовый stress test не показывает систематических xruns/dropouts.
5. Есть измеряемый benchmark относительно Studio Pro.
6. Выявленные performance blockers документируются и исправляются в SHARED Core.

---

## 16. Live Mode performance policy

Live Mode использует тот же engine, но может включать более строгие runtime policies:

- preflight перед show;
- запрет/предупреждение для high-latency chains;
- preload next project/patch;
- минимизация фоновых non-critical jobs;
- усиленная xrun/device diagnostics;
- recovery-oriented state saving.

Это policy/configuration поверх общего engine, а не второй engine.

---

## 17. Licensing / release check

Перед публичным распространением необходимо отдельно проверять актуальные лицензионные условия ASIO SDK, VST3 и выбранного framework/toolchain.

Это release/legal задача и не должна оставаться неявной.


## Stage 1d / 0.1e recording foundation
The existing SHARED AudioEngine now supports raw mono ASIO capture through a fixed
ring and background WAV writer, independent monitoring and one-step take attachment
through the existing ProjectStore/Undo. Same engine/device/transport, archive schema
unchanged. See [recording contracts](RECORDING.md) and
[Windows checklist](MRS_STAGE_1D_CHECKLIST.md). Hardware acceptance pending.
