# ARA editor UX — #179

Локальная реализация P3, 2026-10-18. Parent: #88. Приёмка #179 отдельно;
Save/Open, archive, recovery и долговечные binding-команды реализованы в #180;
контракт см. [ARA2_STATE_180.md](ARA2_STATE_180.md), приёмка P4 отдельная.

## Действия пользователя

Follow-up #88: первые два подменю контекстного меню аудиоклипа — `ARA editor`
и `ARA 2`. В `ARA editor` находятся Open editor, Update playback, Bypass/Enable,
Remove, Retry, Locate WAV и все команды discovery/выбора файла. `ARA 2` содержит
только совместимые плагины, без служебных команд и заголовков. Текущий плагин
отмечен; новая привязка/Relink выбирается для unbound/unavailable/failed source,
при готовой привязке список не позволяет молча заменить плагин.

Двойной ЛКМ по привязанному аудиоклипу открывает/поднимает ARA-редактор и выбирает
его region/track; источники общего документа переиспользуют одно native окно.
Без ARA binding остаётся обычный аудиоредактор; MIDI открывает piano roll.
Правая кнопка и knife mode не открывают редактор двойным кликом. Открытие/выбор
не создаёт project command/Undo entry. Недоступная сохранённая привязка даёт
диагностику; восстановление доступно через первое подменю.

Follow selection / Pin, follow-up #88 (2026-10-19): одиночный ЛКМ по ARA-bound
клипу переводит уже открытый незакреплённый ARA-редактор на выбранный region/track.
В общем документе сохраняются окно и native HWND; для другого plugin/document
открывается его редактор, прежний незакреплённый закрывается. Pin удерживает
текущий клип и окно; закреплённый документ не переключается автоматически,
остальные закреплённые окна сохраняются. Если открыты только закреплённые окна
или ARA-окон нет, выделение не открывает новое. Явный Open/double-click остаётся
явным выбором, включая закреплённый документ. Unbound audio/MIDI, right-click,
knife, trim, Alt-copy и Shift-снятие выделения не запускают follow.
Follow отложен до возврата mouse callback, повторно проверяет project identity,
актуальный selectedClip и Pin; сохраняет клавиатурный фокус аранжировки и работает
при Play. Binding, модель, host Undo, persistence и аудиограф не изменяются.
Дополнение пункта 2: клик передаёт object selection (`timeRange=NULL`), а не
временной диапазон; контракт и предпрослушка — [ARA2_EDITOR_AUDITION_88.md](ARA2_EDITOR_AUDITION_88.md).

Drag/drop follow-up #88: browser показывает группу `ARA 2` из сохранённого
каталога. Перенос плагина на аудиоклип подключает выбранный class к его source
через существующую host-команду и открывает общий редактор. Обычный VST3 leaf
также можно перенести на аудиоклип: неизвестный класс сначала проверяется
изолированным сканером. Insert/instrument targets сохраняют обычную семантику.
ARA-only drop на MIDI/пустое место ничего не создаёт. Все операции выполняются
после возврата JUCE drag callback, на остановке; после проверки повторно
сверяются project store/revision, clip и generation исходного drag.
По прямому решению пользователя от 2026-10-18 source binding применяется сразу
ко всем копиям исходного аудио, без подтверждения. Тот же plugin/class поднимает окно без
замены состояния, другой требует сначала Remove. Missing/unsupported class не
меняет проект. Binding использует прежние host Undo/Save/Open и shared document.
Кнопка `Remove` снимает ARA со всех копий source, сохраняя исходный WAV;
она не отделяет окно от workspace. UI component ID `ara-detach` сохранён.
`Discover ARA plugins...` проверяет
модули из обычного VST3 catalog и стандартного Windows `Common Files/VST3`.
Можно выбрать отдельный `.vst3` файл/пакет. Проверка идёт на worker через
`mrs_ara_scan.exe` с timeout/cancel; обычный VST3 catalog/cache не переписывается.
В списке подключения только доступные классы с совместимой ARA factory.
После завершённого scan совместимые классы атомарно сохраняются в локальный
`MoonRiverStudio/ara-catalog.json` рядом с preferences. При следующем запуске
меню читает каталог без загрузки DLL/повторного discovery. `Rescan ARA plugins...`
нужен после установки/обновления/удаления; выбор отдельного файла дополняет
каталог. Отмена не заменяет прошлый каталог. Повреждённый cache даёт диагностику
и допускает новый scan; отсутствующий module остаётся в меню disabled.
Подключение по-прежнему проверяет actual factory/class; cache не доказывает
лицензию, совместимость текущей версии или сохранность ARA project state.
Missing/unsupported/incompatible/failed discovery даёт диагностику без binding
и без изменения проекта. Наличие factory не доказывает лицензию/совместимость.

