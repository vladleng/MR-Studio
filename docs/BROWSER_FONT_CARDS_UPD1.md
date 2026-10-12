# Browser/menu font and Home cards — 0.3d upd1 / #148

2026-10-12. User accepted 0.3d/#86: «Билд проверку прошел».
Requested follow-up: font/size/width of Zoom+ button for all popup menus and
VST3/Files browser; Home project name/info cards filled like buttons.

Theme browserFont reuses Segoe UI14 logical + horizontal scale1.04, also used by
regular 28px TextButtons. Popup/preset menus, ComboBox captions, VST3 vendor/plugin
rows, file rows/metadata/search/path use this role. Popup rows minimum22 retained
to avoid V4 height shrink. Channel/strip buttons remain12/scale1. Detail labels
outside requested browsers unchanged. Home cards raised #2F3841, outline1/radius6;
selected accent and filename link hover/focus retained. Hit/open/filter behavior,
project/engine/Undo/schema19 are unchanged.

Release/GUI CTest6/6 PASS (44.56s); packaged/100-150 preview checks next.
Long Files metadata uses ellipsis rather than horizontal squeeze; preset menu
width is measured from the shared font instead of capped at340px.
Old packages preserved; persistent MR desktop shortcut will update on delivery.
No push/merge/Actions; other musical sub-issues not started. #148 pending acceptance.

Delivered source `36d7794ce9cf2ee2e68de5f6c9f6dfd8b87760ec`, Release/GUI6/6
PASS (44.56s), packaged J3 PASS (exit0). Fresh Home/menu/VST3/Files100/150
previews inspected: `build/asio-local/validation/browser-font-cards/packaged-smoke`.
Compact13 files / 12 hashes verified:
`Builds/MR-Studio-0.3d-upd1-Browser-Cards-JUCE-ASIO-Windows-local`.
GUI SHA256 `73D04FB764DE1E3A5C99EB0BA86BDA84174B2AE14B6FD3A430A8E175521A925B`.
Symbols `build/asio-local/deliveries/0.3d-upd1-browser-font-cards`. Desktop shortcut
updated, icon retained/target verified; accepted0.3d hash unchanged.
READY WITH MANUAL CHECK; visual acceptance pending, physical mixed-DPI not measured.

2026-10-12: пользователь «Хорошо, пока оставим так. Принимаю».
#148 closed completed, FEATURE ACCEPTED. Пакет/хеши/проверки не переписаны.
