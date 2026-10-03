# SHARED Stage 2 — проверка

Скачать artifact `MR-Studio-SHARED-Stage-2-Windows` из Core contracts CI для Stage 2 PR.
Распаковать всю папку и запустить `Start-Timeline-Test.cmd`.
ASIO и музыкальный файл не требуются: checker проверяет общие сервисы и настоящий
AudioEngine с подготовленным пустым graph, без физического драйвера.

Ожидаются четыре PASS:
1. Arrange and Live share chord/section context
2. marker/section navigation uses shared audio transport
3. musical edits and Undo/Redo visible across workspaces
4. tempo/meter boundaries and snapshot v2 round-trip

И финальная строка:
`SHARED Stage 2 check passed. No GUI or physical ASIO test required.`

При FAIL прислать вывод. Это консольная проверка backend, GUI появится на #20/#28.
Не нужно повторять отложенные длительные ASIO-тесты ради Stage 2.

Сборка из исходников:
```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
& ".\build\Release\mrs_timeline_check.exe"
```
