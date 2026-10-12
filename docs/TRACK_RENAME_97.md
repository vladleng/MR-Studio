# #97 — переименование дорожек и единая типографика каналов

2026-10-09 пользователь подтвердил «Переименования принимаю». Inline rename
0.2g upd5 fix1 принят, #97 закрыт completed; шрифты приняты отдельно ранее.
Ниже сохранены контракт и свидетельства проверок. Следующий косметический footer
upd6 не относится к критериям #97 и не означает начало нового sub-issue.

2026-10-09 пользователь поручил #97 и единый шрифт/размер всех элементов каналов
микшера и треков, как у кнопок плагинов Inserts. Шрифты приняты пользователем:
«Со шрифтами все отлично». Уточнение: имя редактируется прямо в поле, без окна.
Исправление выпускается отдельно как 0.2g upd5 fix1.

## Контракт и слои

Двойной левый клик в области имени audio/instrument/bus в Arrange или Mix
включает TextEditor внутри текущего nameBounds. Enter подтверждает, Escape отменяет,
потеря фокуса подтверждает. Пустое/неизменное имя не создаёт команду. Один клик
по-прежнему выбирает дорожку, одиночный drag имени сохраняет reorder. Master,
body/controls и правый двойной клик не запускают переименование. Во время записи
rename не открывается, как и существующее ограничение Application.

AFFECTED: UI hit/paint/name gesture, команды (существующий RenameTrack), model
имя (существующее Track.name), Undo/Redo и persistence проверки, notifications,
tests/docs. NOT AFFECTED: engine/RT, routing/PDC/plugin lifetime, формат15,
структура файлов проекта, preferences7. SHARED MODEL, JUCE UI; отдельного Live
продукта нет. Новых persisted полей или undo-stack нет. Inline editor transient.

Double-click / Track menu → Strip::beginNameEdit → queued Application::rename_track → RenameTrack
→ ProjectStore → shared UI refresh → existing Undo/Save/Open. SafePointer/store
guards исключают отложенное переименование закрытого/сменённого проекта;
удалённая дорожка игнорируется, неизменное имя не создаёт history entry.

Theme::stripFont — существующий default typeface/plain с logical height11,
буквально тот же font, который ранее рисовал InsertEditorButton. Этот общий role
используют insert captions, Strip имена/readouts и все TextButton в Mix/Master
и Arrange mini strips. Прежний channelFont13 остаётся для browser/preset/ruler;
toolbar/menu/editor sizes не меняются. Цвета, flat buttons, bypass icons и
fixed faders/свободная высота Inserts сохраняются. Shared nameBounds используется
и в paint, и в hit/drag test. Font role LOCKED по прямому запросу пользователя;
типографика принята; приёмка inline-переименования ещё ожидается.

## Проверки

Level2: cached configure, JUCE Release, затронутые core/desktop/persistence/GUI
CTest, отдельный packaged J3/diagnostics. Новая GUI fixture проверяет реальный
inline Enter/Escape/focus loss, отсутствие modal, Cancel без revision, одно Rename/Undo/Redo, синхронизацию
Mix/Arrange, instrument/bus, no-op, исключение Master/body, actual Save/Open,
одинаковый font для channel buttons; software100/150 snapshots с кириллицей и
длинным именем. Existing reorder/fader/plugin/recording fixtures сохраняются.
Физический mixed-DPI и screen-reader review не заявляются.

## Доставка inline fix1

Source eac13f7 + final version rebuild1cf7b07, version0.2g upd5 fix1. JUCE Release PASS; focused CTest8/8
PASS(финальный26.16s; первый28.82s), включая J2/J3/diagnostics/core/persistence/workspaces/files.
Packaged J3+diagnostics exit0. Software inline100/150 просмотрены.
Пакет MR-Studio-0.2g-upd5-fix1-Inline-Rename-JUCE-ASIO-Windows-local.
EXE SHA256:A5D83B0A92570AED95B003A7979003D99EB92907B610004BBFDE3EBAC83924FF.
Diagnostics helpers PASS; final packaged diagnostics log подтверждает fix1 version.
FEATURE READY WITH MANUAL CHECK. Шрифты приняты, inline rename ожидает проверки.
Физический DPI/ASIO/screen-reader не проверялись; старый пакет сохранён.

## Историческая доставка upd5 (диалог заменён по уточнению пользователя)

Source6e7715e, branch mrs/0.2g-fix1-editor-space-local. Cached offline configure
и JUCE Release PASS. Финальный serial CTest118/118 PASS(92.39s), включая обновлённые
modal/right/single-click/stale-dialog tests и существующие drag/plugin/recording
fixtures. Packaged J3+diagnostics exit0, diagnostics helper tests PASS.
Software100/150 inspected; physical mixed-DPI/screen-reader NOT RUN.
Первая compile fixture обращалась к protected AlertWindow::keyPressed;
исправлен public Component virtual dispatch, без изменения продукта ради теста.

Пакет MR-Studio-0.2g-upd5-Track-Rename-Font-JUCE-ASIO-Windows-local;
EXE SHA256:82B6D140D01AB04F02BC88B8BD80DFC0969210FD188177A9EC66F7DFF66EA5B7.
Matching optimized symbols/helpers/assets/licenses, полный test log и SHA256.
FEATURE READY WITH MANUAL CHECK / READY WITH MANUAL CHECK. Issue97 open,
ожидать пользовательскую приёмку: double-click в обоих workspace, Enter/Escape,
Undo/Redo/Save/Open, обычный reorder и единый font. Старые пакеты не изменены.
Следующие #100/#98/#99/#95 и Stage5 не начинать.
