# Editing backlog #150 / 0.4

2026-10-14. По прямому запросу пользователя реализовать все7 пунктов #150 и выпустить
билд0.4 после принятой AI Foundation. База accepted0.3m upd4/source66b712f.
FEATURE ACCEPTED2026-10-14: пользователь «Все принимаю, закрывай» —0.4 и все7 пунктов
#150. Issue closed completed, прежние source/tests/package/manifest/shortcut сохранены.
Приёмка docs/issues-only без новой сборки/tests/paid calls/hardware измерений.

## Пользовательский контракт

- Section popup содержит сохранение шаблона Text+RGB и выбор из global user templates.
  Шаблон не хранит Start/End. Выбор заполняет draft, применение только кнопкой Apply;
  сохранить template можно без изменения проекта/Undo. Имя1–128characters, до64templates,
  повторное имя обновляет template. Хранение optional sectionTemplates в ViewSettings
  JSONv1, existing atomic settings publish; legacy defaults empty, без смены prefs8.
- Pointer(1)/Knife(3) в toolbar и горячие клавиши. Knife click режет clip/chord/section
  на Smart grid; Pointer возвращает обычные gestures. Mode transient, startupPointer.
  Typing/modal guards сохранены; double click в Knife не открывает editor.
- S режет выделенные chords/sections на transport cursor; clips — всю выделенную группу
  на том же sample. Clip knife на выбранном clip режет selected group, иначе один clip.
  Объекты вне точки разреза/на boundary пропускаются; если подходящих нет — понятный отказ,
  без новой Undo. Right entities получают новые IDs; MIDI notes/events получают новые IDs
  существующим SplitMidiClip. Sections/chords сохраняют symbol/name/color.
- Edge drag выбранного audio/MIDI clip сохраняет group selection и меняет соответствующий
  край всех выбранных clips на общий sample delta. Preview не меняет модель; весь delta
  ограничивается допустимыми source/timeline/минимальными bounds группы. MIDI сохраняет
  integer tick precision. Shift после LMB временно отключает Snap как в принятойupd4.
  Release одна transaction/Undo; stale source clip/cancel/invalid range без partial mutation.
- Popup surfaces70% opaque (30%transparent), текст/границы/interactive controls не получают
  общий alpha. Shared Theme и PresetMenuTheme через drawPopupSurface, JUCE translucent peer.
- ПКМ по clip открывает меню в screen click: split/duplicate/merge MIDI/delete/default
  color и custom14-square palette, вдвое больше7 marker colors. Bright RGB palette;
  цвет сохраняется audio/MIDI, применяется ко всей selected group, default resets toblue.
  Contrast-aware caption/note/waveform ink, прежняя clipFillAlpha.72/selection outline.
  Split/copy сохраняют RGB; merge наследует цвет первого source по project order.

## Слои / RT / совместимость

Core Clip optional RGB требует **schema22**, reader1–22; schema1–21 defaults unset.
Archive1 без изменения. InvalidRGB rejected model/reader; lossless roundtrip и legacy21
fixture. Старые0.3m binaries не читают новую schema22: сохранить старые проекты/пакеты.
AI Query/Tools scope не расширяется, память/Skills #168 пока не реализованы.

Application commands: split_clips, trim_clips, split_sections, split_chords, set_clip_colors.
Group changes candidate/preflight→existing edit/edit_timing/ProjectStore. MIDI-only split/
trim/history сохраняют prepared runtime, source/time/head и existing stopped guards.
Mixed/audio edits используют existing clip edit path, никаких callback изменений.
Color/Undo only metadata publish без DSP rebuild/device restart; section ranges сохраняют
existing stopped/loop-off/count-in/document-reference guards. Studio/Live одна Application;
никаких новых provider/network/device/routing действий.

UI/Application/model/persistence/Undo/tests/docs AFFECTED; engine RT callbacks NOT AFFECTED;
existing prepared plan publication RT-ADJACENT reused. Все JSON/settings/UI/allocations
на NON-RT/control thread. Применены feature/UI/build/RT skills. Исходники/пакеты локальны,
GitHub docs/issues-only [skip ci], без Actions/source push/PR/merge.

## Проверки и доставка

Логи build/asio-local/validation/backlog150-*. Первый compile отказ addCustomItem signature
исправлен; второй /WX signed optional comparison fixture исправлен. Initial full145 был
запущен до green test build, поэтому не является final validation;4failed/56.03s сохранён.
Ошибки UI common resize bound и stale legacy fixtures исправлены; intermediate focused
GUI PASS28.39s, native save/open fixture учитывает существующую media-reference normalization.
Final Release configure/full build PASS; **145/145 CTest PASS107.69s** после всех code/test
правок, включая GUI/J3+diagnostics, persistence/legacy/AI/audio/MIDI/VST3 и новые regressions.
Native split/group resize/color/history/SaveOpen, invalid/stale rollback, legacy21 color
defaults, section/chord splits; GUI group preview/commit/Undo, knife1/3, template save/draft,
ViewSettings roundtrip,14colors и alpha background PASS. Snapshots100/150 inspected.
Source `1b69cf16e9c95cbf1c6aa2685410938db914259b`, branch unchanged.
Package `MR-Studio-0.4-Editing-JUCE-ASIO-Windows-local`,13files/12hashes verified;
packaged AI startup smoke exit0. GUI SHA256
`81594312AF44DBFB7894CBC0B9C184E6C16C3EF78F6ECBD641F82801EB5BAD5F`.
Matching PDB deliveries/0.4-editing-150. Existing MR Studio.lnk→0.4,
target/workingdir/emptyargs/original icon verified. Previous0.3m upd4 manifest12/12 unchanged.
No real paid requests/physical ASIO/mixed-DPI measurements; local packages/source only.

## Ручная приёмка

Создать/сохранить/выбрать template после restart;1/3 и knife/S на audio/MIDI/chord/section;
marquee group→S/group left/right trim→Shift/cancel; ПКМ color14/default→UndoRedo/SaveOpen;
меню читаются на реальном DPI. Проверять в копии проекта, новые schema22 файлы не открывать
старым0.3m. Physical ASIO/DPI и real-user acceptance отдельно; не подменять GUI fixtures.
#150 closed completed после явной приёмки всего объёма. #26 при свежей сверке уже closed,
children11/11 приняты. Подробная новая ASIO/DPI матрица не предоставлена. Следующий roadmap
#168/SM01 только по прямому выбору пользователя.
