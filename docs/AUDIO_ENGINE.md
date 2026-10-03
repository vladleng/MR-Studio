# Moon River Live — Audio Engine / ASIO / Performance

## 1. Главный приоритет

Для Moon River Live производительность и стабильность audio engine являются критическим требованием проекта.

Приложение не считается пригодным для live-работы, если при реальной концертной нагрузке появляются:

- audio dropouts / xruns;
- треск;
- нестабильная задержка;
- заметные CPU-spikes при переключении песен или patches;
- зависимость playback от UI, waveform, metadata или network thread.

Ключевой ориентир — **Fender Studio Pro как performance benchmark** на том же компьютере, интерфейсе, драйвере, sample rate, buffer size и сопоставимой plugin-chain.

---

## 2. Основной Windows audio path

Приоритетный режим Moon River Live на Windows:

```text
Moon River Live
      ↓
ASIO Host Layer
      ↓
Vendor ASIO Driver
      ↓
Audio Interface
```

Приложение должно использовать **родной ASIO-драйвер аудиоинтерфейса**, если он установлен.

Примеры:

- RME ASIO;
- Focusrite USB ASIO;
- MOTU ASIO;
- Yamaha/Steinberg ASIO;
- другие 64-bit ASIO drivers, предоставляемые производителем устройства.

Moon River Live **не должен требовать собственного виртуального ASIO-драйвера** для обычной работы.

WASAPI Exclusive / Shared можно оставить как fallback для встроенной аудиокарты, preview и non-live сценариев.

---

## 3. Требования к Audio Device Settings

Минимальная панель настроек должна поддерживать:

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

Moon River Live получает список доступных ASIO-драйверов и позволяет выбрать конкретный vendor driver.

### Sample Rate

Рекомендуемый live-workflow — единый sample rate для всего setlist, например 48 kHz.

Если текущий device sample rate отличается от song/show configuration, приложение должно явно предупредить пользователя и предложить безопасное действие.

### Buffer Size

Нельзя полагаться на жёстко заданный список размеров buffer. Нужно учитывать возможности конкретного драйвера.

Если изменение buffer size выполняется только через vendor control panel, Moon River Live должен корректно открыть эту панель.

### Channel Names

Если ASIO driver предоставляет реальные названия входов/выходов, приложение должно использовать их вместо абстрактных Input 1 / Output 1.

---

## 4. Audio Device Profiles

Нужно поддержать сохранённые профили оборудования.

Пример:

```text
Profile: Live Main Rig
Device: RME Fireface UCX II
Sample Rate: 48 kHz
Buffer: 64

Inputs
1 → Guitar
2 → Vocal
3–4 → Keys

Outputs
1–2 → FOH
3–4 → Monitor
5 → Click
6 → Cue
```

Отдельный профиль может использоваться для репетиции, другого интерфейса или резервного ноутбука.

---

## 5. Dual-path processing

Архитектура должна разделять low-latency live processing и тяжёлый playback processing.

```text
                    Moon River Audio Engine
                             │
             ┌───────────────┴───────────────┐
             │                               │
       LIVE / LOW LATENCY              PLAYBACK PATH
             │                               │
   Guitar / Vocal / Keys              Stems / backing
             │                         Click / cues
       Low-latency VST3                       │
             └──────────── Mixer ─────────────┘
                            │
                        Audio Out
```

Live input path не должен страдать только потому, что playback использует большое количество stems, waveform analysis, preload или metadata.

---

## 6. Realtime rules

Audio callback должен выполнять только realtime-safe операции.

В audio thread запрещается:

- file I/O;
- network I/O;
- UI rendering;
- logging на диск;
- динамическая загрузка plugins;
- перестроение сложного routing graph;
- тяжёлый metadata parsing;
- обычные blocking mutex;
- лишние memory allocations в callback.

Отдельные worker threads должны обслуживать:

- disk streaming;
- plugin loading;
- waveform generation;
- metadata;
- remote/network;
- UI;
- logging.

UI получает позицию playback из audio engine, но UI не управляет временем audio callback.

Если UI зависнет, audio должен продолжать работать.

---

## 7. Plugin strategy

VST3 hosting — потенциально самая тяжёлая часть live engine.

Plugins следует условно классифицировать:

### Live-safe

Подходят для low-latency input path:

