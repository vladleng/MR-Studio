# SHARED Stage 3 — проверка
Скачать artifact MR-Studio-SHARED-Stage-3-Windows из актуального Core contracts CI,
распаковать и запустить Start-Processor-Test.cmd.

Ожидаются четыре PASS:
1. Arrange and Live share graph/patch state and Undo/Redo
2. native processing and sample-offset automation in shared AudioEngine
3. MIDI routing, device abstraction and panic
4. patch capture/restore and latency metadata

Финал: SHARED Stage 3 check passed. VST3 host, hardware MIDI and GUI are future stages.

Это backend-check с настоящим AudioEngine и встроенным gain processor, mock MIDI device.
Подключать MIDI-клавиатуру, ASIO или устанавливать VST3 не требуется.
При FAIL прислать полный вывод. GUI ещё нет.

Сборка:
```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
& ".\build\Release\mrs_processor_check.exe"
```
