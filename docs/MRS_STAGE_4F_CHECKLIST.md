# 0.2f — пользовательская проверка

На копии проекта: MIDI clip → Controllers...

- [ ] Записанные CC/sustain/bend/pressure/Program видны в списке и своих линиях.
- [ ] Add/Apply/Delete; выбор каналов 1–16, CC/полинодного номера 0–127.
- [ ] Double click и drag события, snap, Escape, один Undo/Redo на жест.
- [ ] Bend -8192/0/8191, Program 0/127, CC64 Off/On, pressure endpoints.
- [ ] Редактирование не теряет ноты, другие events и ID. Save/reopen сохраняет всё.
- [ ] Play/seek/loop: последние значения восстанавливаются; Pause/Stop сбрасывают.
- [ ] Два соседних клипа: sustain/bend предыдущего не затирают новый head.
- [ ] Trim/split и source offsets, события в одной позиции, разные каналы.
- [ ] Неверный ввод и editing во время Play/Record не меняют проект.
- [ ] ASIO/VST3 mapping, Windows scaling и длительное playback без зависших нот.

Physical ASIO/VST3/mixed DPI подтверждаются пользователем; software PASS отдельно.
Дополнительные доработки после Stage 4. 0.2g/0.2h остаются следующими пунктами.
