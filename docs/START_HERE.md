# MR Studio / MR Live — быстрый вход

1. Открой [AGENTS.md](../AGENTS.md) — действующие правила и архитектурные ограничения.
2. Открой [назначенную GitHub Issue](https://github.com/vladleng/MR-Studio/issues) (или конкретный запрос пользователя): там находятся задачи, этапы, приёмка и актуальный статус.
3. Для общей архитектуры прочитай нужный раздел [ARCHITECTURE.md](ARCHITECTURE.md). **MR Live — самостоятельное приложение, а не встроенная вкладка DAW.**
4. Найди **один или несколько необходимых контрактов** в [тематическом индексе](README.md); не читай все `docs/`.
5. При необходимости подключи применимый Skill из [`.agents/skills/`](../.agents/skills/), а для сборки — [CLOUD_DEVELOPMENT.md](CLOUD_DEVELOPMENT.md) и текущий Actions workflow.

Принятые решения Studio/Live описывает [MR Live integration](MR_LIVE_NATIVE_INTEGRATION.md); визуальный стандарт — [MR UDS](MR_UNIFIED_DESIGN_SYSTEM.md).

Исторические записи старых Stage, FIX и UPD сохранены в Git history и Issues и не переопределяют текущие решения.

Входные файлы [Claude Code](../CLAUDE.md) и [Gemini CLI](../GEMINI.md) импортируют общий `AGENTS.md`. Skills для всех харнесов хранятся в `.agents/skills/`; при отсутствии автоматического обнаружения читай нужный `SKILL.md` напрямую. Облачная задача должна использовать опубликованную ветку: обновление локального checkout не обновляет уже запущенные облачные задачи.
