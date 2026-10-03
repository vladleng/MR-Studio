# SHARED Stage 1 — проверка Audio Engine / ASIO

Этап #16 проверяет реальный аудиодвижок. Performance gate требует проверки
на компьютере с родным ASIO-драйвером и сравнения со Studio Pro.

## Быстрый старт

1. Скачай artifact `MR-Studio-SHARED-Stage-1-ASIO-Windows` из успешного
   запуска Actions **ASIO audio prototype** для текущей ветки.
2. Распакуй. В корне лежат exe и `Start-Audio-Test.cmd`.
3. Запусти `Start-Audio-Test.cmd` двойным щелчком: консоль останется открытой.
4. Выбери номер родного драйвера, 48000 Hz, 128 frames, 30 секунд.
   Номера входов/выходов — физические, начиная с 1.
5. Первую проверку проведи в режиме **1 — silence**.
6. При успешном запуске повтори **2 — tone**, затем **3 — WAV**.
   WAV должен иметь такой же sample rate, как устройство.
7. Для входа выбери **4 — input monitor**, подключи инструмент и используй
   соответствующий вход. Для микрофона используй наушники.

Закрой Studio Pro/другие приложения, использующие ASIO, если драйвер
не разрешает одновременное открытие. Не изменяй control panel во время теста.

После теста рядом с exe появляется `mrs-audio-report.json`. При повторном тесте
этот файл заменяется: сохрани предыдущий под другим именем или используй --report.
Пришли JSON вместе с моделью интерфейса, версией драйвера и впечатлением на слух.

## PowerShell

Открыть PowerShell в папке распакованного artifact:

```powershell
.\mrs_audio_check.exe --self-test
.\mrs_audio_check.exe --list
.\mrs_audio_check.exe --device 0 --panel
.\mrs_audio_check.exe --device 0 --rate 48000 --buffer 128 --seconds 30 --report silence-128.json
.\mrs_audio_check.exe --device 0 --rate 48000 --buffer 128 --seconds 30 --tone --report tone-128.json
.\mrs_audio_check.exe --device 0 --rate 48000 --buffer 128 --seconds 60 --wav "C:\Audio\playback.wav" --report wav-128.json
.\mrs_audio_check.exe --device 0 --rate 48000 --buffer 128 --seconds 60 --monitor-input 1 --report input-128.json
```

Вместо 0 поставь номер драйвера из --list. Выходы по умолчанию 1,2; можно задать
`--outputs 3,4`. Несколько stems: повторить --wav для каждого файла.
Нагрузочный тест: добавить --voices 8/16/32, не превышая 128 суммарных голосов.

## Что проверять

- --list показывает родной ASIO, vendor channel names и native buffer limits.
- --panel открывает панель драйвера.
- Silence/tone/WAV запускаются при поддерживаемых 48 kHz / 128 samples.
- Повторить на 64 samples, если драйвер поддерживает этот размер.
- Фактический callback_frames_min/max совпадает с запрошенным размером.
- WAV/stems играют синхронно; вход слышен без треска и нестабильной задержки.
- JSON не показывает систематические underflow/overflow/deadline misses.
- Повторить 10–30 минут с playback + monitor и обычной нагрузкой компьютера.
- Ошибки занятого/отключённого устройства явные; повторный запуск после
  восстановления устройства и нового --list работает.

PASS в hardware tester означает, что в данном пробном запуске не наблюдались
перечисленные ошибки. REVIEW требует анализа; ERROR означает отказ устройства/
теста. Offline self-test не оценивает родной драйвер.

## Сравнение со Studio Pro

На том же компьютере/драйвере/48 kHz/реальном 128 и затем 64 buffer:
те же stems, число копий дорожек, те же входы и выходы. Никаких плагинов на
этом раннем сравнении, поскольку MRS ещё не имеет host.
Сравнить стабильность на слух и показатели, которые показывает Studio Pro.

| Setup | Studio Pro | MRS report | На слух / вывод |
|---|---|---|---|
| 48k / 128, WAV | заполнить | wav-128.json | заполнить |
| 48k / 64, WAV | заполнить | wav-64.json | заполнить |
| 48k / 128, WAV + input | заполнить | сохранить JSON | заполнить |
| 48k / 64, WAV + input | заполнить | сохранить JSON | заполнить |
| 10–30 min load | заполнить | сохранить JSON | заполнить |

Reported I/O latency в отчёте — данные driver/host, а не измеренный round-trip.
Benchmark, plugin/patch/full-show stress и performance gate не считаются
пройденными по результатам CI.
