# Insert loading and Send bypass — 0.3c upd8 / #137

2026-10-11. Новый запрос после принятой upd7: медленно добавляются/удаляются
TH-U, native EQ и другие inserts; нужен значок питания для Send. Работа локальная,
без push/merge/Actions. Stage5/#25 не начинать. Пользователь принял 2026-10-11: «Тесты прошли приемку», #137 закрыт completed.

## Причина и реализация

Structural SetInserts раньше создавал новый VST3 для проверки, уничтожая его,
а затем создавал повторно в рабочем графе. Любое добавление/удаление native/VST3
также захватывало состояния всех VST3 и пересоздавало цепи всех каналов/Master,
включая IR loaders. Это кодовый источник лишней работы, не измерение TH-U секунд.

Теперь Application останавливает callback и quiesces/join ahead owners, захватывает
только затронутую цепь и готовит её один раз с фактической конфигурацией устройства.
Состояние удалённых/заменённых VST3 не сериализуется, если его не нужно переносить
в новую цепь; удаление последнего VST3 не вызывает getState.
Та же PreparedGraph используется в рабочем engine graph. Другие каналы сохраняют
свои PreparedGraph, pending parameter state и IR loaders. Новый PDC/routing план
всё ещё строится после Stop/Pause, сохранены transport position и hardware handle.
Retained instruments получают panic перед сбросом MIDI generation. Операции
подготовки/освобождения не выполняются в callback. До model commit ошибка
оставляет документ неизменным и восстанавливает обработку прежнего графа.

Без подключённого устройства цепь проверяется один раз; подключение позднее
готовит её штатно. Внутри изменяемой цепи существующие плагины всё ещё создаются
заново. Structural Undo/Redo и прочие structural edits используют прежний общий
путь. Не заявлены мгновенная plugin-internal загрузка, async UI hosting,
crash isolation, бесшовный structural edit при Play или замеры physical ASIO.

## Send power

Синий power активен, серый — bypass; значок тот же, что у inserts. Верхняя
полоса регулирует сохранённый gain даже в bypass. ПКМ сохраняет меню назначения.
UI callback отложен и проверяет document identity/source/bus, чтобы не менять
новый проект или другой send после устаревшего клика.

Track::Send хранит bypass отдельно от gain. Application SetTrackSends даёт
одну Undo запись, обновляя effective send gain (0 при bypass) через имеющуюся
bounded mixer queue и ramp. Play/record не требуют пересборки DSP или reopening
device. При Undo/Redo флаг считается mixer-only. Preview не может включить
bypassed send. Маршрут остаётся в PDC; хвост downstream ревербератора затухает
штатно, Send power не выключает процессор return. Замена назначения сохраняет
gain/pre-post/bypass; остальные sends не затрагиваются.

## Persistence / compatibility

Project schema18 добавляет флаг 0/1 в строку Send. Reader схем5–17 включает старые
sends по умолчанию; неверный флаг отклоняется. Сохранённый upd8 проект требует
upd8 или более нового reader; прежние packages сохранены. Save As резервной
копии рекомендован при проверке нового пакета. Пользовательские проекты не
изменялись для тестов.

## Проверка и приёмка

PASS: Release build, CTest126/126 (123.17s), Core send roundtrip/legacy17/invalid flag;
Application live bypass/Undo/Redo/no device stop; VST3 fixture create counter
(одна новая instance, ноль для unrelated native add/remove), retained HWND,
getState0 при удалении последнего VST3, invalid-loading recovery;
GUI power/actual return silence/preview immunity/Undo/Redo. Финальная сборка
`build-final.log` без compiler warnings/errors; `ctest.log` — общий набор.
Packaged J3 fixture smoke PASS (exit0), fresh software100/150 previews просмотрены:
power/rail/caption размещены без перекрытия; physical mixed-DPI не измерялся.

Source `0866533ca157bcca6015fbeb4d1417a02e6697f9`, ветка
`mrs/0.2g-fix1-editor-space-local`. Compact13 files / 12 payload hashes verified:
`C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.3c-upd8-Inserts-Send-Bypass-JUCE-ASIO-Windows-local`.
GUI SHA256 `D4D49B452A3A7974047FD42D2CD2C2F565119732CEFACC2E25D0F2CECA18D527`.
Symbols: `build/asio-local/deliveries/0.3c-upd8-inserts-send-bypass`.
Logs/previews: `build/asio-local/validation/inserts-send-bypass`, packaged
`packaged-smoke/exit-code.txt` and `juce-mixer-sends-{100,150}-preview.png`.
Accepted upd7 GUI hash verified unchanged.

Manual: сравнить добавление/удаление TH-U и native EQ в рабочем проекте;
проверить power Send, возврат сохранённого уровня, Undo и Save/Open. #137 закрыт completed после пользовательской приёмки 2026-10-11. Compact runtime и symbols/logs/previews
хранятся отдельно; предыдущие builds не перезаписываются.
