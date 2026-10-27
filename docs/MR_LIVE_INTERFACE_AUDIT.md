# MR Live — сверка интерфейса и задач

Дата: 2026-10-27. Проверен checkout `codex/mr-live-native-entry`, source `f434bd32a752d10893fc1cbabf4a847cbb492194`. Аудит требований; код приложения и сборки не изменяются.

## Источники и приёмка

Приоритет имеют текущие уточнения пользователя, затем принятый layout [v5](../assets/ui/mr-uds/layouts/mr-live-layout-v5.json), [описание поведения](MR_LIVE_LAYOUT_EDITOR.md) и [native handoff](MR_LIVE_NATIVE_INTEGRATION.md). Поведение прототипа проверено по `tools/design-lab/live/`: channels-ui.mjs, runtime-panels.mjs, registry.mjs, visuals.mjs. JSON раскладки не содержит весь show и не заменяет контракт поведения.

Native evidence: `apps/studio-juce/src/LiveLayout.h`, `LiveWorkspace.h`, `LiveTimeline.h`, `LiveChannels.h`, `LiveSetlist.h`, `LiveItemEditor.h`, `LiveShowState.h`. Наличие отрисовки/полей модели не считается завершением интерактивного UI или DSP.

#272 и #274 закрыты после приёмки. Приняты runtime этапы до build 330 включительно, в том числе прямой запуск build 327 и multistem build 329. Build 332 (0.8.5) подготовлен к приёмке; принятие аудита не означает принятие этого билда. Его ручной preload/handoff не доказывает автоматические переходы без разрыва и аппаратную ASIO надёжность.

## Архитектура

MR Live.exe запускается непосредственно в пустое/последнее сохранённое show. Home Studio остаётся дополнительным входом. Документ — .mrlive; прямого открытия .mrsproject через item и переходов в Studio нет. Engine/ASIO/Transport/Project Model/GraphStore/MIDI/plugin host используют общий код Core, без второго backend. Устаревшие формулировки «встроенный режим», «открытый Studio project» исправлены в roadmap и профильных issues.

## Уровни громкости — уточнение пользователя

В **Playback / stems** находятся:
- Индивидуальный уровень каждого stem выбранного item.
- Общий **Playback level этого item**, совместно регулирующий его музыкальные stems с сохранением их относительного баланса.

В **микшере** у каждого канала собственный общий фейдер на всё show. Он не меняется при выборе item/patch. Это требование канальных фейдеров, а не замены их одним Master.

Сохраняются три независимых значения: stem gain (item + stem), Playback group gain (item), channel gain (channel на всё show). Для музыкального stem эффективный уровень в dB — их сумма; результат не записывается обратно в исходные уровни. Item Playback multiplier не относится к Instrument/Player/Audio. Cue/Click сохраняют независимые уровни и маршруты.

Существующий show-wide `performance.nativeLiveAudio.gain` и per-file gain не заменяют требуемый item Playback group gain. При реализации #292 требуется явное сопоставление/миграция существующих значений без потери. Stem/item Playback levels входят в Browser saved content / Save for All; общий channel mix сохраняется в show, но не делает содержимое item dirty.

## Матрица покрытия

Номера ниже — GitHub issues репозитория vladleng/MR-Studio. «Частично» означает конкретный оставшийся scope, не повторную разработку принятого интерфейса.

