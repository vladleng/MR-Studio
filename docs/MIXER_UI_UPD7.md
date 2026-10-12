# Mixer UI / 0.3c upd7 — #136

Запрос после приёмки upd6/#135: «Все работает, закрывай». #135 закрыт completed,
существующий пакет и manifest сохранены. Stage5/#25 не начинать.

## Контракт

ПКМ по insert name или power открывает popup у указателя: Remove, Editor,
Bypass (checked отражает bypass). ЛКМ сохраняет прежние действия. Remove меняет
цепь через Application/Undo, закрывает native editors перед структурной заменой;
во время playback/recording недоступен. Bypass использует существующий быстрый
путь, Editor — существующее окно. Async callback проверяет owner/project/slot.

Browser plugin selection синяя, отдельная от keyboard focus. Browser drag на
канал показывает только границу вставки; positional drop/Undo, limit8 и
instrument-first сохраняются. UI insert/send split общий по высоте каналов:
8px draggable горизонтальный разделитель, минимальные области и scrollbars,
потянуть вверх — увеличить sends. Master сохраняет общую границу. Split временный
для текущего окна, не project state/Undo; схема17 без изменений. Send rail5px
(ранее3px), hit area11px (ранее9px); gain mapping/preview/commit/cancel неизменны.

## Слои и безопасность

UI/actions/tests/docs AFFECTED; model/engine/serialization/files/shared Core
NOT AFFECTED: используется существующий NativeInsert/SetInserts, bypass/send
parameter queues и Undo/persistence. Studio JUCE presentation; Live вне объёма.
Selection/hover/split принадлежат UI. Notifications — существующий refresh.
NON-RT popup/layout/paint; RT-ADJACENT actions переиспользуют Application с
прежним stop/quiesce/graph lifetime. Нового callback-кода, locks/allocations,
DSP ownership или latency behavior нет. Реальный ASIO audition в этой работе
не подменяется synthetic GUI smoke.

## Проверки / поставка

**READY WITH MANUAL CHECK / RT SAFE WITH MANUAL CHECK**.
Local Release PASS, final CTest **126/126 PASS, 140.18 s**; packaged **J3 fixture
smoke exit0**. GUI regression проверяет menu Editor/Remove/Bypass + Undo/stale,
retained bypass generation, positional line-only drop, browser selected paint,
split resize/min-max/shared alignment/no mutation и send hit11px/cancel. Existing
send Save/Open, MIDI/recording/native editors/PDC/allocation suites PASS.

Во время разработки устранены compile /WX shadow и неоднозначный Point overload;
ошибки smoke fixture setup (browser rebuild после catalog injection) и проверки
image до завершения Graphics. Последний полный прогон относится к финальному
source. Окна после Remove скрыты/retired сразу; C++ deletion отложен до timer,
как и ранее. Тест учитывает это, не требует немедленного уничтожения wrapper.

Source `4185cc5bf583dcfc99b395eb99446e20a290343f`, branch
`mrs/0.2g-fix1-editor-space-local`.
Package: `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.3c-upd7-Mixer-UI-JUCE-ASIO-Windows-local`.
13files/12payload hashes verified; GUI SHA256
`4C60B8E9AA93146212504E817F94CF9DD03297FA1B0646ADE8F9BBB7EF137459`.
Symbols отдельно `build/asio-local/deliveries/0.3c-upd7-mixer-ui`.
Logs: `build/asio-local/validation/mixer-ui/build-preview.log`, `ctest.log`;
fresh packaged previews/exit0: `packaged-smoke` под той же validation папкой.
Visual inspection100/150% подтверждает divider alignment/scrolling/send text,
line-only hover и selected browser paint. Previews не входят в runtime package;
для сверки использовать fresh packaged previews, старые общие build PNG могут
содержать предыдущий первый image из-за исторического append smoke output.

Отдельные physical mixed-DPI/screen-reader matrix и actual ASIO audition агентом
в этом запросе NOT RUN. Прежние packages и пользовательские проекты/settings
сохранены. Код/build/docs/packages локально, GitHub issues only, без push/merge/Actions.

## Приёмка 2026-10-11

Пользователь подтвердил «Принимай, с обновлениями пока все». Принят существующий
0.3c upd7 пакет; #136 закрыт completed. Приёмка не изменяет binaries/manifest/
hashes и не добавляет результатов отдельной hardware/accessibility/stress matrix.
Обновления остановлены до новой прямой команды пользователя. #25/Stage5
не начинать. Новых source/build/test изменений при записи приёмки нет.
