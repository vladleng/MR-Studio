# ARA 2 Host Foundation — #177

Дата: 2026-10-18. Parent: [#88](https://github.com/vladleng/MR-Studio/issues/88),
FEATURES [#78](https://github.com/vladleng/MR-Studio/issues/78).
Контракт: [ARA2_HOSTING.md на main](https://github.com/vladleng/MR-Studio/blob/main/docs/ARA2_HOSTING.md).

Локальная P1 foundation принята пользователем 2026-10-18: «#177 принимаю».
Принят implementation commit `b3881559046afeb9acd395d9c9f752adf1e66c66`
с приведёнными ниже ограничениями; это приёмка технического этапа, без clip UX.
Исходная база — `f3916aa91b2552398689735c22837fc3165b6519`, последний пользовательский
пакет **0.5** сохранён. Новый runtime package/ярлык не создавались.
P2/#178 и последующие этапы не начинались. Edit with ARA в продукте пока отсутствует.

## Проверенная архитектура и зависимости

JUCE **9.0.3** отвечает за GUI. Реальный VST3 host — `mrs_vst3`, непосредственно
Steinberg SDK **3.8.1**, root `3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96`.
Поэтому JUCE AudioPluginHost не вводится как второй host/engine.

ARA SDK public tag `releases/2.3.0`, root
`0005c532bb03eb210e7bb490329fc6d7ba1792c7`:

- ARA_API: `65ec5c43b943a48cb5446f448a0492db6af8534b`;
- ARA_Library: `d18a6a5e489816316be84a9de0eaf7307bc1abe4`;
- ARA_Examples: `abd7c8aa5854591995e1fbf16f854c65b0998e8d`.

SDK находится только в игнорируемом `build/_deps/ARA_SDK-2.3.0`.
Лицензия ARA API/Library/Examples — Apache-2.0; исходные LICENSE/NOTICE сохранены
в SDK. При будущей поставке бинарников с ARA необходимо включить применимые
LICENSE/NOTICE. Examples используют отдельные third-party notices; тестовые
fixtures не входят в пользовательский пакет. JUCE лицензируется отдельно.
Ни SDK, ни исходники проекта не публиковались.

`MRS_BUILD_ARA=OFF` по умолчанию. При ON требуются VST3 и явный
`MRS_ARA_SDK_ROOT`. CMake проверяет root/API/Library pins и наличие файлов;
configure не скачивает зависимости. При включённом JUCE используется
`juce_set_ara_sdk_path`. `JUCE_PLUGINHOST_ARA` не включается в продуктовом GUI:
он относится к JUCE host, который продукт не использует.

## Реализованный контракт

`mrs_ara` — отдельная opt-in библиотека. `mrs::ara::probe` сопоставляет audio-effect
class с ARA Main Factory по имени, с однозначным fallback для единственной пары.
Наличие VST3 не означает ARA. Проверяются размер factory, диапазон ARA API,
идентификаторы и обязательные entry points. SDK 2.3.0 использует согласованную
API generation **ARA 2.0 Final**; новые optional возможности не обещаются.

`probe_isolated` запускает `mrs_ara_scan.exe` скрытым дочерним процессом:
таймаут по умолчанию 10 секунд, отмена, завершение зависшего child, ограничение
ответа 1 MiB/4096 classes, проверка формата. Worker может выполнять изолированное
сканирование. Оно не меняет существующий VST3 cache. Относительный module path
разрешается до смены cwd дочернего процесса; helper принимает Unicode arguments.

`Session` владеет пустым DocumentController и VST3 instance с тремя ARA2 roles:
playback renderer, editor renderer, editor view. Model mutation и vendor calls
выполняются только на UI/control thread. Первый вызов in-process API должен быть
на UI thread; дальнейшие вызовы из других потоков отвергаются до vendor calls.
Session нельзя копировать/переносить; уничтожение должно происходить на владельце.
Нарушение потока уничтожения завершает процесс, а не вызывает vendor teardown на RT.

Factory initialization разделяется несколькими документами той же factory.
Последний владелец вызывает uninitialize, пока module и main-factory object живы.
Provider использует существующий Steinberg lifecycle с явным host context,
не меняя глобальный PluginContextFactory обычных inserts.
Teardown: binding/component/controller → DocumentController → host callbacks →
factory uninitialize → module unload. `edit()` балансирует begin/end при исключении,
запрещает вложенные sessions; notify запрещён во время редактирования.
Отмена open проверяется на безопасных границах и выполняет тот же teardown.
Она не прерывает произвольный зависший vendor C API call внутри процесса.

AudioAccess/Archiving callbacks пока не предоставляют sources/archive и явно
отказывают в чтении/записи. Editor role binding не означает открытое native окно.
Обычные VST3 inserts не преобразуются в ARA автоматически.

## Сопоставление с локальным Core для следующего этапа

| Существующий владелец | Будущее ARA соответствие | Состояние P1 |
| --- | --- | --- |
| `Project`, `Clip` (`core.hpp`): id/track/source/start/length/source_offset | document identity, modification/playback region | Модель не менялась; mapping deferred P2 |
| `Application::assets_`, `asset()`, `owned_media_` (`desktop.hpp`) | source identity и reader lifetime | Нет ARA reader; нужен revision-safe adapter P2 |
| `audio::AudioData`, `WavFile`, `Voice`, `ReadAhead` | PCM access, timeline placement | Не использовать RT streaming cursor как ARA reader |
| `MoveAudioClip`, `TrimAudioClip`, `SplitAudioClip`, `ProjectStore` | host model edits и Undo | Не менялись; P2 подключается через Commands |
| `processing::PreparedGraph`, `audio::AudioEngine` | однократный processed render + PDC | ARA ещё не подключена; отдельного engine нет |
| `persistence::ProjectDocument`, `Extension` | archive state и missing-plugin recovery | Deferred P4; schema **27**, archive **1** сохранены |

Runtime Session не владеет долговечными пользовательскими данными. UI, команды,
Undo, persistence, AI и Studio/Live playback не изменены. В audio callback нет
новых вызовов, allocation/free, locks, I/O или ARA model operations. Registry mutex
существует только на control path. No-double-audio и ARA PDC требуют P2-проверок.

## Локальная проверка

Windows x64, MSVC, Release; сборки адресные, parallel=2.
Официальные ARATestPlugIn/TestHost собраны отдельно с `ARA_SETUP_DEBUGGING=OFF`
и существующим VST3 SDK; системная установка plugins не выполнялась.
JUCE ARAPluginDemo собран через `tools/ara-fixtures/CMakeLists.txt` в отдельном tree.

| Проверка | Результат |
| --- | --- |
| ARA library/helper/tests + product GUI target build | PASS |
| `ara_unsupported`, `ara_incompatible`, Celemony/JUCE lifecycle | **4/4 PASS**, финально 1.83 s |
| По 20 циклов: два документа, teardown/reopen, balanced exceptional/nested edits, thread rejection | PASS для обоих ARA plugins |
| Scanner: настоящий plugin, unsupported/invalid factory, missing module/helper, hang timeout, child failure, malformed reply, отмена ожидания | PASS |
| Официальный Celemony ARATestHost с ARATestPlugIn | PASS exit 0; это SDK reference, не MRS clip integration |
| VST3 probe/processing/state/editor/scan/desktop/realtime | **7/7 PASS** при ARA ON |
| ARA OFF, несуществующий SDK path: отдельная сборка и VST3 suite | **7/7 PASS**, 10.56 s |
| Recording/PDC/callback blocks/parallel equivalence/ownership | **5/5 PASS** |
| J1 GUI smoke | PASS |
| J2/J3 GUI smoke | **FAIL**: `reorder synchronizes both views` |

Общая адресная выборка: **17 PASS / 2 FAIL**, 14.96 s; после последнего изменения
только ARA provider повторно собран и ARA suite снова **4/4 PASS**.
Оба GUI failures воспроизведены отдельным запуском сохранённого пакета **0.5**
в `build/validation/ara177-baseline`, с идентичным сообщением и exit 1.
Этот существующий GUI regression не исправлялся в ARA этапе; общий GUI gate не green.

Первичные sandbox попытки MSBuild зависали; отдельные tests не могли атомарно
заменить временные fixtures. Окончательные build/regression результаты получены
вне sandbox; первичные FAIL сохранены в логах, не считаются PASS.

Основные логи: `build/ara-ctest-final.log`, `build/ara-regression-final.log`,
`build/ara-disabled-tests.log`, `build/ara-official-host.log`,
`build/ara-build-provider.log`, `build/ara-juce-build.log`.

**NOT TESTED:** коммерческие Vovious/Melodyne/RePitch, физические ASIO/listening/DPI,
native ARA editor window, source/region playback, transport/PDC ARA, Save/Open,
vendor internal Undo, отдельный memory-leak profiler. Повторные lifecycle циклы
не заменяют leak-profiler и не доказывают поддержку всех vendors.
Полный CTest/упаковка не выполнялись. Приёмка относится только к P1/#177;
issues не закрываются автоматически, P2 требует решения пользователя.
