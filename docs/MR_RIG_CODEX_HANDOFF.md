# MR Rig — Codex Handoff (единственный активный гитарный roadmap)

**2026-10-15:** [#197](https://github.com/vladleng/MR-Studio/issues/197) поглощает дальнейшую разработку [#182 MR Cab](https://github.com/vladleng/MR-Studio/issues/182). **#182 и неисполненные #186–188 остановлены; не запускать их реализации.** Завершены и приняты #183/#184/#185 и **#196, включая 0.4b upd3 Native Editors** (исторические свидетельства сохраняются). **Весь будущий гитарный scope — #198–#204 MR Rig.**

## Начало рабочего чата — дешёвый, безопасный протокол

1. Прочитай локальный `AGENTS.md`, **назначенный Issue** и релевантные разделы [MR_RIG_ROADMAP.md](MR_RIG_ROADMAP.md). Для P0 #198 открой [MR_RIG_ARCHITECTURE.md](MR_RIG_ARCHITECTURE.md) §4, по необходимости [MR_RIG_GUI.md](MR_RIG_GUI.md), [MR_RIG_LIBRARY_PRESETS.md](MR_RIG_LIBRARY_PRESETS.md), [MR_RIG_AUDIO_PEDALS.md](MR_RIG_AUDIO_PEDALS.md). Не читай все MD целиком без нужды.
2. `git status --short`, branch, HEAD; нет reset/stash/checkout поверх чужой незавершённой работы. **Локальная версия исходников может быть новее GitHub**.
3. Из старых documents читать точечно лишь то, что помогает подтвердить принятую 0.4b upd3: [Full Combo #196 delivery](MR_CAB_FULL_COMBO_196_DELIVERY.md), [native editor report](NATIVE_EFFECT_EDITOR_UNIFICATION.md), текущий Cab state/ID. [#182](https://github.com/vladleng/MR-Studio/issues/182) и `MR_CAB_CODEX_HANDOFF` — **архив**, не инструкция начать P4–P6.
4. Current Issue #198 — **только read-only аудит** без code/build/tests. Остальные #199–204 только после приёмки предыдущего/разрешения пользователя.

## Аудит P0 #198 — что выяснить

- Реальные classes/files IR convolution/CabManifest/WAV Full Combo/source identity/GUI native editor/preset/Undo и их status в локальном checkout; никаких новых контейнеров/скрытых параллельных DSP без проверки.
- Варианты (A) safe upgrade existing Cab insert сохраняя ID/state/presets; (B) separate MR Rig insert с явной migration. Предложить user выбрать, не переименовывать идентификаторы автоматически.
- Схема `SourceAsset` vs `ToneModel` (name/artwork/genre/role) vs `RigPreset` (module order/params). IR Models, NAM Models, My Rigs. Готовый WAV звук выбирается через обложку/модель; `Import My IR` вторичен.
- Что минимально нужно из старого #187: selected custom WAV/path mapping, role manually cabinet/full_combo/unknown, valid WAV sample rates, size/integrity, safe missing/relink; **не создавать** универсальный TONEX/third-party кабинетный парсер, composite mic DSP (#186) или отдельный Cab release QA (#188).
- DSP graph: `Input → Pre FX → Core(WAV/Neural) → optional single Cabinet for NAM Amp Only → Post FX → Output`, reuse SHARED processor. User's Polytone/Fender WAV is linear, NAM nonlinear dedicated backend. Full Combo WAV and NAM Full Rig have no extra auto-Cab.
- UI: small flat JUCE Theme, photo/placeholder central Rig Core, pedal tiles left/right with contextual inspector, no fake Amp Gain controls for WAV, one owner/window, no model/preset reset.
- Report actual local branch/commit/current baseline, risks and adapter map, per-stage tests/CPU/RT feasibility. **No implementation in audit**, stop for user decision.

## Следующие stages

- **[#199](https://github.com/vladleng/MR-Studio/issues/199)** — curated Tone Models, optional user WAV import (transferred minimum #187), licensed artwork, models != rigs; not a general cabinet browser.
- **[#200](https://github.com/vladleng/MR-Studio/issues/200)** — visual Rig Shell and minimal RigPreset over existing WAV core.
- **[#201](https://github.com/vladleng/MR-Studio/issues/201)** — real Pre/Post module chain, host graph commands, Undo/PDC (from useful #188 parts).
- **[#202](https://github.com/vladleng/MR-Studio/issues/202)** — real native pedal DSP, approved minimal subset first.
- **[#203](https://github.com/vladleng/MR-Studio/issues/203)** — native NAM model engine (supported architectures measured), Full Rig / Amp Only + optional single Cab.
- **[#204](https://github.com/vladleng/MR-Studio/issues/204)** — full Rig acceptance across Studio/Live, versioned state, no regressions. #186 multi-mic cancelled, not prerequisite.

## Non-negotiable rules

No GitHub source push/merge/Actions/PR. Local code/build/tests/packages only. GitHub issues/docs [skip ci], no unauthorized commercial WAV/NAM/artwork copied. For RT DSP, no allocations/locks/I/O/UI in audio callback; measure max callback/xruns, differentiate high-buffer studio mix vs low-buffer recording/Live. No claim prior CTest covers later binaries. Stage gate: user acceptance. Avoid expensive blanket read of PROJECT_CONTEXT/LOCAL_BUILD_POLICY/entire docs; use search + focused ranges per Issue.

## Готовое сообщение Codex

> MR Cab #182 объединён с MR Rig #197; отдельные Cab задачи #186–188 прекращены. Прочитай обновлённый AGENTS.md и **только нужные разделы** MR_RIG_ROADMAP, MR_RIG_ARCHITECTURE и MR_RIG_CODEX_HANDOFF. Последний принятый Cab/Full Combo baseline — #196, версия 0.4b upd3. Проведи **только P0 аудит #198** локального кода: миграция ID/проектов, переиспользование WAV конвольвера и editor, ToneModel/RigPreset и будущие Pre/Core/Post slots. Не выполняй старый roadmap MR Cab; не пиши код и не запускай сборку. Отчёт и рекомендации передай на мою приёмку.
