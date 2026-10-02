# Moon River Live — UI / UX Concept

## 1. Цель интерфейса

Moon River Live — это live-performance приложение. Интерфейс должен в первую очередь помогать выступлению: быстро считываться на экране ноутбука, не перегружать музыканта и держать все критически важные данные и органы управления на виду.

Ключевой принцип: **performance-first UI**.

Главные требования:

- высокая читаемость на 13–16" ноутбуках;
- крупная и ясная визуальная иерархия;
- current/next информация всегда на виду;
- основные live-controls доступны без поиска по меню;
- edit-режим и perform-режим чётко разведены;
- безопасное поведение: случайный клик не должен ломать концертный сценарий;
- современный, аккуратный и узнаваемый визуальный стиль;
- интерфейс не должен ощущаться как перегруженная DAW.

---

## 2. Основные режимы

### 2.1. Show Mode

Главный режим для концерта.

На виду должны быть:

- текущая песня;
- current chord;
- next chord;
- current section;
- next section;
- bar / beat;
- elapsed / remaining time;
- upcoming cue / marker;
- playback state;
- transport;
- next song.

Это основной режим во время выступления.

### 2.2. Song Mode

Работа с одной песней:

- timeline;
- song notes;
- section notes;
- playback settings;
- patches;
- MIDI actions;
- song-level settings.

### 2.3. Edit Mode

Подготовка песни:

- trim start / trim end;
- playback gain;
- preview;
- marker inspection;
- section behavior;
- playback-edit controls.

### 2.4. Setup Mode

Технический режим:

- audio devices;
- MIDI devices;
- routing;
- VST3;
- diagnostics;
- remote.

---

## 3. Приоритет информации

### Уровень 1 — критически важное

- **Current Chord**
- **Next Chord**
- **Current Section**
- **Bar / Beat**
- **Playback State**

### Уровень 2 — очень важное

- Song Title
- Elapsed / Remaining Time
- Upcoming Cue / Marker
- Next Section
- Next Song

### Уровень 3 — вспомогательное

- BPM / Meter
- Patch Status
- MIDI Status
- Audio Device Status
- Routing Indicators

### Уровень 4 — служебное

- project/source information;
- schema/package version;
- logs;
- diagnostics;
- detailed metadata.

---

## 4. Предлагаемая структура главного окна

```text
┌────────────────────────────────────────────────────────────────────┐
│ Top Bar                                                           │
│ Song | Setlist | Tempo | Meter | Audio | MIDI | Remote | Settings │
├───────────────┬───────────────────────────────────────┬────────────┤
│ Left Rail     │ Main Performance Area                 │ Right Rail │
│               │                                       │            │
│ Setlist       │ Current Chord                         │ Cue /      │
│ Songs         │ Next Chord                            │ Marker     │
│ Notes         │ Current Section                       │ Section    │
│               │ Next Section                          │ Notes      │
│               │ Bar / Beat / Time                     │ Patch /    │
│               │                                       │ Actions    │
├───────────────┴───────────────────────────────────────┴────────────┤
│ Bottom Timeline / Playback Bar                                    │
│ Timeline | Sections | Markers | Trim | Transport | Loop | Volume  │
└────────────────────────────────────────────────────────────────────┘
```

### 4.1. Top Bar

Глобальный контекст и системные статусы:

- название песни;
- название setlist;
- tempo / meter;
- audio status;
- MIDI status;
- remote status;
- sync/save state;
- settings.

### 4.2. Left Rail

Навигация по шоу:

- список песен;
- current / next song;
- быстрый переход между песнями;
- song notes.

В Show Mode панель может быть компактной или сворачиваемой.

### 4.3. Main Performance Area

Главный визуальный фокус приложения.

Здесь должны быть:

- current chord — самый крупный элемент;
- next chord — крупный, но вторичный;
- current section — второй по значимости блок;
- next section;
- bar / beat / time.

Этот блок должен считываться даже быстрым боковым взглядом.

### 4.4. Right Rail

Контекстные live-подсказки:

- upcoming marker;
- cue;
- section notes;
- patch preview;
- MIDI action preview;
- warnings.

### 4.5. Bottom Timeline / Playback Bar

- transport;
- timeline;
- section overview;
- markers;
- loop;
- volume;
- trim handles в Song/Edit Mode;
- progress.

---

## 5. Playback Editing UX

Так как в roadmap уже входит базовое редактирование playback, интерфейс должен поддерживать:

