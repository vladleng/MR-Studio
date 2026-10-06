# Приёмка 0.2a / Stage 4a

Issue #63. [Контракт и запуск](MIDI_LIVE_INPUT.md).
Проверять на копии проекта: Core 11 читается новой сборкой, старые 1–10 мигрируют.

- [ ] ASIO подключается с прежними rate/buffer/workers и аудиодорожки работают.
- [ ] Track → Add instrument track; назначить VST3-инструмент первым insert в Mix.
- [ ] Выбрать клавиатуру и All channels; I включён. Ноты/velocity звучат при Stop и Play.
- [ ] Sustain отпускается, pitch bend работает у инструмента с соответствующим mapping.
- [ ] Выбор одного MIDI channel фильтрует другие каналы; Input Off/I Off выключают ingress.
- [ ] Gain/pan, mute/solo, bus/send/Master и эффекты после инструмента работают.
- [ ] MIDI panic и Stop/Seek не оставляют зависших нот/sustain. Release/reverb tail допустим.
- [ ] Отключить клавиатуру: статус missing, ноты сбрасываются; вернуть устройство и проверить вход.
- [ ] Сохранить/открыть проект: инструмент, preset/state, вход/канал/monitor и routing восстановлены.
- [ ] Undo/Redo назначения/monitor и создания/удаления дорожки работают; прежние plugin editors/Pin сохранены.
- [ ] Подключить сохранённый проект с отсутствующим инструментом/портом: ясный статус,
      состояние сохранено, остальные аудиоканалы продолжают работать.
- [ ] Проверить читаемость MIDI input и Mix на 100%/150% DPI.

Не считать этим checklist пройденными MIDI recording, external output, P4/#54,
полную sustained/reconnect matrix #16 или acoustic latency benchmark.
