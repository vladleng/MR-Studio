# MR Cab 2.0 — Codex first-slice handoff (issue #183)

**Это документ для старта после отдельной команды пользователя, не свидетельство реализации.**
**Версия плана:** 2026-10-14. **Parent:** [#182](https://github.com/vladleng/MR-Studio/issues/182).
**Начинать строго с:** [#183 — аудит Legacy Cab / MR Reverb и локального Celestion](https://github.com/vladleng/MR-Studio/issues/183).

## Read first and preserve

1. `AGENTS.md`, `docs/PROJECT_CONTEXT.md`, `docs/LOCAL_BUILD_POLICY.md`. `git status --short`, branch/HEAD, current local source vs GitHub docs main. **Не checkout/reset/pull поверх незавершённой работы, не затирать код других чатов.**
2. `docs/MR_CAB_COLLECTIONS.md` — канон; `docs/MR_CAB_ROADMAP.md` — последовательность. Далее `docs/CAB_IR.md`, `docs/DSP_MODELING.md`, `docs/MR_REVERB.md`, `docs/REVERB_LIBRARY_IMPLEMENTATION.md`, `docs/REVERB_EDITOR_IMPLEMENTATION.md`.
3. `docs/PROCESSING_RELIABILITY.md`, `docs/ENGINE_PERFORMANCE_PLAN.md`, `docs/NATIVE_INSERTS.md`, `docs/PERSISTENCE.md`, `docs/VERSIONING.md`. Relevant local skills: mr-feature-implementation, mr-realtime-audio-safety, mr-studio-ui-design-system, mr-studio-build-test; read their SKILL.md as appropriate.
4. Closed #23 and #128 = **existing accepted foundation**, do not reopen; #111 = parent category. Work under new #182, children #183–#188.

## Local discovery: Celestion as first real library

Do **not** assume paths from this document. The user has already downloaded Celestion WAVs. Locate through user-provided directory or local file-picker; **do not download/redownload, redistribute or publish vendor WAV**. Example screenshot tree (actual local inventory required):
```text
Vox V412 BN 412 C/
  Neumann TLM 107 Room/
  Royer R-121/
  Sennheiser MD421/
  Shure SM57/
  Vox V412 BN 412 C Hi-Gn 121+57 Celestion.wav
  Vox V412 BN 412 C Hi-Gn 421+57 Celestion.wav
  Vox V412 BN 412 C Hi-Gn 421+121 Celestion.wav
  Vox V412 BN 412 C Hi-Gn All Celestion.wav
  Vox V412 BN 412 C Hi-Gn All+Room Mono Celestion.wav
  Vox V412 BN 412 C Hi-Gn All+Room Stereo Celestion.wav
  ... Lo-Gn equivalents ...
```
Names are **examples from a screenshot**, not a claim about deeper files or all available variants. Inspect actual WAV file headers, channel count, sample rate, duration, names/nesting, duplicate rates, metadata/content rights. At audit stage retain **only metadata and locally stored logs**, not copyrighted PCM in GitHub issues/docs/test assets.

## Утверждённые рабочие IR-профили перед инвентаризацией

Смотри канон [MR_CAB_COLLECTIONS.md, §3.4](MR_CAB_COLLECTIONS.md). Пользователь выбрал:
- `48.0 kHz / 200 ms` — **основной** кабинет и заводские dry/close-mic ready mixes, основа Studio/Record/Live.
- `48.0 kHz / 500 ms` — прежде всего Room/Room-included и A/B 200↔500, если реально присутствуют полезные поздние отражения.
- `44.1 kHz` (оба хвоста) — только если нужны exact-rate исходники для проектов 44.1; `96` и `88.2 kHz` — опциональные original packs для соответствующих проектов/benchmarks, **не MVP mandatory**.
- Runtime sample rate всегда от проекта. Выбирать exact rate того же микрофона/микса/Room/длительности или worker resample; **не заменять другую длину/IR молча**. 500 ms — длина отклика, не 500 ms delay. Не обещать CPU/latency без измерений.
- Не удалять/конвертировать исходную полную Celestion; можно пользоваться отдельной рабочей копией 48k 200/500 с сохранением нужных Room вариантов.

**В P1** обязательно проверить, действительно ли есть 200/500 каждого интересующего файла, и сверить 48k/200↔500 на одинаковом Mic/position/Room; записать sample counts, header rates, хвосты, unknown/missing combinations и план A/B. Анализ не является автоматической подрезкой хвостов. Не сканировать/копировать весь пакет без предоставленного пользователем локального пути.

## Deliverables of P1 / #183

- **Audited code inventory:** classes/ownership & safe reuse mapping for Cab IR, MR Reverb library/loader/editor/state, shared native graph and project persistence. Verify local implementation before choosing IDs/schema.
- **Actual Celestion inventory:** per cabinet counts, supported WAV formats, sample rate variants, mic/positions, factory mixes and room variants, ambiguous and unsupported, with evidence from local files.
- **Design decisions:** selector behavior for availability, premixed vs component, room-included metadata, how duplicate rates are grouped, generic manifest v1 draft; explicit preserved old Cab IR/Rev contract.
- **Proposed implementation and tests** for next P2/#184: synthetic filenames/IR generated in local test fixture (copyright-safe); opt-in test on the user's local library; no long-running CI and no copying vendor WAV.
- If user did not provide path or local access unavailable, inventory is **BLOCKED**, not fabricated. Complete safe code audit and request exactly missing path.

**P1 acceptance = audit report**, no fabricated GUI/build/pass claims. User approval required before #184.

## Planned P2–P6 after P1 accepted

- [#184](https://github.com/vladleng/MR-Studio/issues/184): generic manifest/celestion parser + preview.
- [#185](https://github.com/vladleng/MR-Studio/issues/185): Simple UI like Reverb, preset/state, active single WAV, smooth switching.
- [#186](https://github.com/vladleng/MR-Studio/issues/186): Advanced multi-mic/phase/room only when components actually present.
- [#187](https://github.com/vladleng/MR-Studio/issues/187): TONEX WAV/generic importer, manual mappings, opt-in user packs.
- [#188](https://github.com/vladleng/MR-Studio/issues/188): future modular slots specification, final tests and acceptance.

**Hard stop:** do not implement Amp/Pedal/NAM nor decode TONEX .txm/.txp. Do not automatically start future stage. Runtime convolver owned by SHARED Engine; no UI/network/disk/alloc/mutex in audio callback; raw capture unchanged, proper PDC and versioned project/Undo.

## Handoff report template

```text
MR Cab P1/#183 status: AUDIT / BLOCKED / ACCEPTED
Local checkout branch/HEAD/dirty:
Current baseline: Legacy Cab/IR/Reverb classes & state versions:
User Celestion directory: [local only; avoid full personal paths in public GitHub issue]
WAV inventory counts/modes/supported/rate/mics:
Known ambiguity / missing facets:
Reuse and no-regression design:
Next #184 contract proposal:
Code edits/build/tests: NONE unless separately requested
Physical listening/ASIO: NOT RUN unless tested
GitHub documentation issue updates: ...
Outstanding / ask user:
```

**Publication:** issue/docs only, `[skip ci]`, no source push/PR/merge/Actions; always avoid protected content. All changes to documentation preserve more recent work and current context at top of `PROJECT_CONTEXT.md`.