- master playback gain;
- trim start;
- trim end;
- reset;
- preview / audition.

Требования:

- редактирование неразрушающее;
- визуальные trim-handles;
- видны исходные и отредактированные границы;
- numeric fields для точной подстройки;
- в Show Mode trim-инструменты скрыты или сильно упрощены;
- в Edit Mode доступен полный контроль через перетаскивание и точные значения.

---

## 6. Визуальный стиль

### 6.1. Характер

Интерфейс должен быть:

- современным;
- аккуратным;
- сценическим;
- технологичным;
- визуально спокойным;
- не перегруженным как DAW.

Образ: **ночной концертный интерфейс с чистой типографикой и мягкими световыми акцентами**.

### 6.2. Цветовая идея

Базовая тема — тёмная.

База:

- глубокий графит / тёмно-синий фон;
- очень тёмные панели;
- мягкое разделение поверхностей.

Акценты:

- сине-фиолетовый — основной фирменный акцент;
- янтарный — playback / cue;
- зелёный — ready / connected;
- красный — stop / warning / danger.

Важно: цвета секций Arranger Track не должны конфликтовать с цветами системных статусов.

### 6.3. Формы и плотность

- умеренные скругления;
- достаточно воздуха;
- минимум декоративных линий;
- крупные читаемые блоки вместо плотных таблиц;
- второстепенные панели могут сворачиваться.

### 6.4. Типографика

- нейтральный modern sans-serif;
- очень крупный current chord;
- короткие подписи;
- минимум мелкого текста на главном экране.

Иерархия:

- XXL — current chord;
- XL — current section;
- L — next chord / next section;
- M — song title / timer / status;
- S — secondary details.

---

## 7. Ноутбучный сценарий

Основной целевой сценарий:

- 14" Full HD;
- 15–16" Full HD / 2K;
- расстояние до экрана больше, чем при обычной DAW-работе.

Интерфейс должен:

- хорошо масштабироваться;
- работать при 100–125–150% UI scale;
- иметь полноэкранный Performance Mode;
- избегать слишком мелких иконок;
- избегать плотных таблиц в Show Mode.

---

## 8. Поведенческие принципы

### 8.1. Show Mode — read-mostly

Во время концерта интерфейс ориентирован прежде всего на чтение и подтверждённые действия, а не на редактирование.

### 8.2. Edit Mode — explicit editing

Пользователь должен ясно понимать, что находится в режиме редактирования. Edit-controls, save/apply/reset и визуальный акцент должны это подчёркивать.

### 8.3. Минимум модальных окон

Во время live-работы нежелательны модальные окна, перекрывающие основной контент.

Предпочтительно использовать:

- side panels;
- drawers;
- right inspector;
- bottom panels;
- fullscreen-friendly overlays только для действительно опасных действий.

### 8.4. Безопасность

Опасные действия нужно защищать от случайного нажатия:

- stop all;
- next song;
- destructive reset;
- device/routing changes;
- критические MIDI-команды.

Live-кнопки должны быть достаточно крупными, а destructive controls визуально отделены.

---

## 9. Базовые UI-компоненты

Нужно предусмотреть единую библиотеку компонентов:

- top app bar;
- left rail;
- right inspector;
- transport buttons;
- large chord card;
- section badge;
- cue banner;
- timeline strip;
- trim handles;
- status pills;
- device indicators;
- danger buttons;
- collapsible info blocks.

---

## 10. Что нужно зафиксировать до старта реализации UI

UI/UX-дизайн считается частью **Stage 0**.

### 0.1e — UI / UX Concept & Wireframes

- определить основные режимы: Show / Song / Edit / Setup;
- зафиксировать layout главного окна;
- спроектировать Show Mode;
- спроектировать Song/Edit Mode;
- описать playback-edit UX;
- определить design tokens: цвета, типографика, spacing, states;
- определить safe/danger interaction patterns;
- утвердить базовый визуальный стиль Moon River Live.

### Acceptance

- есть согласованный UX-концепт;
- есть базовые wireframes;
- есть набор ключевых UI-компонентов;
- есть зафиксированный визуальный стиль, достаточный для старта реализации интерфейса.

---

## 11. Ключевая формула проекта

Интерфейс Moon River Live должен ощущаться как **live-performance utility / musical confidence monitor**, но с собственной айдентикой Moon River Studio и упором на:

- chords;
- arranger sections;
- live readability;
- performance safety;
- быстрый доступ к ключевым действиям;
- понятность на небольшом экране ноутбука.
