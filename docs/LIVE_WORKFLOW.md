# Moon River Live — Live Workflow

## 1. Подготовка песни

Рабочая схема:

1. Песня редактируется в Fender Studio Pro.
2. В проекте оформляются:
   - tempo;
   - Chord Track;
   - Arranger Track;
   - markers;
   - tracks/folders;
   - playback assets.
3. Пользователь запускает Moon River Bridge.
4. Bridge экспортирует/обновляет `.moonlive` package.
5. Moon River Live открывает пакет и проверяет целостность данных.

## 2. Подготовка к выступлению

Перед концертом пользователь:

- собирает setlist;
- проверяет audio outputs;
- проверяет MIDI devices;
- проверяет click/cue routing;
- выполняет preload песен;
- запускает preflight check.

Preflight должен сообщать о проблемах до начала выступления:

```text
✓ 14 songs loaded
✓ Audio device connected
✓ Guitar MIDI connected
✓ Click output available
! Song 07: cue.wav missing
```

## 3. Экран песни

Минимальный live-view:

```text
┌───────────────────────────────────────────────┐
│ MOON RIVER                       112 BPM  4/4 │
├───────────────────────────────────────────────┤
│                                               │
│                    Gmaj7                      │
│                                               │
│                  NEXT: Em7                    │
│                                               │
├───────────────────────────────────────────────┤
│ INTRO │ VERSE │ VERSE │ SOLO │ CHORUS │ OUTRO│
│                         ▲                     │
├───────────────────────────────────────────────┤
│ BAR 33 / 64                 02:13 / 04:21     │
├───────────────────────────────────────────────┤
│ Current: SOLO                                  │
│ Next: CHORUS                                   │
│ Cue: Женя вступает                             │
│ Guitar: Lead                                   │
└───────────────────────────────────────────────┘
```

Интерфейс должен считываться с расстояния и не требовать мелких точных действий мышью.

## 4. Основные live-команды

Первая версия:

- Play;
- Pause;
- Stop;
- Previous Song;
- Next Song;
- Jump to Section;
- Restart Song.

Позже:

- Loop Section;
- Stop at End;
- Continue to Next;
- Skip Section;
- Rehearsal loop;
- emergency fade out.

## 5. Поведение секций

Arranger Section является основной единицей музыкальной навигации.

Каждая секция может иметь live-настройки:

```text
Section: SOLO
├── guitar patch: Lead
├── keys patch: Rhodes
├── note: 8 bars
├── midi actions
├── loop policy
└── stop/continue policy
```

## 6. Playback modes

### Stereo mode

Самый ранний и надёжный режим:

```text
backing.wav → Main Out
```

### Stems mode

```text
Drums ───────┐
Bass ────────┤→ Main Out
Percussion ──┤
Back Vocals ─┘

Click → Phones / IEM
Cue   → Phones / IEM
```

## 7. Song transition

При завершении песни приложение заранее подготавливает следующую.

Желаемый сценарий:

```text
Song A playing
      ↓
Song B preload
      ↓
Song A ends
      ↓
ready state / optional auto-continue
```

Загрузка следующей песни не должна вызывать dropout текущего playback.

## 8. Foot control

Будущий pedal workflow:

```text
Button 1 → Play / Pause
Button 2 → Next Section
Button 3 → Previous Section
Button 4 → Next Song
Long press → Stop
```

Mapping должен быть настраиваемым.

## 9. Rehearsal Mode

Отдельный режим, где допустимы действия, опасные на концерте:

- свободный seek;
- loop section;
- repeated section;
- выбор отдельных stems;
- solo/mute;
- быстрое редактирование заметок.

В Performance Mode интерфейс должен быть строже и защищён от случайных действий.

## 10. Performance Mode

Принципы:

- крупные элементы;
- минимум диалогов;
- подтверждение опасных действий;
- отсутствие фоновых обновлений;
- отсутствие обязательного интернета;
- сохранение last known show state;
- предсказуемое поведение transport.

## 11. Recovery

После перезапуска приложение должно предложить:

```text
Resume last show?
Setlist: Kitchen Lab
Song: Moon River
Last position: bar 33
```

Самовосстановление playback позиции во время реального концерта должно быть отдельной, осторожно реализованной функцией. На раннем этапе достаточно восстановить выбранную песню и показать последнюю позицию.

## 12. Remote workflow

Телефон/планшет на первом этапе remote работает как дополнительный монитор:

```text
CURRENT: Gmaj7
NEXT: Em7
SECTION: Solo
NEXT SECTION: Chorus
BAR: 33
```

Управление transport с remote добавляется только после того, как desktop transport станет стабильным.

## 13. Концертный критерий качества

Любая новая функция считается готовой только после проверки минимум в трёх сценариях:

1. обычная репетиция;
2. длинный непрерывный playback;
3. стресс-тест с переходами между песнями и секциями.
