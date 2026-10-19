# ARA 2 Clip / Timeline / Playback Regions — #178

Дата: 2026-10-18. Parent [#88](https://github.com/vladleng/MR-Studio/issues/88).
P1/#177 принят пользователем; база этого этапа — `56c5fbe`.
Пользовательский пакет **0.5** сохранён. Это backend-этап, без Edit with ARA UI.
Канонические границы: [ARA2_HOSTING.md на main](https://github.com/vladleng/MR-Studio/blob/main/docs/ARA2_HOSTING.md).

## Модель и семантика

`Session::synchronize(Project, SourceSnapshot[])` создаёт ARA AudioSource →
AudioModification → PlaybackRegion. Источники истины остаются `Project/Clip`,
существующие Commands/ProjectStore и неизменяемый `audio::AudioData`.
SourceSnapshot содержит opaque asset ID, revision и PCM snapshot. Смена revision
или PCM identity существующего source требует явного detach/relink; старый
кэш не подставляется под новые данные. Исходный файл никогда не перезаписывается.

Один source имеет **одну общую modification**. Все его вхождения, включая
split/copy/duplicate, разделяют правки этой modification. Независимый clone edits
не подразумевается. Move/trim обновляют region properties того же Clip ID.
Удаление уничтожает region; source/modification деактивируются и сохраняются
для Undo до закрытия binding. Undo/Redo восстанавливают regions с теми же
source/modification, не создавая повторно vendor edits. Source offset и duration
выражаются в source seconds; playback placement — в project seconds.

Моно/стерео и совпадающие source/project rates поддерживаются. Несовпадение
частот отвергается явно, как в действующем Audio Arrangement; новый resampler
или stretcher не добавлялся. Source и project при 48/96 kHz проверены отдельно.
`MusicalContext/RegionSequence` группируют regions. Опциональный ARA content
provider tempo/key/scale/chords **не объявляется**. Нет фиктивного экспорта Chord
Track. VST3 process context при подготовке содержит sample position, точный PPQ,
tempo/meter/bar; блоки разделяются на границах tempo/meter.

## Подготовка PCM и общий аудиограф

P2 использует **stopped offline preparation**, не vendor DSP в ASIO callback:

1. UI/control thread последовательно обновляет модель в edit session.
2. File-backed PCM декодируется отдельным worker с независимым file handle.
   RT ReadAhead cursor не заимствуется. Host readers держат shared immutable
   PCM и поддерживают planar float/double, padding вне source и явные ошибки.
3. ARA renderer получает ровно один region. VST3 offline process работает
   worker-блоками до512 samples; UI владеет моделью и ждёт join перед её изменением.
4. Reported latency до262144 samples удаляется из результата подготовки:
   renderer обрабатывает clip+latency, первые latency samples отбрасываются.
   Изменение latency во время подготовки отвергается. Возвращается ClipCache.
5. `Arrangement::apply` заменяет `Voice.asset` на обработанный PCM и offset на0;
   start/length/routes/fades остаются хостовыми. Второй dry voice не создаётся.
   Общий AudioEngine выполняет seek/loop/mix/inserts/record/PDC как раньше.

У кэша нулевая дополнительная runtime latency: vendor latency уже учтена.
Она не добавляется повторно к mixer PDC. Renderer head/tail за пределами
авторского clip range не расширяют клип автоматически.

Model callbacks увеличивают content revision. Snapshot Clip + TimeMap + rate +
source identity + vendor revision определяют валидность кэша. Notifications
проверяются до/после render; изменившийся во время подготовки content не публикуется.
При ошибке/неподдерживаемом режиме/недоступном source возвращается dry fallback
с диагностикой; stale cache не используется. Отмена проверяется между блоками,
worker присоединяется, activation/binding корректно закрываются, частичный PCM
не публикуется. Она не может прервать зависший произвольный vendor call.

Application API: `attach_ara_source`, `detach_ara_source`, `refresh_ara`,
`ara_status`, `ara_caches`. Attach применяется к source и всем его occurrences;
замена уже привязанного source требует явного detach. Все операции stopped-only.
Существующие clip Commands/Undo/Redo вызывают синхронизацию при rebuild.
Project replace закрывает ARA runtime после остановки графа.

Особый tempo/meter transaction сохраняет insert DSP: `TimingPlan` опционально
содержит новые PCM voices. Они валидируются и меняются после stop/quiesce вместе
с timing, MIDI/click; при backend restart failure swap возвращает прежние
timing **и PCM**, ProjectStore transaction откатывается. Обычные MIDI-only swaps
и проекты без ARA этот путь не включают. Routing/fades/geometry менять им нельзя.

Runtime binding пока не сохраняется в `.mrsproject` и сам attach/detach не
объявляется долговечной Undo-командой: это UX/state этапы #179/#180. Существующие
clip Undo/Redo работают. Schema27/archive1 не менялись. AI не затрагивался.

## Потоки, lifetime, ограничения

ARA model/factory/controller/editor roles принадлежат UI/control thread.
Decode/DSP workers полностью завершаются до edit/teardown. AudioEngine владеет
только immutable PCM; Session, reader handles и vendor process в callback не попадают.
Новые операции allocate/free, I/O, mutex или model mutation в callback не добавлены.
Замена/retirement shared PCM происходит в существующем quiescent control path.
Offline DSP worker применяет тот же ScopedNoDenormals.

Лимиты: до32 sources/session, source snapshots до256MiB (включая Undo-retained),
render cache до256MiB/session; до8 Application bindings, aggregate source/cache
preparation budget256MiB. Старые пользовательские проекты и медиа не меняются.

Preparation API синхронный на control thread: тяжёлые decode/DSP выполняются
workers, но вызывающий UI ждёт завершения. Progress/cancel UX и автоматический
refresh открытого vendor editor ещё относятся к #179. Внутренние notes/pitch blobs
не становятся универсально редактируемыми host API.

## Проверки и приёмка

Локально Windows x64/MSVC/Release, адресные targets, parallel2.
Используются официальный Celemony TestPlugIn и отдельный тестовый derived
processor с gain0.5/latency17; original SDK sources не менялись.
`tools/ara-render-fixture` собирает его с Apache-2.0 ARA_Examples и исходными notices.
Системные VST3 plugins не устанавливались. В пользовательский пакет fixture не входит.

Проверяются mono/stereo, offset/position, split/move/trim, shared modifications,
17 regions/2 sources, delete/deactivate/Undo/Redo, source revision rejection,
rate mismatch,48/96kHz, stale cache, file reader/WAV integrity, error/dry fallback,
Application attach/playback/history/refresh/project replace, tempo transaction
и backend restart rollback, cancellation посреди slow render с восстановлением.
Playback включает stop/seek/loop и blocks1/7/32/128/129/511. Gain fixture сравнивается
с фактическим dry baseline общего mixer, учитывая принятый channel gain/pan.
Allocation/free probe и max callback/deadline измеряются на1000 callbacks128frames.

Окончательный backend-прогон: **6/6 ARA PASS (2.04s)**, включая исходные P1 tests;
**38/38 L3 PASS (54.28s)** после изменения TimingPlan; **12/12 ARA OFF PASS (11.08s)**
с отсутствующим SDK, включая обычные VST3 и clip/tempo/meter/history.
У processed fixture gain зависит от VST3 tempo/meter context: проверены точные
границы100/31967 samples, реальное изменение звука после tempo edit и восстановление
после Undo/backend failure. Не только пересоздание cache pointer.

В четырёх синтетических callback-прогонах (identity/processed, mono/stereo) по1000
блоков128 frames: **alloc/free0, deadline misses0**, максимумы5.8–15.1µs.
Это software profiling, не ASIO hardware измерение.
Основные логи: `build/ara178-final-ara.log`, `build/ara178-regression-final.log`,
`build/ara178-disabled-final.log`; сборки и все fixtures остаются локально.

Финальная GUI-сборка PASS. Последовательный scanner smoke PASS; J1 PASS;
J2/J3 **FAIL** с тем же `reorder synchronizes both views`, ранее воспроизведённым
в сохранённом пакете0.5. Общий GUI gate не green. Логи:
`build/ara178-gui-final-build.log`, `build/ara178-gui-final-smoke.log`.
Первый relink scanner завершился LNK1104 из-за одновременно работавшего scan test;
после завершения тестов выполнены последовательные успешные build/scanner smoke.
Это не исправление исходников scanner и не скрытый PASS первоначальной попытки.

**NOT TESTED:** физические ASIO/listening/xruns/DPI, коммерческие
Vovious/Melodyne/RePitch, vendor native editor UX, ARA archive/SaveOpen/recovery,
vendor internal Undo и отдельный memory-leak profiler. Synthetic callback timing
не доказывает hardware ASIO performance. Полный CTest/упаковка не запускались.
Ранее J2/J3 GUI smoke падали на `reorder synchronizes both views`, включая пакет0.5;
это не принято за PASS. #178 ожидает отдельной приёмки, #179 не начинался.
