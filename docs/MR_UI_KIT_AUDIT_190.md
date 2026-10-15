# MR UI Kit #190 — аудит и план пилота MR Rig

2026-10-15. **AUDIT COMPLETE — ожидает приёмки аудита.**
Основание: #189/#190, MR_UI_KIT.md, MR_UI_GALLERY.md, MR_UI_KIT_ROADMAP.md,
MR_UNIFIED_DESIGN_SYSTEM.md; пользователь согласовал GUI-план с MR Rig первым
пилотом командой «Давай так, поехали».

Проверен единый локальный checkout `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio`.
Ветка `mrs/0.2g-fix1-editor-space-local`, исходный HEAD
`913a3e51767514e9ff2dd96577f0c03d2ef53ae9` (принятая0.4f).
Начальный git status чистый. GitHub использован для Issues/спецификаций;
исходники прочитаны локально, pull/reset/branch switch не выполнялись.

## 1. Фактические владельцы и точки повторного использования

| Локальный источник | Фактическая роль | Решение для UI Kit |
|---|---|---|
| apps/studio-juce/src/Controls.h:10–38 | constexpr палитра, drawCard, popup helpers, radius6/border1 | Сохранить принятые defaults; opt-in controls получают семантические роли из экземпляра Theme |
| Controls.h:40–146 | final ui::Theme, JUCE LookAndFeel_V4, fonts, buttons/menus/combo/text editor, transport SVG cache | Расширять существующий Theme, не создавать второй глобальный LookAndFeel и не снимать final ради параллельной темы |
| Controls.h:149–198 | InsertPowerButton, InsertEditorButton, MixerInsert; отдельные зоны power/editor, keyboard focus, accessible titles | Сохранить mixer semantics; общая power-графика возможна позже, не смешивать bypass и selection |
| Controls.h:201–280 | Handle: custom normalized0..1 slider/rotary, actual handle hit-test, Ctrl fine, keyboard, Escape/focus cancel, accessibility | Не заменять его сразу в микшере; новый MRKnob на JUCE Slider не наследует автоматически эти контракты |
| Desktop.cpp:427–505; Desktop.h:199–245 | Strip owns gain/pan Handle; normalized volume -> -60..+12dB, pan -> -1..1, mix preview/commit | Mixer rollout отдельно (#205); не менять шкалу/Undo ради нового вида |
| PluginPreset.h:7–17 | PresetMenuTheme — существующий узкий popup adapter, общие popup helpers и Theme::browserFont | Сохранить владельца и async-menu lifetime; это не новый источник продуктовой палитры |
| Desktop.h:133–135; Desktop.cpp:44–54,93–96 | Desktop owns Application + Theme, attaches LAF; constructor connects offline engine; destructor detaches/closes editors | Не использовать Desktop как корень беззвучной Gallery; Gallery — plain JUCE Component/DocumentWindow с sandbox values |
| Editors.cpp:61–185 | FxPanel, native controls, preview gesture, cancel, timer; для Rig делегирует RigPanel | Сохранить native shell, внедрять общий control/binding в один редактор постепенно |
| Editors.cpp:314–335; Desktop.cpp:30–42 | openInsert reuses window; configure hidden, resize limits, owner, attach Space, show once | Не переписывать EditorWindow и HWND lifecycle для визуального пилота |
| NativeEditorSpace.h | Space bridge с исключением text/combo/modal и generation-aware владельцем | Сохранить; ввод числа/поиск не должен запускать transport |
| RigPanel.cpp:20–110,142–157 | PRE/CORE/POST, selected module inspector, five controls, real descriptor units, per-editor gesture; refresh15Hz | Первый product adopter. Текущий shell/история остаются; новая графика и органы управления подключаются opt-in |
| Editors.cpp:100–123; ReverbBrowser.cpp:34–64 | Reverb controls/browser, поиск/папка/active source/options, coherent status,300ms prefs debounce | Регрессионный сосед, не второй пилот и не новый DSP; GUI refresh отдельно #207 |
| Settings.h:14–22,52,73 | bounded per-insert NativeEditorView: band/options/search/folder/Rig filters | UI preferences не смешивать с RigPreset или Gallery theme draft |
| apps/studio-desktop/src/application.cpp:702–719 | preview_inserts / cancel_insert_preview / set_inserts, parameter_only, pending graph parameter batch, SetInserts | Единственный существующий command/model маршрут; UI Kit не получает прямую запись в DSP |

Отдельных MR UI Gallery / MRThemeRoles / MRControlBinding в текущем JUCE frontend
не найдено. Слово Gallery в ToneBrowser smoke относится к библиотеке моделей,
а не к UI component sandbox.

## 2. Принятый baseline и документальные расхождения

Issues #145 и #146 закрыты/приняты. Более поздняя **#148 также закрыта/принята**
и уточняет menu/browser typography. Актуальный код согласуется с #148:

| Роль | Принятая0.4f / фактический код |
|---|---|
| channel/editor/window/tree | #212830 |
| content card | #2D353E |
| обычная button fill | #2F3841 |
| Arrange canvas | #262E37 |
| outline / separator | #3D444D / #343C45 |
| text primary / secondary | #DCE3EA / #9DAABC |
| accent selection | #006DCC; некоторые focus indicators используют skyblue |
| shape | radius6, border1 logical px |
| detail/channel/mixer button | Segoe UI12, horizontal scale1 |
| обычные buttons | Segoe UI14/1.04, height<=22 ->13/1.04 |
| popup/preset menus, ComboBox, browser typography | Segoe UI14/1.04 по #148; minimum normal popup row22 |
| прочие Theme::font calls | embedded Noto Sans; не все надписи уже имеют одну semantic font role |

`UI_REFERENCE_UPD9.md` и отдельные абзацы UI Kit хранят историческую фазу #146
с menu12 / pending acceptance. Они не отменяют последующую #148. Не уменьшать
меню назад до12 и не переоткрывать #145/#146/#148.
`MR_RIG_GUI.md` содержит исторические awaiting #201 / четыре utility-типа;
текущий статус: #201 accepted, Compressor+Drive из #202 accepted в0.4f.
Источник текущего статуса — Issues #197/#201/#202 и MR_RIG_PEDALS_202.md.
Эти расхождения документированы здесь; массовая синхронизация docs не выполнена.

MR UDS задаёт **целевой** dark blue-graphite/тонкий contour стиль: window#171E28,
surface#222C3C, raised#2D394E, deep#192333, text#DAE5F4, muted#7188AB,
border#4B5F7B, focus#91BBEE, green#7AB69E/red#E68F88/amber#E2C070.
Утверждённое направление не означает, что эти значения уже применены в0.4f.

## 3. Дубли и риски адаптации

1. Rig core sliders, module sliders, Cab sliders и Reverb sliders повторяют
   настройку textbox/style/fine/reset и begin/update/end. Их display semantics
   различны: linear gain отображается в dB, Bell EQ gain уже в dB, Mix в процентах,
   Compressor ratio/attack/release имеют реальные descriptor units. Нельзя просто
   заменить все callbacks одним generic normalized setter.
2. Текущие цвета используются как constexpr и напрямую в paint, включая Handle
   и browser rows; один setLookAndFeel на новой Gallery не перекрасит такой paint.
   Opt-in controls должны явно разрешать роли через Theme instance. Старые helpers
   остаются с текущими defaults до отдельных migration slices.
3. Theme final; новая независимая MRLookAndFeel рядом со старой создаст второй
   источник истины. Рекомендуется один Theme с профилем/role resolver и immutable
   validated style snapshot. Типографику mixer/menu хранить разными ролями.
4. Для Gallery нельзя создавать FxPanel/RigPanel с настоящим Desktop: это создаст
   Application/offline engine и даст preview доступ к проекту. Собрать те же будущие
   controls с отдельным sandbox parameter source и счётчиком simulated gestures.
5. Не менять preview timer/meter ballistics при замене вида. Rig15Hz/FxPanel20Hz —
   фактический baseline, а не результат нового performance-теста.
6. При teardown остановить GUI timers, cancel gesture, detach LAF до уничтожения
   scoped Theme. Не хранить raw model/processor pointers в controls/menu callbacks.
7. Dedicated DAW automation lanes для Rig modules в0.4f не реализованы. Подключение
   UI Kit к preview/SetInserts не должно ложно объявлять их готовыми.

## 4. Предлагаемый минимальный путь реализации

**Пилот: MR Rig**, согласован пользователем. MR Reverb и DAW/Live остаются
регрессионными соседями; их миграция не входит в этот аудит.

### #191 — роли и изолированный preview

- Добавить semantic color/font/metric roles как расширение существующего Theme.
- Два явно различимых профиля: `Accepted0.4f` (defaults побайтно/численно прежние)
  и `MR UDS Preview` (утверждённые visual targets только в sandbox).
- Preview context имеет одного владельца; не вызывать global setDefaultLookAndFeel,
  не менять constexpr палитру DAW, product settings или project schema.
- Первый маленький JUCE preview показывает surfaces/text/border/focus/status,
  реальный button/selector и набор чисел/units. Это опорный preview #191,
  не заявление о готовой полнофункциональной Gallery #193.
- Удобный первый экран для выбора вида: comparison accepted/target рядом,
  normal/hover/pressed/disabled/focus, reset to profile defaults,100/125/150/200%.
- Проверить границы roles, theme lifetime и отсутствие изменений в существующих
  DAW/плагинах; только затем расширять каталог.

### #192–#194 — одинаковые controls в Gallery и продукте

Первая полезная семья: MRKnob (compact/standard), horizontal MRFader,
MRValueField (dB/Hz/ms/%/ratio), MRButton/PowerToggle, MRSelector, ModuleTile.
Рисовать JUCE vector primitives, использовать штатный input/accessibility;
иконка/иллюстрация не должна подменять интерактивный control.

Общий binding contract: `begin(identity, old state)` -> `preview(physical value)`
-> `commit once` или `cancel`. В Gallery он пишет только sandbox; в Rig адаптер
вызывает существующие Application API, проверяет project/insert/module identity,
сохраняет native unit mapping, не создаёт другой history owner. Typed entry,
reset, keyboard и wheel должны иметь явно проверенный transaction contract.
Извлекать поведение пошагово; не менять микшерный Handle одновременно.

Gallery #193 разворачивает каталог/inspector/drafts и simulated commits на тех же
компонентах. Future meters/graphs маркировать DEMO и не ждать их ради первого
предпросмотра основных органов управления. Theme drafts — локальные UI metadata,
не RigPreset, не user audio prefs. #194 завершает host binding/Undo tests.

### #195 / #206 — product pilot

Opt-in visual MR Rig: PRE/CORE/POST и один inspector остаются. Compact contour
module tiles, читаемые power/bypass/selection, общие knobs/value fields вместо
локальной настройки sliders. Сохранить реальные model cards и secondary Import,
Current/Loading/Missing/Unsupported, Save Rig/My Rigs, variant, input/output.
Не рисовать динамические amp controls для WAV; NAM остаётся unavailable.

Пилот не требует новых DSP, смены schema, RigPreset, ASIO или EditorWindow.
Переход Reverb/DAW/Live выполняется после отдельной приёмки пилота.

## 5. Существующие assets и подготовленные проверки

`assets/ui/mr-uds/manifest.json`:65 entries; все65 путей существуют локально.
Есть knob_arc/knob_minimal/knob_soft_arc/knob_dual, slider_horizontal/vertical,
power_button, segmented_ab, preset_selector, module_tile, parameter_readout,
tabs/status_leds. Это visual reference/static art, не готовые JUCE widgets.
Токены: tokens/mr-plugin-colors.json и mr-plugin-tokens.css. Генератор не запускался,
SVG/manifest не редактировались и повторно не объявлялись прошедшими XML QA.

Для будущего кода найдены текущие CTest hooks в cmake/Juce.cmake:
juce_rig_shell, juce_native_editors, juce_reverb_editor, juce_reverb_browser,
juce_tone_library, juce_j1_smoke, juce_j2_smoke, juce_j3_smoke. Model/host neighbours:
core_rig, processing_rig, desktop_inserts. Перед запуском перепроверить ctest -N.

Новые checks по этапам: default Theme parity, preview theme isolation/teardown,
no project/history/device changes on Gallery open, units/range/default, typed
Enter/Escape, Ctrl fine/reset, focus/Tab/Space, per-gesture single Undo/cancel,
preset/close/reopen and stale generation; screenshots100/125/150/200 plus min/wide.
Physical mixed-DPI и реальные accessibility/hardware sessions указывать отдельно.

## 6. Что выполнено и что не выполнялось

**В #190 выполнены только чтение/поиск локального кода, сверка актуальных GitHub
Issues/спецификаций, metadata inventory assets и этот migration plan.**
Исходники UI/DSP, assets, accepted packages, ярлык, Saves и настройки не менялись.
Configure/build/CTest/GUI smoke/ASIO **в этом аудите не запускались**.
Свежие screenshots не создавались. Имеющиеся0.4f screenshots/test logs относятся
к ранее принятой #202 и не являются новыми результатами #190.

#190 остаётся открытой до приёмки аудита. #191–#195/#206 не реализованы этим отчётом;
следующий самостоятельный этап — #191, scoped roles/preview. Никакого
автоматического массового GUI rollout. Source/build/tests/assets локально;
на GitHub — этот отчёт и Issue status, [skip ci], без source push/Actions/PR/merge.

