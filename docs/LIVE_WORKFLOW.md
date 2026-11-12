# MR Live — базовый workflow и границы

**MR Live — отдельное сценическое приложение на Shared Core**, а не Live Mode внутри MR Studio. Действующий подробный контракт: [MR_LIVE_NATIVE_INTEGRATION.md](MR_LIVE_NATIVE_INTEGRATION.md); актуальные требования и приёмка — [#14](https://github.com/vladleng/MR-Studio/issues/14), [#281](https://github.com/vladleng/MR-Studio/issues/281), [#302](https://github.com/vladleng/MR-Studio/issues/302) и профильные Sub-issues.

## Документы и переходы

- MR Studio работает с DAW-проектом `.mrsproject`.
- MR Live работает со своим show-документом `.mrlive`, items, setlist, performance каналами и Playback/stems.
- Запуск MR Live из Home Studio допустим, но Live самостоятельно управляет show. Прямого открытия Studio song через Live item нет.
- Studio → Live как готовый Playback — отдельная будущая функция экспорта, не автоматическая ссылка на редактируемый Studio-проект.

## Исполнение и надёжность

- Shared Core/Audio/MIDI/Transport/Plugin Host переиспользуются; отдельный движок Live не создаётся.
- GUI, загрузка моделей/плагинов, сеть и дисковые операции не должны блокировать audio callback.
- Item, patch/sub-patch и изменения шоу проходят принятый модельный/Undo/сохранение контракт; детали проверять по текущим Issues.
- MR Live standalone собирается **без ARA2**; MR Studio — ARA2 ON по умолчанию.
- Для сценических функций необходима фактическая пользовательская проверка; CI PASS не равен hardware acceptance.

Исторические планы встроенного «Live Mode» и прямых `.mrsproject` ссылок из предыдущей редакции этого файла отменены решениями пользователя 2026-10-25 и правилами [AGENTS.md](../AGENTS.md). Они остаются только в истории Git.
