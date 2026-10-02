# Moon River Live

**Moon River Live** — проект концертного приложения Moon River Studio для живых выступлений, тесно связанного с **Fender Studio Pro**.

Главная идея: использовать Studio Pro как среду подготовки песни, а Moon River Live — как специализированную среду исполнения. Приложение должно получать из проекта Studio Pro музыкальные и структурные метаданные, превращать их в устойчивую универсальную модель песни и использовать её для live-навигации, playback, MIDI-автоматизации, патчей и удалённого управления.

> Статус проекта: **проектирование / Stage 0**. Код live-движка ещё не начат.

## Основная концепция

```text
Fender Studio Pro
       │
       │ Moon River Bridge / Export
       ▼
┌──────────────────────────────┐
│      Song Metadata Model     │
│                              │
│ tempo / tempo map            │
│ time signatures              │
│ chord track                  │
│ arranger track               │
│ markers                      │
│ tracks / folders             │
│ clips / events               │
│ custom Moon River metadata   │
└──────────────┬───────────────┘
               │
               ▼
        Moon River Live
```

Studio Pro остаётся источником музыкальной структуры. Moon River Live не должен зависеть от внутреннего формата Studio Pro сильнее, чем это необходимо.

Основной путь интеграции:

```text
Studio Pro → Bridge / Extension → универсальные metadata → Moon River Live
```

Прямой разбор `.song` может появиться как дополнительный/fallback-механизм, но не должен быть единственным фундаментом проекта.

## Что планируется импортировать из Studio Pro

- название и общие данные песни;
- tempo и tempo map;
- размер и изменения размера;
- Chord Track;
- Arranger Track;
- названия, границы и цвета секций;
- markers;
- tracks и folders;
- clips / events и их расположение;
- пользовательские метаданные Moon River Studio;
- в дальнейшем — данные, связанные с live-патчами, MIDI и автоматизацией.

## Что должен уметь Moon River Live

### Live View

Во время выступления приложение показывает музыканту только действительно нужную информацию:

- текущий и следующий аккорд;
- текущую и следующую секцию;
- timeline песни;
- Arranger Sections;
- markers / cues;
- текущий bar / beat;
- tempo и размер;
- заметки для секции;
- live-патчи и состояния оборудования.

### Playback

Дальнейшие этапы предусматривают:

- stereo backing track;
- multitrack stems;
- отдельные click и cue outputs;
- переход к секции;
- loop section;
- stop at end;
- next / previous song;
- безопасное переключение песен в setlist.

### Live control

В перспективе:

- MIDI Program Change / Control Change;
- автоматическое переключение гитарных/клавишных патчей;
- VST3/live inputs;
- управление внешним оборудованием;
- foot controller;
- remote-интерфейс для телефона или планшета.

## Формат песни

Проект предусматривает собственный переносимый пакет, рабочее название:

```text
My Song.moonlive
```

Концептуально пакет может содержать:

```text
My Song.moonlive
├── manifest.json
├── metadata.json
├── audio/
│   ├── backing.wav
│   ├── drums.wav
│   ├── bass.wav
│   └── ...
├── click/
│   ├── click.wav
│   └── cue.wav
├── midi/
└── assets/
```

Формат должен быть версионируемым и не зависеть от UI приложения.

## Архитектурные принципы

1. **Studio Pro — редактор, Moon River Live — исполнитель.**
2. **Metadata-first:** музыкальная структура отделена от UI и audio engine.
3. **Не привязывать всё приложение к закрытому внутреннему API Studio Pro.**
4. **Live-first reliability:** на сцене важнее предсказуемость, чем количество функций.
5. **Fail-safe playback:** ошибка визуального или metadata-модуля не должна обрывать звук.
6. **Offline-first:** концерт не должен зависеть от облака или интернета.
7. **Расширяемая модель:** один и тот же Song Metadata слой сможет использоваться Moon River Live, Arranger Manager и будущими инструментами Moon River Studio.

## Документация

- [`docs/PROJECT_VISION.md`](docs/PROJECT_VISION.md) — цели, границы и концепция продукта.
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — архитектура приложения и границы модулей.
- [`docs/DATA_MODEL.md`](docs/DATA_MODEL.md) — универсальная модель Song Metadata и `.moonlive`.
- [`docs/STUDIO_PRO_INTEGRATION.md`](docs/STUDIO_PRO_INTEGRATION.md) — стратегия интеграции с Fender Studio Pro.
- [`docs/LIVE_WORKFLOW.md`](docs/LIVE_WORKFLOW.md) — предполагаемый концертный workflow.
- [`docs/ROADMAP.md`](docs/ROADMAP.md) — этапы разработки от прототипа до стабильного релиза.

## Roadmap в одном экране

| Stage | Цель |
|---|---|
| 0 | Foundation: архитектура, формат и технический прототип |
| 1 | Studio Pro Bridge и импорт metadata |
| 2 | Live Timeline: chords, arranger, markers |
| 3 | Playback и live-навигация |
| 4 | Multitrack stems, click и cue |
| 5 | Live inputs, VST3 и patches |
| 6 | MIDI automation и hardware control |
| 7 | Setlists и Show workflow |
| 8 | Remote / mobile companion |
| 9 | Reliability, recovery и release |

Подробные критерии каждого этапа находятся в [`docs/ROADMAP.md`](docs/ROADMAP.md) и GitHub Issues.

## Связь с экосистемой Moon River Studio

Moon River Live задуман не как изолированный плеер, а как один из клиентов общей музыкальной модели:

```text
                  Fender Studio Pro
                         │
                         ▼
                 Moon River Bridge
                         │
          ┌──────────────┼──────────────┐
          ▼              ▼              ▼
   Arranger Manager  Moon River Live  future tools
```

Таким образом, получение Chord Track, Arranger Track, markers, tempo, tracks и clips решается один раз и затем используется несколькими приложениями.

## Лицензия

Лицензия проекта пока не определена.
