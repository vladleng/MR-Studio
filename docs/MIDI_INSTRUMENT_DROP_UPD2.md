# 0.3d upd2 — VSTi и MIDI drag/drop (#149)

Запрос 2026-10-12, после приёмки #148: «Хорошо, пока оставим так. Принимаю».
Пакет upd1 и его хеши сохранены, #148 закрыт completed. Работа локально.

## Контракт

- VSTi в свободной области ниже дорожек Arrange (включая левую колонку) или
  свободном поле Mix создаёт instrument track с выбранным инструментом.
- VSTi на существующем instrument track, в том числе его заголовке Arrange:
  пустой слот заполняется; занятый — диалог Replace / Do not touch. Replace
  меняет первый processor, сохраняя следующие эффекты, клипы и маршруты.
  Ответ на устаревший диалог после смены проекта/revision игнорируется.
- Files показывает WAV, MID, MIDI. MIDI также принимается из Windows Explorer.
  Смешанный WAV/MIDI batch не принимается; импортируйте типы отдельно.
- SMF 0/1 с PPQ; часть — пара source track/channel, включая channel events.
  Track name и номер канала отражаются в именах. Conductor-only tracks пропускаются.
  Первый part занимает выбранную instrument track; остальные получают новые
  instrument tracks. При сбросе в пустую область или на audio/bus создаются новые.
  Назначения инструмента на новых MIDI-дорожках нет: выберите его отдельно.
- Все части начинают в одной позиции drop; паузы внутри MIDI сохраняются.
  PPQ переводится в 960, ноты остаются минимум 1 tick. Незакрытые ноты
  закрываются на EOT; note-off без note-on игнорируется. Tempo/meter metadata
  не меняют темп проекта. SysEx пропускается, SMF2/SMPTE не поддерживаются.
- Пока кнопка мыши удерживается, MIDI shadow показывает длительность и дорожки
  по текущему project tempo/snap; при выходе/drop тень очищается. На левом
  заголовке позиция 0. Ошибочный MIDI не имеет тени и отклоняется при drop.
- Импорт всего batch — одна запись Undo, данные встроены в проект, исходный
  MIDI не требуется при повторном открытии. Replace также одна запись Undo.

## Слои и безопасность

UI/commands/model/Undo/tests/docs затронуты. Shared Core содержит bounded
SMF parser: 16 MiB, 128 source tracks/parts, 200000 raw events; существующие
project limits notes/events/voices проверяются до commit. IO/parse выполняются
на control/UI worker, никогда в audio callback. UI shadow временный.
Сериализация использует имеющиеся MidiClip/NativeInsert, schema19/archive1.
Engine callback/renderer не изменены; plugin replacement через существующий
set_inserts stop/quiesce/staged graph, create через принятую instrument factory.
Cache VST3 v2 хранит optional instrument category; v1 читается, выбранный
старый entry разрешает metadata только при drop. Новые версии scan helper
поставляются вместе с host. Проектные данные общие для Arrange/Edit/Live.

## Проверки и приёмка

Release PASS; full CTest **127/127 PASS (150.96s)**. Финальные GUI6/6 PASS
после добавления browser selection/mixer/left blank regressions. Проверены SMF0/1,
running status, перекрывающиеся ноты, CC/program/pressure/bend, SysEx skip,
conductor skip, corruption/format2/SMPTE rejection, batch atomicity, Undo/Redo,
Save/Open, device restart failure rollback, unchanged insert generation,
cachev1 read/v2 roundtrip, Replace/cancel с сохранением effects и single Undo.
GUI проверяет selection .mid в реальном Files tree, MIDI ghost/drop/exit,
reuse instrument, browser→Mix MIDI и VSTi в blank Arrange/Mix/left header.
Логи `build/asio-local/validation/midi-drop149`. Packaged smoke/preview далее.
MIDI import prepares graph before ProjectStore commit, retains existing plugin
instances, panics held notes only after stop/quiesce; device failure restores old
graph/model/history. Callback files unchanged. READY WITH MANUAL CHECK пока
packaged checks не завершены; hardware ASIO/physical pointer/DPI NOT RUN.
Ручная приёмка: реальный VSTi из браузера → пустая область/занятый инструмент,
Replace/cancel; .mid/.midi mono/multi → свободная область/инструмент, тень,
проигрывание назначенным VSTi, Undo/Redo и повторное открытие.

