# MR Cab P2/#184 — Collection manifest / Celestion parser

2026-10-14. **FEATURE READY, ожидает приёмки #184.**
#183 принят пользователем («#183 принимаю, начинай #184») и closed completed.
Требование совместимости старого Cab снято. #185 не начат.

## Реализовано локально

- NON-RT CabManifest v1: collection ID, stable root-relative asset IDs, semantic variant IDs,
  rate/frames/channels/bits/bytes/mtime/FNV fingerprint, raw relative filenames,
  mapping provenance, optional mic/position/mix/voicing/room/channel_mode и свободные tags.
- Collection→Cabinet→variant представлена collection ID и cabinet facet; физические
  rate/length assets сохраняются отдельно. Никакого Cartesian product или invented variants.
- Parser фактической Celestion: factory Hi/Lo и 121+57/421+121/421+57/All/All+Room,
  R-121/MD421/SM57 positions включая Dark 2/Rear, TLM107 RoomL/RoomR/Stereo.
  Ready mixes — premixed; room-included отмечен, не создаёт второй processor.
  Несовпадение channel label/header не классифицируется; unknown — Unclassified.
- Immutable import preview snapshot, async cancellable scan, generation guard, joined lifetime.
  Используется существующий bounded Reverb traversal (10000 WAV, no symlink traversal),
  затем physical headers перечитываются без Reverb pair interpretation.
- Стабильный manifest text codec с quoted UTF-8 strings, version/count/size/path guards;
  durable manual overrides/tags и missing assets сохраняются при reindex.
  Absolute root не сериализуется: relocation/relink использует новый local root.
- Ambiguous same variant/rate/frames помечается error, selection его исключает.
- Selector требует ту же variant и длину, предпочитает exact project rate,
  иначе48k/детерминированный fallback с необходимостью worker resample в #185.
  Selector **не выполняет DSP/resampling**.
- Worker integrity API проверяет size/digest и canonical containment до применения;
  changed/missing отклоняется без похожего fallback.
  Индексирование читает заголовки и raw bytes для fingerprint, PCM не декодирует.

## Границы

Это backend import preview/manifest, не подключённый GUI. Ручное mapping доступно
через manifest overrides API; интерактивный wizard — #187, Simple GUI — #185.
Нет новых project schemas, insert IDs, commands/Undo или callback changes.
App-global persistence представлен codec; подключение к global prefs UI будет #185.
Unicode paths сохраняются UTF-8; normalization распознанных токенов ASCII case-fold,
Unicode label case-fold не обещается. Неизвестные структуры требуют manual mapping.
Нет GUI/ASIO/listening acceptance или новых пользовательских пакетов; accepted0.4 сохранён.
FNV fingerprint — identity consistency, не security signature.

## Проверки

Release configure PASS. Full incremental Release build PASS.
Final targeted CTest: audio_cab_collection, audio_reverb_library,
processing_cab_ir, processing_reverb — PASS (точный итог в PROJECT_CONTEXT).
Synthetic: codec roundtrip/version rejection, deterministic reindex, Dark vs Dark2,
channel mismatch, actual-only length selection, exact/fallback SR, free tags/manual persistence,
changed/missing/relink, duplicates, Cyrillic paths, cancelled and superseded scan.
Opt-in реальный указанный Hempback: **68/68 WAV classified без errors**,
без копирования PCM в исходники или fixtures.
Раннее C++20 u8path warning-as-error исправлено native char8_t path constructors.
Полный CTest не запускался; GUI и hardware не требуются для metadata-only P2.

## Файлы и продолжение

core/audio/include/mrs/cab_collection.hpp, core/audio/src/cab_collection.cpp,
tests/cab_collection_tests.cpp, CMakeLists.txt. Всё source/build local.
#184 остаётся open до отдельной приёмки; #185 только после разрешения пользователя.
