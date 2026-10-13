# AI Preview / Browser tab — #163, 2026-10-13

Запрос пользователя: «Начинай работу над #163. Я бы окно для ассистента сделал
в браузере отдельной вкладкой». Native Browser MR Studio получил вкладку AI рядом
с VST3/Files. Это не внешний веб-браузер. AI menu → Assistant открывает эту вкладку,
показывает sidebar и ширину не меньше420logicalpx (existing divider limits).
Компонент создаётся lazy и сохраняется при переключении VST3/Files/AI; чат и запрос
не сбрасываются при переключении. Старого отдельного Assistant window больше нет.
Connection раскрывает настройки, Permissions использует прежнюю policy panel.
Tab selection/chat/proposals transient, sidebar width сохраняется existing view prefs.

## Пользовательский путь

В SUGGEST запросить marker change → увидеть локальный diff → Reject либо review.
Для Apply выбрать EDIT в Permissions **до запроса**. Смена permissions отзывает
старый grant/preview: потребуется новый запрос. Apply — явное подтверждение всего
показанного preview. После Apply доступна одна Undo на группу marker operations.
AUTO принимает только явно включённые marker operations, тоже через ручной Apply;
автономного исполнения нет. READ не предлагает/не пишет; SUGGEST никогда не пишет.

Pending отражает worker request; предложения переходят в Preview, Accepted,
Rejected, Stale или Failed. Ошибки параметров/группы/лимита/permissions видимы.
После ручной правки/Undo/Redo/graph revision preview Stale: Apply disabled.
Refresh preview локально переоценивает прежнее намерение на текущем Project,
строит новый diff и требует отдельного Apply. Сеть/оплата для Refresh не нужны.
При смене проекта/session/permissions Refresh не переносит grant: новый запрос.
Новый Send отвергает предыдущий неисполненный preview. Reject/Cancel не меняют
project/history/dirty. Cancel отменяет network job, Reject отклоняет proposal.

## Shared контракт

Application.ai_preview принимает1–8 зарегистрированных ToolPlan. Model JSON не
получает before/after/confirmed flags и не создаёт host preview. Operation IDs
уникальны внутри группы; unknown/invalid member запрещает **всю** группу, без
частичного Apply. Current ContextKey/policy epoch проверяются на preview/Apply.
Симуляция применяет существующие Core commands к локальному candidate Project,
validate без ProjectStore publish, engine/device/plugin capture или Undo.

Diff: stable target ID, property, before/after и scope. Markers — name/tick,
позиция tick; MIDI — pitch/velocity, clip/note IDs, visible trimmed project tick
range/channel. Повторные изменения одного свойства в группе показывают один
итоговый before→after. До1024 changed fields/64KiB diff, до8 retained previews.
Пустой diff не применяется. Большой/конфликтующий diff отклоняется явно.

Preview приватно удерживает before/candidate и compiled plans; caller не может
подменить модель или state. Apply принимает только точный registered Preview,
проверяет key/epoch/mode/Live/record/count-in, затем идёт через общий
Application.edit_metadata, тот же metadata command path, что UI. Группа marker
commands превращается в один confirmed candidate command с expected Project;
staged ProjectStore transaction обеспечивает одну publication/Undo и rollback
при незавершённой транзакции. Показанный candidate и исполненный candidate совпадают.

Session-local applied ledger request ID+operation ID предварительно выделяется,
публикуется после успешного commit без новых allocations. До1024 entries; duplicate
Apply/rebuild/retry не создаёт вторую mutation даже после Undo. Новый session
сбрасывает ledger и отзывает previews. Это supporting safety для #163; полный
расширенный AI transaction/history contract #164 не считается автоматически принятым.

## Границы текущего Apply

Реально исполняются add_marker/rename_marker, включая atomic marker groups.
MIDI transpose/velocity имеют Core diff, но Apply blocked до #165 MIDI contract.
Selection не имеет bound editor-selection application path: explicit unsupported
preview, provider больше не объявляет set_selection. Mixed marker+MIDI group
не применяется частично. Routing/devices/files/transport/destructive tools отсутствуют.
Core registry прежний; wire tools теперь4 suggestion tools (2markers+2MIDI).
Новые tracks/clips/automation/batch dependencies/new-entity references не добавлены.

Live+playing/recording/count-in deny Apply. Arrange marker metadata при playback
следует существующему human path; новых RT callback/engine changes нет.
Все copies/strings/transactions на control thread; network остаётся bounded worker.
Schema21/archive1, permission config8, credential/provider settings прежние.
Preview не сохраняется в project, accepted markers используют обычный Save/Open.

## UI и слои

Browser tab с responsive bounds192–620px и scrolling text; Connection collapsed по
умолчанию. Chat отдельно от before/after diff, Apply/Reject/Refresh отдельно от
Send/Cancel. Prompt/connection TextEditors блокируют DAW typing shortcuts через
existing guard. Usage компактно показывает input/output и reasoning/cached при
наличии; стоимость unknown. Theme/fonts/menu/layout family прежние.

UI/Application/command composition/history use/tests/docs: AFFECTED.
Core marker/MIDI commands: REUSED, AI DTO extended. Engine/DSP/device/audio callback:
NOT AFFECTED. Project schema/archive: NOT AFFECTED. Existing user prefs width:
REUSED; AI chat/proposal persistence NOT APPLICABLE. Studio/Live shared authority.
#164 broader history/#165 MIDI application: DEFERRED.

## Проверки и приёмка

ai_preview: batch diff vs actual IDs/values, Cancel/no Undo/dirty, one Undo/Redo,
Save/Open, stale/manual edits/rebuild, duplicate replay after Undo, conflicting
rename group, duplicate member, MIDI diff+gate, Live playback deny, revoke/session.
GUI: SUGGEST Apply disabled, EDIT Apply/Undo, Reject, revoke/error/redaction,
Browser ownership/tab preservation/shortcut guard, narrow/wide bounds,100/150.
Final build/CTest/package/source evidence: PROJECT_CONTEXT.
Software snapshots не заменяют physical mixed-DPI. User API+Apply/Undo+SaveOpen,
ASIO playback с network и native monitor transitions остаются ручной приёмкой.
#163/#164/#165/#26 не закрываются автоматически; source/build/package local,
docs/issues GitHub отдельно, без source push/Actions.
