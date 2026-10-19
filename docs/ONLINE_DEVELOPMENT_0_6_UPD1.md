# Онлайн-передача MR Studio 0.6 upd1 — 2026-10-19

Документальная основа — актуальный GitHub main. Snapshot подготовлен поверх main146ac1f; удалённые тематические документы сохранены. Source/apps/core/assets/cmake/tools/tests перенесены из локального618c3b5; skills включены с последними пользовательскими правилами, включая исключение тестов для чистой косметики. Локальная GUI-спецификация содержит более новые явные пользовательские решения.

## Что включено
Исходники C++/JUCE, CMake, синтетические fixtures/tests, MR UDS assets с manifest/generators и проектные `.codex/skills` с supporting resources. Генераторы assets не запускались. Отсутствующие на main локальные тематические документы добавлены; существующие удалённые не заменены старыми копиями.

## Что остаётся вне Git
Builds/build/PDB/EXE, Saves/архивы/отчёты и пользовательские WAV/NAM/настройки/ключи. Локальные SDK не загружены. Эта передача — исходный baseline, не GitHub Release с бинарным пакетом.

## Сборка
Core по существующим CMake targets доступен отдельно от Windows GUI. Для Windows GUI использовать JUCE9.0.3, VST3 SDK и PortAudio из проверенных зависимостей; ASIO SDK предоставляется отдельно. `tools/Build-Local.ps1` — локальный Windows helper с проверкой cached dependencies, не готовая команда для облачного Linux.
ARA opt-in: MRS_BUILD_ARA=ON требует MRS_BUILD_VST3=ON и populated MRS_ARA_SDK_ROOT официального ARA SDK releases/2.3.0; точные pins проверяет cmake/Ara.cmake. Конфигурация ARA не скачивает SDK. Без доступных SDK использовать ARA OFF и не заявлять проверку native ARA/ASIO. Существующие workflows не являются доказательством проверки Windows GUI baseline; Actions не запускались при переносе.

## Проверки и оставшаяся работа
Локальная версия618c3b5: Release GUI, ARA editor/J1 и packaged J1 PASS. Полный CTest для0.6 upd1 не выполнялся. Старый J2/J3 reorder synchronizes both views — отдельная #239, причина не локализована. Физические ASIO/MIDI/DPI, полная vendor matrix NOT TESTED. Пользователь подтвердил сохранение mixed-vendor проекта и все шесть AI ARA сценариев.
#177–181 приняты. #88 остаётся открытой для native sub-issue #240 Render and Restore: обратимая фиксация Event FX, durable original/archive, Restore после Save/Open, ПКМ и иконка клипа. Код #240 не начат.

## Работа дальше
Issue → актуальный контракт → код → адресные проверки → отчёт и отдельная пользовательская приёмка. GitHub main — документальная основа; не заменять его docs устаревшим локальным архивом. Source commits/push разрешены переходом на онлайн-разработку; CI/PR/merge не запускать автоматически. Не добавлять секреты и лицензируемые SDK.