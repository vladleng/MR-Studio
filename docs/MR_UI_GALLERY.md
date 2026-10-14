# MR UI Gallery — интерактивная витрина/конструктор JUCE-компонентов

**Parent:** [#189](https://github.com/vladleng/MR-Studio/issues/189). **MR UI Kit:** [MR_UI_KIT.md](MR_UI_KIT.md); **roadmap:** [MR_UI_KIT_ROADMAP.md](MR_UI_KIT_ROADMAP.md). **Статус:** design / not implemented, 2026-10-14.

## 1. Зачем Gallery

В существующем MR Studio открыть отдельное **MR UI Gallery** (development/design-preview panel) и сразу видеть будущие controls нативных плагинов без необходимости пересобирать MR Cab/MR Reverb/MR Strip после каждой косметической итерации. Gallery также служит **visual/interaction regression fixture**.

### Базовый layout

```text
┌──────────────────────────── MR UI Gallery ──────────────────────────────┐
│ Components | Tokens | Gallery Presets | Tests | Reset | Save Draft      │
├────────────────┬───────────────────────────────────────┬──────────────┤
│ Component Tree │ Live Preview (compact, normal, wide) │ Inspector     │
│  Buttons       │ Knob/Switch/Fader/Selector + states  │ Size/spacing  │
│  Knobs         │ Normal Hover Press Disabled Focus   │ Palette       │
│  Faders        │ Semantic labels/value, unit preview │ Fonts         │
│  Switches      │                                          States        │
│  Selectors     │ Dynamic visible clipping/focus rect │ Border/icon   │
│  Meters/Graphs │                                       │ 100/125/150/ │
│  Panels        │                                       │ 200% DPI      │
├────────────────┴───────────────────────────────────────┴──────────────┤
│ Theme baseline reference · Unsaved preview · Apply/Reset status       │
└───────────────────────────────────────────────────────────────────────┘
```

Это структурный wireframe, не обязательный пиксель-в-пиксель контракт. Ориентир визуального стиля — accepted `ui::Theme` и `docs/UI_REFERENCE_UPD9.md` плюс MR Reverb. Компактный размер, адаптивная сетка/скроллинг, mouse/keyboard navigation, no clipping.

## 2. Пользовательские сценарии

1. Выбрать на левой панели тип `Knob`. Справа настроить `mini / standard / large`, форму индикатора, размер, шаг, font. В центре одновременно видеть весь knob family и разные значения (`min`, `mid`, `max`, `-∞` для dB, где уместно).
2. Сравнить compact/comfortable density и варианты оформления с одним набором значений на 100/125/150/200% UI scale. **Логический preview zoom не заменяет тест на физическом DPI Windows**; отдельно отобразить это ограничение.
3. Переключать визуальное состояние component: Normal/Hover/Pressed/Disabled/Focused/Selected/Warning/Error. Для интерактивной версии allow real pointer hover/focus/press — принудительный вид статуса не должен отправлять команды или менять глобальные prefs.
4. Протестировать interaction: drag, wheel, Ctrl fine, double-click reset, keyboard keys, numeric input+Enter/Escape. Показать `current value`, `gesture phase`, число simulated commits и default/units. Gallery binding uses **sandbox parameter**, not project/DSP state. Explicit simulation mode for begin/update/end/cancel.
5. По желанию сохранить theme **Draft**: только text/JSON-like metadata: token values, density, preview selections, app/UI-kit schema version, accepted-default base version. Export/Import with whitelist, version migration and validation; no path traversal/scripts/fonts/executables. При закрытии с несохранённым preview — предупреждение/отмена. Reset вернуть approved defaults.
6. Отдельное действие `Apply to product` (в будущем) возможно только с проверкой UI regression и явным согласием. **MVP: никакого глобального auto-apply**; preview изолирован.

## 3. Настраиваемые параметры и пределы

| Category | Inspector | Safety |
|---|---|---|
| Geometry | width/height of visual control, knob diameter, fader track, padding, radius, stroke, icon size | min/max bounds, hit target and clipping checks, no negative/out-of-range |
| Density | compact/regular, grid columns/spacing, module padding, label spacing | preserves physical readable text and scroll/accessibility |
| Palette | surface, raised, border, text, accent, warning/error, meter | roles from existing Theme; contrast + distinctions across states |
| Typography | Segoe UI default/fallback, logical font size, weight/scale, value/meter labels | accepted button/menu/mixer exceptions; no arbitrary font packaging |
| States | standard, hover, pressed, disabled, selected, focus, error, warning | visible semantic cues and keyboard focus; no false functional status |
| Motion | hover transition / meter animation / focus ring | reduced motion, stable CPU/refresh and no RT side effects |
| DPI | 100%, 125%, 150%, 200%; layout min/nominal/wide | render snapshots + physical DPI manual matrix |
| Variants | knob modes, fader horizontal/vertical, icon/text buttons, menu, meter | selection solely changes preview design until application accepts |

**Do not make arbitrary CSS editor / JS scripting engine.** Gallery composes documented controls and finite state combinations. Large font sizes vs compact density must show warnings/clipping; don't silently crop units.

## 4. Architecture & isolation

- Gallery uses the same **real reusable JUCE components** as plugin editors (not separate mock implementations), but receives a scoped `PreviewThemeContext` and `PreviewParameterSource`; no global theme mutation and no DSP graph access.
- Opening it must not trigger loading VSTi, opening ASIO device, playing sound or registering plugin host audio node.
- Preview state independent of project state / current track / active insert / transport. Shutdown/reopen cancels timer and pointers safely.
- Real plugin controls bind via `MRControlBinding` to verified host APIs only in a later slice; Gallery may simulate gestures without writing commands.
- Meter sample data synthetic/visual demo only, an explicit `Demo` watermark/label: a fake meter cannot be shown as actual engine output.
- Optional gallery snapshot harness (headless/hidden JUCE): screenshots per component, density, scale and all meaningful states; baseline images/version changes **only after user visual acceptance**. Avoid image font binary files in repo.
- Prefs storage as app/user-local draft, whitelisted inputs bounded size, atomic safe write; importing drafts must not allocate unbounded bitmaps or instantiate external plugins.

## 5. Acceptance matrix

- [ ] Opens from an explicit menu/developer entry and works without audio engine/ASIO.
- [ ] **Same components** used in at least one pilot native editor (no duplicated demo copies), and theme scoped exclusively to preview until consent.
- [ ] Catalog includes all core controls (Knob, Fader, Button, Switch, Selector, Value, Meter, Graph, Panel) or has explicit deferred markers that do not lie.
- [ ] Each required control shows normal/hover/pressed/disabled/focus; toggles selected and errors; keyboard/tab/drag/typed validation works.
- [ ] Runtime preview refresh under palette/font/size/density changes; reset/draft save/reopen/import/export where in scope, with no mutation of DAW palette or project.
- [ ] 100/125/150/200% software zoom renders; width constraints and component clipping tested at min/nominal/wide, windows accessibility name/value focus. Physical monitor DPI annotated TESTED/NOT TESTED.
- [ ] No audio I/O/locks/alloc from Gallery in callback, no host parameter/model/Undo side effects from preview, event handlers cancel on window destroy.
- [ ] Regression on existing host/native MR Reverb/VST3 browser accepted GUI, full affected CTest/GUI smoke at acceptance gate; no staged screenshots represented as user acceptance.

## 6. Rollout and boundaries

First focus on core component family and visual workflow. Do **not** wait for all future meters/graphs to create an early interactive Gallery. Progressive slices let the user choose a favorite knob and button before MR Strip/Amp UI design.

Do not reopen accepted #145/#146; avoid forced update to MR Reverb and MR Cab while their own development continues. Full feature still requires user-approved design and separate implementation issues. Source/build/tests/gallery snapshots local; GitHub docs/issues only [skip ci].
