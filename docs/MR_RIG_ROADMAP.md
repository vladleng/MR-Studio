# MR Rig — единый Roadmap (после объединения MR Cab #182)

**Решение 2026-10-15. Канонический активный parent — [#197 MR Rig](https://github.com/vladleng/MR-Studio/issues/197).** Отдельное направление [#182 MR Cab 2.0](https://github.com/vladleng/MR-Studio/issues/182) прекращено / закрыто как superseded; **его выполненные результаты сохраняются**, не удаляются и не переписываются. [#111 Native Plugins](https://github.com/vladleng/MR-Studio/issues/111) остаётся категорией. Это перенос roadmap, **не реализация кода**.

## 1. Реализованный фундамент (historical accepted)

| Issue | Статус | Что переиспользует MR Rig |
|---|---|---|
| [#183](https://github.com/vladleng/MR-Studio/issues/183) | **Accepted, closed** | Аудит IR-кода и коллекции Celestion |
| [#184](https://github.com/vladleng/MR-Studio/issues/184) | **Accepted, closed** | Manifest, импорт/идентификация WAV, Celestion data |
| [#185](https://github.com/vladleng/MR-Studio/issues/185) | **Accepted, closed** | Существующий IR convolver, async switching, native editor, presets |
| [#196](https://github.com/vladleng/MR-Studio/issues/196) | **Accepted, closed — 0.4b upd3** | Full Combo WAV, роли cabinet/full_combo/unknown, изменения native editors |

Принятая локальная поставка #196 — 0.4b upd3 Native Editors; **не** «pending acceptance». Существующий DSP является внутренним переиспользуемым компонентом MR Rig. Кабинетный Celestion IR остаётся как **дополнительный ассет**, а не отдельный пользовательский плагин с новым roadmap; никакие исходные WAV или пользовательские проекты не удалять.

## 2. Старые незавершённые задачи — решение

- **[#186 Advanced mic/phase/room/composite](https://github.com/vladleng/MR-Studio/issues/186)** — **отменить**, не переносить в обязательные функции MR Rig.
- **[#187 Generic TONEX WAV / IR Collections importer](https://github.com/vladleng/MR-Studio/issues/187)** — **отменить как отдельный продукт**. Только практичный `Import My IR` и manual role/artwork/tags, external asset identity, safe missing/relink и нужные WAV formats идут в [#199](https://github.com/vladleng/MR-Studio/issues/199). **Не переносить** универсальный каталог микрофонов, многофирменные вендорские профили и полноценный import wizard.
- **[#188 Cab-only modular/QA](https://github.com/vladleng/MR-Studio/issues/188)** — **отменить как отдельный этап**. Общие ограничения RT, shared host, module extension, PDC и QA распределить в [#198](https://github.com/vladleng/MR-Studio/issues/198), [#201](https://github.com/vladleng/MR-Studio/issues/201), [#204](https://github.com/vladleng/MR-Studio/issues/204). Не реализовывать второй аудиодвижок.

Архивные документы [MR_CAB_COLLECTIONS.md](MR_CAB_COLLECTIONS.md), [MR_CAB_ROADMAP.md](MR_CAB_ROADMAP.md), [MR_CAB_CODEX_HANDOFF.md](MR_CAB_CODEX_HANDOFF.md) — **справка о принятых Cab этапах, не активное ТЗ**. Спецификация дальнейшей разработки только в `MR_RIG_*.md` + активных #197–204.

## 3. Единственная активная последовательность

| Этап | Issue | Реальный результат | Dependency / gate |
|---|---|---|---|
| **P0** | [#198](https://github.com/vladleng/MR-Studio/issues/198) | Read-only аудит существующего 0.4b upd3 backend, plugin ID/state, пути перехода к Rig | **Начать только по запросу**, согласовать migration strategy |
| **P1** | [#199](https://github.com/vladleng/MR-Studio/issues/199) | Curated IR Models / NAM Models / My Rigs с фото, тегами и **опциональным Import My IR** (поглотил нужное из #187) | P0 accepted, реальный файловый asset |
| **P2** | [#200](https://github.com/vladleng/MR-Studio/issues/200) | Компактный MR Rig Shell: PRE FX слева / модель в центре / POST FX справа, первым реальным backend WAV | P1 accepted, UI acceptance |
| **P3** | [#201](https://github.com/vladleng/MR-Studio/issues/201) | Reusable ordered Pre/Post module slots, bypass/reorder, one SHARED DSP graph, Undo/PDC | P2 accepted; часть требований бывшего #188 |
| **P4** | [#202](https://github.com/vladleng/MR-Studio/issues/202) | Нативные педали (компактный сначала утверждённый subset) | P3 accepted |
| **P5** | [#203](https://github.com/vladleng/MR-Studio/issues/203) | NAM engine, Full Rig/Amp Only/Unknown, optional one Cab only for Amp Only, CPU safety | P0–P3 accepted; необязательно ждать все педали |
| **P6** | [#204](https://github.com/vladleng/MR-Studio/issues/204) | Сквозной RigPreset/GUI/Studio+Live QA, accepted release | Все согласованные готовые capabilities, user signoff |

**Не создавать отдельную линию #182/#186–188 в дополнение к таблице**. P1 reusable asset loader не должен становиться бесконечным универсальным IR browser. Не обязателен multi-mic mixing или самостоятельный cabinet editor.

## 4. Product/technical invariants

- Главный UX **модель звука с обложкой**, а не WAV file picker; full RigPreset = selected ToneModel/source + Pre/Post FX modules/params/output. Artwork user-owned/licensed, fallback safe.
- WAV Full Combo = статический **линейный** захват целого пути, один existing convolver без второго cabinet; WAV Cabinet Only честно так обозначен и требует реальной amp processing где нужно.
- NAM — **другой** engine; Full Rig contains cab/mic → no extra Cab; Amp Only → optional single Cabinet IR, Unknown → manual classification. До реализации NAM tab помечен Planned/Unsupported.
- Не менять вслепую `mrs.cab-ir`, native plugin ID или schema; P0 #198 проверить migration и user preset compatibility. Код/тесты/пакеты локально, GitHub docs/issues [skip ci], сторонние коммерческие WAV/NAM/обложки не публиковать.
- Realtime no alloc/free/locks/I/O/UI in callback, async prep, no double signal path, command model/Undo/SaveOpen/PDC, качественное измерение studio vs record/live.
- Начинать новый этап только после команды пользователя, отдельная приёмка каждой ступени. **Ни один MR Rig этап не реализован этим обновлением документации.**

## 5. Кодекс

Рабочий чат читает **текущий [AGENTS.md](../AGENTS.md) по принципу Issue-first**, [MR_RIG_CODEX_HANDOFF.md](MR_RIG_CODEX_HANDOFF.md) и необходимые разделы спецификаций для **назначенного** issue. Не сканировать `MR_CAB_*` как обязательный roadmap. Начинать с #198, read-only. Старый #182 и его native sub-issues — история; #198–#204 **нативно прикреплены к #197 (проверено API, 7/7)**, Markdown-ссылки остаются навигацией. GitHub-progress зависит от реальных состояний задач.
