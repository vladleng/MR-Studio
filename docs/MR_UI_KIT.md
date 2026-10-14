# MR UI Kit — общий JUCE Design System и компоненты для MR Studio

**Документ создан:** 2026-10-14. **Статус:** план, реализация MR UI Kit не начата. **Parent:** [#189](https://github.com/vladleng/MR-Studio/issues/189) (категория [#78 MRS FEATURES](https://github.com/vladleng/MR-Studio/issues/78)); это общая UI-инфраструктура, НЕ модуль #111 Native Plugins.
**Gallery contract:** [MR_UI_GALLERY.md](MR_UI_GALLERY.md); **этапы:** [MR_UI_KIT_ROADMAP.md](MR_UI_KIT_ROADMAP.md); **handoff:** [MR_UI_KIT_CODEX_HANDOFF.md](MR_UI_KIT_CODEX_HANDOFF.md).
**База:** [UI_REFERENCE_UPD9.md](UI_REFERENCE_UPD9.md) (действующие `ui::Theme` / `Controls.h`; последние уточнения #145/#146), [REVERB_EDITOR_IMPLEMENTATION.md](REVERB_EDITOR_IMPLEMENTATION.md), [MR_REVERB.md](MR_REVERB.md), [NATIVE_INSERTS.md](NATIVE_INSERTS.md), [AGENTS.md](../AGENTS.md). **Не переписывать принятую тему из эстетического вкуса.**

## 1. Цель / product boundary

Одна библиотека **MR UI Kit** — controls, semantic tokens, gestures и binding contracts, и отдельная **MR UI Gallery** — интерактивная витрина/тестовый стенд для live preview и выбора стилевых решений. Однажды проверенный компонент применяется в DAW и редакторах MR Cab, MR Strip, Reverb, Saturator, Amp, Harma Waves; каждый editor сохраняет собственную информационную архитектуру/расположение. Не превращать все плагины в одинаковый rack.

**Реальная исходная точка:** в принятом UI есть `ui::Theme`, `Controls.h`, host chrome, Reverb browser/editor, native insert shell, сохранившаяся typographic exception для mixer buttons 12/scale1; обычные buttons Segoe UI 14 logical, compact <=22 →13, scale 1.04 (по UI_REFERENCE_UPD9 #146). Tokens для reference theme: surface #212830, raised control #2D353E, card in accepted Reverb #2D353E, button accepted #2F3841, tree/list #212830, canvas Arrange #262E37; эти значения **не заменять автоматически**. Проверить актуальную локальную реализацию прежде любого изменения.

### Принципы
- Native JUCE C++ controls и общий `juce::LookAndFeel` **интегрированы с существующей Theme**, не создавать альтернативный источник палитры. Имена исходных классов/папок ниже — **предложение**, локальный audit обязателен.
- Единство визуальных правил, но разные размеры/варианты: density compact/comfortable, knob small/standard/large, contextual toolbar/inspector layouts.
- Плоские, современные, визуально компактные элементы; светлые надписи, restrained accent. Свечение только как тонкий доступный feedback и без дорогого непрерывного анимирования, если нужно.
- Доступность: все meaningful states разборчивы, keyboard focus, tab navigation, screen-reader name/value/units/role, distinct warning/error/armed/recording, достаточный effective hit target. Размер ручки ≠ область взаимодействия.
- Действия пользователя контролируют хост, нет обходов commands/Undo/automation; одиночный gesture = одно корректное undoable commit.
- Динамические/animated controls должны обновлять UI за UI timer, не из audio callback, при отсутствии editor DSP продолжает работать. GUI refresh не блокирует DAW и не открывает аудиоустройство.

## 2. Предлагаемые слои (адаптировать к локальному коду)

```text
existing ui::Theme / Controls.h   accepted tokens, current typography
      ↓  (extension / adapter, not wholesale replacement)
MRThemeRoles + MRMetrics + MRLookAndFeel
      ↓
MRButton / MRKnob / MRFader / MRSelector / MRToggle / MRMeter / MRGraph / MRPanel
      ↓
MRControlBinding (host parameter metadata, gestures, model/commands/history/automation)
      ↓
MRNativeEditorShell / plugin-specific layouts / DAW inspectors
      ↘
       MR UI Gallery (local showcase + preview overrides; no audio dependency)
```

Экспонировать ровно один public interface для параметров/основных controls. Разделять `style role/size` (передаётся theme) и `functional semantic` (value/format/gesture commands). GUI Preview отображает отдельное sandbox value, не пишет в project model.

### Условное разбиение исходников — после P1 audit

```text
.../ui/design/MRThemeRoles.*
.../ui/design/MRMetrics.*
.../ui/lookandfeel/MRLookAndFeel.*
.../ui/controls/{MRKnob,MRFader,MRButton,MRToggle,MRSelector,MRMeter,MRGraph,MRPanel}.*
.../ui/bindings/MRControlBinding.*
.../ui/gallery/{MRUIGallery,MRGalleryComponentCatalog,MRGalleryInspector}.*
```

Нельзя переименовывать/перемещать уже работающие `ui::Theme`/`Controls.h` и renderer классы до полного поиска usages и migration plan. Существующие программы могут содержать дубли; P1 должен оценить стоимость общего кода, а не сделать большой рефакторинг.

## 3. Semantic tokens и стиль

**Токены, а не захардкоженные цвета в отдельном эффекте.**

| Группа | Примеры | Семантика |
|---|---|---|
| Colors | `surface.window/panel/raised`, `border.subtle/active`, `text.primary/secondary`, `accent.focus/selected/power`, `meter.peak/gr`, `record.warning` | Роли, связываются с текущей accepted Theme |
| Typography | `font.control.default/compact`, `font.value`, `font.heading`, `font.menu` | Правила #146, separate mixer exception, Windows font fallback |
| Spacing/layout | `space.xs/sm/md/lg`, `control.height`, `hitTarget`, `module.gap` | Компактно, но не терять hit area, resize и минимум окна |
| Shape/painting | `radius`, `stroke`, `knob.arcWidth`, `indicatorSize`, `iconStroke` | Consistent visual rhythm без bitmap dependencies |
| Motion | `hoverFadeMs`, `dragHighlight`, `meterRefreshHz` | GUI-only bounded redraw; reduced-motion support |

Versioned default token schema с whitelisted editable roles и bound ranges; Gallery может накладывать **temporary preview override** без пересоздания state плагина. Пресеты цвета/плотности **не** делают отдельные медиа-пресеты плагинов. `Reset Defaults` восстанавливает текущий approved MRS reference, `Save Draft` сохраняет только локальный пользовательский draft, `Apply Global` — только после явного согласия пользователя и совместимости. Не делать hot-swap LookAndFeel ownership с dangling pointers.

## 4. Shared control catalog (минимальный scope)

| Компонент | Варианты | Инварианты |
|---|---|---|
| MR Knob | Mini / Standard / Large, arc/line indicator | Ctrl fine, double-click default, drag-relative, wheel policy, numeric edit, bounded values |
| MR Fader | Horizontal / Vertical, bipolar/centered | Actual handle drag, keyboard arrows, units, readable scale, no accidental container drag |
| MR Button | Icon-only, text, primary/secondary, momentary | Hover/pressed/disabled/focus, accessible name, proper toggle semantics |
| MR Switch / Toggle | On/Off, A/B, segmented | One state owner, no style-dependent parameter semantics |
| MR Selector | Dropdown, segmented, preset/select list | Unicode, keyboard search, menu widths, accepted font #146 |
| MR Value Field | Editable units dB/Hz/ms/%/ratio, optional logarithmic | Validation, localized text, min/max, precision, cancel/commit |
| MR Meter | Peak/RMS/GR, simple mono/stereo | Off-thread-safe mailbox/snapshot; sample->UI data, never DSP callback redraw |
| MR Graph | EQ, dynamics transfer, response, envelope preview | UI samples reflect approved parameters; scientific graphs must not fake DSP measurements |
| MR Panel / Module | Group header, collapsible/advanced, tab, separator | Reflow/responsive, scroll/focus, no inactive fake audio processing |
| Shared Editor Shell | Title, bypass, preset toolbar, status/help/error | Reuse current native insert shell; don't duplicate business logic |

**Assets:** prefer vector JUCE Paths / SVG for icons with permitted licensing, no raster knobs required. No unlicensed fonts redistribution. Gallery uses only UI illustrations and synthetic control values.

## 5. Input, parameter binding, Undo и RT

Контракт для every interactive control:
1. `BeginGesture` UI thread — lock parameter/instance ID (verified current host API), save old value once, show preview.
2. `UpdatePreview` — validate/clamp/map linear/log/bipolar value; send to existing host parameter mailbox without building project command for every mouse move; optionally current automation write policy, only when explicitly supported.
3. `EndGesture` — once commits host command/Undo/automation (current behavior may depend on setting/mode). `Cancel` returns previous state safely.
4. Display value in physical units and exact host parameter metadata; value entry/keyboard/wheel/reset obey same validation/commit path. `bypass` often has structural vs realtime modes; reuse established semantics.
5. Editor destroy/reopen/multi-instance/UI thread/graph change cannot leave stale callbacks or hanging pointers. No allocations/locks/file I/O/widget access in the **audio callback**. Current DAW transport Space shortcuts must retain accepted behavior when text edit is not active.

Do not invent generic parameter writes to third-party VST3 plugin GUIs. For our hosted generic parameter editor only inspect exposed VST3 parameter APIs and plugin host policy.

## 6. Gallery / tests / roll-out

[MR_UI_GALLERY.md](MR_UI_GALLERY.md) owns UI Gallery exact UX, state, profiles and visual acceptance. Gallery works standalone in-app without starting audio backend; previews values only; no project mutations or plugin instantiation by default.

**Testing policy:**
- Small paint/font change: build + affected GUI smoke + screenshot diff; visual inspection 100/125/150/200%.
- Input/parameter binding: targeted unit/command/history/automation regression and per-editor focus events.
- Shared Theme/LookAndFeel broad changes: affected editor/DAW screen matrix and wider CTest where necessary.
- Candidate user acceptance: full CTest once + full affected GUI smoke, manual focus/DPI/hit gesture in packaged candidate, explicit NOT TESTED for unavailable physical mixed DPI.
- Existing #145/#146 and MR Reverb / MR Cab (in progress) accepted GUI remains unchanged until opt-in pilot and separate acceptance. Do not regress Windows 100%/150% keyboard and Segoe font exceptions.

**Rollout:** audit → tokens → controls → Gallery → host bindings → pilot adopter + QA → user approval → subsequent native plugins use kit. Don't block current MR Cab #185 before Kit is ready. No automatic mass migration of old native plugin GUIs; legacy old Cab migration not required per current [MR_CAB_ROADMAP](MR_CAB_ROADMAP.md) user override, but accepted Reverb and existing DAW project behavior must remain stable.

## 7. Out of scope

New DSP, synths, instruments, amp modeling, VST3 third-party editor replacement, audio thread redesign, HTML/CSS frontend, AI-image skins, automatic changing all global widgets, custom font file redistribution, marketplace/theming engine, zero-latency claims. All code/builds/tests/packages local; GitHub issues/docs only [skip ci]; request before starting implementation.
