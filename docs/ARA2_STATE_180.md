# ARA project state — #180

Контракт P4, 2026-10-18. Parent #88; P3/#179 принят. Реальные результаты
проверок и приёмка публикуются в #180; этот файл описывает формат и ownership.

## Durable model / migration

Core Project schema28 добавляет `ara_bindings`, независимо от MRS_BUILD_ARA.
Readers1–27 мигрируют с пустым списком. Пустая секция в snapshot28 может быть
опущена; непустая записывается как `ARA_BINDINGS`. Envelope MRSARCH1/version1
и unknown extension chunks сохраняются. Старые приложения schema28 не читают;
новый формат появляется при Save, принятые packages не перезаписываются.

Каждая source binding хранит source media reference, постоянный persistent ID,
module path, VST3 class, name, ARA factory/version/archive ID, bypass, opaque ARA
archive, companion component/controller state и checksums. Prepared PCM, HWND,
controller pointers и Undo history не сериализуются. Clip/offset/start/length
остаются в обычном Project.clips; одна modification общая для source occurrences.

Persistent ID source/modification не зависит от пути WAV: Save As и relink того
же материала не меняют ID. Источник проверяется по checksum decoded float PCM,
каналам и sample rate. Checksum — обнаружение порчи/подмены, не security hash.
Пути клипов и binding ремапятся вместе через существующий MediaCopy.

Лимиты P4: восемь bindings; 1 MiB на каждый opaque blob; суммарно 4 MiB ARA data
в проекте; прежний snapshot limit16 MiB/archive limit64 MiB. Превышение даёт
ошибку до перезаписи проекта, без усечения state. Лимит не гарантирует поддержку
архивов любого коммерческого vendor/длины материала.

## Store / restore / unavailable

Все state API вызываются на UI/control owner. Перед archive/state операциями
transport worker остановлен/joined. `storeObjectsToArchive` вызывается вне edit
session через bounded memory callbacks; companion state захватывается отдельно.
Save и retirement checkpoint для Undo требуют завершённого анализа; до этого
операция отклоняется с предложением дождаться анализа, без удаления binding.
Сохранение capture failure не заменяет предыдущий файл проекта. Decode и PCM
checksum готовятся на joined preparation workers, не в audio callback.
Controller blob — host envelope `MRSCTL01`: raw vendor controller bytes и
нормализованные параметры (не более4096). Это сохраняет изменения контроллера,
которые ещё не дошли до DSP до Update playback. Restore сначала применяет
companion state, затем параметры контроллера; следующий bake передаёт их DSP.
Обычный Application snapshot читает последний checkpoint без vendor calls;
актуальный state захватывается при Save и retirement binding.

При открытии project metadata читаются без обязательного наличия SDK/vendor.
Для playback/editor создаются document/companion, проверяются class/factory,
archive ID и точная версия; затем companion state, source/model/region graph,
`restoreObjectsFromArchive` внутри editing session, затем PCM preparation.
Model persistent IDs связывают архив с восстановленными объектами.

Missing module, incompatible version/archive, checksum mismatch и decode failure
оставляют source/binding/opaque bytes. При доступном WAV звучит dry; отсутствующий
ARA-bound WAV не создаёт voice, проект/остальные дорожки остаются доступными.
Повторное Save сохраняет opaque bytes. Retry повторно открывает saved plugin;
Relink выбирает совместимый plugin path/class; Locate source WAV меняет media
reference с проверкой материала. Неверный WAV отклоняется с rollback модели.
Данные неизвестного plugin не интерпретируются и не удаляются автоматически.

## Commands / Undo / private editing

Attach/bypass/detach/plugin relink проходят через SetAraBindings и транзакции
ProjectStore; media relink — через отдельную host command. Rollback возвращает
модель/runtime при отказе. Обычные split/trim/move/duplicate/delete/Undo используют
прежние clip commands и синхронизацию ARA regions/dormant source.

Перед retirement binding захватывается opaque checkpoint; Session/PCM освобождаются,
а Undo восстанавливает editor state в новом Session по persistent ID. Retired
checkpoints ограничены64 MiB; переполнение отклоняет дальнейшее изменение binding,
предлагая Save/Reopen, без тихого выбрасывания данных. Project replace уничтожает
runtime/checkpoints после остановки workers и удаления native view.

Отдельные pitch/note gestures плагина не добавляются в host Undo: их история
остаётся у плагина. Уведомления plugin edits помечают проект несохранённым;
Save захватывает actual opaque state. Сохранение/восстановление не обещает
воссоздать внутреннюю Undo-history vendor.

## Boundaries

Общий AudioEngine по-прежнему получает один immutable prepared PCM на clip;
archive/model/vendor work в его callback не добавляется. Прежний stopped
Update playback остаётся; double-click, drag/drop, delayed progress и
resize/Attach/Full screen — отдельный future UX backlog #88. P5/#181 не начат.
Physical ASIO, commercial state compatibility и physical DPI требуют отдельной
фактической проверки; synthetic fixture не доказывает их поддержку.

