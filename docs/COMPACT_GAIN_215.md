# Компактная громкость дорожек — #215

Follow-up к принятому #212, 2026-10-17. Пользователь запросил не скрывать gain
при уменьшении дорожек и выбрал `fader_slide_fill_sample.svg`.

## MIDI

Пользователь подтвердил: тишина клипов и live MIDI была следствием Solo гитары.
Solo GTR Arp закономерно изолировал остальные дорожки. Ошибка инструментов не
воспроизведена и пользователем снята; DSP/MIDI/routing код не меняется. Synthetic
fixture проверки не подменяют проверку сторонних плагинов/физической клавиатуры.

## Compact gain

Если фактический Track Header ниже 84 logical px (track pitch ниже 86 при зазоре
2 px), gain остаётся видимым как тонкое cyan заполнение. Minimum pitch 40 px
сохраняется. Pan/input/meters в таком компактном режиме остаются скрыты.
Выше порога сохраняется принятый обычный MR Slide; Mixer не меняется.

Геометрический векторный reference — существующий локальный
`assets/ui/mr-uds/svg/controls/concept_pack/fader_slide_fill_sample.svg`:
viewBox 224×40, line x=12..122 при sample_normalized 0,55, stroke 3 px,
round caps, цвет #00B8EF. Генераторы и reference SVG не меняются.

Runtime рисует реальное текущее значение, а не фиксированные 55%: заполнение
вдоль rail с отступами 12 px; faint base rail, небольшой unity tick. Hover/drag
дают сдержанный accent, keyboard focus — отдельный контур, disabled приглушён.
Высота области gain 12–16 px; невидимый endpoint target шириной 18 px.

Сохраняется фактический принятый DAW `Handle` с существующими Strip preview /
commit / cancel bindings и bounded mixer queue. Новый независимый fader/DSP
pipeline не вводится. Закон -60..+12 dB, 0 dB=60/72, signal metering отдельно.

Drag начинается на endpoint и остаётся относительным, без скачка на rail click.
Ctrl fine, arrows/Ctrl-arrows, Escape/focus loss cancel; release — одна команда
Undo, no-op не меняет history. В компактном режиме double-click возвращает
0 dB одной командой. Gain сохраняется обычным Project state, нового формата нет.
При достаточной высоте виден прежний dB label; gain tooltip содержит dB.

## Preview и приёмка

Реальные JUCE Track Headers при pitch 40/56/84/128 и Gallery с тем же Handle:
minimum, SVG55%, unity, maximum, disabled, keyboard focus и active drag;
software 100/125/150/200%. Это реальный control preview, не bitmap skin.
Проверяются настоящие gain/preview/Undo/Redo/fine/cancel/reset/no-op/SaveOpen.

Фактические результаты/source/EXE и ограничения записываются в #215.
Пользовательская приёмка compact gain ожидается отдельно; #212 остаётся принятым,
#213 не запускается. Physical DPI/ASIO/слух не выводятся из PNG/software fixtures.

