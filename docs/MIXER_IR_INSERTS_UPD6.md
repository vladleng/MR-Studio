# I/R и mixer inserts — 0.3c upd6 / #135

Запрос пользователя 2026-10-11 после принятой upd5/#134. Работа локальная;
source/build/tests/packages/docs не публикуются. Issue135 закрыт completed:
пользователь подтвердил «Все работает, закрывай». Принят существующий upd6,
package/manifest не изменялись. Последующие UI дополнения: #136/0.3c upd7.

## Причина задержки и переходы

`set_track_armed` использовал `rebuild_audio(false)`, audio monitor при смене
ahead/device ownership использовал structural `edit` с capture/recreate,
а MIDI monitor — structural `SetMidiInput`. Поэтому даже переключение флага
могло пересоздавать все plugins и редакторы, загружать state/IR заново.

Audio I и необходимые R/input-domain transitions теперь retain existing
PreparedGraph instances. Device callbacks/producer/workers останавливаются и
присоединяются до изменения ownership/маршрутов; DSP имеет одного владельца.
Существующее требование Pause/Stop при изменении audio ahead/live ownership или
наборе физических входов сохранено. Actual hardware reopen при новом входе
не устраняется; скорость драйвера не заявляется гарантированной. Fresh offline
open может намеренно ещё не иметь runtime: первый R подготавливает его один
раз, затем экземпляры сохраняются. Уже monitored
track при изменении R не требует reprepare. R не устанавливает Recorder до
начала записи; raw capture остаётся до effects/PDC. На quiesced reprepare
удерживаемые ноты retained instruments отпускаются до сброса MIDI generation/
tracking; следующий валидный note-on не теряется из-за отложенного panic.

MIDI input/monitor проходит через command/model, live `MixerUpdate` и existing
MIDI route publication. Выбранные MIDI ports остаются открытыми при I/R; monitor
gate находится в device-owned audio DSP. Выключение I отпускает tracked live
notes/pedals этого канала через preallocated MIDI buffer, без общего panic.
При pathological overflow используется existing per-processor panic и счётчик
dropped. Публикация bounded, queue-full retry по existing revision/poll. Обычная
применяемость — граница callback; отпускание live notes — следующий callback.
Смена самого MIDI port/channel по существующему backend может переоткрывать
WinMM inputs; специальный seamless port-switch не входит в этот запрос.

## Контракт и слои

- Включение R включает I для audio/instrument. Выключение R сохраняет I.
  I можно выключать отдельно на armed track; raw recording от I не зависит.
- R — прежний временный session arm, не сохраняется и не является project Undo.
  Автоматически включённый I — обычный project monitor command, сохраняется,
  поддерживает Undo/Redo; Undo I не снимает R. Schema17 не меняется.
- Audio monitor Undo при смене ownership также retains inserts/editor. JUCE
  заранее классифицирует чистую input/monitor history через Application; её
  Undo/Redo больше не закрывает editor до выполнения команды. Structural history
  по-прежнему закрывает native views до уничтожения processors.
  MIDI port/channel/monitor Undo публикует model state без graph reload.
- Общий Application/Core используется существующими frontend targets; новые
  mixer presentation/drop geometry относятся к JUCE. Live Mode не расширяется.
- RT: новый флаг проходит в fixed MixerUpdate; временные release flags и
  MIDI buffers принадлежат DSP owner. Нет новых host allocations/locks/I/O/UI
  внутри callback, нет второго владельца одного instance.

## Интерфейс

I/R добавлены под M/S в mixer audio/instrument strips, с той же моделью, что
в arrangement. Bus/Master не имеют I/R. Активный R красный `#d94747` в обоих
видах; буква R сохраняет смысл независимо от восприятия цвета. Удалён Inserts,
`+` находится справа в title и открывает существующее insert menu. Название и
его редактор не перекрывают кнопку. Output перенесён ниже нового ряда I/R;
высота fader/meters сохранена. Insert/send viewports используют освободившийся
верхний ряд.

Browser drag подсвечивает insert viewport полупрозрачным accent blue и рисует
линию предполагаемой вставки поверх child controls. Общая геометрия используется
для paint/hit-test/drop. Позиция вычисляется по серединам строк и scroll offset,
viewport auto-scroll позволяет достигнуть скрытых slots. Drag вне insert area
вставляет в конец; существующий instrument first slot защищён, FX вставляется
после него. Empty/Bus/Master используют обычные chain rules, limit8 сохраняется.
Hover/drop временные, в проект не пишутся. Drop deferred до выхода JUCE drag
callback, проверяет owner/project/revision и неизменность browser descriptor, далее existing SetInserts/Undo/state
validation. Structural plugin insertion по существующим Pause/Stop правилам.

## Проверка и поставка

Статус: **READY WITH MANUAL CHECK / RT SAFE WITH MANUAL CHECK**.
Local Release PASS; final full CTest **126/126 PASS, 137.73 s**.
Fixture регрессии проверяют engine/allocation/MIDI monitor, note/pedal release,
audio ownership/Undo, retained instrument note release при reprepare, unchanged
insert/MIDI generations. JUCE J2/J3 подтверждают exact native editor HWND
retention при I/R/Undo, auto I/red R/plus title, browser positional drop/Undo и
instrument-first clamp. Software UI snapshots100/150% визуально проверены:
geometry, red R, blue I и blue insertion line поверх child rows.

В ходе проверки исправлены прежние тестовые ожидания R без I и синхронного drop,
fresh Offline bootstrap, прежнее GUI preclose при Undo input-only history.
После изменения audio header выполнена согласованная пересборка всех consumers;
итоговые результаты относятся к окончательному source, не промежуточным логам.

Source commit: `1e82e990a8539d63429e79991a25f63f0666b467`.
Пакет: `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.3c-upd6-Mixer-IR-Inserts-JUCE-ASIO-Windows-local`.
13files /12payload SHA256 verified; 3EXE,8licenses,README,manifest.
GUI SHA256: `18AE3496D3635DA1F7098A8B70498605E11D4A07CCD0480B544CFCF5EF72E43B`.
Packaged EXE fixture smoke **exit0**, отдельный working directory, без изменения
пакета и пользовательских settings/projects. Final logs: `build/asio-local/validation/mixer-ir/ctest.log`,
`build-smoke.log`; previews/packaged exit-code там же в `gui-debug` и `packaged-smoke`.
Matching application/crash PDB: `build/asio-local/deliveries/0.3c-upd6-mixer-ir`.

Actual ASIO/input/record audition, физические mixed-DPI и keyboard/screen-reader
matrix пока NOT RUN. Пользователь проверяет скорость I/R, live input после R,
выключение I при armed, запись, open editors и удобство drop между plugins.
Предыдущие принятые packages сохраняются.
