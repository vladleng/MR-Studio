# Локальная разработка и проверка

Проверено по текущей конфигурации 2026-10-06. Команды выполнять из корня исходного
репозитория, а не папки чатов. Правила: [AGENTS.md](../AGENTS.md),
[LOCAL_BUILD_POLICY](LOCAL_BUILD_POLICY.md); текущая поставка — [PROJECT_CONTEXT](PROJECT_CONTEXT.md).

## Конфигурация этой машины

Существующий `build/asio-local`: Windows x64, Visual Studio 18 2026 / MSVC,
Release; ASIO, VST3, Desktop и JUCE UI включены. Зависимости уже закешированы.
CMakePresets.json сейчас отсутствует. Не придумывай presets и не создавай новый
toolchain вместо работающего. Проверь CMakeCache.txt перед использованием на другой машине.

```powershell
$mrsCmakeBin = 'C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin'
& "$mrsCmakeBin/cmake.exe" -S . -B build/asio-local -DFETCHCONTENT_FULLY_DISCONNECTED=ON
# Продолжать только при exit 0.
& "$mrsCmakeBin/cmake.exe" --build build/asio-local --config Release --parallel 4
# Продолжать только при exit 0.
& "$mrsCmakeBin/ctest.exe" --test-dir build/asio-local -C Release --output-on-failure
```

Сохраняй build/test логи в build-папку с именем этапа. Для небольшого UI-изменения
можно собрать target `MoonRiverStudioJuce` и выполнить затронутые GUI-проверки;
при shared/RT изменениях расширяй scope по риску. Docs-only: проверь UTF-8,
ссылки и `git diff --check`, не пересобирай приложение без причины.

## Выходные файлы и GUI smoke

- GUI: `build/asio-local/MoonRiverStudioJuce_artefacts/Release/Moon River Studio JUCE.exe`.
- Scanner, fixture и test tools: `build/asio-local/Release/`.
- Синтетический VST3: `mrs_vst3_fixture.vst3` (файл, не папка).
- CTest содержит `juce_j2_smoke` и `juce_j3_smoke`; J3 запускает также J2.

Для отдельной проверки упакованного GUI подставь проверенный путь к новому пакету:

```powershell
$mrsPackage = 'ABSOLUTE_PATH_TO_PACKAGE'
$mrsSmoke = Start-Process -FilePath (Join-Path $mrsPackage 'Moon River Studio JUCE.exe') `
    -ArgumentList @('--j3-smoke', '--fixture', ('"' + (Join-Path $mrsPackage 'mrs_vst3_fixture.vst3') + '"')) `
    -WorkingDirectory $mrsPackage -WindowStyle Hidden -Wait -PassThru
$mrsSmoke.ExitCode
```

`-Wait` обязателен: обычный запуск GUI из PowerShell может вернуть управление
до завершения теста. При ошибке смотри exit code и `juce-j2-failure.txt`, если он
создан. Превью `juce-*-preview.png` проверяй визуально. JUCE software snapshot не
доказывает отрисовку native child VST3 или физический переход между DPI-мониторами.

## Пакет и приёмка

Создавай новый каталог в `Builds/` папки чатов, не перезаписывай старый. Клади GUI,
scanner, актуальные документы/лицензии и необходимые fixtures/логи; не копируй
старые результаты под видом новой проверки. После packaged smoke сформируй
SHA256 manifest (без самого manifest), проверь каждый файл и соответствие EXE
исходной Release-сборке. Укажи source commit и границы ручной проверки.

Приёмка пользователя фиксируется в текущем контексте и тематических docs/issues;
исходные артефакты выпущенного пакета сохраняются. Пользовательские Saves и
установленные плагины не являются одноразовыми тестовыми fixtures.

Не отправляй локальную ветку с кодом на GitHub ради docs. Если обновление GitHub
нужно по задаче, используй отдельное изменение только выбранных документов
с `[skip ci]`; issues обновляй без фиктивной приёмки и без запуска Actions.
