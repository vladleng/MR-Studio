# Облачная разработка и Windows CI/CD

Задача миграции: [#241](https://github.com/vladleng/MR-Studio/issues/241). Baseline0.6 upd1; тематическая ветка `codex/cloud-cicd-migration`, merge после отдельной приёмки.

## Источники и среда
GitHub main — основа исходников, документации и Issues. Codex Cloud работает в тематической ветке с текущим AGENTS.md и проектными skills. Локальная Windows-машина нужна преимущественно для пользовательской приёмки, физических ASIO/MIDI/DPI и licensed vendor проверок. Исторические local-only записи в LOCAL_BUILD_POLICY/LOCAL_WORKSPACE не являются действующим запретом CI/source push.

По [актуальному руководству Codex Cloud](https://learn.chatgpt.com/docs/environments/cloud-environments), создать среду через Work in > Cloud > Create environment либо Settings > Codex Cloud > Environments, подключить vladleng/MR-Studio, проверить подготовку и опубликовать среду (Publish). Для изменения подготовленной среды нужен Republish; существующие задачи сохраняют своё состояние. Legacy Cloud — отдельный процесс для прежних интеграций, не основная инструкция для новых задач.

В диалоге подготовки указать Linux, CMake>=3.22, C++20 compiler и Git; предложить `bash tools/ci/Setup-CodexCloud.sh` как команду Install script из репозитория. Скрипт конфигурирует Core без закрытых SDK и полного CTest. На ветке задачи после обновления исходников повторить configure при изменении CMake. Для проверки среды собрать профильные Core targets и выполнить соответствующие CTest, затем сохранить отчёт подготовки. Windows GUI проверяется отдельно Actions. Наличие скрипта не доказывает публикацию среды или успешный запуск задачи Codex Cloud; это пока NOT TESTED. Доступ к GitHub и сети настраивается средствами среды, секреты не помещать в скрипт.

## SDK и кэш
`tools/ci/sdk-lock.json` закрепляет JUCE9.0.3, VST3 SDK и PortAudio immutable commits; ARA SDK2.3.0 — отдельный профиль. Submodule commits определяются gitlinks закреплённого родителя. Actions также закреплены SHA.
`Prepare-Sdks.ps1` получает SDK только из указанных официальных origins, проверяет commit/dirty state/submodules. Public-source cache keyed lock hash + ARA profile; без restore-prefix fallback. Fork PR не записывает общий cache. ASIO ZIP скачивается с официального HTTPS endpoint и проверяется существующим SHA256; изменение архива — ошибка с явным review версии/лицензии, не автоматическая смена pin.
ASIO ZIP и извлечённые ASIO sources находятся вне кэшируемого `build/sdk`, не попадают в Git/artifacts. В package включается license notice, не SDK. Не вносить токены/плагины/пользовательские WAV/NAM в caches. Публичный source cache — оптимизация, а не доказательство корректности: pin checks обязательны на hit.

## Профили проверки
- Fast Core: Ubuntu Release, без ASIO/VST3/ARA/GUI; Core/AI contracts. Это не Windows product gate.
- Windows Studio: windows-2022, Visual Studio2022 x64, JUCE9.0.3/VST3/ASIO, Release. Быстрые profile checks включают Core/AI и GUI startup/AI. ARA opt-in добавляет host fixtures, не установленные коммерческие плагины.
- Full integration: `workflow_dispatch` с integration=true либо commit marker `[ci full]` в migration branch. Полный CTest без исключения #239/J2/J3. Не применять после каждой мелкой правки.
- Для `codex/cloud-cicd-migration` с открытым PR обычный commit автоматически проверяется **одним** Windows quick Release через `pull_request`. Дополнительный `push` Windows job пропускается, чтобы не компилировать тот же commit дважды. Маркеры `[ci full]`/`[ci ara]` запускают **дополнительный** Windows `push` job с расширенным профилем; обычный PR quick при этом остаётся отдельной обязательной проверкой. Для `main` push Release сохраняется. J2/J3 из #239 выполняются и остаются видимыми в полном CTest, но по решению пользователя не блокируют приёмку облачной инфраструктуры #241.
- ARA: input ara=true либо `[ci ara]`; сборка runtime с ARA. В отчёте указывать, какие plugin/render fixtures были реально созданы, а какие отсутствовали.
  В CI одна и та же обработанная ARA render-fixture применяется для `ara_clip_model` и `ara_clip_processed`. Её штатный коэффициент при 120 BPM составляет 0.5; `MRS_ARA_TEST_PLUGIN_GAIN` передаёт это значение в generic model suite (по умолчанию 1 для нейтральных внешних fixtures). Оба теста сохраняются; недопустимо маскировать ошибку удалением теста.

Новый workflow в ещё не принятой ветке впервые запускается push trigger; dispatch API требует доступности workflow на default branch. Не merge ради dispatch. Для этой миграции пользователь прямо разрешил контрольные runs.

## Artifacts
Runtime artifact: EXE, scanners, crash reporter, лицензии, README, source/run provenance, CI-TEST-RESULT.txt и SHA256.json. Символы — отдельным artifact, тестовые logs/JUnit — отдельным. SDK/fixtures/user settings не включаются. Artifact загружается и при test failure после успешной сборки для диагностики, но manifest содержит результат и job остаётся FAIL: такой пакет нельзя объявлять интеграционно принятым.
Скачать runtime artifact, сверить все SHA256/размеры/Windows version/source, проверить отсутствие SDK/secrets/fixtures/PDB. Затем выполнить user acceptance/hardware проверки. Срок хранения runtime14 дней, symbols7 дней; это Actions Artifact, не автоматический публичный GitHub Release и не подтверждение коммерческой лицензии продукта.

## Риски и приёмка
Исторический #239 (`reorder synchronizes both views`) не маскировать. Никаких continue-on-error/необоснованного отключения warnings/тестов ради green. Устранять первую содержательную ошибку; повторять затронутый контур, полный — при изменении интеграционного кандидата.
No alloc/free/locks/I/O/UI в callback; Commands/Undo/project/media сохранены. CI PASS не подтверждает физическую ASIO latency/xruns, слух, native mixed-DPI или совместимость всех vendors. Фактические результаты и ссылка на контрольный artifact ведутся в #241. Merge в main запрещён до отдельной приёмки.
