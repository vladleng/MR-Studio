# Интерфейс DAW — #212

Контракт локальной реализации [#212](https://github.com/vladleng/MR-Studio/issues/212),
2026-10-17. Пользовательская приёмка ожидается; #213/Live не входят в этот этап.

## Actions и расположение

Верхняя панель: Pointer (1), разрезание (3), Undo, Redo, Duplicate (D), Zoom In,
Zoom Out, Fit и Snap. Иконки 14 logical px, без постоянных рамок; прежние callbacks,
hotkeys, disabled/focus и toggle feedback сохраняются. Отдельная кнопка Split не
возвращается после принятого #211; команда S/меню остаётся.

Click и Count Off перенесены к транспортным кнопкам. Метроном переиспользует
assets/ui/icons/transport/metronome.svg, Count Off — круг диаметром 8 px. Области
нажатия 32 px; смена вида не меняет настройки click/count-in и recording guards.
Transport selectors/числовые значения не заменяются glyphs.

Browser: FX, Files, AI имеют 14 px иконки и 32×28 px области нажатия. Группа
начинается в x=8, последующие x=42 и x=76 (зазоры 2 px), независимо от ширины
Browser. Поиск, папки, AI и состояние вкладок сохраняются.

SVG переиспользуются из MR UDS и transport; недостающие action/Count/AI SVG
оригинальные, зарегистрированы в manifest. Generator не запускается.

## Zoom и сетка

- Высота track pitch: **40–360 px**, прежний минимум **128 px**. Ниже 84 px
  заголовок сохраняет имя и M/S/R/monitor, скрывает gain/pan/input/meters.
- Горизонтальный zoom: **0,1–2400 px/с**, прежний минимум **2 px/с**.
  При timeline width 674 px это 6740 с вместо 337 с видимого времени.
- Ctrl-wheel — вертикальный zoom с прежним anchor центра; Ctrl+Shift-wheel —
  горизонтальный относительно мыши. Кнопки горизонтального zoom сохраняют
  музыкальное время в центре viewport, пока граница начала проекта не ограничит
  scroll. Fit показывает проект с новым диапазоном. Scroll неотрицательный.
- UI preferences version 1 принимают новые пределы; формат проекта не меняется.
- Snap остаётся на прежней smart ladder с минимальным расстоянием 10 px.
  Только paint использует spacing ×2,4: ориентир **24 logical px** между линиями.
  При уменьшении раньше исчезают 1/16, 1/8, четверти, затем промежуточные такты;
  при zoom обратно восстанавливаются. Tempo/meter/позиции/Snap не переписываются.
- Track headers и clip rows: **2 logical px** между соседями, как у Mixer
  (86 px strip при pitch 88). Track pitch и trackAt остаются согласованы.

## Popup menus

Ограниченное исключение из общего запрета decorative glass: только поверхности
MR-owned PopupMenu/context/select menus DAW; не панели DAW, системные dialogs
и не собственный GUI сторонних VST3.

Прозрачность поверхности — **30% (alpha 0,70)**, текст/иконки непрозрачные.
Corner radius **4 logical px**. Рабочая трактовка «15% blur» — Gaussian radius
**3 logical px** (15% от условной шкалы 0–20 px), kernel 7×7. Это конкретная
настройка preview для пользовательской визуальной приёмки, не стандарт JUCE.

При открытии меню UI thread один раз снимает область собственного MR Desktop
под ним и подготавливает blur. Кэш принадлежит Theme, закрытые записи очищаются при следующем
открытии меню или уничтожении Theme; нет repaint-loop capture, screen capture других приложений или
RT/аудио работы. Popup text/highlights/arrows/keyboard navigation рисует JUCE.
За границами MR окна и без доступного backdrop — alpha 0,70 без blur. Это
статичный snapshot при открытии, не live DWM/acrylic blur поверх других программ.

## Проверка и границы

Адресная Release-сборка GUI, новый interface212 smoke, затронутые shell/assets
и J3 gesture regression; software 100/125/150/200%, min/nominal окна, zoom bounds,
anchor/selection/model invariance, preference roundtrip, компактные controls,
toolbar callbacks/count, menu normal/contrast/edge/keyboard previews.

Фактические PASS/FAIL и source/EXE записываются в #212. Physical DPI, ASIO,
слух и производительность hardware callback этим GUI smoke не подтверждаются.
Исходные пользовательские проекты/настройки не используются как test fixtures.

