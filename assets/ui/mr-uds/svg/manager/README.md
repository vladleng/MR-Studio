# MR Manager SVG Kit v1

46 отдельных оригинальных SVG под принятый скриншот MR Live (2026-10-26), без растровых вложений. Папки: `status/`, `priority/`, `progress/`, `indicators/`, `components/`, `controls/`, `actions/`.

- [Галерея](../../previews/manager.html)
- [Каталог](../../catalogs/manager-v1.json)
- [Цвета и размеры](../../tokens/manager.json)

`reference` — образцы с текстом и показаниями из фиктивного UI. `template` — фон и геометрия без имён и значений. `icon` — динамически раскрашиваемые в JUCE пиктограммы. Статус, прогресс, приоритет, текст, M/S/R и фактические данные рисует host, а не статический SVG. Добавление в manifest не означает, что эти файлы уже встроены в Windows executable. Старый build_kit.py нельзя запускать: он переписывает manifest.
