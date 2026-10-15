# Единый локальный каталог MR Studio

С 2026-10-15 разработка, сборка и работа с навыками выполняются в:

`C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio`

Здесь находятся `.git`, `AGENTS.md`, `.codex/skills`, `CMakeLists.txt`, `apps`,
`core`, `tests`, `tools`, `docs`, SVG-библиотека `assets/ui/mr-uds`, `Builds` и `Saves`.
Прежний `C:/Users/Vladislav/Documents/GitHub/MR-Studio` оставлен для отката.
Не продолжать разработку или сборку в прежнем checkout.

## Сборка

Из корня проекта в PowerShell:

```powershell
.\tools\Build-Local.ps1 -Step Configure
.\tools\Build-Local.ps1 -Step Build
.\tools\Build-Local.ps1 -Step Test -TestRegex '^(core_|rig_|juce_rig_shell)'
# Полный CTest только для кандидата к приёмке:
.\tools\Build-Local.ps1 -Step Test -Full
```

Скрипт вычисляет корень относительно себя. Конфигурация: Visual Studio 18 2026,
x64, Release, JUCE 9.0.3, VST3, ASIO, BUILD_TESTING=ON. По умолчанию сборка
ограничена двумя параллельными задачами (`-Jobs`). Тесты автоматически не запускаются.
Для проверки выбранной группы сначала уточнить имена через `ctest -N`.

SDK восстановлены из существующего локального кеша в `build/_deps/`:
`JUCE-9.0.3`, `vst3sdk`, `portaudio-src`, `mrs-asiosdk.zip`.
Скрипт не скачивает зависимости. CMake проверяет JUCE 9.0.3 и SHA-256 ASIO
`d5ebf0c20dd2c5f43771fd0c1418f4b361bf52434ee670097cfa6b3a335e2eca`.
PortAudio source соответствует закреплённой ревизии
`873e3c83fbe2f57ebcf59083e627a3f8fa051ffe`.

Активная сборочная папка — `build/asio-local/`. GUI после сборки:
`build/asio-local/MoonRiverStudioJuce_artefacts/Release/Moon River Studio JUCE.exe`.
Компактные пакеты запуска — `Builds/`; проекты пользователя — `Saves/`.
`Builds`, `Saves`, `build`, `Архивы` исключены из Git.

## Архив и откат

Полная проверенная копия обоих каталогов до настройки:
`C:/Users/Vladislav/Documents/MR-Studio-Migration-Backup-20261015-120627`.
Внутри `source/`, `target/`, SHA-256 manifests и `history-all-refs.bundle`.
Полные `.git` сохранены, включая объекты, отсутствующие на GitHub и dangling objects.
Bundle — дополнительная копия веток, не замена полного `.git`.

Предыдущие `build/asio-local/deliveries/` и `validation/` перенесены с проверкой
содержимого в одноимённые пути нового каталога. Остальная старая сборочная папка
сохранена в `build/legacy-pre-migration/`; её абсолютные пути являются историческими.
Не запускать там CMake, старые упаковщики и patch-скрипты. Старые одноразовые
скрипты и превью из корня проекта перенесены с проверкой SHA-256 в
`Архивы/legacy-root-tools-20261015/` (49 файлов, исходные пути в `manifest.json`).
Текущая команда — `tools/Build-Local.ps1`.
Принятые пакеты и пользовательские проекты не перезаписываются.

Для отката сначала сохранить новые изменения отдельно, закрыть сборку/приложение,
затем восстановить только нужные файлы из `target/` с проверкой manifest. Не заменять
весь каталог поверх новых Saves. Старый checkout и backup не удалять до приёмки.
Исходные сторонние WAV/NAM остаются по внешним путям и в перенос не включены.

Git remote: `https://github.com/vladleng/MR-Studio.git`. Ветка локальной разработки:
`mrs/0.2g-fix1-editor-space-local`. Не выполнять pull/reset/checkout main ради
синхронизации документации: локальная история исходников опережает опубликованный код.
На GitHub публикуются только документация/issues с `[skip ci]`; код/SDK/сборки остаются локально.

Результаты проверки миграции и manifest нового пакета хранятся локально в
`build/asio-local/validation/workspace-migration/`.

## Результат проверки 2026-10-15

Локальный commit настройки: `3cb8f5ab71254f649433d0648602fcfdf8a58e73`.
Свежие configure и полная Release-сборка успешны (CMake 4.3.1-msvc1,
MSVC 19.51.36260.0). Активный CMakeCache не содержит старого пути.
Сохранены 25 веток и 46 refs; `git fsck --full` завершился успешно.
168 файлов `apps/core/tests` совпадают с исходными побайтно. Все 65 SVG разобраны
и соответствуют manifest; 6 skills доступны в едином Git-каталоге.

Полный CTest: **154/155 PASS, 202.57 s**. Единственный сбой — `ai_authorization`:
`audio state busy; poll again`. Повтор только этого теста: **20/20 PASS** без
изменения исходников. В `AudioEngine::state()` есть ограниченный опрос состояния,
а AI-тест вызывает его при ожидании playback. Это согласуется с временным
конфликтом чтения; исходный полный прогон не считается полностью зелёным.
Исправление AI/движка не входило в перенос. Все исходные логи сохранены.

Новый отдельный пакет: `Builds/MR-Studio-0.4e-Workspace-Migration-JUCE-ASIO-Windows-local`.
13 файлов, 12 записей SHA-256; исполняемые файлы скопированы из новой Release-сборки.
Пакетные MR Rig smoke и diagnostics/J3 smoke прошли с exit code 0.
Ярлык `C:/Users/Vladislav/Desktop/MR Studio.lnk` указывает на новый пакет,
рабочая папка соответствует ему, прежняя иконка сохранена. Старый ярлык:
`launcher-before.lnk` в резервной копии. Символы новой сборки:
`build/asio-local/deliveries/0.4e-workspace-migration/`.

Проверены неизменность 703 файлов прежних пакетов и 20 файлов Saves.
Исходный checkout не изменён. Физическое ASIO/прослушивание/xruns/смешанный DPI:
**NOT TESTED**. Продуктовые этапы MR Rig этим переносом не принимаются и не переключаются.
