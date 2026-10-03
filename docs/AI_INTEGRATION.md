# Moon River Studio — AI / ChatGPT Integration

## 1. Цель

Moon River Studio в будущем должен поддерживать AI-слой, который понимает структуру DAW-проекта и может анализировать или изменять проект через строго определённый API.

AI — не часть realtime Audio Engine и не должен влиять на стабильность playback/monitoring ни в production workspaces, ни в Live Mode.

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
- current selection/context пользователя;
- Live Mode context, если он активен: current section/chord/cue/setlist state.

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
get_live_state()

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

Все команды должны проходить через MRS Command/Undo system, а не менять внутренние объекты напрямую.

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

Результат — структурированный набор MIDI events, который MRS валидирует перед применением.

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

Предпочтительно передавать подготовленные features/snapshots или выбранные offline-render fragments, а не неконтролируемый raw realtime stream.

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

- быть обычной MRS command;
- попадать в Undo/Redo;
- иметь diff/preview для сложных операций;
- не выполнять destructive file operations без явного подтверждения;
- не менять audio device/routing во время Live Mode без специальных разрешений;
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

Если AI недоступен, Moon River Studio и Live Mode продолжают работать без деградации audio path.

## 10. Voice control

Позднее AI layer может поддерживать голосовой интерфейс, например:

- "поставь loop на припев";
- "создай marker перед соло";
- "покажи только guitar tracks";
- "сделай копию этой MIDI-партии на октаву выше".

Такие команды также преобразуются в структурированные MRS commands.

В Live Mode набор разрешённых голосовых действий должен быть особенно ограниченным и предсказуемым.

## 11. Live Mode

AI может быть доступен и в Live Mode, но только как надстройка над тем же Project Context API.

Возможные безопасные use cases:

- показать/объяснить upcoming cue;
- найти нужную песню/setlist item;
- прочитать заметку;
- подготовить изменение во время rehearsal;
- диагностировать non-realtime issue.

Критические transport/routing/device действия в performance state требуют отдельной permission/safety policy.

## 12. Архитектурное требование уже сейчас

Даже если AI появится значительно позже, Project Model и Command architecture должны изначально иметь:

- stable object IDs;
- serializable project snapshots;
- query API;
- command API;
- validation;
- Undo/Redo;
- permissions/capabilities;
- понятные semantic names для track/clip/section/chord entities.

Это позволит добавить AI без переписывания DAW Core.
