# 0.2h upd1 — копирование клипов

2026-10-08. Пользователь запросил update после сообщения «Пока вылетов нет».
Это не утверждение об устранении всех возможных plugin/native crashes и не
автоматическая приёмка всего 0.2h. Внешний MIDI / 0.2g здесь не начинается.

## Контракт

- Общая кнопка Duplicate копирует активный аудио- или MIDI-клип на ту же дорожку.
  Поиск идёт вперёд от первого полного такта после конца оригинала. Занятый
  такт пропускается; вся длина копии также должна быть свободна. Другие дорожки
  не блокируют размещение. Смещение оригинала внутри такта сохраняется в ticks;
  аудио сохраняет также остаток позиции меньше одного tick в samples.
- Meter/tempo map учитываются Timeline. Если смещение не помещается в более
  короткий такт, поиск продолжится с подходящего meter segment либо безопасно
  отклонит операцию. Переполнение диапазона отклоняется без частичной правки.
- Alt, удерживаемый при захвате клипа, включает копирование вместо перемещения
  или edge trim. Оригинал остаётся на месте, копия следует за мышью до отпускания
  ЛКМ. Белая рамка и `[Copy]` отличают transient preview. Snap использует обычную
  сетку Arrange; Snap off позволяет свободную sample-позицию для аудио.
- Выделенную группу можно Alt-копировать одной командой с общим смещением и
  сдвигом дорожек. MIDI допускается только на instrument track. Неверное место
  отклоняет всю команду. Alt-click без перемещения, Escape и потеря фокуса
  не создают клипов и не добавляют запись Undo.
- Копия становится выделенной. Один Undo отменяет Duplicate или всю Alt-группу;
  Redo восстанавливает те же clip/note/event IDs. Новые MIDI note/event IDs
  независимы от оригинала; содержимое, channel, source offset, gain, имя и
  source reference сохраняются. WAV не дублируется физически.
- Правки доступны при Stop/Pause, не при Play/Record (существующий edit contract).

## Слои и границы

Core DuplicateClip отвечает за свободный такт и клонирование данных. Application
готовит и проверяет candidate целиком, затем выполняет одну undoable команду.
UI владеет только drag preview; он не persisted и не обрабатывается движком.
Committed clips входят в существующий playback rebuild и обычное сохранение.
Project schema 13 / Preferences v7 не меняются; round-trip и старые форматы
используют прежнюю сериализацию. Общая модель одна для Arrange/Edit/Mix/Live;
не добавляется отдельный Live UI или новый формат.

Новые allocations, поиск тактов и клонирование идут на message/control thread.
Application::edit останавливает обработчики перед playback replacement и
сохраняет существующие plugin instances (`retain_processors=true`), как в
принятом fix1. В callback/DSP/worker ownership/PDC/raw recording изменений нет.
Fix3 bounded snapshot reads и fix2 crash logging сохранены.

## Проверка

Model tests: off-grid MIDI, занятый такт, tempo/meter change, свежие note/event
IDs, Undo/Redo и serialization. Application audio tests: off-grid Duplicate,
mixed audio/MIDI group, invalid destination/negative range/duplicate IDs rollback.
JUCE J3: Duplicate для обоих типов, Alt preview без revision changes, release,
Escape, no-motion, stable Redo IDs; software snapshots 100/150%.

Итоговые build/CTest/package результаты находятся в PROJECT_CONTEXT.md.
Реальная ASIO/визуальная пользовательская приёмка update требуется отдельно.
