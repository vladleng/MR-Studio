# Локальная диагностика — исторические примеры (2026-10-06)

**Не действующая инструкция запуска сборок.** Ниже сохранены отдельные диагностические команды прежнего Windows-окружения; они не проверены для текущего локального checkout. За актуальными параметрами обращайся к реальным CMake/PowerShell-скриптам; справка — [LOCAL_BUILD_POLICY](LOCAL_BUILD_POLICY.md).

## Прежнее окружение

На 2026-10-06 использовались Windows x64, Visual Studio 18, MSVC Release и `build/asio-local` с JUCE, ASIO и VST3. Не следует предполагать, что такое дерево существует сейчас или что в нём свежая сборка.

## Исторический GUI smoke

В том окружении применялись `juce_j2_smoke`/`juce_j3_smoke` и синтетический `mrs_vst3_fixture.vst3`. Проверенный путь к новому runtime и реально доступные параметры запуска нужно определить по текущему коду и тестам. Пример старого сценария:

```powershell
$mrsPackage = 'ABSOLUTE_PATH_TO_PACKAGE'
$mrsSmoke = Start-Process -FilePath (Join-Path $mrsPackage 'Moon River Studio JUCE.exe') `
    -ArgumentList @('--j3-smoke', '--fixture', ('"' + (Join-Path $mrsPackage 'mrs_vst3_fixture.vst3') + '"')) `
    -WorkingDirectory $mrsPackage -WindowStyle Hidden -Wait -PassThru
$mrsSmoke.ExitCode
```

`-Wait` нужен для получения кода завершения процесса; software snapshot не подтверждает работу native VST3 child window или физических DPI-переходов.

## Исторические P4 profiling fixtures

Схемы измерений см. в [ENGINE_PROFILING.md](ENGINE_PROFILING.md). Запуски не следует выполнять параллельно: конкуренция за CPU искажает результаты. Ранее применялись команды:

```powershell
& build/asio-local/Release/mrs_channel_bench.exe > build/asio-local/P4-channels-off.csv
& build/asio-local/Release/mrs_channel_bench.exe --profile > build/asio-local/P4-channels-on.csv
& build/asio-local/Release/mrs_ahead_bench.exe --all-workers > build/asio-local/P4-ahead-off.csv
& build/asio-local/Release/mrs_ahead_bench.exe --all-workers --profile > build/asio-local/P4-ahead-on.csv
& build/asio-local/Release/mrs_ahead_bench.exe --topology mixed_disk8 --frames 128 --workers 4 --process 1024 --seconds 60 --record --profile > build/asio-local/P4-sustained-record.csv
```

Перед повторным использованием проверь существование бинарников, CLI-флагов, fixtures и код завершения. Не изменяй пользовательские проекты и настройки ради диагностики.
