# 0.2g / Stage 4g — внешний MIDI (implementation contract)

2026-10-08: реализация разрешена пользователем после принятой 0.2h upd1.
Версия 0.2g выходит после 0.2h намеренно. Parent Stage 4 не закрывается здесь.

Instrument track получает независимый MIDI output port и channel (Original либо
1–16). Playback clips и monitored live input идут на выход; при наличии VST3
он также продолжает получать события. Без VST3 дорожка работает как внешний
инструмент. Monitor off подавляет thru, но не запись входа или playback клипов.
UI: MIDI input/status menu в Arrange, отдельное MIDI output в нём; Panic и
Reconnect. Поля сохраняются и Undo/Redo; проектная схема 14 читает 1–13 с Off.
Runtime handles, status, queues и inventory не сохраняются.

RT boundary: device-owned MIDI scheduler → fixed SPSC event queue → отдельный
NON-RT output worker → WinMM midiOutShortMsg. Никаких MIDI driver calls, locks,
allocation или ожидания worker в callback. Generation/epoch сбрасывают pending
events при Stop/Pause/Seek/Panic/replace. Queue overflow должен fail closed с
Panic, без бесконечных retries. Worker единственный владелец выходных handles;
закрытие/reconnect отправляют reset. Live/input bridge остаётся отдельным владельцем.
External tracks остаются device-owned, не отправляют speculative ahead events.

Notes, CC/sustain, bend, program, channel/poly pressure поддерживаются; SysEx,
MIDI Clock/MTC, external audio-latency calibration и multi-hop feedback detection
не входят в 4g. Прямой input/output loopback с одинаковой идентичностью порта
блокируется; пользователь должен отключить MIDI echo на внешнем устройстве,
если оно возвращает MIDI через другой порт. Не утверждать детекцию всех кабельных
циклов. Missing/unavailable port отображается, автоматически перепроверяется.

Timing: sample offsets остаются в engine; worker использует monotonic due time
от начала callback. WinMM/Windows/USB/DIN добавляют jitter, worker poll и driver
delay; это не sample-accurate hardware output и не измеренная ASIO sync/PDC.
Физическая проверка на внешнем устройстве обязательна для приёмки routing/timing.

Очередь хранит 8191 packet, pending worker — 8192; переполнение защёлкивает fault
до stopped Reconnect/reprepare. Route generation исключает доставку старого
track index в новый маршрут. Epoch предотвращает отправку pending событий после
Panic/Seek/Stop; уже выполняющийся driver call не прерывается, затем worker делает
Panic. Due time упорядочивается стабильно; Note Off не обгоняется Note On с тем
же timestamp. Worker poll запрашивает 1 ms, но Windows может дать больше; это
не гарантированная верхняя граница jitter. Driver calls никогда не идут на RT.

Отдельный device-owned external scheduler применяет channel remap до merge
перекрывающихся нот, не меняя каналы VST3. Mute/Solo gate подавляет внешнее MIDI
и прекращает held output, unmute chases playback notes. CC state следует общей
seek/loop policy. Worker учитывает повторные live/playback Note On одного pitch
на одном port/channel; Note Off отправляется после последнего release.
Все внешние каналы получают Panic при изменении gate/route/inventory; это
консервативная reset policy, не бесшовное переключение. На hot reconnect
воспроизводятся будущие события; Seek/Play восстанавливает уже звучащие ноты.
Возврат аудио внешнего инструмента подключается обычной audio track/input route;
его latency/PDC автоматически не калибруется.

Affected: shared model/commands/Undo/serialization, Application, MIDI scheduler,
Windows bridge, JUCE control/status, tests/docs. Project media NOT AFFECTED;
shared core для всех workspace, отдельный Live UI DEFERRED. Audio DSP/PDC и
raw audio recording не меняются. Compatibility: older builds reject schema 14
явно; новые читают старые проекты. Tests: model/old versions/Save/Open/Undo,
fake backend playback/thru/channel/transport/overflow/reconnect/shutdown,
RT allocations, full CTest, JUCE snapshots; hardware отдельно NOT RUN.