Привязка общая для всех occurrences одного source, как в #178. Редактор
переиспользуется для документа (legacy source или shared group из #88), выбранный
region передаётся через ARA EditorView. Контракт общего документа см.
[ARA2_SHARED_DOCUMENTS_88.md](ARA2_SHARED_DOCUMENTS_88.md).
Region sequence назначается EditorRenderer при создании и снимается перед
teardown. При открытии редактора host один раз запрашивает отсутствующие
analyzeable content types для enabled source, предварительно проверяя лицензию
без открытия activation dialogs. Завершение проверяется также через
`isAudioSourceContentAnalysisIncomplete`, а не только progress notifications.
Выделение region/sequence и временного диапазона повторяется после attachment
native view и восстанавливается после host model edits. `Ready` для PCM само
по себе не доказывает, что плагин уже проанализировал ноты или изменил pitch.
В MR host chrome доступны `Update playback`, `Bypass`/`Enable`, `Remove`.
Последнее снимает runtime binding со всех копий; исходный WAV не меняется.
Состояния `analysing`, `ready`, `pending refresh`, `disabled`, `failed` видны
в меню и открытом окне. Runtime missing plugin/несовместимый класс отклоняется
при подключении; сохранённые placeholders/relink пока относятся к #180.

Внутренние правки плагина отмечают PCM устаревшим через ARA model notifications
и VST3 component handler. `Update playback` готовит новый PCM на остановке.
MR Play также проверяет уведомления и обновляет PCM перед стартом. Во время
проигрывания продолжает звучать последний подготовленный вариант; статус окна
это сообщает. Новые правки применяются после остановки/обновления/нового Play.
Исправление по ручной проверке #88: private document state и ARA updates с
`signal scope remains unchanged` отмечают project/plugin state изменённым,
но не инвалидируют PCM. VST3 endEdit завершает жест без повторного изменения
аудиоревизии; restart без audio-affecting flags также не сбрасывает звук.
Настоящее изменение сигнала во время рендера не публикует устаревший результат:
сохраняется прежний подходящий cache и статус pending; attach не откатывается
как при неисправном плагине. На закрытии активного ARA editor MR активируется
до уничтожения focused native child; background close не крадёт foreground.
Это исправляет лишние пересчёты и transient отказы. Обновление 2026-10-19 добавляет region cache и частичный render для надёжного
ARA time range; при NULL range полного render source избежать нельзя. Контракт:
[ARA2_INCREMENTAL_PLAYBACK_88.md](ARA2_INCREMENTAL_PLAYBACK_88.md). Бесшовность
для всех vendors и editor audition этим не объявляются реализованными.
Follow-up #88: на остановке host ждёт 600 ms неизменной content/model revision
и завершения mouse gesture/modal UI, затем заранее готовит edited PCM. Пока идёт
analysis, автоматическая подготовка отложена. Play использует готовый cache и
при необходимости готовит только dirty documents, без общей invalidation.
Явный Update playback сохраняет принудительное обновление (включая плагины без
надёжных notifications). Короткая подготовка не создаёт окна и не меняет focus;
после 350 ms ожидания появляется progress с Cancel. По-прежнему работают joined
PCM workers и thread-affine model/vendor control, только progress HWND получает
сообщения во время ожидания. Это предварительная подготовка на stopped/control
пути, не обещание полностью неблокирующего vendor API. После отмены сохраняются
предыдущие подходящие PCM caches, Play не запускается; автоматических повторов
той же revision нет, новая правка или Update/Play допускает повторную попытку.

Нет live audition второго vendor renderer и второго аудиотракта. Native Play,
Stop, Seek и cycle requests плагина направляются в общий MR transport через
ограниченную очередь (32 запроса на Session). Stop редактора соответствует
MR Pause и сохраняет текущую позицию. Запросы не исполняются внутри callback
плагина; seek/loop во время записи отклоняются существующими Core guards.
Закрытие view очищает очередь, смена проекта уничтожает её вместе с Session.

Открытый editor получает снимки опубликованной позиции Engine, Playing,
loop/PPQ/tempo/meter через companion VST3 ProcessContext. Отдельный non-RT
worker выполняет короткий 64-frame process block с тихими входами и заранее
подготовленными буферами; весь выход отбрасывается, подключения к графу нет.
Zero-frame flush оказался недостаточен: плагин может принять его, пропустив
audio callback, из которого editor читает playhead. Vendor audition не слышен.
На UI/control thread остаются выделение буферов, setup/activation и владение;
worker join-ится до edit/bake/teardown. UI tick 30Hz передаёт последний coherent
снимок, промежуточные снимки coalesce. Ошибка process block отображается как
`Editor transport unavailable`; готовое аудио при этом не уничтожается.

## Threads, cancellation, windows

### Editor workspace follow-up #88 (2026-10-19)

ARA использует общий `EditorWindow` и единственный docked editor slot MR Studio,
как piano roll. Контейнер изменяет размер; если `IPlugView::canResize()` разрешает,
новый физический размер проходит `checkSizeConstraint()` и `onSize()` на UI thread.
Fixed-size GUI сохраняет native размер и получает горизонтальную/вертикальную
прокрутку контейнера. Хост не масштабирует рисунок стороннего плагина.

Attach — иконка справа сверху рядом с Pin; в docked режиме она меняется на Detach.
Full screen / Restore — соседняя иконка и F11, включая фокус внутри native child.
При Full screen из docked режима Restore возвращает редактор в dock; из отдельного
окна — к прежней плавающей геометрии. Attach/Detach/Full screen перемещают один
AraPanel/native parent HWND через SetParent, без повторного создания vendor view,
ARA Session или DSP и без изменения project/host Undo. Slot может быть занят
обычным аудио/MIDI редактором; явный Attach использует этот же slot.

Pin и follow-selection учитывают docked editor. Обычный выбор клипа не заменяет
его на Audio editor; закрытие проекта/Remove/closeEditors завершает docked native
view до уничтожения HWND. Space bridge ограничен конкретным AraPanel и его native
поддеревом, сохраняет исключения text/caret/modal/modifiers. Перед скрытием старого
floating HWND активируется MR owner, чтобы Windows не выбирала внешнее приложение.
Native размеры и viewport вычисляются в физических координатах текущего peer;
initial floating frame ограничивается рабочей областью монитора.
По уточнению пользователя 2026-10-19 прикреплённый ARA editor можно развернуть
до верхней панели, свернув arrangement до нулевой высоты; разделитель остаётся
доступным для обратного уменьшения. Нижний transport и browser сохраняются.
Адресная Release GUI сборка и ARA/J1 smoke 2/2 PASS.
Follow-up рывков при растягивании: combined moved/resized публикует одну native
компоновку после size negotiation; неизменные HWND bounds не переотправляются,
одинаковый vendor размер не вызывает повторный onSize. Перемещение clipping
viewport не копирует старые пиксели (SWP_NOCOPYBITS). Пошаговый splitter probe
проверяет ≤1 viewport change/шаг и 0 изменений неподвижного fixed-size inner HWND;
повторная неизменная layout даёт 0 geometry messages. Release build и ARA/J1
smoke 2/2 PASS. Плавность с реальным vendor и physical DPI требует ручной проверки.
Пользователь сообщил, что `8964286` рывки самой границы не устранил. Следующий
follow-up переносит ARA splitter mouse bursts в один latest-height update за
display VBlank; mouseUp применяет последнюю позицию без ожидания следующего кадра.
Drag начинается с фактически видимой высоты, включая oversized saved preference;
повторное clamped/unchanged значение не запускает layout. SafePointer и workspace
guard отбрасывают обновление для закрытого/заменённого dock. Тесты mouseDown/Drag/Up
проверяют burst60, stationary/synthetic events, final release, oversized preference
и реальную доставку VBlank callback. Это не подтверждение физической плавности
коммерческого GUI — требуется ручная проверка пользователя.
В том же прогоне выявлен Cancel focus defect: progress HWND скрывался до включения
всех owner HWND. Cancel теперь лишь запрашивает отмену; teardown сначала включает
owners, затем уничтожает progress. Обычные editor close сохраняют прежнее hide.
Release build и финальные ARA/J1/native editor smoke 3/3 PASS.
Следующее уточнение пользователя: `6400472` сгладил верхнюю границу, но native GUI
и нижняя overview-панель визуально отставали. Найдено нарушение sizing sequence
из VST3 `iplugview.h`: parent HWND должен получить constrained size до onSize.
Session теперь соблюдает порядок и восстанавливает parent size при отказе view.
AraPanel блокирует redraw собственного viewport на время изменения размеров/
положения; pending MR chrome paint и финальный native paint выполняются до
возврата из display callback. Сторонний HWND не subclass-ится/не пересоздаётся.
Fixture теперь запрашивает eager paint внутри onSize; проверяет actual parent
size в onSize и постоянный экранный bottom при растягивании top edge. Release
build и ARA/J1/native editors 3/3 PASS. Реальный Vovious drag/mixed DPI по-прежнему
требует ручной проверки; завершение UX по synthetic проверке не заявляется.
Решение пользователя 2026-10-19: восстановление b67256a отменено; работа идёт от
последнего 18a3045. Пользователь сравнил Studio Pro и наблюдает сходное дрожание
границ; дальнейшая борьба с ним снята с текущего объёма (vendor bug не доказан).
Осталась задача убрать добавленное host-side мерцание, особенно VocAlign.
PaintTransaction/WM_SETREDRAW и forced synchronous repaint удалены из resize;
SWP_NOREDRAW/NOCOPYBITS тоже сняты, применяется обычный native paint lifecycle.
Visibility меняется только при реальном show/hide, не на каждом layout/timer.
SDK parent-before-onSize, верхний предел Attach, frame-coalesced drag и прочий
редакторный UX сохранены. Вместо screen-bottom paint gate тестируется отсутствие
WM_SETREDRAW и WM_SHOWWINDOW во время floating/docked resize; parent size check
сохранён. Release build и ARA/J1 2/2 PASS. Реальное мерцание VocAlign/Vovious
ожидает пользовательской проверки. Предыдущий runtime/PDB 18a3045 сохранён отдельно.

Режим Attach и floating geometry помнятся в текущей UI session/открытом редакторе.
Project schema, ARA archive и существующая piano-roll preference не меняются;
долговечные per-project/window preferences не добавляются этим пунктом.

Проверки 2026-10-19: Release build PASS; 12 уникальных адресных CTest PASS
(ARA GUI, J1, native editors, theme/controls, ARA lifecycle/clip/processed,
VST3 editor/realtime, Desktop audio). Финальный ARA/J1/native-editor прогон
3/3 PASS. ARA fixture проверяет сохранение HWND при dock/fullscreen, Pin/follow,
Space из native child и toolbar, fixed/resizable GUI, close/project replacement,
отмену preparation с возвратом фокуса. J2 FAIL на прежнем baseline
`reorder synchronizes both views` (#178/#88); общий GUI gate не green.
MR chrome snapshots 100/150% просмотрены; vendor HWND в них не входит.
Реальный resize/dock Vovious/RePitch/VocAlign, physical ASIO и mixed DPI
NOT TESTED, ожидают пользовательской приёмки. Полный CTest не запускался.

### Исходный lifecycle

Session/model/controller/view создаются, используются и уничтожаются на одном
UI/control thread. Native VST3 view принадлежит Session, HWND — MR AraPanel.
При close, Remove, смене проекта и закрытии приложения view снимается до HWND,
document и module. Повторное retire идемпотентно: старое скрытое окно не закрывает
новый view. Selection очищается перед удалением/заменой region refs.

Обычный editor bridge MR переиспользован для Space, ownership/focus и размеров.
Не меняется vendor-owned GUI. Оболочка использует существующие MR Theme/controls.
Host toolbar повторяет обычный PluginPanel: power/pin glyphs, framed text
actions, 86px toolbar и тот же scoped concept Theme при включённом SVG skin.
Pin действует только в текущей ARA session; archive проекта не сохраняет HWND/UI.
Progress сначала показывает свой owned HWND и лишь затем отключает MR окна;
при завершении сначала включает ранее отключённые окна, затем закрывает HWND и
возвращает прежние active window/keyboard focus (если пользователь не ушёл
в другое приложение). Это сохраняет Space после Update playback и ошибки.
Обновление обычного insert-host пропускает ARA окна: их Session/processor и
keyboard bridge живут независимо. Явное close/project replace по-прежнему
снимает ARA view до удаления HWND/document/module.
Software snapshots проверяют MR chrome; они не доказывают physical mixed-DPI
или содержимое стороннего native HWND.

Decode/PCM processing остаются на joined workers. Во время подготовки MR и
открытые vendor окна отключаются до старта worker. Owner thread обрабатывает
только отдельный progress HWND: основной event loop/таймеры/Commands/ARA model
не могут повторно войти в Session. Cancel выставляет atomic flag; worker проверяет
его между блоками. Перед изменением модели и teardown worker всегда joined.
Произвольный зависший вызов vendor внутри загруженного процесса нельзя прервать
этой кнопкой; sandbox/timeout обеспечены для discovery, не для native редактора.
Никаких новых вызовов ARA, allocation/free, locks или GUI в audio callback.

## Undo и сохранение

Host clip move/split/trim/copy/delete Undo/Redo используют прежние Commands.
Внутренние pitch/note edits отменяются средствами самого плагина; MR Undo их
не обещает. С #180 attach/bypass/detach/relink — durable host commands с Undo/Redo.
Project schema28 хранит ARA/companion archive и persistent source ID; Save/Open
восстанавливает model/state перед PCM preparation. Missing/incompatible/corrupt
state сохраняется opaque для relink. Ограничения/лимиты — [P4](ARA2_STATE_180.md).

## Матрица доступности на 2026-10-18

| Plugin | Версия | Проверка | Ограничение |
| --- | --- | --- | --- |
| Celemony ARATestPlugIn | SDK releases/2.3.0 | lifecycle + clip/model PASS | Без native editor |
| MRSAraRenderFixture | 1.0, локальное расширение SDK fixture | processed PCM + native edit/transport/lifecycle PASS | Synthetic gain/latency, не pitch editor |
| JUCE ARAPluginDemo | JUCE 9.0.3 | lifecycle PASS | Native editor/audio этого fixture NOT TESTED |
| Vovious | 1.0.16 | isolated capability PASS | Installed; лицензия/edit/audio/UI NOT TESTED |
| RePitch VST | 2.0.0 | isolated capability PASS | Installed; лицензия/edit/audio/UI NOT TESTED |
| Melodyne | — | NOT TESTED | Не найден в стандартном VST3 каталоге |

Physical ASIO/listening/xruns, physical DPI и коммерческая совместимость не
считаются проверенными по synthetic PCM/GUI тестам. Известный baseline J2/J3
`reorder synchronizes both views` отслеживается в #88; отдельный ARA smoke его
не исправляет и не делает общий GUI gate успешным.

## Проверки первичной реализации `0278bb8`

- ARA core/lifecycle/clip/processed: 6/6 PASS; отдельный `juce_ara_editor` PASS.
  GUI smoke не зависит от J2: native gain button → dirty revision → MR Play →
  новый PCM с точным gain, bypass/dry, host clip Undo, window reuse/close/reopen,
  отложенный destructor старого окна, project replacement, progress cleanup.
- Focused VST3/Application/audio/PDC regression: 37/37 PASS.
- ARA OFF/no SDK: 13/13 PASS (Core/Application/VST3; отдельный GUI OFF не собирался).
- Четыре synthetic 1000×128 callback прогона: alloc/free=0, deadline misses=0,
  max callback 6.9–10.5 µs. Это не аппаратный ASIO/xrun тест.
- GUI build и J1 PASS. J2/J3 FAIL на прежнем `reorder synchronizes both views`;
  обе текущие ошибки совпадают с воспроизведением на принятом пакете 0.5.
- Software 100/125/150/200% MR chrome snapshots; native HWND не входит в JUCE
  snapshot. Physical DPI, native focus/Space на коммерческих plugins,
  лицензии/слуховая проверка/ASIO и plugin-private Undo NOT TESTED.
- Полный CTest, packaging и смена shortcut не выполнялись. Пакет 0.5 сохранён.

Логи: `build/ara179-ara-tests.log`, `build/ara179-regression-tests.log`,
`build/ara179-disabled-tests.log`, `build/ara179-final-gui-tests.log`,
`build/ara179-gui-final.log`. Проверки показывают готовность к ручной приёмке
с перечисленными ограничениями; они не являются пользовательской приёмкой #179.
