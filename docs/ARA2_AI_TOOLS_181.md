# ARA host tools — #181

Контракт 2026-10-19. Дополнение существующих AI Foundation registry, snapshot,
receipt, permission и Preview/Apply API; project schema и audio callback не меняются.
Issue: https://github.com/vladleng/MR-Studio/issues/181, parent #88.

## Доступные операции

| Tool | Вход | Результат |
| --- | --- | --- |
| list_ara_capabilities | cursor/limit | Имена, версии, доступность и opaque plugin_id из локального ARA discovery cache |
| query_ara_binding | clip_id, cursor/limit | Audio clip IDs, привязка, bypass, доступные действия |
| get_ara_status | clip_id, cursor/limit | Sanitized runtime status и те же capability flags |
| attach_ara | clip_id, plugin_id | Одна durable привязка source со всеми его copies; host Undo |
| open_ara_editor | clip_id | Выбрать клип, открыть/поднять существующий native editor; без изменения проекта/Undo |
| set_ara_bypass | clip_id, bypass | Включение/обход source, все copies; host Undo |
| remove_ara | clip_id | Закрыть editor, удалить привязку source, оставить оригинальный WAV; host Undo сохраняет edits |
| reanalyse_ara | clip_id | UNSUPPORTED: подтверждённый vendor reanalysis contract пока отсутствует |
| Изменение ARA нот/pitch | — | UNSUPPORTED: универсального ARA note-edit API нет |

Read tools доступны также как query_project queries. GUI assistant получает ARA
capabilities/status в обычном bounded snapshot; новое provider read-loop не вводится.
Без MRS_BUILD_ARA ARA read/actions возвращают UNSUPPORTED, обычные AI tools работают.

Attach не открывает editor автоматически: это отдельное reviewed действие.
Один ARA action на preview; смешанные ARA/model batches возвращают UNSUPPORTED,
чтобы runtime/plugin transaction не могла оставить частично применённую группу.

## Authority и безопасность

- Plugin IDs выдаёт Application по доверенному результату desktop discovery;
  ID сохраняется до изменения записи catalog/session и никогда не является путём.
  AI не запускает scanner. Missing plugin требует явного discovery/recovery в UI.
- Model-controlled module paths, class IDs, pointers, scripts, PCM и opaque archives
  не принимаются. Запросы строгие и bounded; query JSON не содержит медиа/module
  paths, component/controller/archive bytes или секретов провайдера.
- Catalog projection сохраняет только sanitized ID/name/version/availability;
  неизменяемый ReadView можно читать с worker. SDK/model actions выполняет только
  Application owner thread через уже принятые ARA команды.
- Project/session/revision, receipt/snapshot authority, permission epoch, exact
  operation ID и текущая GUI selection повторно проверяются до Apply. Смена catalog
  отзывает ранее выданные read/proposal authority. Отмена request отзывает receipt
  и pending proposals; поздний model reply не получает права на Apply.
- READ читает, SUGGEST строит preview без writes. Новые ARA actions требуют EDIT,
  displayed preview и ручной Apply. AUTO allowlist/mask не расширен, ARA AUTO denied.
- Apply ARA выполняется на остановке, без recording/count-in/Live performance и loop.
  Preview показывает число затронутых source copies. Open — transient действие,
  остальные mutations используют existing Commands/Undo/archive rollback.
- Missing/unknown plugin, stale selection/project, revoked policy, cancellation,
  backend failure и unsupported note commands не обходят эти gates.

## Проверка пользователем

Использовать тестовый проект и уже найденный ARA plugin; живой provider/лицензии и
physical ASIO/DPI не считаются проверенными по mocks/fixture.

1. В READ запросить ARA status выбранного аудиоклипа.
2. В SUGGEST предложить bypass/Remove: проект остаётся прежним, Apply запрещён.
3. В EDIT предложить bypass, проверить source copies в preview, Apply и Undo/Redo.
4. Запросить открыть bound clip в ARA: окно появляется только после Apply.
5. Для unbound audio предложить attach к известному plugin_id; затем отдельно Open.
6. Между preview и Apply сменить selected clip или проект: прежний preview запрещён.
7. Запрос сдвинуть ARA-ноту возвращает unsupported, а не обещание pitch correction.

Статус реализации/проверок и отдельная пользовательская приёмка — в #181.

