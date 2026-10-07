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

## 0.2f upd1 — дополнительные проверки

- [ ] CC1 точки связаны прямыми отрезками; CC64 остаётся ступенчатым.
- [ ] Lane под piano roll; horizontal zoom/scroll и trim offsets совпадают.
- [ ] Ctrl + wheel vertical, Ctrl + Shift + wheel horizontal, Shift + wheel scroll.
- [ ] Double left click удаляет ноту, пустое место создаёт; Undo/Redo.
- [ ] Растущий MIDI take виден до Stop, held notes удлиняются, Stop/Undo/Redo прежние.
- [ ] Edit открывает audio/MIDI clip; audio waveform/trim и Undo.
- [ ] Attach/Detach, Mix/Edit/Arrange, panel resize, selection/zoom сохраняются.
- [ ] Save/reopen сохраняет edits; dock/zoom transient, Windows 100%/150% DPI.

Эти семь доработок отдельно запрошены пользователем 2026-10-07.
