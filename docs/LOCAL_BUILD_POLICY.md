# MR Studio / MR Live — локальная сборка (Windows)

Справочник по локальному окружению и интерфейсу сборки. Применяется в режиме LOCAL, выбранном в AGENTS.md; статус задач и приёмка в этом режиме — DSH Task Manager. **Полномочия, Git и приёмка** — в [AGENTS.md](../AGENTS.md); **выбор объёма проверок L0–L4** — в [mr-studio-build-test](../.agents/skills/mr-studio-build-test/SKILL.md). Этот файл открывай **по нужному разделу**, а не целиком перед каждой сборкой. В режиме CLOUD действует [облачный процесс](CLOUD_DEVELOPMENT.md); локальная справка не запрещает удалённые проверки и публикацию тематической ветки, разрешённые AGENTS.md.

## Окружение и каталоги

- Visual Studio 18 с MSVC, Windows SDK, CMake и Ninja устанавливаются вне checkout. Версии зависимостей закреплены в `tools/ci/sdk-lock.json`; при подготовке SDK проверяй commits и ASIO checksum. Скрипт подготовки — `tools/ci/Prepare-Sdks.ps1`.
- Для локальных SDK используются `build/sdk` и `build/private-sdk`; профили собираются в отдельных деревьях `build/local-windows/<profile>`. Сборки, кэши, символы и логи остаются вне Git. Наличие папок не означает, что SDK или toolchain готовы.
- Для пользовательской приёмки runtime размещай отдельно от checkout и сохраняй прежние принятые сборки. Ограничения записи при запуске из checkout, наблюдавшиеся 2026-11-08, относились к конкретной среде запуска, **не ко всем Windows-ПК**; проверяй фактические права.
- Исходники и историю Git резервируй отдельно от ПК. Пользовательские проекты, настройки и WAV/NAM не удаляй и не изменяй ради сборки.

## Developer PowerShell

Из корня checkout для ручных инструментальных команд:

```powershell
& .\tools\Enter-DevEnvironment.ps1
# При необходимости: -VsInstallationPath '<каталог Visual Studio>'
```

Скрипт выбирает завершённую установку Visual Studio 18, настраивает MSVC host/target x64, проверяет Windows SDK и добавляет CMake/Ninja в PATH **текущего процесса**. Не меняет профиль, системный PATH или текущий каталог, не устанавливает SDK и не запускает сборку. В новом PowerShell-сеансе вход требуется повторить. Для `Build-Local.ps1` отдельный вход не нужен: окружение подключается автоматически; запуск DSH не меняется.

## `tools/Build-Local.ps1`

Перед исполнением **сверяй параметры с текущим скриптом и CMake**: примеры ниже описывают состояние присланной документации, а не проверенный интерфейс текущего checkout. Скрипт самостоятельно подключает окружение, использует подготовленные SDK и ничего не скачивает.

| Параметр | Поведение |
| --- | --- |
| `-Product Core\|Studio\|Live\|Both` | По умолчанию Studio; Core собирает `mrs_core_tests` без явно выбранного target. Both собирает Studio с существующей зависимостью от Live. |
| `-Step Configure\|Build\|Test\|All` | Build делает configure и собирает target/приложение; Test не делает configure/build; All — configure, адресная сборка и выбранные тесты. |
| `-Target`, `-Jobs`, `-Configuration` | Цель, параллелизм, `Release`/`Debug`/`RelWithDebInfo`. |
| `-BuildDir` | Отдельное дерево сборки; относительный путь считается от корня checkout. |
| `-NoAra` | Отдельное диагностическое дерево Studio/Both без ARA. |
| `-TestRegex` или `-Full` | Для Test обязателен выбор тестов; `-Full` сам по себе **не собирает** все тестовые targets. |
| `-DryRun` | Показывает план без configure, компиляции и тестов. |

Studio — **ARA ON**, Live standalone — **ARA OFF**; в совместной сборке ARA включена только для Studio. Studio ARA OFF — диагностический вариант по отдельному запросу. Скрипт не упаковывает runtime и не меняет ярлыки.

Примеры адресного запуска из корня checkout:

```powershell
& .\tools\Build-Local.ps1 -Product Studio -DryRun | ConvertTo-Json -Depth 6
& .\tools\Build-Local.ps1 -Product Core -Target mrs_core_tests -Jobs 1
& .\tools\Build-Local.ps1 -Step Test -Product Core -TestRegex '^core_(model|timeline|transport|commands|serialization)$'
& .\tools\Build-Local.ps1 -Product Studio -Jobs 1
& .\tools\Build-Local.ps1 -Product Live -Jobs 1
```

### Особенности тестового запуска

- Test проверяет соответствие существующего дерева выбранному профилю. Необходимые тестовые targets нужно собрать заранее; полный CTest — только согласно уровню L4 из Skill.
- Для fixtures скрипт временно перенаправляет `TEMP`/`TMP` в `<build-dir>/Testing/Temp` и восстанавливает исходные значения даже при ошибке.
- ARA render/model-проверки требуют `-AraFixture` при configure/build/test. Fixture конфигурируется отдельно, при Build/All собирается до конфигурации продукта; для обеих suite используется один plugin с ожидаемым gain `0.5`. Configure только подготавливает деревья: **перед тестами fixture необходимо собрать**. Полный ARA-прогон требует `-AraFixture`; для Core/Live параметр недопустим.
- Статические проверки интерфейса локального скрипта: `& .\tools\tests\Local-Workflow.Tests.ps1`.

## Границы проверки и выпуска

Объём тестов, RED → GREEN, исключения для визуальных изменений, изоляция Studio/Live и отчётность определяются [mr-studio-build-test](../.agents/skills/mr-studio-build-test/SKILL.md) и [AGENTS.md](../AGENTS.md). Не заменяй реальную проверку ASIO/MIDI/DPI/vendor фактом успешной компиляции. Локальные логи не добавляй в Git.

Перед назначением версии или подготовкой пакета используй [VERSIONING.md](VERSIONING.md). Принятые runtime и диагностические артефакты сохраняй отдельно; права на merge, push, Actions и публикацию определяются только `AGENTS.md` и текущим запросом.
