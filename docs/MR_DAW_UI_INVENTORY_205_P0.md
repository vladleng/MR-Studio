# MR Studio #205 — P0: аудит DAW/Live перед внедрением Kit

Дата: 2026-10-16. Статус: P0 выполнен, ожидает пользовательской приёмки; P1–P6 не начаты. Основание — #205: каждый инкремент требует отдельной команды и визуальной приёмки. Назначение этого документа — карта внедрения и baseline, а не утверждение нового GUI.

## Фактическая база

- Активный локальный checkout: `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio`, ветка `mrs/0.2g-fix1-editor-space-local`.
- Код/бинарник аудита: `c35e0c4dbbdb47cf948cc6c65297c64f35ea41a7` (#195); исходный Git status clean. Инфраструктура #189/#190–#195 принята пользователем. Это не означает миграцию остальных окон.
- Проверен SHA-256 принятого пакета `Builds/MR-Reverb-Kit-Pilot-195-JUCE-ASIO-Windows-local` и его sourceCommit. Обычный Desktop по умолчанию использует прежний UI; новый Kit включается только в opt-in Reverb.
- В P0 не изменены C++/assets/DSP/commands/schema/settings. Нет configure/build, нового runtime-пакета, смены основного ярлыка или подключения ASIO. Свежие снимки сделаны существующими fixture entry points принятого бинарника.
- Предыдущий полный CTest этого неизменённого кода: 162/162 PASS, 225.13s (#195). Это прежнее свидетельство, не новый полный прогон P0.

## Карта активного JUCE GUI

Все source paths ниже относятся к указанному локальному коду, который опережает GitHub main.

| Область | Реальная реализация / владелец | Состояния и поведение, которые надо сохранить | Этап |
|---|---|---|---|
| App shell / Home | `Desktop.h:31`, `Desktop.cpp:44`, `ProjectHome.h:22`, `Main.cpp:12` | Recent/New/Open/return to project; window geometry; dirty project prompt; Home не должен запускать performance | P1/P4 |
| Меню и заголовок | `Desktop.cpp:152`, `Desktop.cpp:238`, `Controls.h:81` | File/Edit/Track/Transport/AI, доступность пунктов, async actions, project title/dirty marker | P1/P4 |
| Верхняя toolbar | `Desktop.cpp:240` | Pointer/Knife, Undo/Redo, Split/Duplicate, Zoom/Fit, Snap, Click/Count; enabled/disabled/selected/hover/focus | P1 |
| Transport / navigation | `Desktop.cpp:267`, `Desktop.cpp:400`, `Desktop.cpp:409` | Play/Pause/Stop/Record, previous/next section, loop, Edit/Mix/BROWS; Space latch и F2/F3/F5; реальный playback/recording state | P1 |
| CPU/latency/error/footer | `Desktop.cpp:285`, `Desktop.cpp:317` | Errors/count-in отдельно от CPU tooltip; измеренное/неизмеренное не превращать в ложный Ready; driver latency ≠ acoustic round trip | P1 |
| Musical context | `Desktop.cpp:363`, Chord/Section/Meter/Tempo track sources | Current/next chord, section, marker; authoritative musical/timeline state, непрерывная позиция | P1/P2 |
| Arrange / timeline | `Desktop.h:254`, `Desktop.cpp:238`, `Desktop.cpp:699` onward | Grid, waveform/MIDI, hit testing, scroll, selection, move/trim/copy/group, drag/drop, zoom/snap, Undo | P2 |
| Track headers | `Strip` mini layout `Desktop.cpp:552`; rows owned by Arrangement | Rename, track type/color, M/S/R/input monitoring, gain/pan/input and selection; не подменять clip selection | P2 |
| Musical lanes | `ChordTrack.cpp:73`, SectionTrack/TempoTrack/MeterTrack/Marker sources | IDs, metadata vs actions, moving playhead/context; lane resize/focus and editor bounds | P2 |
| Mixer / Master | `Desktop.h:203`, `Desktop.cpp:428`, `Desktop.cpp:496`, `Desktop.cpp:552` | Legacy Handle fader/pan, meters, insert bypass/open, M/S/R/I, routing; command/preview/commit and accepted font12 | P3 |
| Inserts / sends | `Controls.h` InsertButton, `SendLevel.h`, `Desktop.cpp:1030` | Input/output targets, send order/level/bypass, safe stopped-only structural actions, scroll/resize dividers and one Undo | P3 |
| Routing menus | `Desktop.cpp:1028`, `Desktop.cpp:1059` | Master/bus/hardware outputs, graph validation, stopped policy, SafePointer/store guards; no graph redesign | P3/P4 |
| VST3 browser | `Desktop.cpp:926`, `Desktop.cpp:978` | Vendor tree, search, selection, scan/cancel/import; внешний vendor UI исключён | P4 |
| Files browser | `Desktop.cpp:950`, `Desktop.cpp:966` | Folders/Up/path/breadcrumbs, async list, preview, WAV/MIDI multi-selection and drag/drop | P4 |
| AI tab / settings | `AiAssistant.cpp`, `AiProvider.cpp`, `Desktop.cpp:927` | Suggest/Edit/Auto permissions and recording/performance guards; network behavior не менять ради внешнего вида | P4 |
| Docked clip editors | `MidiEditor.cpp`, `ControllerEditor.h:6`, Desktop dock | MIDI/audio/controller editor focus, F2/F3, timeline alignment, close/reopen and project Undo | P2/P4 |
| Host chrome | `Editors.cpp:20`, `Editors.cpp:319`, `NativeEditorSpace.h:8` | Native owner, preset A/B/A in place, lifetime/generation guards; no global Topmost, no vendor child restyling | P3/P4 |
| Dialogs / context menus | `Desktop.cpp:133`, PopupMenu/PresetMenuTheme, audio settings and project dialogs | Modal keyboard guards, Enter/Escape, safe async owner, menus and mixer typography exceptions | P4 |
| Inspector | Dedicated Inspector component отсутствует в активном JUCE Desktop | Текущие функции распределены между track mini-strips, Mixer и docked editor. Не заявлять существующим отдельный Inspector и не изобретать новый workflow в P1 | P4, решение отдельно |
| Live Mode / show | `LIVE_WORKFLOW.md`, `UI_UX_CONCEPT.md`; enum Workspace::live существует | Полного show/setlist/.mrlive performance экрана в текущем JUCE runtime нет. Сохранённое workspace=live при startup заменяется Arrange (`Desktop.cpp:91`); resized различает только Mix/Edit и обычный Arrange. Это не реализованный Live UI | P5 после готовности product workflow |

## Шрифты, роли и размеры as-built

| Правило | Текущая реализация | Условие миграции |
|---|---|---|
| Общие buttons | Segoe UI14 logical / horizontal scale1.04; высота≤22 →13 | Не заменять глобально из scoped P1 |
| Mixer / strip buttons | Segoe UI12 / scale1; properties flatMixerButton/stripFont | Принятое исключение #145/#146, сохранить до отдельного P3 |
| Меню | Theme::channelFont12, минимум row22; PresetMenuTheme сохраняет собственную typography | Не смешивать с body14; P4 отдельная проверка |
| Body/detail | Theme::detailFont12; browserFont14/1.04; label roles зависят от parent | Кириллица, unit/value и long labels в регрессиях |
| Accepted colors | canvas/panel #212830, Arrange #262E37, card #2D353E, button #2F3841, text #DCE3EA | Target UDS #111720/#171E28/#222C3C/#2D394E и #91BBEE применяются поэтапно через существующий Theme |
| Верхняя геометрия | menu26; toolbar y34/h28; context y66/h22; content starts y94 | Нет автоматического применения всего UDS к Desktop.theme: это затронет дочерние окна |
| Нижняя геометрия | transport42x30; footer58 при width<1500, иначе42; Edit/Mix/BROWS78x30 | В P1 предложить coherent grouping, минимум hit area32 где возможно, без потери команд |
| Mixer / browser | strips86, шаг88; faderHeight145; browser clamp200..700 | P1 не меняет strip/fader mapping, meters, divider constraints |
| Existing refresh | Desktop30Hz, Files browser10Hz; native Reverb20Hz; binding15Hz | Это UI timers; не переносить работу в audio callback и не заявлять FPS/xrun измерением этих частот |

## Библиотека для повторного использования

`assets/ui/mr-uds/manifest.json`: 65 SVG, все пути существуют. Генератор не запускался. `svg/icons/` уже содержит plus/power/menu/settings/routing/save/chevrons; MRControls использует canonical assets. `assets/ui/icons/transport/` содержит 11 SVG: play/pause/stop/record/previous/next/loop/metronome/rewind/fast-forward/return-to-start; они уже загружаются и кешируются Theme.

Kit готов: DesignTokens / instance-scoped Theme, MRKnob/MRFader/MRButton/MRToggle/MRSelector/MRValueField и общий MRControlBinding. Это точки внедрения, а не повод параллельно рисовать новый набор widgets. Legacy Mixer Handle/InsertButton/SendLevel нельзя менять автоматически вместе с shell. SVG/controls sketches не являются обязательной заменой программного renderer MRKnob.

## Свежие before-снимки и проверки P0

Локальный QA root: `build/asio-local/validation/ui205-p0/`.

- Принятый executable запускался с `--reverb-kit-smoke` в `matrix/`: exit0, 17.7830655s wall time. Реальные private Desktop/IR fixtures, без user prefs и physical device reconnect.
- С `--j3-smoke --fixture <local Release/mrs_vst3_fixture.vst3>` в `j3/`: exit0, 47.6065916s wall time. Локальный synthetic VST3, не TH-U/Nuro.
- Всего242 PNG: matrix84 (72 DAW +12 native pilot), J3158. Все PNG декодированы, размеры и SHA-256 записаны в screenshots.json. Это количество файлов, не количество вручную просмотренных окон или CTest cases.
- Матрица DAW: Mixer+VST3 / Arrange+VST3 / Arrange+Files ×1200x700/1400x850/1800x1000 ×100/125/150/200%. 36 пар accepted/pilot-flag совпали pixel-for-pixel; pilot flag не перекрасил DAW.
- J3 содержит текущие Home/hover, audio/MIDI/controller editor, chord/section/tempo lanes, selected Browser, Files, Mixer bypass/height, host pin/preset selector/popup и другие before states. Большинство этих дополнительных snapshot paths —100/150%; отдельный section snapshot200%. Не утверждается полный DPI/state Cartesian product для всех popup/dialog.
- Визуально просмотрены: Mix minimum100%, Arrange nominal125%, Files nominal150%, Mix maximum200%, preset popup100%. Для остальных доступны оригиналы и индекс; это не утверждение ручной визуальной приёмки всех242 PNG.
- Hardware mixed-DPI/monitor transfer, physical ASIO/audio/xruns, screen reader и полный четырёхмасштабный захват всех OS dialogs: NOT TESTED. Live-show/Inspector screenshots: N/A, соответствующих экранов нет.
- Артефакты: `fresh-smokes.json`, `screenshots.json`, `baseline-comparison.json`, `preservation-before/after.json`, `review.html`, `review.md`.

## Результат аудита и границы P1

1. As-built остаётся принятым graphite UI. UDS roles уже есть, но значительная часть Desktop/Arrangement/Browser рисует legacy constants напрямую. Простое applyPreview на общий Desktop.theme перекрасит также Mixer/host/native children — слишком широкое изменение для P1.
2. Shell разделён на верхние tools/context и нижние transport/workspace controls; при узком окне footer меняет высоту. Первый миграционный кандидат должен улучшить иерархию и grouping этих существующих команд в scoped области, сохранив keyboard/Space и transport semantics.
3. Body12 и mixer12 — текущая принятая плотность, не вновь найденная ошибка. Масштабируемое отображение и контраст нужно оценить в P1/P3, не переоткрывая #145/#146.
4. Menus используют собственный размер/font policy; textDialog создаёт JUCE AlertWindow без явного scoped Theme. Унификация dialogs/menus и проверка owner/lifetime относится к P4, не к P1.
5. Широкие Arrange/Mixer пустые области и границы dock/browser являются реальным baseline; перенос layout/track densities относится к P2/P3, не к глобальной перекраске shell.
6. Live — целевой workflow, не готовая страница. P5 может стилизовать только реально реализованный show UI и moving Chord Track; аудиту нельзя подменять его макетом или ещё одним движком. Отсутствующий Inspector также требует явного решения в P4.

Рекомендуемая первая поставка P1: scoped shell/navigation/transport на существующем Theme и canonical SVG, ясные idle/playing/paused/recording/error states из текущего host state; сохранение всех commands/shortcuts/focus/native ownership; без изменения Mixer/Arrange/Browser/принятых native editors и без нового Live button/show runtime. Отдельные before/after и проверка100/125/150/200%, min/normal/max; перенос defaults только после приёмки этого среза.

P0 не закрывает #205. P0 ожидает приёмки; P1 начнётся только по отдельной команде пользователя. #206, #208 и MR Rig автоматически не запускались. Source/build/assets остаются локально, GitHub — только docs/issues [skip ci].
