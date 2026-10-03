# Moon River Studio — AI / ChatGPT Integration

## 1. Цель

Moon River Studio должен в будущем поддерживать AI-слой, который понимает структуру DAW-проекта и может анализировать или изменять проект через строго определённый API.

AI — не часть realtime audio engine и не должен влиять на стабильность playback/monitoring.

## 2. Архитектура

```text
ChatGPT / OpenAI API
        |
        v
   AI Controller
        |
        v
 DAW Context + Tool API
        |
   +----+-----------------------------+
   |              |          |        |
Project Model   MIDI Model  Mixer   Analysis
```

Главный принцип:

> AI работает со структурированным состоянием проекта, а не управляет интерфейсом мышью и не пытается распознавать DAW по скриншотам.

## 3. Что AI должен уметь читать

Через Project Context API:

- project metadata;
- tracks/folders;
- audio/MIDI clips;
- MIDI notes, velocity, duration, CC;
- Chord Track;
- Arranger Track;
- tempo/meter map;
- markers;
- mixer state;
- routing;
- plugin chains;
- plugin parameters, когда это разрешено;
- automation;
- basic audio-analysis summaries;
- current selection/context пользователя.

## 4. Пример Tool API

```text
get_project_summary()
get_tracks()
get_track(trackId)
get_clip(clipId)
get_midi_clip(clipId)
get_chord_track()
get_arranger_sections()
get_mixer_state()

create_midi_clip(...)
insert_midi_notes(...)
move_notes(...)
quantize(...)
transpose(...)

set_track_gain(...)
set_pan(...)
create_bus(...)
insert_plugin(...)
set_plugin_parameter(...)

create_marker(...)
create_section(...)
move_clip(...)
```

Все команды должны проходить через MRS command/undo system, а не менять внутренние объекты напрямую.

## 5. MIDI generation

AI может генерировать собственные MIDI-партии на основе контекста проекта.

Примеры:

- drums для выбранной секции;
- bass line по Chord Track;
- piano voicing/comping;
- strings;
- fills;
- reharmonized MIDI draft;
- вариация существующего MIDI clip.

Контекст генерации может включать:

```text
Tempo
Meter
Key / Chord Track
Arranger Section
Existing Bass
Existing Melody
Playing Range
Style/Profile
Density
Humanization
```

Результат должен быть структурированным набором MIDI events, который MRS валидирует перед применением.

## 6. Анализ аранжировки и микса

В будущем AI может анализировать:

- register overlap;
- arrangement density;
- MIDI note distribution;
- duplicated voices;
- track activity by section;
- mixer levels;
- plugin chains;
- spectral/statistical summaries, подготовленные самой DAW.

AI не должен получать бесконтрольный raw realtime audio stream без необходимости. Лучше передавать подготовленные features/snapshots или выбранные offline-render fragments.

## 7. Уровни разрешений

### READ

AI только читает контекст и отвечает.

### SUGGEST

AI предлагает изменения, но ничего не применяет.

### EDIT

AI формирует команды, которые пользователь подтверждает.

### AUTO

Разрешённый набор безопасных операций может выполняться автоматически.

Рекомендуемый старт: **READ + SUGGEST**.

## 8. Safety / Undo

Любое AI-изменение проекта должно:

- быть представлено как обычная MRS command;
- попадать в Undo/Redo;
- иметь diff/preview для сложных операций;
- не выполнять destructive file operations без явного подтверждения;
- не менять audio device/routing во время performance без специальных разрешений;
- не влиять на realtime thread напрямую.

## 9. Realtime isolation

Строго запрещено:

```text
ASIO callback -> network/OpenAI request
ASIO callback -> JSON parsing
ASIO callback -> AI inference
```

Правильная схема:

```text
Audio Thread
     |
 atomic / lock-free state
     |
Application Core
     |
AI Worker / Network
```

Если AI недоступен, MRS и MRL должны продолжать работать без деградации audio path.

## 10. Voice control

Позднее AI layer может поддерживать голосовой интерфейс, например:

- "поставь loop на припев";
- "создай marker перед соло";
- "покажи только guitar tracks";
- "сделай копию этой MIDI-партии на октаву выше".

Такие команды также должны преобразовываться в структурированные MRS commands.

## 11. Архитектурное требование уже сейчас

Даже если AI появится значительно позже, Project Model и command architecture должны изначально иметь:

- stable object IDs;
- serializable project snapshots;
- query API;
- command API;
- validation;
- Undo/Redo;
- permissions/capabilities;
- понятные semantic names для track/clip/section/chord entities.

Это позволит добавить AI без переписывания DAW core.
