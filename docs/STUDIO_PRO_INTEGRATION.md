# Moon River Live — Fender Studio Pro Integration

## 1. Цель интеграции

Получать из Fender Studio Pro музыкальную структуру проекта и переносить её в Moon River Live без ручного дублирования данных.

Минимальный целевой набор:

- song title / project identity;
- tempo / tempo map;
- time signatures;
- Chord Track;
- Arranger Track;
- markers;
- tracks / folders;
- clips / events;
- source metadata для повторной синхронизации.

## 2. Основная стратегия

Предпочтительная архитектура:

```text
Studio Pro
   ↓
Moon River Bridge / Extension
   ↓
Moon River Song Metadata
   ↓
.moonlive package
   ↓
Moon River Live
```

Bridge является адаптером. Live-приложение не должно знать, каким именно способом Bridge добыл данные.

## 3. Почему не стоит строить всё на прямом доступе к Studio Pro

Возможные способы интеграции могут зависеть от внутренних/неполностью документированных механизмов Studio Pro.

Поэтому необходимо:

- минимизировать область зависимости;
- держать Studio Pro-specific код внутри `bridge/`;
- покрывать adapter интеграционными тестами;
- версионировать экспортированный контракт;
- иметь fallback-путь.

Если Studio Pro изменит внутренний API, должен ломаться Bridge, а не весь Moon River Live.

## 4. Режимы интеграции

### 4.1 Export

Пользователь вручную запускает:

```text
Export to Moon River Live
```

Bridge создаёт/обновляет `.moonlive`.

Это первый целевой вариант, потому что он максимально предсказуем.

### 4.2 Sync

В будущем:

```text
Sync with Moon River Live
```

Обновляются source-owned данные, но сохраняются live-owned настройки.

### 4.3 Direct open / parser

Возможный дополнительный режим:

```text
Open Studio Pro project
```

Moon River Live или отдельный importer читает `.song` напрямую.

Этот путь считается fallback/advanced до тех пор, пока не доказана стабильность формата.

## 5. Матрица данных

| Тип данных | Stage 1 цель | Комментарий |
|---|---:|---|
| Project identity | Да | Нужен стабильный source link |
| Tempo | Да | Базовый BPM обязателен |
| Tempo map | Да/если доступно | Поддержать модель сразу |
| Meter | Да | 4/4 не должен быть предположением |
| Meter map | Да/если доступно | Для сложных песен |
| Chord Track | Да | Ключевая live-функция |
| Arranger Track | Да | Ключевая навигация |
| Section colors | Да | Полезны для быстрого считывания |
| Markers | Да | Cues и будущая автоматизация |
| Tracks | Да | Для контекста и stems mapping |
| Folders | Да | Сохранять иерархию |
| Clips/events | Да | Для будущих интеграций |
| Plugin state | Нет, позднее | Не блокирует MVP |
| Mixer routing | Нет, позднее | Будущий advanced import |

## 6. Повторная синхронизация

Главная проблема не первый импорт, а второй.

Пример:

1. Пользователь экспортировал песню.
2. В Moon River Live назначил MIDI patch на `Solo`.
3. В Studio Pro изменил длину Verse и передвинул Solo.
4. Повторно синхронизировал.

Ожидаемое поведение:

- границы `Solo` обновились;
- назначенный live patch остался привязан к той же секции;
- другие live-owned настройки не потерялись;
- удалённые source-объекты помечены корректно;
- новые source-объекты добавлены без дублей.

Для этого нужны стабильные IDs и явная ownership-модель.

## 7. Конфликты

На раннем этапе source-owned данные всегда выигрывают для полей структуры.

Например, имя и границы Arranger Section приходят из Studio Pro.

Moon River Live хранит рядом дополнительные данные:

```text
Studio Pro section
├── name
├── start/end
├── color
└── live extension
    ├── notes
    ├── patch
    └── actions
```

Если пользователь хочет изменить музыкальную структуру, он делает это в Studio Pro и синхронизирует снова.

## 8. Первый технический spike

До разработки полноценного Bridge необходимо подтвердить реальную доступность каждого типа данных.

### Spike checklist

- [ ] Получить project/song identity.
- [ ] Получить текущий tempo.
- [ ] Получить tempo events/map.
- [ ] Получить time signature.
- [ ] Получить Chord Track events.
- [ ] Получить Arranger Track sections.
- [ ] Получить section name/start/end/color.
- [ ] Получить markers.
- [ ] Получить tracks.
- [ ] Получить folder hierarchy.
- [ ] Получить clips/events.
- [ ] Проверить наличие стабильных source IDs.
- [ ] Проверить поведение после редактирования и повторного чтения.

Результаты spike должны быть задокументированы таблицей `supported / workaround / unavailable`.

## 9. Graceful degradation

Если какой-либо тип metadata недоступен, экспорт всей песни не должен падать.

Например:

```text
Chord Track: available
Arranger Track: available
Markers: available
Tempo map: unavailable → exported fixed tempo
Clips: unavailable → skipped with warning
```

Bridge должен возвращать отчёт об экспорте.

## 10. Диагностика

Каждый экспорт должен иметь debug report:

- версия Studio Pro;
- версия Bridge;
- schema version;
- количество sections;
- количество chord events;
- количество markers;
- количество tracks/clips;
- warnings;
- unsupported fields.

Это критично для поддержки разных версий Studio Pro.

## 11. Безопасность

Bridge не должен модифицировать Studio Pro project без явного действия пользователя.

Первая версия интеграции должна быть read-only относительно проекта Studio Pro.

Любые будущие write-back функции требуют отдельного этапа и отдельного подтверждения архитектуры.