- amp sim;
- EQ;
- compressor;
- basic modulation;
- low-latency reverb/delay.

### High-latency / playback-only

Могут быть нежелательны в live monitoring path:

- тяжёлый lookahead;
- linear-phase processing;
- большой oversampling;
- plugins с существенной собственной latency.

Moon River Live должен уметь показывать reported plugin latency и предупреждать о потенциально проблемной live-chain.

---

## 8. Patch switching

Нельзя загружать тяжёлый plugin/preset непосредственно в момент перехода секции.

Правильная модель:

```text
Current Patch → ACTIVE
Next Patch    → PRELOADED / WARM

Section Boundary
      ↓
fast switch / safe crossfade
```

То же правило желательно применять к следующей песне.

---

## 9. Song preload

Во время исполнения current song следующая песня может заранее подготовить:

- audio assets;
- metadata;
- routing;
- plugins/presets;
- MIDI state.

Цель — убрать тяжёлую инициализацию из момента `Next Song`.

---

## 10. Playback optimization

Moon River Live имеет преимущество перед полноценной DAW: в live-mode ему не нужны многие editing-функции DAW.

Для stems следует использовать:

- read-ahead buffers;
- заранее открытые audio assets;
- минимальный realtime resampling;
- sample-locked synchronization;
- preload current/next song.

Чистый playback path должен быть максимально лёгким.

---

## 11. ASIO compatibility cases

Нужно корректно обрабатывать:

### Device already in use

Некоторые ASIO drivers могут не разрешать одновременный доступ нескольким приложениям.

Приложение должно показать понятную ошибку, а не зависнуть.

### Sample-rate conflict

Пользователь должен видеть различие между show/sample-rate и device/sample-rate.

### Device disconnect/reconnect

На ранних этапах достаточно безопасного отказа; позже нужен controlled reconnect workflow.

### Driver control panel

Должна быть доступна команда открытия фирменной панели устройства.

---

## 12. Performance metrics

Нельзя оценивать engine только по среднему CPU.

Минимальный набор метрик:

- xruns/dropouts;
- maximum callback time;
- callback load percentiles;
- reported input/output latency;
- round-trip latency, если измеряется отдельно;
- plugin latency;
- CPU spike при patch switch;
- CPU spike при song switch;
- stability under long playback;
- device reconnect behavior.

---

## 13. Studio Pro Performance Benchmark

До перехода к тяжёлым Stage проект должен пройти сравнительный тест.

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

Пример matrix:

| Test | Studio Pro | Moon River Live |
|---|---:|---:|
| 48 kHz / 128 samples | baseline | target |
| 48 kHz / 64 samples | baseline | target |
| Guitar live chain | baseline | target |
| Keys VST | baseline | target |
| 4–8 stems | baseline | target |
| Click + cue | baseline | target |
| Patch switching | baseline | target |
| Full-show stress test | baseline | target |

Главный критерий:

> **При конфигурации, в которой Studio Pro стабильно работает на данном компьютере, Moon River Live должен стремиться к сопоставимой live-stability без xruns/dropouts.**

Если этот критерий не достигается, дальнейшее наращивание функций не должно иметь приоритет над оптимизацией audio engine.

---

## 14. Stage 0 Performance Gate

В Stage 0 необходимо провести отдельный audio-engine spike до начала сложного UI/VST/show workflow.

Минимум:

- ASIO device enumeration;
- открытие vendor driver;
- input/output callback;
- выбор sample rate / buffer;
- channel enumeration;
- vendor control panel;
- stereo playback;
- basic live input passthrough;
- измерение callback stability;
- prototype stress test;
- сравнение со Studio Pro на одном setup.

### Acceptance

Stage 0 audio performance gate считается пройденным, если:

1. родной ASIO driver интерфейса работает напрямую;
2. routing входов/выходов стабилен;
3. playback и live input работают без архитектурной зависимости от UI thread;
4. базовый stress test не показывает систематических xruns/dropouts;
5. есть зафиксированный benchmark относительно Studio Pro;
6. выявленные performance blockers задокументированы до дальнейшего расширения host-функций.

---

## 15. Licensing / release check

Перед публичным распространением необходимо отдельно проверить актуальные лицензионные условия ASIO SDK и выбранного audio framework/toolchain.

Это release/legal задача и не должна оставаться неявной.