Parser reference: [MIDI Association — Standard MIDI Files](https://midi.org/standard-midi-files).

Delivered source `f45546676a69335d500041ed8aeda161ad6b24fc`.
Release/full CTest127/127 PASS150.96s; final GUI6/6 PASS41.49s; packaged J3
PASS exit0, fresh MIDI ghost/Files100/150 previews inspected:
`build/asio-local/validation/midi-drop149/packaged-smoke`.
Compact13 files/12 hashes rechecked:
`Builds/MR-Studio-0.3d-upd2-MIDI-Instrument-Drop-JUCE-ASIO-Windows-local`.
GUI SHA256 `1F0C509A69C5057444A82661F3FDC0C05CD89E5DA7CB345B987B94478DA0ECA5`.
Symbols `build/asio-local/deliveries/0.3d-upd2-midi-instrument-drop`.
Persistent desktop MR Studio.lnk now points to upd2; MR icon preserved and target
verified. Older packages untouched. FEATURE READY WITH MANUAL CHECK / RT SAFE
WITH MANUAL CHECK; #149 user audition acceptance pending. No code push/merge/Actions.

## Follow-up upd3: приёмка выявила два сбоя

upd2 не принята. MIDI SMF0/PPQ960 содержит 7605 notes/1989 CC на channel0;
прежние4096 notes/clip/playback ограничивали валидный длинный файл, ошибочно
сообщалось unsupported. Общий project budget32768 сохраняется, каждая clip и
playback track теперь может использовать его полностью. Deserialize также
поддерживает32768; schema19 остаётся, старые приложения ограничивают4096.
Recording-specific4096 budget не меняется. Callback алгоритм interval tree и
буферы активных128/event256 остаются прежними; увеличение только подготовленных
non-RT note/event/chase vectors. Проверка chase на максимальном budget обязательна.

JUCE DragAndDropContainer::findTarget вызывает interest до присвоения local cursor.
Interest теперь проверяет payload, location проверяется при itemDropped;
ruler/occupied audio/bus VSTi drop остаются защищены. В тестах повторён JUCE walk
component→parent с SourceDetails position0, затем передаются actual drop coords.
Build/test результатов пока нет; packageupd2/его хеши не переписываются.

Release build прошла после исправления имён переменных в тесте (MSVC/WX).
`mrs_midi_import_tests.exe <user MIDI>`: PASS, parts1/notes7605/events1989.
Реальный файл импортирован, сохранён/открыт во временном проекте и обработан
ManualDevice; исходный SHA256 до/после одинаковый:
`E00FCE4B0AEB6EBDD0B26030EF2CA57863410412230854CFC6CF675FCBC0258C`.
Синтетические7605-note import/Undo/Redo/serialize, maximum32768-note parser,
32769-note rejection и chase последней ноты на максимальном budget прошли.
Hardware audition не подменяется ManualDevice. Full127 и package далее.

Первый full run upd3:125/127, два GUI failures в новом discovery regression.
Причина fixture: standalone Desktop был invisible, Component::getComponentAt
возвращал nullptr ещё до target walk. Test Desktop теперь visible без hardware
peer; это соответствует DAW child внутри окна. Исправление продукта остаётся
payload-only interest + location validation at drop. Новый full run далее.

Финальный Release/full CTest **127/127 PASS (146.99s)**, включая JUCE
discovery/drop/Replace/MIDI shadows, RT/MIDI/legacy persistence. Packaged checks
следом; generic VST3 fixture, без повторения принятых installed TH-U проверок.

Delivered **0.3d upd3**, source `1c0aba5ca1c01647658794b62481044674a669ba`.
Release/full CTest127/127 PASS146.99s; real user MIDI import/SaveOpen/ManualDevice
PASS notes7605/events1989; packaged J3 PASSexit0. Fresh100/150 previews generated,
100% inspected (visual layout unchanged). Compact13 files/12 hashes rechecked:
`Builds/MR-Studio-0.3d-upd3-MIDI-Drop-Fix-JUCE-ASIO-Windows-local`.
GUI SHA256 `A512E3B41E89B874A54CBAB3DECEB0560011BBE077B914BC4CA824BF14BCCE4C`.
Logs `build/asio-local/validation/midi-drop149-fix`, symbols
`build/asio-local/deliveries/0.3d-upd3-midi-drop-fix`. Persistent MR shortcut
updated, icon retained/target verified. Original MIDI hash and old packages unchanged.
FEATURE READY WITH MANUAL CHECK / RT SAFE WITH MANUAL CHECK: user real pointer
VSTi/MIDI drag/audition acceptance #149 pending; hardware ASIO/physical DPI NOT RUN.
No push/merge/Actions. Following musical issues not started.

2026-10-12: пользователь «Да, все ок, принимаю. Что у нас дальше?»
#149 closed completed, FEATURE ACCEPTED на0.3d upd3. Артефакты/manifest
не переписывались; source1c0aba5 и ранее пройденные127/127/packaged checks
остаются доказательствами. Следующий рекомендованный slice #80/0.3e не начат.
