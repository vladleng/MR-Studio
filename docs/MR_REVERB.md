# MR Reverb — native convolution reverb (MVP / Codex handoff)

**Статус:** согласованная концепция и backlog, **реализация не начиналась**. Дата: 2026-10-10.  
**Parent:** [#128 MR Reverb](https://github.com/vladleng/MR-Studio/issues/128), направление [#111 NATIVE PLUGINS](https://github.com/vladleng/MR-Studio/issues/111).  
**Рабочие задачи:** [#129 DSP](https://github.com/vladleng/MR-Studio/issues/129) → [#130 IR library](https://github.com/vladleng/MR-Studio/issues/130) → [#131 GUI/state](https://github.com/vladleng/MR-Studio/issues/131) → [#132 QA](https://github.com/vladleng/MR-Studio/issues/132). Это связанные issues; **нативные GitHub sub-issues не предполагаются только по ссылкам**.  
**Редактируемый компактный GUI-wireframe:** [docs/design/MR_REVERB_GUI.svg](design/MR_REVERB_GUI.svg).

## 1. Зачем нужен эффект

**MR Reverb** — простой качественный свёрточный ревербератор (Convolution Reverb) для обычной обработки дорожек, шин и Master в **той же DAW MR Studio**, через existing native insert shell и **SHARED processor graph**. Пользователь один раз назначает папку со своими импульсами и дальше выбирает их **внутри окна эффекта**, без повторных файловых диалогов.

Дизайн: минимально, современно, плоско, компактно — визуальная семья MR Strip и MR Saturator, **НЕ** skeuomorphic rack/аналоговый прибор; без огромных knobs, фальшивого объёмного свечения и декоративных картинок помещений. Привычная логика: **IR → Pre-delay / Tone / Mix**.

Три обязательных качества:
1. **Честное звучание импульса** (не портить исходный IR необоснованным resize/stretch/normalization/truncation).
2. **True Stereo**, а не ошибочно названная так диагональная stereo convolution.
3. **Надёжная локальная работа** (длинный IR и поиск файлов не блокируют аудиопоток).

## 2. Строгий MVP (что видно пользователю)

### Основная панель
- Заголовок: `MR Reverb`, активное имя IR, доступный native host Bypass.
- **IR Folder**: `Choose folder…` (один раз); запоминается между сессиями **как пользовательская настройка**, не дублируется в каждом инстансе. При необходимости смены — та же кнопка.
- **Browser**: компактное дерево вложенных папок + список IR, поле Search (по имени), строка выделения; один клик выбирает IR, загрузка off-thread. Можно хранить избранное позже, **не обязательно для MVP**.
- **Selection info**: имя IR, длительность, частота файла, **Mono / Stereo / True Stereo**. Текстовые статусы `Loading`, `Ready`, `Missing`, `Unsupported`. Небольшой waveform preview **необязателен** — не задерживать релиз ради него.
- Три плоских регулятора (slider / компактный dial — в зависимости от существующей UI-библиотеки):

| Параметр | Default (insert) | Диапазон MVP | DSP-смысл |
|---|---:|---|---|
| **Pre-delay** | 0 ms | 0–200 ms | Дополнительная задержка **только wet path**, сверх заложенной в IR; без изменений сухого сигнала. |
| **Tone** | 0 dB | −6 … +6 dB | Однополосная широкая high-shelf коррекция **только wet**, центральная граница/shape фиксирована и откалибрована; 0 dB строго нейтрально. |
| **Mix** | 25% | 0–100% | Линейный dry/wet, 0% = dry, 100% = wet; c latency alignment и smoothing. На FX return вручную ставится 100%. |

**Host shell** уже владеет Bypass, Save/Load preset и lifecycle; не дублировать лишними кнопками. Рядом с имени IR разрешить маленькую кнопку выбора папки; отдельную панель инженерных параметров не делать.

### Явно вне первой версии
- Нет Size/Length/Decay/time-stretch, reverse, IR editing, Early/Late mix, stereo width, multiband EQ, modulation, algorithmic tail, microphone positions, room image, A/B, oversampling toggle.
- Нет пользовательских quality profiles, skin switcher, дополнительных вкладок или обязательного увеличенного интерфейса.
- Нет отдельной VST3-версии; позднее можно вынести DSP из общей native библиотеки без переписывания.
- Нет полноценного Spatial/перемещения инструментов по комнате: это [ACOUSTIC_SPACE_PROTOTYPE.md](ACOUSTIC_SPACE_PROTOTYPE.md), **другая** инициатива.
- Нет Bricasti/Samplicity коммерческой модели или фирменных IR в составе программы.

## 3. Форматы импульсов и правильная математика

### Первые поддерживаемые форматы
- RIFF/WAVE PCM 16/24/32-bit и IEEE float32 (строго по результатам фактического декодера; если исключения — явно сообщить в GUI, а не молча испортить данные). Mono, 2-channel stereo, 4-channel quad; отдельно **пара из двух двухканальных WAV**.
- Разные sample rates исходников; корректное offline resampling к текущей project rate. Проверить 44.1/48/96 kHz; предельные rates файла задаёт decoder validation, не обещать бесконечный диапазон.
- Смысл 4ch quad и paired true-stereo подтверждать channel map. **Не считать любой 4ch WAV автоматически LL/LR/RL/RR без валидации раскладки.**
- Отдельный IR может содержать leading delay — не срезать автоматически. Не выполнять peak normalization / эквализацию по умолчанию.
- Целевой тестовый класс room IR — до **20 с**, включая хвосты 8–10 с. Это ориентир приёмочных тестов, **не объявленный до измерения жёсткий лимит**; защитный предел зависит от фактических RAM/CPU budget и пользователь увидит понятную ошибку.

### Routing

Для **single-channel mono IR**: независимая обработка каждого входного канала одним ядром (dual mono в stereo track), без выдуманного stereo crossfeed.

Для **two-channel stereo IR**: диагональная свёртка (L→L, R→R), честное обозначение **Stereo**, не **True Stereo**.

Для **True Stereo**: четыре ядра `h_LL, h_LR, h_RL, h_RR`; индекс 1 — *вход*, индекс 2 — *выход*. 
```text
wet_L = conv(in_L, h_LL) + conv(in_R, h_RL)
wet_R = conv(in_L, h_LR) + conv(in_R, h_RR)
```
*Dual-stereo*: один stereo WAV соответствует возбуждению левого входа (L→L, L→R); второй — правого (R→L, R→R). Кандидат на поддержку библиотеки **Samplicity Bricasti M7**, в которой существуют **quad-channel** и **dual-stereo** файлы. Детектировать парные L/R по проверенному именованию и совпадению всех существенных метаданных; при сомнении — предложить безопасное связывание пары или показать Unsupported; **не подбирать пару по первой похожей строке**. Альтернативные 44.1/48k версии одного IR — **не** две половины одного пресета. Проверить на реальных пользовательских файлах перед принятием поддержки конкретного архива.

Mono-input / stereo-output route: если existing graph разрешает стереовыход после mono insert, использовать корректный соответствующий режим; если нет — сохранить mono output / показать ограничение и рекомендовать stereo bus, **не заявлять phantom stereo**, не изменять тип дорожки без согласования.

## 4. DSP архитектура и производительность

```text
input ────────────────────────────── latency-aligned DRY ─────────┐
  └─ Pre-delay (wet) ─ IR convolution (mono/stereo/4-kernel)       │
                       └─ Tone high-shelf (wet) ─ Wet gain ──────┤ Mix ─ output
```
- Первым делом провести **аудит локального** `Cab IR 0.1m` и [CAB_IR.md](CAB_IR.md). Cab IR с 128-tap direct head + uniform 128 partition tail и лимитом **1 сек** — нельзя просто включить для 8–20 с и назвать готовым ревербератором. Переиспользовать проверенные decode/resample/FFT/state utilities, сохранив функциональность и совместимость Cab IR. Для долгих хвостов спроектировать/измерить **non-uniform partitioned convolution** или иной эквивалент с приемлемым CPU, не копировать автоматически заведомо неэффективное решение.
- Подготовка IR, каталогизация, декодирование WAV, FFT precompute, ресэмплинг, буферы и состояния — **не audio thread**. Поменять prepared kernel через безопасный handoff / non-blocking state exchange; памяти для старого объекта нельзя освобождать в audio callback.
- Во время playback выбор нового IR может показывать Loading; прежний IR продолжает звучать, пока новый полностью не подготовлен. Swap — smoothed overlap/crossfade (ориентир 50–150 ms, затем слуховая/RT проверка), без pop/click и освобождения состояния раньше конца использования; ошибки не сбрасывают прежнее исправное ядро.
- Pre-delay, Tone, Mix имеют непрерывные сглаженные updates; изменение одного параметра не ведёт к reassignment graph, reopening ASIO или синхронной загрузке IR.
- Измерить/reported algorithmic latency в samples, включая блоки/FFT/буферизацию; компенсировать dry path при ненулевой latency согласно текущему MRS PDC. **IR leading delay является частью импульса, не автоматически plugin algorithm latency**. Нулевая reported latency — только при подтверждённой конструкции, а не обещание.
- Offline render не отрезает хвост: согласовать экспорт, stop/seek/reset и tail life-cycle с движком. Никаких выдуманных extended tail после STOP без существующего контракта: сначала аудит, затем тест.
- Общие RT-инварианты: allocations/locks/file I/O/network/UI calls = 0 в callback; bounded buffers, denormal protection, safe ownership, reject NaN/Inf corrupted IR, no silent channel swap, deterministic state.
- Студийный Mix ориентирован на **качество, предсказуемость и экономный CPU**, а не искусственный обязательный 128-sample budget. Recording monitoring и будущий Live Mode требуют отдельных **измеренных** latency/CPU gates; не создавать второй DSP-движок.

## 5. Сканирование папок и сохранение проекта

**User Library**: корневая папка — preference приложения (первичный выбор один раз), поддержать рекурсивные подпапки; сканирование/поиск/разбор metadata фоновый и отменяемый; UI scrolling/search не блокирует транспорт. Состояния Ready/Loading/Error информативны. Не автоматически копировать чужие IR в поставку.

**Per-instance state**:
- stable effect ID `mr_reverb` + schema version;
- selection: library-relative path (when within root), absolute location as optional fallback, deterministic file identity/hash (digest can be off-thread), paired-file mapping if present;
- Pre-delay, Tone, Mix, Bypass via current host/native state/automation;
- no duplicated global library setting in every plugin instance.

**On reopen**: resolve selected IR against configured library folder first, fallback location, hash when available; validate before audio activation. On missing file, show `IR missing — choose folder/relink`, **don't silently substitute another similarly named IR**. Dry/bypass fallback must be safe and predictable; save does not erase selected identity or user settings. Undo/Redo, `.mrspreset`, project serialization respect existing native insert command flow. Не увеличивать текущую embedded Cab IR quota и не ломать чтение старых проектов; схема мигрирует согласно проверенному codebase, не придумывать номер версии в ТЗ.

## 6. Flat GUI layout (рабочий wireframe)

Ориентир — [editable SVG](design/MR_REVERB_GUI.svg). Compact desktop modal/editor roughly **900×420 at 100%** (ориентир, не строгий pixel contract), scalable DPI; минимум декоративных рамок.

```text
┌ MR Reverb                       Hall Natural          ● ON ┐
│ IR FOLDER [ D:/IR/Bricasti M7             ] [Choose…]    │
├ FOLDERS         ┬ IR LIST                 ┬ SELECTED       ┤
│ ▾ Halls         │ Hall Natural   3.2 s    │ Hall Natural   │
│ ▸ Rooms         │ Hall Warm      2.9 s    │ True Stereo    │
│ ▸ Chambers      │ Hall Bright    3.6 s    │ 48 kHz / 3.2 s │
│ Search...       │                          │ Ready           │
├─────────────────┴──────────────────────────┴─────────────────┤
│ PRE-DELAY   0 ms     TONE     0 dB     MIX        25%          │
│ ━━━━━━━━━○━━━━       ━━━━━━━━○━━━━      ━━━○━━━━━━━━━━━━       │
└───────────────────────────────────────────────────────────────┘
```

UI rules:
- Pure flat graphite (#111315/#191D22), subtle dividers, pale typography, cool restrained blue highlight for selection, single warm small accent only where needed; reuse actual project design tokens/skills if they differ.
- Не использовать имитации алюминия, большие глянцевые knobs/неон/лампы, фотореалистичную комнату, десяток подписей/метров и переключателей, которые не реализованы.
- Числа легко читать и редактировать; Ctrl/fine adjustment; правильный keyboard focus, DPI, high-contrast, русские пути/Unicode, Space Play/Stop не захватывается редактором.
- Визуализация waveform, лицензии, избранное и additional tabs — будущие задачи, если реально понадобятся.

## 7. Acceptance / regression checklist

### DSP truth
- Golden/reference convolution по mono/stereo/4-kernel dual-stereo; crossfeed impulse в отдельных каналах, всё смешивание верное и без перепутывания каналов.
- IR pre-delay и 0 dB Tone нейтральны, dry passthrough при Mix=0, wet-only при Mix=100; flat IR и silence сохраняют предсказуемую амплитуду.
- Сравнение output с offline convolution reference на коротких/длинных IR, произвольных callback splits, project rate changes 44.1/48/96 kHz, full-scale, silence, NaN/Inf guards, latency offsets.
- Без щелчка при обновлении Tone/Mix/Pre-delay и IR swap; rapid switch/stop/seek; sample-accurate или явно описанный smoothing.

### Performance & safety
- Замерить CPU per callback and percent deadline/headroom, max spikes, memory и loading duration на реальной Windows x64 ASIO машине. Нагрузки: 1/2/8/16 инстансов, 8–20s long-tail samples, project 44.1/48/96k, с параллельным VSTi; вывод по Live **только после** отдельного gate.
- Проверить no audio-thread allocation, lock, file I/O. Старый Cab IR не регрессирует.
- 100/1000 WAV и вложенные папки индексируются без зависания звука, сломанные/missing/переименованные файлы не крашат.
- Сохранение/открытие проекта, пресета, Undo/Redo, automation, missing IR, track/bus/Master, DPI/editor lifetime, offline export tail; реальные long IR из user library тестировать без упаковки в бинарник.

### Listening / signoff
- Сравнить идентичные **пользовательские** IR в эталонном convolution plugin и MR Reverb с matched gains/rates; импульсные удары, голос/фортепиано, широкая стереокартина, mono fold-down, длинный хвост. Точность относительно аппаратного Bricasti **не утверждать** без измерений.
- User acceptance после локальной сборки и прослушивания. Успех CTest сам по себе не закрывает #128.

## 8. Приоритет, зависимости, команда рабочему чату

Перед первым кодом прочесть [AGENTS.md](../AGENTS.md), [PROJECT_CONTEXT.md](PROJECT_CONTEXT.md), [LOCAL_BUILD_POLICY.md](LOCAL_BUILD_POLICY.md), [PROCESSING_RELIABILITY.md](PROCESSING_RELIABILITY.md), [ENGINE_PERFORMANCE_PLAN.md](ENGINE_PERFORMANCE_PLAN.md), [NATIVE_INSERTS.md](NATIVE_INSERTS.md), [CAB_IR.md](CAB_IR.md), актуальные issues #128–#132. Проверить локальную ветку/status без потери чужих изменений. Применять `mr-feature-implementation`, `mr-realtime-audio-safety`, `mr-studio-ui-design-system`, `mr-studio-build-test` skills из local project.

1. **#129**: аудит DSP и проверяемый True Stereo long-tail engine. Начать с ядра и эталонных тестов.
2. **#130**: user folder, indexed IR browser / dual-stereo grouping / safe async load.
3. **#131**: компактный GUI и полноценные state/Undo/automation/preset paths.
4. **#132**: производительность, сравнение с reference и user signoff.

Не подменять один крупный этап автоматически всеми четырьмя. Готовность и результаты фиксировать в issue и PROJECT_CONTEXT только после реальных локальных проверок. **Планирование ≠ согласие на немедленную реализацию.** Сборка, source и тесты — в существующем **локальном** MR-Studio checkout, GitHub только документация/issues, документальные коммиты `[skip ci]`. Не запускать CI/Actions, не делать source push/PR/merge без отдельной команды.

## 9. Соседние направления и лицензии

- [#23 Plugins/Native DSP](https://github.com/vladleng/MR-Studio/issues/23) содержит Cab IR, host & framework foundation; MR Reverb — **новый продуктовый native insert**, который переиспользует эти основы, но не отменяет и не расширяет автоматически scope #23.
- [#105 MR Saturator](https://github.com/vladleng/MR-Studio/issues/105) и [#115 Harma Waves](https://github.com/vladleng/MR-Studio/issues/115) — независимые native-plugin tracks.
- Свёртка статичного IR **не моделирует изменение геометрии комнаты при перемещении источников**. Для Spatial см. [ACOUSTIC_SPACE_PROTOTYPE.md](ACOUSTIC_SPACE_PROTOTYPE.md).
- [Samplicity public download page](https://samplicity.com/downloads/): пользователь может отдельно скачать free Bricasti M7 IR, включая quad и dual-stereo WAV. **Доступность бесплатно не равна праву на включение файлов в MR Studio**; до любого распространения запросить разрешение/проверить условия, и не публиковать IR assets в репозитории. Контрольная библиотека при разработке — свои синтетические IR под разрешённой лицензией.
