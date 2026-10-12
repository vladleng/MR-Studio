# Быстрый Bypass и CPU-индикатор — 0.3c upd5

2026-10-11, #134. В работе по прямому запросу пользователя; Stage5 не начат.

Причина задержки: Application считал VST3 bypass структурной правкой; GUI
отсоединял редакторы, graph исключал bypassed latency из PDC. Это вызывало
state capture/restore, пересоздание процессоров и повторное открытие редакторов.

Новый bypass передаётся через существующую bounded parameter queue на границу
аудиоблока, включая playback/Undo/Redo. Экземпляр и editor сохраняются.
PDC всегда учитывает собственную latency insert; заранее подготовленный dry
CompensationDelay непрерывно получает вход и заменяет выход при bypass.
Задержка пути не скачет. При panic/anticipation reset delay очищается поколением.
Бюджет памяти остаётся64MiB на graph, теперь включает dry delay каждого node.
Новый DSP path без allocation/free/locks/I/O. Structural edits и изменение
latency пресетом остаются отдельными операциями. Fade при переключении не
добавлялся: это быстрый host bypass; слуховые переходы требуют проверки.

CPU: message-thread exponential smoothing с tau0,7s, общая сглаженная величина
для числа и полоски; число с одним десятичным знаком. Raw callback/profiling
данные не менялись. Полоска4px вместо10px, accent #006dcc, overload остаётся
виден числом >100% и исходными диагностическими счётчиками/tooltip.
Сглаживание — временное UI state, не сохраняется в проект.

Source `9d1a8ee5999b3b3180d3551ca0f387225ff655da`, текущая локальная ветка.
Release build PASS, CTest126/126 PASS(131.93s), numerical PDC bypass impulse и allocations,
VST3 editor identity/generation и toggle во время playback, CPU smoothing smoke PASS.
Packaged J2 smoke с synthetic VST3 fixture exit0. Первый test failure ожидал
zero-latency dry у bypassed VST3; обновлён на7-frame PDC, final suite зелёный.
Пакет13files/12hashes: Builds/MR-Studio-0.3c-upd5-Bypass-CPU-JUCE-ASIO-Windows-local.
GUI SHA256 `A1CDD612B157225F4532DA736B1C665AA3D30ADD24A3B469349AEA974BC2835E`.
Symbols/logs/previews отдельно build/asio-local deliveries/validation.
READY WITH MANUAL CHECK / RT SAFE WITH MANUAL CHECK. #134 открыт до приёмки.
Actual ASIO/слуховая и физическая визуальная приёмка остаются пользователю.

## Приёмка 2026-10-11

Пользователь подтвердил «Все работает, отлично. Закрывай»; #134 закрыт completed.
Принят существующий пакет 0.3c upd5; manifest и binaries не менялись. Приёмка
не добавляет сведений о длительности stress или физической mixed-DPI matrix.
