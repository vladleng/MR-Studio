# MR Cab #185 — 0.4a upd1: активный IR, unavailable rows, окно редактора

2026-10-14. **READY WITH MANUAL CHECK**, пользовательская приёмка ожидается.
По уточнению пользователя сначала исправляем несовпадение выделения/Ready и
оранжевые записи, плюс окно плагина поверх Studio. Автоподбор через меню **отложен**;
меню пока фильтруют список. #186 не начат, #185 не закрыта.

## Причины и изменения

Read-only проверка текущего пользовательского manifest показала536 записей,
из них202 Missing WAV после смены корня коллекции. Сам каталог намеренно
сохраняет missing identities для восстановления; UI показывал их вперемешку
с рабочими файлами и без объяснения оранжевого цвета.

- Missing assets скрыты по умолчанию; **Unavailable** показывает их при необходимости.
  Сохранённые references не удалены. Ошибки существующих unsupported WAV видны:
  причина отдельной строкой и в tooltip.
- Синяя заливка = **реально активный** WAV, matching verified path + fingerprint.
  Последний click/keyboard focus больше не выдаётся за успешную загрузку.
  При отклонённом выборе сохраняется выделение рабочего IR и выводится причина.
  Loading сохраняет прежнее active выделение до подтверждённой публикации.
- После relink используем фактический root-relative path проверенного файла из
  loader identity, а не старый requested key. Fresh instance без active kernel
  не подсвечивает прежнюю сохранённую ссылку как звучащую.
- После resizable-конфигурации повторно назначаем Windows owner HWND редактора.
  JUCE может пересоздать peer при setResizable и потерять прежний GWLP_HWNDPARENT.
  Owned window остаётся выше главного окна Studio; global always-on-top не включён.

## Границы

Только UI/Window ownership/version. DSP, loader, project schema23/archive1,
Undo/serialization и WAV не изменены. Пользовательские prefs только прочитаны
для диагностики; их содержимое вручную не переписано.
Начатые вспомогательные изменения для автоподбора отменены до фиксации.
Source: **8aec1612ad75bf7e80818159616c34d7973ca4d7**, branch
`mrs/0.2g-fix1-editor-space-local`. Changed: CabBrowser.h, Editors.cpp, version.hpp.

## Проверки / пакет

Release GUI target build PASS. **Targeted Cab/Reverb GUI2/2 PASS2.04s**.
Cab smoke: собственные200/500 IR, missing record скрыт, unsupported4ch row
не меняет active/selected row; native GW_OWNER после peer recreation совпадает
с main HWND, editor остаётся выше main после SetWindowPos(main, HWND_TOP).
Фикстура4ch записана синтетически; первая попытка через Recorder отклонена его
mono/stereo selector contract и исправлена RIFF fixture writer, продуктовый recording не менялся.
Packaged Cab smoke **exit0**. Screenshot100% inspected: active blue/unsupported reason visible.
Полный CTest **не запускался для upd1**. Прежние148/148 относятся к базовой0.4a.
Физический mouse/DPI transition и прослушивание — manual user check.

Package `MR-Studio-0.4a-upd1-MR-Cab-JUCE-ASIO-Windows-local`:13files/12hashes verified.
GUI SHA256 **AB522B59A9DA77A0271375CF4D483665087324D7A65E307F9DB694CA62DCF391**.
Symbols `build/asio-local/deliveries/0.4a-upd1-cab-ui`.
Existing MR Studio shortcut→upd1; icon retained; previous0.4a/0.4 packages preserved.
Code/build/packages local; GitHub only docs/issues [skip ci], no Actions/source push.
