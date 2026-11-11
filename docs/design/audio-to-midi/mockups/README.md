# Audio-to-MIDI UX mockups

В этой папке — **семь векторных схем** интерфейса из ТЗ [`../UX_SPEC.md`](../UX_SPEC.md).

| Схема | Файл |
|---|---|
| Drag & Drop стема на Instrument Track | [01_timeline_drag_drop.svg](01_timeline_drag_drop.svg) |
| Компактное окно выбора модели и одного инструмента | [02_stem_convert_dialog.svg](02_stem_convert_dialog.svg) |
| Общее окно прогресса / chunks / elapsed / стадия | [03_shared_progress.svg](03_shared_progress.svg) |
| Стартовый диалог полного микса | [04_mix_convert_dialog.svg](04_mix_convert_dialog.svg) |
| Выбор извлечённых MIDI-партий галочками | [05_select_extracted_parts.svg](05_select_extracted_parts.svg) |
| Preferences → Audio-to-MIDI | [06_preferences.svg](06_preferences.svg) |
| Два сценария на одной схеме | [07_two_workflows.svg](07_two_workflows.svg) |

**Формат:** SVG — читаемые браузером GitHub векторные воссоздания согласованных макетов, с теми же сценариями, полями и композиционной логикой. Они **не являются пиксель-в-пиксель копиями PNG**, ранее включённых в `MR_Studio_Audio_to_MIDI_UI_Mockups.zip` и DOCX в чате. Для точного визуального сопоставления рабочий чат может использовать приложенные пользователем исходные PNG вместе с данной структурной спецификацией. Исходный ZIP в GitHub здесь **не загружен**, поскольку текущий способ публикации через GitHub connector не поддерживает загрузку локальных бинарных вложений напрямую.

Внедрение UI должно наследовать актуальную MR Unified Design System и структуру текущего кода; реальную реализацию сверять с этими схемами и исходными PNG. Эти файлы — **design references**, не runtime assets и не доказательство реализации.
