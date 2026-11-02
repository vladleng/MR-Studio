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
missing file показывает ошибку, не молчаливую замену dry audio. Старый project без
separation metadata открывается как прежде; формат расширяется только в #336.

## Адресный план проверки и gates

| Этап | Обязательные проверки |
| --- | --- |
| #333 (этот) | Сверка pinned API/package/license/config источников, ссылок, source integration points; diff/whitespace. Без inference/build/CTest. |
| #334 | Чистый Windows x64 runtime lock/wheels; CPU 2-output fixture, Unicode/space paths; fake sidecar protocol/cancel/timeout/crash/flood/malformed/late result; source revision/session rejection; network disabled + missing subweight/config; gain >1, exact geometry 44.1/48/96 kHz and nonzero source offset, NaN/Inf/partial output. RT/UI isolation review. GPU probe/fallback на реальном устройстве отдельно. |
| #335 | Download consent/license unknown reject, checksum mismatch, interrupted/oversize download, cache corruption/quota/concurrent install, offline full closure, hostile checkpoint/config reject; dependencies/weights notices и SBOM. |
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
Raw backend logs уходят в NUL, сохраняются только ограниченные protocol events;
не реализован обещанный выше диагностический log ring. UI-интеграция — #336.

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
каталог не должен содержать прежние файлы. Runtime candidate закрепляет верхние
версии; полный resolved freeze сохраняется CI как evidence, не объявлен готовым
hash-locked distributable. Модели/Python не пакуются; Studio runtime содержит
только `mr_separation_worker.py`. В UI действия Separate пока нет (#336).

CI `Separation CPU contracts`: Windows x64/CPython 3.11; Python synthetic contract
tests, C++ fake-sidecar faults, затем real official HTDemucs CPU smoke с отключённой
сетью worker. Результаты — отдельный synthetic evidence artifact без weights/runtime.
`PlaybackProbe` одновременно проводит существующий engine process + raw recorder
через fake device; это изоляция сервиса, не ASIO benchmark. Обычная Windows Studio
сборка остаётся JUCE/VST3/ASIO/ARA2 ON и проверяет host fault suite.

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
