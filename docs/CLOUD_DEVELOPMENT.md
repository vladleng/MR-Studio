# Облачная разработка и Windows CI/CD

Историческая миграция облачной разработки: [#241](https://github.com/vladleng/MR-Studio/issues/241) (закрыта 2026-10-20). Ниже описан **действующий** процесс на `main`; старые триггеры миграционного этапа не определяют текущую политику сборок.

## Источники и среда
GitHub main — основа исходников, документации и Issues. Codex Cloud работает в тематической ветке с текущим AGENTS.md и проектными skills. Локальная Windows-машина нужна преимущественно для пользовательской приёмки, физических ASIO/MIDI/DPI и licensed vendor проверок. Исторические local-only записи в LOCAL_BUILD_POLICY/LOCAL_WORKSPACE не являются действующим запретом CI/source push.

По [актуальному руководству Codex Cloud](https://learn.chatgpt.com/docs/environments/cloud-environments), создать среду через Work in > Cloud > Create environment либо Settings > Codex Cloud > Environments, подключить vladleng/MR-Studio, проверить подготовку и опубликовать среду (Publish). Для изменения подготовленной среды нужен Republish; существующие задачи сохраняют своё состояние. Legacy Cloud — отдельный процесс для прежних интеграций, не основная инструкция для новых задач.

В диалоге подготовки указать Linux, CMake>=3.22, C++20 compiler и Git; предложить `bash tools/ci/Setup-CodexCloud.sh` как команду Install script из репозитория. Скрипт конфигурирует Core без закрытых SDK и полного CTest. На ветке задачи после обновления исходников повторить configure при изменении CMake. Для проверки среды собрать профильные Core targets и выполнить соответствующие CTest, затем сохранить отчёт подготовки. Windows GUI проверяется отдельно Actions. Наличие скрипта не доказывает публикацию среды или успешный запуск задачи Codex Cloud; это пока NOT TESTED. Доступ к GitHub и сети настраивается средствами среды, секреты не помещать в скрипт.

## SDK и кэш
`tools/ci/sdk-lock.json` закрепляет JUCE9.0.3, VST3 SDK, PortAudio и ARA SDK2.3.0 immutable commits. ARA2 входит в обычную Windows-сборку по умолчанию; ручной ARA-OFF — только для диагностики. Submodule commits определяются gitlinks закреплённого родителя. Actions также закреплены SHA.
`Prepare-Sdks.ps1` получает SDK только из указанных официальных origins, проверяет commit/dirty state/submodules. Public-source cache keyed lock hash + ARA profile; без restore-prefix fallback. Fork PR не записывает общий cache. ASIO ZIP скачивается с официального HTTPS endpoint и проверяется существующим SHA256; изменение архива — ошибка с явным review версии/лицензии, не автоматическая смена pin.
ASIO ZIP и извлечённые ASIO sources находятся вне кэшируемого `build/sdk`, не попадают в Git/artifacts. В package включается license notice, не SDK. Не вносить токены/плагины/пользовательские WAV/NAM в caches. Публичный source cache — оптимизация, а не доказательство корректности: pin checks обязательны на hit.

## Профили проверки и запуск Actions

**Источник технического поведения — актуальные `.github/workflows/*.yml` текущей ветки.** `AGENTS.md` и `.agents/skills/mr-studio-build-test/SKILL.md` задают последовательность и условия проверки. Документация не заменяет анализ workflow перед запуском, особенно в тематической ветке, где YAML может отличаться от `main`.

- **Fast Core contracts** — `.github/workflows/core-contracts.yml`: Ubuntu Release, без ASIO/VST3/ARA/GUI; Core/AI и другие доступные контрактные тесты. Автоматически на подходящих изменениях в PR и push по настроенным веткам/paths, плюс ручной запуск. `concurrency.cancel-in-progress: true` отменяет устаревший незавершённый запуск того же профиля на той же ветке/PR. **PASS Fast Core не подтверждает Windows/JUCE/ASIO/ARA/VST3.**
- **Separation CPU contracts** — `.github/workflows/separation.yml`: специализированная Windows CPU-проверка separation; автоматический push ограничен предусмотренной в YAML веткой и путями, доступен ручной `workflow_dispatch`. Это **не** общий Windows Release.
- **Windows Studio Release** — `.github/workflows/audio-engine.yml`: Windows 2022 / MSVC / JUCE / VST3 / ASIO, Studio ARA2 **ON по умолчанию**. Текущий workflow запускается **только вручную через `workflow_dispatch`**: нет автоматических `push`/`pull_request` триггеров. Предназначен для обоснованных Windows-специфичных проверок, интеграционного этапа либо готового пакета пользовательской приёмки, **не после каждого commit или bugfix**.
- **Адресные профили Windows**: перед ручным запуском выбрать минимальный реально поддерживаемый `checks` (`studio`, `studio-live`, `live`, `separation`, `separation-ui` или `profile`). Live-only исключает ARA2; для Studio флаг `ara=true` по умолчанию, `false` — только явно запрошенная диагностическая Studio-сборка. Windows workflow может включать компиляцию, CTest и упаковку даже в адресном профиле — запускать его только с основанием.
- **Full integration (L4)**: только ручной `workflow_dispatch` Windows workflow с `integration=true` для согласованного интеграционного/релизного кандидата либо документированного технического риска. Маркеры коммитов `[ci studio]`, `[ci live]`, `[ci full]` **сами по себе больше не запускают Windows Studio Release**. Полный CTest не требуется для мелких исправлений; его нельзя подменять успешным коротким профилем, когда он действительно обязателен.

### Порядок и ограничения повторных проверок

1. Для задачи определить область изменения и уровень L0–L4; сначала адресный RED → GREEN при багфиксе, затем затронутые контрактные тесты и применимый Fast Core. Docs-only и чисто визуальные исключения сохраняются.
2. Если быстрые **обязательные** проверки упали, изучить их логи и устранить причину до тяжёлого Windows Release. При Windows-специфичном изменении нельзя считать задачу проверенной одним Linux CI; Windows gate выполнить перед завершением соответствующего этапа или подготовкой приёмочного пакета.
3. Не запускать одинаковые Windows-проверки параллельно и не инициировать Release ради проверки статуса предыдущей. После FAIL выяснить первую существенную ошибку, выполнить адресную диагностику и повторить только затронутые проверки; повторный Windows Release — при новой гипотезе/исправлении или подтверждённом инфраструктурном сбое.
4. Опрос GitHub Actions — **не чаще раза в 60 секунд**, после трёх безрезультатных попыток одного сбоя прекратить слепые повторы и зафиксировать диагностику в Issue. Эти правила для Codex закреплены в `AGENTS.md`; автоматические триггеры и отмена устаревших запусков определяются YAML.
5. Версии в run-name, заголовке приложения и артефактах должны совпадать для каждого компонента; их нужно проверить по текущему workflow и source header **до ручного запуска**, чтобы не тратить Windows CI на заведомый mismatch.

### ARA2 и совместимость

По умолчанию Studio включает ARA2 (и ARA scanner в соответствующем runtime ZIP); Live-only — без ARA2. Для ручного Windows workflow `ara=true` по умолчанию, `false` допускается только для диагностической Studio-сборки. В отчёте указывать реальные результаты plugin/render fixtures; нельзя маскировать ошибку отключением тестов.

В CI одна и та же обработанная ARA render-fixture применяется для `ara_clip_model` и `ara_clip_processed`. Её штатный коэффициент при 120 BPM составляет 0.5; `MRS_ARA_TEST_PLUGIN_GAIN` передаёт это значение в generic model suite (по умолчанию 1 для нейтральных внешних fixtures). Оба теста сохраняются.

## Artifacts
Runtime artifact: EXE, VST3-сканер, **ARA2-сканер при обычной сборке**, crash reporter, лицензии, README, source/run provenance, CI-TEST-RESULT.txt и SHA256.json. Символы — отдельным artifact, тестовые logs/JUnit — отдельным. SDK/fixtures/user settings не включаются. Artifact загружается и при test failure после успешной сборки для диагностики, но manifest содержит результат и job остаётся FAIL: такой пакет нельзя объявлять интеграционно принятым.
Скачать runtime artifact, сверить все SHA256/размеры/Windows version/source, проверить отсутствие SDK/secrets/fixtures/PDB. Затем выполнить user acceptance/hardware проверки. Срок хранения runtime14 дней, symbols7 дней; это Actions Artifact, не автоматический публичный GitHub Release и не подтверждение коммерческой лицензии продукта.

## Риски и приёмка
Не маскировать исторические регрессии (включая #239), не отключать тесты, warnings и обязательные проверки ради зелёного CI. Устранять первую содержательную ошибку и повторять затронутый контур; полный — только когда это требуется для кандидата или области риска.

No alloc/free/locks/I/O/UI в callback; Commands/Undo/project/media сохраняются. CI PASS не подтверждает физическую ASIO latency/xruns, слух, native mixed-DPI или совместимость всех vendors. Результаты, ссылки на запуск/артефакт и ограничения фиксируются в **актуальной** Issue/Sub-issue; #241 — архив миграции. Merge в `main` и закрытие задачи — только после соответствующей пользовательской приёмки.
