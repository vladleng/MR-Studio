# MR Separation Engine — контракт этапа #333

Родитель: [#74](https://github.com/vladleng/MR-Studio/issues/74).
Этап: [#333](https://github.com/vladleng/MR-Studio/issues/333), плановая версия
**MR Studio 0.9.1**; линия родителя **0.9.0**, следующие этапы 0.9.2…0.9.8.
Предыдущая линия Studio — 0.8.x (#331/#332); MR Live имеет независимую нумерацию.
Контракт #333 дополнен реализацией локального runner #334 для **MR Studio 0.9.2**.

**Уточнение пользователя 2026-11-02:** лицензия пока не блокирует разработку;
необходимые сборки выполнять самостоятельно, подтверждение нужно для merge в main.
Аудит лицензий сохраняется как незавершённый, разрешение на распространение весов
не заявляется. Для development smoke допускается явная загрузка official HTDemucs;
worker сам ничего не скачивает, Python/runtime/weights не входят в installer.

## Границы и точки интеграции

Только Studio DAW. Разделяется неизменяемый снимок dry audio выбранного клипа,
без track/master FX, mixer gain, fades, ARA processing и Live. Начальный путь:
stereo, положительная длина, unity time/pitch; для обработанного/растянутого
клипа — `unsupported_input`, пока отдельный согласованный freeze/render не даёт
точного соответствия времён. Нельзя молча разделять исходник вместо слышимого
обработанного клипа. Двухканальный WAV snapshot готовит host вне UI/audio callback.

| Слой | Решение и владелец |
| --- | --- |
| Jobs | Новый NON-RT separation service; единственный владелец состояний jobs и sidecar. Существующие `mrs::ai::ContextKey`/cancel/deadline из `core/include/mrs/ai.hpp` — образец, не расширять LLM text request бинарным аудио и не занимать его очередь длительным inference. |
| Model/Commands | `Clip::source`, `source_offset`, `length`, `stretch` в `core/include/mrs/core.hpp`; снимок захватывается control thread. Позднее один Command регистрирует результат и опциональные дорожки через существующий ProjectStore. |
| Persistence | `ProjectDocument` и atomic save в `core/include/mrs/persistence.hpp`, `core/persistence/src/files.cpp`. Durable asset metadata проектируется в #336; jobs/model cache не сериализуются как project state. |
| Engine | Существующие audio decode/read-ahead/graph publication. Ни Python, ни IPC, ни смена модели, ни ожидание worker не входят в callback. |
| UI/Undo | #336; в #333 нет меню, окон, Commands или миграций. Completion не меняет проект из worker. |
| Backend | `python-audio-separator` первым, MSST/remote только отдельными адаптерами. Общий job/result contract без зависимости Core от Python headers/ABI. |

## Проверенный upstream и Windows-карта

Аудит публичных исходников: **2026-11-02**. Ссылки [S1–S9] ниже закрепляют версии.
Чтение исходников подтверждает API/заявления upstream, а не исполнение на Windows.

| Компонент | Подтверждено источниками | Решение/непроверенная часть |
| --- | --- | --- |
| audio-separator | Release **0.48.0**, commit `4dd3477c0a8f5bfb0fb053c6ed444bb935cb150e`; MIT [S1,S2]. API `Separator(...)`, `load_model(model_filename=...)`, `separate(audio_file_path, custom_output_names=None)` возвращает пути. CLI `--list_models`, `--list_format json`, `--env_info`, `--download_model_only`, `--model_file_dir`, `--output_dir`, `--use_soundfile` [S1,S3]. | Использовать собственный IPC wrapper над Python API. CLI/log text не является стабильным machine protocol. Native progress/cancel contract в проверенном `Separator` не найден. |
| Python/packages | Python `>=3.10, !=3.14.1`; опубликованные зависимости широкие: numpy>=2, torch>=2.3,<3 для Windows, onnx-weekly, samplerate==0.1.0, scipy>=1.13,<2; CPU extra onnxruntime>=1.17; CUDA extra onnxruntime-gpu>=1.17; DML extra onnxruntime-directml>=1.17 + torch_directml [S2]. | Кандидат Windows x64 CPython 3.11, torch 2.8 CPU; **не готовый lock**. Poetry Windows constraint torch<2.9 отличается от широкого PEP621 metadata. В #334 resolve wheels в чистом Windows, затем pin всех wheels+hashes, SBOM и smoke. Нельзя выдавать pip install latest за воспроизводимую поставку. |
| CPU | Есть CPU device/ORT provider [S3]. | Обязательный отдельный CPU runtime; fp32, eager, thread limits. Время обработки/RAM не измерены. |
| CUDA | PyTorch CUDA для Demucs/RoFormer, ORT CUDA для MDX [S3]. | Отдельный runtime lock; probe реальной модели/операций, DLL/driver compatibility. Наличие NVIDIA не означает готовый backend. |
| DirectML | Opt-in experimental; MDX ONNX заявлен upstream; MDXC/RoFormer по умолчанию на CPU; Demucs не поддержан (fused LSTM). README содержит одновременно старое «not tested in CI» и более новое описание T4/WDDM CI [S1]. | В MVP выключен; не обещать AMD/Intel GPU support по CI на NVIDIA. Повторно проверить по архитектуре в #337. |
| Формат/утилиты | Default output FLAC, rate 44100; writer может наследовать input subtype и независимо нормализует стемы, default threshold 0.9. Имеется проверка FFmpeg [S1,S3,S4]. | Host snapshot — float WAV; adapter должен сохранить общий gain (см. ниже). Чистый Windows WAV path без FFmpeg проверить в #334; при обязательности FFmpeg отдельно определить сборку/лицензию, не брать случайный binary. |
| MSST | MIT репозитория; commit `9cc76c03ccd4da5ddf3477e93d386a1648a77732`. requirements включают torch>=2.0.1, demucs==4.0.0, rotary_embedding_torch==0.3.5, segmentation_models_pytorch==0.3.3, wxpython и другие training/UI packages [S8]. | Отдельное окружение, не объединять с audio-separator (в нём rotary>=0.6.1,<0.7). Windows wheels/VRAM не проверены. Пока нет gap, оправдывающего второй backend; #340 отложен. |

JUCE 9.0.3/MSVC x64 общаются только с отдельным процессом по pipes. Python/torch
не линкуются в EXE, не загружают DLL в plugin host и не меняют ASIO/PDC.

## Capabilities и лицензии моделей

Capabilities принадлежат **точной комбинации backend version + config + weights**.
`available_outputs` — не список, выдуманный UI; `native_outputs` и
`derived_outputs` различаются. Rate ниже — рабочая цель adapter, не утверждение,
что произвольный `--sample_rate` делает checkpoint совместимым с любой частотой.

| Модель / идентичность | Реальные outputs; рабочий rate/формат | CPU/GPU | Код / веса / решение |
| --- | --- | --- | --- |
| Official Demucs `htdemucs.yaml` → `955717e8-8726e21a.th` | Native Drums/Bass/Other/Vocals; 44.1 kHz stereo; MR adapter → WAV float32. Vocals/Instrumental — **derived 2-output profile**, Instrumental=sum(Bass,Drums,Other), не native 2-stem inference [S5,S6]. | CPU или CUDA; DML нет. Требования RAM/VRAM уточняются измерением #334. | Demucs repo MIT, official manifest связывает этот checkpoint [S6,S7]. **Первый технический кандидат** благодаря официальному происхождению; отдельное явное разрешение именно на внешний файл весов в прочитанных источниках не найдено. Redistribution approval остаётся открытым до фиксации подтверждения; development download разрешён пользователем; MIT wrapper не закрывает gate. |
| Official `htdemucs_6s.yaml` → `5c90dfd2-34c22ccb.th` | Native Bass/Drums/Other/Vocals/Guitar/Piano, 44.1 kHz; piano experimental [S1,S6]. | CPU/CUDA; DML нет. | То же незакрытое weights gate; #337, не включать в MVP/installer. |
| BS-RoFormer Viperx `model_bs_roformer_ep_317_sdr_12.9755.ckpt` + одноимённый YAML | Target Vocals, complement Instrumental; config stereo 44100, `num_stems: 1` — это не 6 самостоятельных источников [S1,S9]. | PyTorch CPU/CUDA; DML CPU fallback upstream. | Wrapper/MSST MIT не подтверждают права конкретного Viperx checkpoint. **Blocked для каталога поставки**, до checkpoint-specific terms и config/hash audit. |
| MelBand `vocals_mel_band_roformer.ckpt` + `vocals_mel_band_roformer.yaml` | Upstream list: Vocals/Other; точный rate/формат и трактовку complement нужно подтвердить config при admission [S1,S5]. | PyTorch CPU/CUDA; DML CPU fallback upstream. | Указание автора Kimberley Jensen и публичная ссылка не являются лицензией. **Blocked**, не рекомендовать как разрешённый preset. |
| MDX `UVR_MDXNET_KARA_2.onnx` | Upstream 2-stem MDX family; точные label/rate должны пройти model-data admission, а не выводиться из имени файла [S1,S3,S5]. | ORT CPU/CUDA; DML кандидат #337. | Наличие checksum не даёт права распространять веса. **Blocked**, пока нет checkpoint-specific license и полного capability record. |

Решение для #334: проектировать Vocals/Instrumental на official HTDemucs с
**отложенным аудитом разрешения весов**. Первой юридически проверенной модели пока нет: соответствующий критерий #333
остаётся отложенным. По прямому указанию пользователя development smoke с этим
checkpoint разрешён; распространение весов отложено до аудита. CPU fallback обязателен:
при Auto-local probe failure до старта — CPU с отображением effective device;
после CUDA OOM — failed и явный retry CPU, без незаметного повторного долгого job.

Checksum из manifest audio-separator v0.48.0 [S5] (публичные утверждения upstream,
самостоятельный download/hash больших весов в этом этапе **не выполнялся**):

- `955717e8-8726e21a.th`: `8726e21a993978c7ba086d3872e7608d7d5bfca646ca4aca459ffda844faa8b4`.
- `htdemucs.yaml`: `239c445d0b14454d541ad8bd9bb271c9e536d267e8a4625208744cbb2e7bb66c`.

Model record перед admission: stable ID/version, backend/version, immutable source
URLs, filenames/full sha256 для **всех** configs/weights/metadata, code license,
weights license+evidence+redistribution policy, notices, native/derived stem IDs,
training/inference rate, channels/dtype, allowed device/precision, measured minimum
resources (до измерений null), byte size/quota. License `unknown` сохраняется в audit record. По текущему поручению это не
блокирует development download/run official HTDemucs; для остальных untrusted
checkpoint paths проверка происхождения/целостности остаётся обязательной.

## Job contract v1 (проект интерфейса)

Все uint64 sample/revision/sequence/byte counters в wire JSON — decimal strings;
enum/ID ограничены схемой, произвольного shell command/URL/code в settings нет.
JSON UTF-8, protocol major=1; неизвестный major или обязательная capability → reject.

| Объект | Обязательные поля/семантика |
| --- | --- |
| `JobRequest` | `protocol_version`, UUID `job_id`/`attempt_id`; `context`={project_id, session_id, project_revision, clip_id}; `input`={asset_id, source_revision, source_sha256, snapshot_sha256, host-assigned snapshot token, source_rate, source_start_frame, source_frame_count, snapshot_rate, snapshot_frames, channels=2}; source range `[start,start+count)`. `placement`={project_rate, timeline_start_sample, target_frame_count}. `model`={id, version, config_sha256, weights_sha256[]}; backend ID/version; profile ID, bounded typed settings; requested/effective device, fp32; deadline/resource policy. |
| `ModelCapabilities` | backend/version + model hashes; native/derived outputs and derivation version; supported rates/channels/formats/devices; license admission state; limits. Запрос неподдержанного stem/profile → error, не подстановка другой модели. |
| `Event` | job/attempt/session IDs, strictly increasing sequence, type, phase, completed/total (nullable при неизвестном объёме), bounded/redacted message; terminal event ровно один. Не разбирать tqdm как точный процент. |
| `ArtifactManifest` | version/job/attempt; все input/model/backend identities и runtime-lock hash; requested/effective device/precision, seed/shifts/overlap/segment/settings; started/completed UTC, derivations/gain policy; на output: semantic stem ID, relative path, sha256, byte count, rate/channels/dtype, exact frame count, origin=0, compensated delay. Только полностью проверенный набор. |
| `StructuredError` | code, phase, retriable, safe message, bounded details, optional provider code; без credentials/full local paths. Codes: invalid_input, unsupported_input/model/device, license_unverified, model_missing, integrity_failure, stale_source/context, resource_limit, timeout, cancelled, worker_crash, protocol_error, io_error, backend_failure. |

`ISeparationBackend` концептуально: `probe()`, `capabilities(model)`,
`submit(snapshot, settings, control)`, `poll_events()`, `cancel(job)` и
`shutdown()`. Вызовы только control/service threads, bounded очереди. Не новый
audio engine и не расширение AI chat permissions для отправки аудио.

State machine:
`queued → validating → preparing → running → verifying → ready`;
из любого нетерминального состояния `→ cancelling → cancelled` или `→ failed`.
`ready/failed/cancelled` terminal; пользовательский retry создаёт новый attempt.
Timeout является `failed(timeout)`, completion после cancel request игнорируется.
Верификация и принятие результата сериализованы с cancel на service owner thread.

Progress: фазы validation/load/inference/write/verify; inference может быть
indeterminate. Повтор/перестановка sequence отбрасываются. UI читает coalesced
snapshot, не получает callbacks из Python. Закрытие проекта инвалидирует session,
отменяет job; поздний результат не применяется. Изменение несвязанного track не
требует новой inference: при commit повторно проверить clip/source/range/placement;
изменившийся источник/trim/move требует явного нового действия, не stale insert.

## Sidecar, ресурсы, offline и recovery

Windows CreateProcess с фиксированным executable/argv (без shell), private pipes,
`STARTUPINFOEX` handle allowlist; Python isolated mode, управляемый runtime и
очищенный от произвольных PYTHONPATH/plugin путей import path. Один sidecar/job,
не более одного running + двух queued jobs; пределы — начальная политика,
не измеренные hardware requirements. На Windows Job Object: kill-on-close,
лимит процессов/памяти; OMP/MKL/torch thread budget `max(1,min(4,logical_cores-2))`.
Лимиты input/duration/disk оцениваются до запуска; отмена/ошибка по превышению,
не бесконечный swap. VRAM нельзя считать жёстко ограниченной Job Object.

IPC: host→worker `hello/submit/cancel`; worker→host `hello/event/result/error`.
Dedicated protocol pipe: little-endian uint32 byte length + JSON, max frame 64 KiB,
max manifest 48 KiB; stderr отдельно, bounded ring 256 KiB, max log event 2 KiB,
coalescing progress до 4 Hz. Бинарное audio только files, не pipe/base64.
Wrapper перехватывает stdout библиотек в log sink. Неверный frame/EOF → protocol_error.
Handshake deadline 30 s, process-supervisor heartbeat каждые 2 s, потеря связи 15 s;
heartbeat отдельным supervisor, не доказательство продвижения inference.
Общий deadline задаётся запросом; живой heartbeat его не продлевает.

Cancel: сразу прекратить публикацию; wrapper пробует stop между фазами.
У upstream нет гарантии cooperative cancel внутри model call. Через 5 s host
завершает весь Job Object, drain/join вне UI/audio thread, staging удаляется.
Watchdog/kill не зависит от занятого inference GIL. Crash/timeout не перезапускает
job сам; при следующем старте orphan staging не считается готовым результатом.

Расположение (пути определяет host, worker не выбирает произвольный output):

- immutable runtime: versioned per-user app-data `Separation/runtimes/<lock-id>`;
- admitted model cache: `Separation/models/<model-id>/<content-hash>`;
- private work/cache: local-app-data `Separation/jobs/<job-id>/<attempt-id>`;
- после проверки и Command в #336: relative project asset area
  `<project-stem>.assets/separation/<result-id>/`, рядом с project file.
  Для unsaved project — session staging до Save As; cache не является durable asset.

Download — только отдельное действие Model Manager (#335), с сохранением license audit record;
HTTPS allowlist без произвольных redirects, byte limits, .part + sha256,
atomic publish directory, notices, quota/eviction без удаления referenced assets.
Configs/checkpoints считаются исполняемыми данными: PyTorch pickle может исполнять
код, checksum подтверждает лишь конкретные bytes. Только reviewed catalogue,
никаких untrusted checkpoint paths/YAML constructors. Не обещать pipes/isolated
mode как полноценную OS sandbox; безопасный loader/минимальные права — gate #334.

`load_model()` upstream может загружать веса, Demucs subweights и UVR metadata.
Offline adapter должен предзагрузить полный dependency closure и блокировать
все fetch paths до вызова; network guard в worker возвращает model_missing,
не автоматически обращается к CDN. Проверить с отключённой сетью и cold caches.
Отсутствие weights при offline не меняет модель. Remote adapter пока unsupported;
Auto означает только выбор local device, никогда upload. В будущем remote endpoint,
auth/TLS, cost/retention/consent должны идти отдельным контрактом #339.

## Синхронизация, gain и атомарность результатов

Source domain и project domain различаются. Host фиксирует исходный диапазон до
inference, off-thread resample один раз в model rate 44100. Все outputs одного job
должны иметь одну origin, rate и точное `snapshot_frames`. Resample обратно одним
детерминированным mapping в `target_frame_count` на project_rate; явно сохранить
rounding/delay compensation в manifest. Нельзя независимо time-align стемы по пику.
Любая неизвестная задержка/необъяснённое расхождение длины → verification failure;
trim/pad допускаются только по документированной модели latency, не маскируют сбой.

Upstream defaults normalise каждый стем отдельно. Adapter в #334 должен отключить
такую обработку через проверенный write hook либо сохранить/обратить точные gain
factors до derived sum. Не считать `normalization_threshold=1` отключением: сигнал
выше 1 всё ещё масштабируется. Float32 output без clipping; NaN/Inf reject.
Instrumental складывается из трёх **ненормализованных** native outputs с единым gain.
Не использовать mix-minus-vocals под тем же profile ID. Manifest различает model
outputs/derived outputs; phase/quality/reconstruction error измеряются отдельно,
нулевой reconstruction error не гарантирован ML-моделью.

Worker пишет только `.part` внутри private staging. Host проверяет basename/path
containment (включая Windows reparse points, UNC/ADS/absolute paths), формат,
headers/bytes/hash/duration, уникальность semantic stem IDs и полноту набора.
Затем публикует manifest последним атомарно. Cross-volume перенос в project asset
area — copy to sibling staging + reverify + rename; не считать rename атомарным
между дисками. Failure/cancel оставляет исходник и проект неизменными.

`ready` означает проверенный cache result, не изменённый проект. В #336 один Command
принимает manifest, регистрирует hidden assets и при явном выборе создаёт tracks/
clips с исходной placement. Undo снимает ссылки/дорожки, Redo использует те же IDs
и files без повторного inference. Файлы держатся пока нужны undo/redo/durable save;
GC отдельный, не в callback. Save As копирует referenced assets с проверкой hashes;
**Уточнение пользователя 2026-11-11:** при Save/Save As записи результатов, у которых
на диске отсутствует manifest или хотя бы один stem, удаляются из меню результатов
и сохраняемого проекта. Очистка публикуется одной Undo-командой только после
успешной записи проекта; неудачный Save сохраняет прежний список. Существующие
дорожки/клипы и файлы не удаляются. Если отсутствующий файл ещё используется
аудиоклипом, Save сохраняет прежнее поведение ошибки; dry audio не подставляется.
Существующие повреждённые или недоступные для чтения файлы не считаются
отсутствующими и по-прежнему требуют исправления. Старый project без separation
metadata открывается как прежде; формат расширяется только в #336.

## Адресный план проверки и gates

| Этап | Обязательные проверки |
| --- | --- |
| #333 (этот) | Сверка pinned API/package/license/config источников, ссылок, source integration points; diff/whitespace. Без inference/build/CTest. |
| #334 | Чистый Windows x64 runtime lock/wheels; CPU 2-output fixture, Unicode/space paths; fake sidecar protocol/cancel/timeout/crash/flood/malformed/late result; source revision/session rejection; network disabled + missing subweight/config; gain >1, exact geometry 44.1/48/96 kHz and nonzero source offset, NaN/Inf/partial output. RT/UI isolation review. GPU probe/fallback на реальном устройстве отдельно. |
| #335 | Explicit download; checksum/size mismatch, interruption/cancel/safe retry, cache corruption, disk-space preflight, concurrent install/in-use leases, offline full closure. Метаданные/лицензии доступны; лицензионная приёмка отложена по указанию пользователя. |
| #336 | Hidden assets default; optional tracks одной Undo transaction, Redo без inference, cancel/close during completion; Save/Open/Save As/moved project/missing media, старый project без metadata, trimmed source alignment. |
| #337 | Реальные stem labels/count/rates каждой admitted модели, sample/time/phase consistency, CPU/CUDA/DML matrix только по фактическим тестам. |

Owned/synthetic fixtures отдельно от пользовательских файлов. Слух/ASIO/xruns,
hardware GPU/VRAM, качество на авторизованных музыкальных примерах и сравнение
Moises/RX — **NOT TESTED**. Полный CTest и #335–#340 автоматически не запускаются.

Отложенный аудит: checkpoint-specific разрешение official Demucs. По указанию
пользователя оно не блокирует #334 и сборки. Windows runtime lock, loader policy,
write hook без per-stem gain и offline dependency closure проверяются в #334/#335.
Юридическая приёмка весов не заявляется; merge в main требует отдельного подтверждения.

## Реализованный local runner #334

`apps/studio-juce/src/SeparationRunner.h/.cpp`: service-thread API `start`, `status`,
`cancel`, `take`; один outstanding job/result, без очереди. Construction не запускает
Python. Windows CreateProcess без shell, ограниченный handle inheritance, отдельный
Job Object (memory limit/один процесс/kill-on-close). Фоновый thread владеет IPC и
проверкой WAV/hash; audio engine/model pointers в нём отсутствуют. Destructor
cancel/kill/join вызывается вне UI/audio callback. Cancel даёт worker до 5 s, затем
весь процесс завершается; общий deadline и 30-s silence watchdog независимы от ML.
Raw backend stderr дренируется отдельно от protocol pipe; в памяти сохраняется
только хвост до 256 KiB. `diagnostics()` — явное локальное чтение, лог может содержать
пути и не попадает в manifest/project/provider. Stack trace включается только явным
`MR_SEPARATION_DIAGNOSTICS=1` в developer smoke. UI-интеграция — #336.

Cancel reader опрашивает pipe (Windows PeekNamedPipe / POSIX select) и читает только
доступные bytes, с ограниченным frame buffer. Начальный request читается unbuffered,
чтобы не спрятать ранний cancel во внутреннем BufferedReader. Во время native imports
нет фонового блокирующего `_read(stdin)`: на Windows он может удерживать CRT fd lock
и блокировать native NumPy startup. Heartbeat/watchdog и принудительное завершение
через 5 s сохраняются. Дополнительные import-фазы позволяют различить startup и ML.

`tools/separation/worker.py`: isolated Python, framed stdin/stdout; job/sequence IDs,
phase/heartbeat/result/error, CPU/CUDA/Auto-local. До импорта ML устанавливается
process-local запрет Python socket connect/DNS; native malicious code этим не
изолируется. Штатный fixed HTDemucs adapter не вызывает upstream discovery/download
и не использует FFmpeg. SHA256 обоих файлов проверяются до loader; произвольные
модели не принимаются. Python runtime — явный абсолютный путь, а не PATH lookup.

Вход: существующий stereo WAV, source start/count, session/source revision,
timeline start и project rate. В #334 host передаёт path+полный hash, worker сам
читает диапазон и делает неизменяемый snapshot (концептуальный host snapshot из
контракта отложен до интеграции клипа). Source 44.1/48/88.2/96 kHz, не более 10 min
и 512 MiB; целевой rate из фиксированного списка 8…96 kHz. Snapshot → 44.1 kHz;
после inference все native stems имеют точную длину snapshot, без скрытого trim/pad.
Детерминированный polyphase SRC обратно использует округление к ближайшему frame,
излишек ceil длины от SRC обрезается по общему mapping. ML outputs не нормализуются:
write hook пишет FLOAT, Instrumental=sum(Bass,Drums,Other). Затем два float32 WAV
публикуются с manifest через `result.part → result`. Seed=0, shifts=0, overlap=0.25.
Bit-identical результаты между разными hardware/runtime не обещаются.

Host повторно проверяет source hash, session/revision echo, placement, выходные
имена, reparse points, SHA256, bytes/headers/rate/frames и finite PCM. Только verified
result передаётся caller; ошибки/cancel убирают весь job staging. Caller владеет
успешной cache directory и должен удалить её после использования. Автоматический
orphan GC после аварийного завершения самой DAW пока не реализован; неполный каталог
не принимается как результат. Глобальное ограничение нескольких Runner instances
должен обеспечивать единственный будущий Studio service (#336).

Прямой development smoke (PowerShell, только подготовленный отдельный runtime):

```powershell
python -m pip install torch==2.8.0 --index-url https://download.pytorch.org/whl/cpu
python -m pip install -r tools/separation/requirements-cpu.txt
python tools/separation/install_smoke_model.py <new-model-directory>
mrs_separation_check.exe --real <absolute-python.exe> <absolute-worker.py> <model-directory> <new-evidence-directory>
```

Установка модели — отдельная осознанная операция development helper, не запуск
worker и не готовый Model Manager #335. Она проверяет full SHA256 и размер;
каталог не должен содержать прежние файлы. В #335 полный resolved набор версий закреплён в `requirements-cpu.txt`; SHA256
канонического списка проверяется до установки. Wheel hashes не закреплены:
это version-pinned installation, не hash-locked distributable. Модели/Python
не пакуются; Studio содержит `mr_separation_worker.py` и
`mr_separation_requirements.txt`. В UI действия Separate пока нет (#336).

CI `Separation CPU contracts`: Windows x64/CPython 3.11; Python synthetic contract
tests, C++ fake-sidecar faults, затем real official HTDemucs CPU smoke с отключённой
сетью worker. Результаты — отдельный synthetic evidence artifact без weights/runtime.
`PlaybackProbe` одновременно проводит существующий engine process + raw recorder
через fake device; это изоляция сервиса, не ASIO benchmark. Обычная Windows Studio
сборка остаётся JUCE/VST3/ASIO/ARA2 ON и проверяет host fault suite.

## Реализованный Model Manager #335 (0.9.3)

Options → Separation models открывает отдельное окно Studio: установка, проверка
и удаление CPU runtime; выбор модели, размер, загрузка/удаление, Details с ID,
версией, native/derived outputs, источниками, SHA256 и лицензионными сведениями.
Прогресс, отмена и ошибки отражают состояние сервиса; неизвестная лицензия не
блокирует разработку по указанию пользователя, разрешение bundling не заявляется.
MR Live меню и ресурсы менеджера не получает. Проект/Undo менеджер не изменяет.

`SeparationModels.h/.cpp` управляет `%LOCALAPPDATA%/MoonRiverStudio/Separation`:
`models/<catalogue-id>`, `runtimes/cpu-py3119-audiosep048-v1`, `locks`, `jobs`.
Один разрешённый checkpoint HTDemucs: native four sources, в MVP только derived
Vocals/Instrumental. HTDemucs 6s показан как unsupported с причиной; CUDA профиль
не установлен и явный запрос CUDA отклоняется. Нет импорта произвольных YAML/весов.

Runtime устанавливается отдельной явной кнопкой: embedded CPython 3.11.9 x64,
не меняющий системный Python/PATH/registry; bootstrap Python/get-pip проверены по
размеру и SHA256. pip использует явные официальные индексы, binary wheels,
закреплённые версии, без prompt/compile/cache. Torch/torchvision — CPU wheels.
`pip check`, импорт/CPU tensor probe, полный file inventory и resolved freeze
предшествуют публикации. `managed.json` содержит provenance и хэши файлов;
локальные package dist-info содержат собственные notices. SBOM/юридический аудит
всех зависимостей не объявляются завершёнными.

Все операции выполняются на service thread: lock между процессами, уникальный
`.part-*` staging, bounded HTTPS/byte limits, проверка hash, atomic directory
publish. Ошибка/отмена убирает staging текущей операции; повтор начинается заново.
Никакого скрытого обновления установленного профиля: явное remove/reinstall.
Удаление проверяет managed inventory, не удаляет лишние пользовательские файлы,
отклоняет reparse paths. Shared leases удерживают runtime/веса при inference;
удаление занятой установки возвращает `in_use`. Офлайн refresh не запускает Python
и сеть; Verify проверяет все hashes и offline probe. Открытие окна не скачивает.
Первое открытие Separate into Stems или Model Manager проверяет локальные файлы
асинхронно; общий сервис хранит этот snapshot до конца текущего запуска Studio.
Повторное открытие любого из двух окон не пересчитывает SHA256 всех весов.
Установка, удаление и явная Verify обновляют snapshot; неудачная/отменённая
операция оставляет возможность повторного сканирования. Изменения файлов извне
видны после явной Verify/refresh либо перезапуска Studio. Worker по-прежнему
проверяет полный SHA256 выбранных checkpoint/config перед inference.

Worker запущен с `-I -B`; Numba cache находится только в private job staging и
удаляется после завершения процесса. Полная проверка runtime после реального
inference подтверждает отсутствие изменений. Installer/probe — private pipes,
ограниченные память/process tree/deadline/log; закрытие окна отменяет операцию,
service переживает окно без callback в разрушенный GUI. Это не OS sandbox для
враждебного native кода, поддерживается только pinned catalogue.

Проверки: native manager faults и host contracts; Windows embedded bootstrap,
offline reopen, leased removal, реальный CPU inference и повторная полная проверка
runtime; Python worker contracts. GUI smoke использует только synthetic tiny
model, проверяет реальные download/remove bindings, Details, close/reopen и
неизменность project pointer, сохраняет software snapshots min/nominal/max при
100/125/150/200%. Физические DPI/ASIO/GPU и пользовательская приёмка — NOT TESTED.
Clip action/hidden assets/Undo/persistence остаются исключительно в #336.

## Workflow клипа и project assets #336 (0.9.4)

Studio-only: контекст audio clip → Separate into Stems; Options → Separation
results открывает общий список скрытых результатов. Единственный ProjectJob
принадлежит Desktop, окно только читает coalesced status и передаёт команды.
Local CPU / установленный HTDemucs → Vocals / Instrumental; никаких сетевых
загрузок из job. Новый backend/4–6 outputs остаются #337, remote — #339.

Вход — исходный stereo WAV в project rate, выбранный source_offset/length;
track processing не рендерится. Clip Time/Pitch нужно Reset, ARA — detach;
не выдавать dry результат за обработанный/растянутый клип. Фейды сохраняются
как параметры размещаемых clips, без повторной нормализации WAV. Исходник
и исходная дорожка не меняются и не заглушаются автоматически.

Project.separation_results хранит ID, source clip/hash/range/timebase, model/version,
inline provenance и ссылки/SHA256/размеры двух FLOAT WAV и manifest. RegisterSeparation
повторно проверяет полный clip snapshot и sample rate; unrelated track edits
разрешены. Подготовка/копирование/проверка/Runner destruction происходят на service
thread. Смена ProjectStore или Save As path отменяет текущую job; поздний результат
не применяется. Cancel и публикация outcome сериализованы, закрытие utility window
только отменяет job, обычное закрытие не ждёт Python в GUI callback.

Saved project: verified sibling .part directory → atomic publish в
`<project-stem>.assets/separation/<result-id>/`; ownership подтверждается после
успешной команды, отклонённый/отменённый результат удаляется background cleanup.
Unsaved project: приватное session storage до Save As. Media leases удерживаются
проектом и prepared audio. Saved project assets не удаляются автоматически.

По умолчанию добавляется только hidden record, без аудиографа/дорожек. Verify result
проверяет manifest и оба файла; Place on separate tracks асинхронно проверяет SHA256
и геометрию, затем одна команда создаёт две обычные tracks/clips с исходным start,
frames и fades. Используется существующая безопасная Application::edit при
остановленном playback. Remove result снимает hidden record; уже размещённые clips,
исходник и Undo/Redo media сохраняются. Повторный Separate создаёт новый ID/manifest,
не перезаписывает предыдущий результат. Идентификаторы Redo стабильны, inference
не повторяется. Максимум 64 текущих records и 128 retained job groups за сессию;
неиспользуемые durable files не удаляются без отдельного GC, так как backups/Undo
могут сохранять ссылки.

Save/Open/Save As используют ProjectDocument и MediaCopy: stable runtime/Undo keys
разрешаются через owned_media_, serialized refs становятся relative; все текущие
и retained separation files копируются с SHA256, чужой destination не перезаписывается.
Missing/corrupt current result даёт явную ошибку; failed save не заменяет старый
archive. Metadata-only результат можно удалить и повторить из доступного source.
Core reader принимает старые schemas; wire schema 33 применяется только при
непустых separation_results, проекты без них сохраняют schema 32 (включая MR Live).
В MR Live нет нового меню/job/UI и не меняется его show workflow.

Адресные проверки: portable SHA256 vectors, register/placement/cleanup Undo/Redo,
legacy/roundtrip, Save As и удаление старой папки, missing/corrupt media/foreign files;
GUI fixture с реальными handlers/commands, отмена, смена проекта, close/reopen и
software scale snapshots. Отдельный Windows CPU project check проходит реальную
job → hidden assets → placement → Undo/Redo → relocated Save/Open; fixtures synthetic,
без пользовательской музыки. Физические DPI/ASIO/GPU и слуховая приёмка отдельно.

## Расширенные capabilities #337 (0.9.5)

Единый pinned catalogue `tools/separation/mr_separation_profiles.json` компилируется
в native host и поставляется рядом с worker. Старый ID derived-2 сохранён.
HTDemucs: derived Vocals/Instrumental или native Vocals/Drums/Bass/Other;
HTDemucs 6s: Vocals/Drums/Bass/Guitar/Piano/Other (piano experimental).
BS-RoFormer Viperx ep317 и Mel-Band RoFormer Kimberley Jensen дают два выхода,
а не 4/6: Vocals и Instrumental (в Mel config `other` — complementary source).
MDX KARA2 имеет primary **Instrumental**, complementary Vocals; это не native
6-source модель. Его точные MDX параметры взяты из pinned upstream metadata,
проверенного по tail-MD5 **1d64a6d2c30f709b8c9b4ce1366d96ee**, при этом admission
самих весов требует **полный SHA256**, не MD5. Источник metadata:
`TRvlvr/application_data@3826b05b570dbd4fbedbc807758803b35348ba1b`.

Worker проверяет полный config/weights closure до model load и не вызывает
upstream discovery/download. Model Manager выполняет явные загрузки. В каталоге
фиксированы имена, размеры и SHA256 всех файлов; лицензии checkpoint остаются
review deferred по указанию пользователя, веса не входят в runtime ZIP.

Mono WAV дублируется в stereo, исходник не меняется. Поддержаны входные
44.1/48/88.2/96 kHz; worker resample_poly приводит snapshot к 44.1 kHz модели,
выходы — к project rate с точным округлённым frame count и общим origin=0.
DAW сохраняет прежнее условие input rate = project rate, импорт других rates
переиспользует существующую конвертацию. Не вводится pitch/time обработка.

Все модели используют float32, без независимой нормализации/clipping источников.
Chunk default: Demucs model default, RoFormer 256 STFT frames; small: Demucs 4 s,
RoFormer 128 STFT frames. RoFormer batch=1/overlap=2, MDX фиксирован на 256 frames
для ONNX CPU и не допускает small. Chunk и mono-policy записываются в manifest.
CPU UI предупреждает о длительной обработке. Managed CUDA/DirectML runtime
отсутствует; UI не предлагает неработающий GPU. Для отдельного sidecar CUDA probe
проверяет allocation/operation и консервативный free-VRAM guard (оценка, не замер
минимума). MDX CUDA исключён в этом CPU runtime, OOM не запускает скрытый retry.
Фактические GPU/VRAM/слуховые проверки отдельно, CI их не доказывает.

Project records допускают проверенные наборы 2/4/6 roles. Формат schema 33 уже
хранит variable count; layout/meaning полей не меняются. Старый 0.9.4 открывает
2-stem records и явно отклоняет неизвестные 4/6, plain проекты остаются schema 32.
Placement/Undo/Save As используют один существующий путь и копируют каждый stem
по своему безопасному имени. Оригинал остаётся без изменений.

## Chunk progress #337 (0.9.5-Upd1)

Pinned backend iteration counters (Demucs/RoFormer/MDX) передаются через bounded
IPC как decimal-string completed/total/current_pass/passes. Завершение chunk
считается после inference, не при постановке в очередь; cancel проверяется между
chunks, события ограничены 4 Hz, финальное событие прохода не теряется. Патч
локального tqdm binding действует только на время одного sidecar inference;
строки stderr не парсятся, fake progress отсутствует. MDX имеет два прохода,
UI показывает pass и chunks, bar распределён по проходам (это доля работы,
не обещание линейного времени). Loading/verification остаются indeterminate.

Studio CPU deadline теперь max(30 min, 30×clip duration + 10 min), максимум 6 h;
это верхний предел, а не ETA. Для 5 min source — 160 min вместо прежних 30 min.
Manual cancel и отдельные host/worker deadlines сохраняются. Никаких изменений
числа CPU потоков, DSP, качества модели, project schema или cache identity.

## Выбранные outputs и модели #338 (0.9.6, в разработке)

`stems` в local request — непустое уникальное подмножество точных capabilities
модели; отсутствие поля означает default outputs профиля. Обычные модели по
умолчанию сохраняют все источники. UI All/None не запускает
задачу с пустым набором. Возвращаются/сохраняются только выбранные WAV, manifest
содержит available_stems/selected_stems и additive_outputs. Общее inference может
вычислять больше источников; ускорение не обещается. SW independent mask
heads ограничиваются выбранными indices после загрузки точных pinned weights;
это технический adapter к pinned MDXC, а не выдуманные residual instruments.

Internal Core schema34 допускает1..53 безопасных уникальных role IDs. Plain
проекты сохраняют wire32, полные legacy Vocals/Instrumental,4- и6-source sets —
wire33; selected/new-category sets — wire34, который старые версии явно отклонят.
Typed manifest/file refs, Undo/Redo и Save As используют существующий путь.
Действующий32-streamed-voice budget engine не увеличивается:53hidden categories
могут храниться, но превышение бюджета при Place отклоняется до публикации Core.
Mega 53 отложена по решению пользователя и отсутствует в активном каталоге.

SCNet XL IHF: exact v1.0.15 config/checkpoint и узкий MIT SCNet adapter из
MSST@f0b527736e7da24eae5b561a2bb1efd0569ec757. Source notice хранится в
SCNet-MIT-LICENSE.txt; adapter canonical-LF SHA256 закреплён в общем catalogue.
Новых runtime dependencies для SCNet не требуется; checkpoint loading только
после проверки полного closure SHA256. Реальная Windows admission проверяется
отдельно, до объявления READY. GPU/объективное качество отдельно.

HTDemucs FT добавлена вместо Mega 53: official `htdemucs_ft.yaml` и четыре
специализированных .th checkpoints, суммарно 336565233 B. Полный closure закреплён
URL/размерами/SHA256. Native order: Drums/Bass/Other/Vocals. Выбор outputs сохраняет
только отмеченные WAV, но текущий Demucs adapter вычисляет все четыре прохода;
прогресс считает четыре прохода независимо от selection. Target-specific запуск
не заявляется. FT обычно медленнее обычного HTDemucs; качество требует A/B.
Реальный inference FT на Windows/CPU/GPU пока NOT TESTED.

Studio-only scope `[ci studio]`/`checks=studio` конфигурирует MRS_BUILD_LIVE=OFF,
не создаёт Live executable/Live tests и не включает Live в package. Совместная
integration и отдельные Live проверки остаются явными опциями. Studio ARA2 ON.
Синяя полоса uses current Theme focus/accent, без глобального изменения Theme.

## Источники аудита

- **S1** [audio-separator v0.48.0 release](https://github.com/nomadkaraoke/python-audio-separator/releases/tag/v0.48.0), [pinned README: CLI, devices, outputs](https://github.com/nomadkaraoke/python-audio-separator/blob/4dd3477c0a8f5bfb0fb053c6ed444bb935cb150e/README.md).
- **S2** [pinned pyproject/dependencies](https://github.com/nomadkaraoke/python-audio-separator/blob/4dd3477c0a8f5bfb0fb053c6ed444bb935cb150e/pyproject.toml), [MIT license](https://github.com/nomadkaraoke/python-audio-separator/blob/4dd3477c0a8f5bfb0fb053c6ed444bb935cb150e/LICENSE).
- **S3** [Separator API, device selection/download/model data](https://github.com/nomadkaraoke/python-audio-separator/blob/4dd3477c0a8f5bfb0fb053c6ed444bb935cb150e/audio_separator/separator/separator.py).
- **S4** [writer, subtype and normalisation](https://github.com/nomadkaraoke/python-audio-separator/blob/4dd3477c0a8f5bfb0fb053c6ed444bb935cb150e/audio_separator/separator/common_separator.py), [normalise implementation](https://github.com/nomadkaraoke/python-audio-separator/blob/4dd3477c0a8f5bfb0fb053c6ed444bb935cb150e/audio_separator/separator/uvr_lib_v5/spec_utils.py).
- **S5** [model catalogue](https://github.com/nomadkaraoke/python-audio-separator/blob/4dd3477c0a8f5bfb0fb053c6ed444bb935cb150e/audio_separator/models.json), [sha256 manifest](https://github.com/nomadkaraoke/python-audio-separator/blob/4dd3477c0a8f5bfb0fb053c6ed444bb935cb150e/audio_separator/model-checksums.json), [Demucs adapter/output mapping](https://github.com/nomadkaraoke/python-audio-separator/blob/4dd3477c0a8f5bfb0fb053c6ed444bb935cb150e/audio_separator/separator/architectures/demucs_separator.py).
- **S6** [official Demucs README/models](https://github.com/facebookresearch/demucs/blob/e976d93ecc3865e5757426930257e200846a520a/README.md), [htdemucs config](https://github.com/facebookresearch/demucs/blob/e976d93ecc3865e5757426930257e200846a520a/demucs/remote/htdemucs.yaml), [6s config](https://github.com/facebookresearch/demucs/blob/e976d93ecc3865e5757426930257e200846a520a/demucs/remote/htdemucs_6s.yaml), [checkpoint filenames](https://github.com/facebookresearch/demucs/blob/e976d93ecc3865e5757426930257e200846a520a/demucs/remote/files.txt).
- **S7** [official Demucs MIT license](https://github.com/facebookresearch/demucs/blob/e976d93ecc3865e5757426930257e200846a520a/LICENSE). Repo license + linked weights: источник происхождения и кандидатных условий; не отдельное checkpoint-specific разрешение.
- **S8** [MSST README](https://github.com/ZFTurbo/Music-Source-Separation-Training/blob/9cc76c03ccd4da5ddf3477e93d386a1648a77732/README.md), [MIT license](https://github.com/ZFTurbo/Music-Source-Separation-Training/blob/9cc76c03ccd4da5ddf3477e93d386a1648a77732/LICENSE), [requirements](https://github.com/ZFTurbo/Music-Source-Separation-Training/blob/9cc76c03ccd4da5ddf3477e93d386a1648a77732/requirements.txt).
- **S9** [MSST pretrained model references](https://github.com/ZFTurbo/Music-Source-Separation-Training/blob/9cc76c03ccd4da5ddf3477e93d386a1648a77732/docs/pretrained_models.md), [Viperx BS-RoFormer config](https://github.com/ZFTurbo/Music-Source-Separation-Training/blob/9cc76c03ccd4da5ddf3477e93d386a1648a77732/configs/viperx/model_bs_roformer_ep_317_sdr_12.9755.yaml).

## CPU selection and timing — 0.10.1-Upd4 (based on 0.10.1-fix1)

Separate into Stems has an explicit **CPU threads** selector: Auto / 1 / 2 / 4 /
8 / 16 / 32. Auto preserves the earlier maximum of four; manual choices use the
same process-affinity and maximum-32 limits as Audio to MIDI. These are software
threads, not a promise of physical cores or a measured CPU quota. The selection
is captured for each job and disabled while work is pending.

The source summary shows the selected clip duration in seconds, length and
source offset. The operation timer updates during processing and freezes on
completion/failure/cancel; it includes startup, checks, model work and keeping
assets. It also times a later explicitly requested Verify/Place operation. The
manifest separately records `host_wall_seconds` (worker/startup/verification),
`configured_threads`, and the real Torch pool sizes, version and reported CPU
capability. Repeated jobs start fresh workers; these are not warm-session timings.

Separation keeps the official Torch 2.8.0 CPU runtime, which dispatches CPU
instructions automatically. The dialog displays the runtime's reported ISA
(e.g. AVX2) during/after inference; that is not a profiler result for every
operator. Audio to MIDI retains its existing generic and AVX2 native engines and
CPU/OS-aware automatic selection. No new model or native separation backend is
introduced by this update. The unfinished instrument-conditioned MIDI repro
tracked in #346 is not fixed by this change.

MDX also receives the selected intra-op count and inter-op count 1 in its ONNX
SessionOptions during model loading; Torch settings alone do not limit that pool.
The developer CPU probe verifies official Torch CPU/AVX2 dispatch and the existing
ONNX Runtime pool configuration without weights or performance measurements.

## Startup and remembered choices — 0.10.1-Upd5

Opening Separation uses a cached, metadata-only inventory of model ownership,
presence, safe paths and expected sizes. It does not hash every installed model's
weights. Critical runtime executable checks remain. Selected weights are still
fully verified by the worker before loading; the model manager's installed-model
button becomes **Verify model** for an explicit full check of that model only.
Explicit full-refresh API checks remain; known corruption is not cleared by a
metadata-only refresh. Install/remove invalidate the cached inventory.

The selected catalogue **model ID** and requested **CPU threads** save immediately
through existing ViewSettings and atomic application settings, including before a
job is started. Reopening/restarting restores them; legacy/invalid fields use
safe defaults. These choices do not modify the project or Undo history.

The main dialog keeps clip duration and live/final operation time with the actual
configured CPU count. **Show diagnostics / Hide diagnostics** controls the raw
provenance/hash/JSON and runtime ISA panel; it starts collapsed and hidden content
is not reformatted on every timer tick. Processing and cancellation remain usable
with the panel closed.
