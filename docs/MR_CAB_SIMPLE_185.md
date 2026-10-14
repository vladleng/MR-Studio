# MR Cab P3/#185 — Simple editor / external state / smooth switching

2026-10-14. **FEATURE READY WITH MANUAL CHECK / ожидает приёмки #185.**
#184 принято пользователем: «#184 принимаю. Пререходи к #185»; closed completed.
#186 не начат. Совместимость старого Cab не обязательна по уточнению пользователя.

## Поставка

Local source **ae9800c05e501fdc5fcde24ee22ec1a6e4b4eb03**, branch
`mrs/0.2g-fix1-editor-space-local`. Version **0.4a MR Cab**.
Package `MR-Studio-0.4a-MR-Cab-JUCE-ASIO-Windows-local`:13 files/12 SHA256 hashes verified.
GUI SHA256 `05EB2DBBBA1E6227352CB303E844C502EC5F2690E37C5B2DEDE01023F6D41CF5`.
Matching PDB: `build/asio-local/deliveries/0.4a-mr-cab-185`.
Existing MR Studio desktop shortcut updated; icon retained, working directory package, empty args.
Accepted0.4 and prior package folders preserved. No vendor WAV included or modified.

## Пользовательский сценарий

Inserts → **Add MR Cab** → **Collection folder…** → выбрать существующий корень Celestion.
Cabinet / Mic-Mix / Position / Voicing / Room / Length и filename search
фильтруют реальные assets. Недоступная комбинация даёт пустой список, без подстановки звука.
Фильтры не переключают IR: click/Return выбирает один WAV. Favorites global.
Default length filter200ms при наличии; Room included/component предлагает500ms при наличии.
Сохраняется фактически загруженный вариант/frames, не только текущее состояние фильтров.

Exact project source rate предпочитается **для той же variant/length**;
иначе worker resamples выбранный исходник. Source Hz, ms, mono/diagonal stereo и project Hz видны
в active metadata. Hi/Lo — labels, не DSP gain. All+Room — один готовый WAV,
без второй Room-свёртки. RoomL/R остаются независимыми mono variants.
Gain/Mix/low cut/high cut/polarity/Bypass; Ctrl fine adjustment и double-click reset.
Нативные Save/Load preset + preset library; factory коммерческих IR нет.
IR selection async/no graph rebuild during playback. IR history/preset/structure
следуют принятой Pause/Stop policy; scalar preview/commit/Undo работает live.
Loading показывает предыдущее active audio; failure сохраняет last good kernel.
Fresh missing instance dry + visible missing/error; durable reference/name/length сохраняются.
Folder move/relink требует совпадения identity, без похожего-name substitution.

## Слои / архитектура

- **Model/persistence AFFECTED:** schema **23**, archive1; reader1–23.
  Append enum `InsertKind::cab_collection`; native processor ID `mrs.cab-v2`.
  `CAB_SELECTION 1` stores collection/variant IDs; reusable external `ReverbState/REVERB2`
  stores source name/rate/channels/frames and root-relative/absolute-fallback/size/digest.
  Cab controls use existing gain/ir control fields; decoded PCM is not authored by Cab2.
  Old apps cannot read schema23. No mandatory legacy Cab adapter/upgrade.
- **Engine/RT AFFECTED:** shared convolution runtime/atomic kernel handoff reused through
  `ReverbProcessor(true)`; default false retains MR Reverb semantics.
  Cab mode exposes its own parameter IDs and wet first-order HP/LP + gain/polarity/mix.
  Pre-delay/tone/gate disabled in Cab mode. Algorithmic latency0; source leading delay preserved.
  20ms scalar smoothing, existing80ms kernel overlap; no normalisation/trimming.
- **Workers/lifetime AFFECTED:** existing per-node ReverbLoader, graph lease, cancellation,
  generation guard, stop/join before graph replacement, off-thread retirement.
  Cab catalog separate from Reverb root; selection uses expected scan fingerprint
  through worker restore, rejecting changed assets before publication.
- **UI AFFECTED:** `CabBrowser.h` and native FxPanel/preset toolbar in current JUCE Theme.
  Minimum1000×500, default1000×550; labels/accessibility titles, raw filename/error metadata.
  Space uses existing native editor bridge/focus exclusions.
- **Commands/Undo AFFECTED:** poll successful active revision → central SetInserts;
  scalar gesture preview creates no history, release one command; native preset shell reused.
- **Global settings AFFECTED:** optional cabRoot/cabManifest/cabFavorites in existing ViewSettings
  v1 JSON; app-global metadata only, UTF-8 codec. Decode budget32MiB supports bounded manifests.
  Search/facet selections are transient. User projects/preferences not changed by smoke.
- **Studio/Live SHARED CORE:** one existing graph; Live suitability not measured.
- **Advanced/generic wizard DEFERRED:** #186/#187; no Amp/NAM/pedals/TONEX proprietary decoding.

## Проверки и ограничения

Release configure/full build PASS. **Full CTest148/148 PASS188.24s**
after UTF-8 correction. Final enhanced Cab/Reverb editor checks **2/2 PASS3.03s**;
packaged Cab editor smoke **exit0**. Loaded own synthetic200/500ms IR,
default200 selection, durable variant identity, slider preview/one Undo/Redo,
repeated editor lifetime and software snapshots100/150 inspected (no clipping).
No commercial samples in fixtures. Actual Hempback header/parser inventory remains68/68 from P2.

DSP: mono/stereo at44.1/48/96k, neutral amplitude, polarity, dry mix,
200/500ms kernels, final switch gain and per-channel discontinuity bound<0.02
for the synthetic constant-input change, host allocation/free0 during process and switching.
Desktop: external state roundtrip, IR history, scalar history, rates,
unchanged device open count for controls, failed fingerprint preserves active kernel.
Existing full suite covers Reverb, generic VST3, raw recording/PDC and project/editor regressions.
No special installed TH-U/Nuro rechecks.

Initial failures preserved in logs: Windows min/max macro collision, overlapping build PDB,
96k fixture insufficient smoothing time, accidental Windows1251 text conversion
(context initial chord failures in J3/diagnostics). Root causes fixed:
JUCE min/max, sequential builds, longer fixture render, explicit UTF-8 + restored original text.
Final preview stream truncates before writing (prevents appended stale PNG images).
No failures remaining; these early failures are not represented as PASS.

RT boundary: decode/hash/resample/FFT/ownership retirement NON-RT;
control publication/lifecycle RT-ADJACENT; convolution/handoff/ramps RT-CRITICAL.
One node producer, no new callback filesystem/UI/locks/heap operations.
**RT SAFE WITH MANUAL CHECK:** offline synthetic guarantees checked;
physical ASIO CPU/max callback/xruns, audible switching, long-session operation,
real Celestion A/B and physical mixed-monitor DPI **NOT RUN**.

## Ручная приёмка #185

В новом пакете выбрать предоставленную Hempback папку. Сравнить готовые121+57/421+57,
Hi/Lo и All+Room, проверить single selected WAV и200/500ms, play switching и controls.
Save/Open, native preset Save/Load, Undo/Redo; временно недоступный внешний IR
должен сохранить identity и показать Missing. Пользователь выполняет операции
над своими исходниками только по собственному решению; автоматические тесты их не меняли.
#185 remains open until user acceptance. Не запускать #186 автоматически.