| Требование | Фактический native статус / остаток | Ответственность |
|---|---|---|
| Standalone вход, .mrlive, Home browser | Принято; прямой запуск без промежуточного окна | #274 закрыта; #29 |
| Layout v5, resize, графитовый стиль | Bundled bounds loader и масштабирование есть; полный пользовательский layout pipeline не завершён | #28 |
| Setlist / item Browser / organizer | Основной workflow принят; оставшиеся duplicate/load saved/folder/context действия проверить как доступные UI actions | #286 → #28 |
| Общие каналы, +Track, CRUD | Автосоздание Playback channel при WAV attachment не заменяет общий редактор каналов | #279 → #28 |
| Mixer gain/pan/M/S и bindings | Полного native Mixer нет; текст M/S/R на прочих каналах не равен контролам | #279 → #28; #29 |
| Item Playback group + stem gain | Уровни в Playback / stems; постоянные канальные фейдеры в Mixer; контракт выше | #292 → #31 |
| Playback/stems spoiler | Отдельное требование: текущий browser Playback list плоский; native selector не spoiler | #280 → #28 |
| Сворачивание контейнера item | Другое действие: в прототипе collapsedItems по occurrence/entry ID | #280 → #28 |
| FX / Files / MR Library Browser | Отдельный assets browser; существующий item Browser его не заменяет | #281 → #28 |
| Header/toolbar/sidebar | Mixer/Browser actions, transport navigation/readout, скрытие Playback/Status с возвратом места Setlist | #286 → #28 |
| Контекстные меню | Сохранить graphite/manager assets/90% opacity, открытие справа снизу указателя с clamp; закончить пропущенные actions | #286 → #28 |
| Sections / Chord Track | Импорт/отрисовка и seek/loop есть; native create/edit/move/resize/copy/context ещё нужны | #282 → #28; #29 |
| Origin / lead / scale / item length | Данные читаются; полноценные жесты/контролы прототипа не перенесены | #282 → #28 |
| Playback, WAV, stems, trim, routes | Реальный общий Core PCM runtime принят до 330; не объявлять весь родитель завершённым | #31 |
| Waveform неактивных items | Реальные waveform приняты; не равны резидентности всего аудио в RAM | #31 |
| Pointer/Scissors, multi-clip editing | Source trim window не заменяет performance.clips / move/split/delete/restore | #283 → #31 |
| Click и Cue WAV | Приняты; сохранять независимый routing и gain | #31 |
| Текстовые markers/cues/notes | Item notes есть; marker/text cue UI и subscriptions ещё нужны; Cue WAV их не заменяет | #29 |
| Item/section normal/skip/stop/loop | Частичный section loop не завершает end policies и linked stopRecord | #287 → #30 |
| Next-item preparation | Build 332 готов, приёмка ожидается; автоматический boundary transition отдельно | #30/#31; #287 |
| Preflight/relink/recovery | Open/save/last saved startup не равны autosave/recovery позиции; чинить inactive item без потери active playback | #288 → #30 |
| Patch/sub-patch | Spans отображаются; CRUD/free geometry/parameter overrides/DSP bindings не завершены | #289 → #32 |
| Instrument/input/plugin graph | Общий host implementation; input selection/warm switch/latency/live-safe policy ещё нужны | #32 |
| Record и linked record tracks | Record в standalone пока disabled; metadata не capture; Instrument/Player child, Audio standalone, Playback без child | #284 → #29 |
| Takes / comp | Browser demo metadata не реальная native запись/редактирование/проигрывание comp | #285 → #284 |
| MIDI mappings/actions/hardware | Общие #24/#65 не закрывают Live-specific actions/reconnect/learn и внешний MIDI | #33 |
| Tuner/meters | Статическая заглушка tuner разрешена; реальные показания не симулировать | #28/#29 |
| Performance Mode | Отдельный последующий экран; hidden blocks не удалённые требования основного режима | #290 → #14 |
| Studio → Live Playback export | Будущая отдельная функция, не прямое открытие Studio project | #291 → #14 |
| Remote и концертная надёжность | Отдельный scope; аппаратные длительные проверки в конце выбранного этапа | #34/#35 |

Mixer в v5 скрыт до открытия, а не исключён из интерфейса. Дополнительные lyrics/moving-chord blocks из registry не обязаны появиться в утверждённой основной раскладке. Looper/DMX/video/ARA из #138 остаются идеями, не молчаливо добавленным scope native baseline.

## Проверенная GitHub иерархия

До аудита #14 имел реальные children #272, #28, #274, #29–#35; у #28–#35 не было native children. Markdown scope списки не считались sub-issues.

Созданы и проверены через GitHub parent/sub_issues API реальные связи:
- #28 → #279, #280, #281, #282, #286.
- #29 → #284 → #285.
- #30 → #287, #288.
- #31 → #283, #292.
- #32 → #289.
- #14 → #290, #291.

Обновлены формулировки/карта scope #14, #28–#33, #35 и #138. Принятые #272/#274 и общие Core #21/#22/#23 не переоткрывались. Закрытый backend mixer #22 не означает готовый native Live Mixer #279.

## Следующий рабочий этап

Первый UI/bindings пакет: #279 + #292 + #280 — общие каналы и Mixer, item Playback group/stem controls в Playback / stems, два разных fold. Затем оставшиеся UI/editing/browser/recording/patch задачи по зависимостям, без нового макетирования. Автоматические переходы — #287 после ручного handoff.

Аудит не запускает реализацию, новый build, полный CTest или отдельную проверку Studio.exe. При разработке — адресные проверки поведения, широкий регрессионный/аппаратный прогон в конце соответствующего родителя.
