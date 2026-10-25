#pragma once
#include "Controls.h"

namespace ui {
// Scoped palette from the accepted online editor (editor.css). Reuse Theme and
// its semantic roles; do not recolour Studio or create another LookAndFeel.
inline DesignTokens liveReferenceTokens() {
    auto tokens = DesignTokens::target();
    tokens.colours = {0xff191a1c, 0xff202123, 0xff242629, 0xff36383c, 0xff292b2e, 0xffdae5f4,
                      0xffa9bfdc, 0xff4b5f7b, 0xff91bbee, 0xff7ab69e, 0xffe68f88, 0xffe2c070};
    return tokens;
}
} // namespace ui
