# MR Studio — технические направления разработки

Этот документ описывает **разделение ответственности**, не историю Stage и не состояние отдельных задач.

- **Studio:** DAW, Arrange/Edit/Mix, проект `.mrsproject`.
- **Live:** **отдельное приложение** MR Live, show-документ `.mrlive`, performance UX и item/setlist workflow.
- **Shared:** Core, аудиограф/ASIO, transport, MIDI, VST3 hosting, модели, persistence и общие механизмы Commands/Undo.
- **Native/AI/UI:** DSP, MR Rig, separation/transcription workers и MR UDS; конкретную область изменения задаёт назначенная Issue.

**Правило зависимости:** приложения могут параллельно разрабатывать собственные UI, но не должны создавать два разных звуковых движка. Привязка Studio song к Live item не подразумевается; будущая передача Playback — отдельная задача. Разные приложения имеют независимые версии.

Текущие задачи, декомпозиция и статус — [GitHub Issues](https://github.com/vladleng/MR-Studio/issues); границы реализации — [ARCHITECTURE.md](ARCHITECTURE.md), [MR_LIVE_NATIVE_INTEGRATION.md](MR_LIVE_NATIVE_INTEGRATION.md), [AGENTS.md](../AGENTS.md). Исторический Stage tracker доступен в Git history.
