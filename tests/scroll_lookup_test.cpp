// A text that follows a scroll position through a lookup, in the global section.
// tests/fixtures/scroll_lookup.json: a 4-notch horizontal scroll writes @pos; a textLookup in a
// canvas's global section (written before the root) picks names[round(@pos / 300)] into
// "@name"; a bold text (component id 501, wrap width) shows it.
//
// Two bugs made the text keep "One" at its old width after a swipe to the third page:
//   - CoreDocument::applyOperations re-applied the position's FloatConstant every pass, so the
//     top-level lookup, evaluated before the scroll paints, always read the initial 0 (C54);
//   - RemoteContext::loadText did not invalidate the layout, so a text whose string changed
//     kept the width measured for the old one (C55).
//
//   scroll_lookup_test <fixtures>

#include "rccore/WireBuffer.h"
#include "rccore/CoreDocument.h"
#include "rccore/RemoteContext.h"
#include "rcskia/SkiaPaintContext.h"

#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::fprintf(stderr, "FAIL: %s\n", msg); ++failures; } \
    else         { std::fprintf(stderr, "ok:   %s\n", msg); } \
} while (0)

int main(int argc, char** argv) {
    std::string dir = (argc > 1) ? argv[1] : "tests/fixtures";
    std::string path = dir + "/scroll_lookup.rc";
    std::ifstream f(path, std::ios::binary);
    std::vector<uint8_t> data{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
    if (data.empty()) { std::fprintf(stderr, "FAIL: cannot read %s\n", path.c_str()); return 1; }

    rccore::WireBuffer buffer(data.data(), data.size());
    rccore::CoreDocument doc;
    if (!doc.initFromBuffer(buffer)) { std::fprintf(stderr, "FAIL: fixture does not parse\n"); return 1; }

    const int W = 300, H = 300;
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(W, H));
    SkCanvas* canvas = surface->getCanvas();
    rccore::RemoteContext ctx;
    rcskia::SkiaPaintContext paint(ctx, canvas);
    ctx.setPaintContext(&paint);
    ctx.setDocument(&doc);
    ctx.mWidth = static_cast<float>(W);
    ctx.mHeight = static_cast<float>(H);
    doc.registerListeners(ctx);
    float now = 0;
    auto frame = [&] {
        now += 0.016f;
        // Override, not load: the document writes the wall clock into this id every frame.
        ctx.overrideFloat(rccore::RemoteContext::ID_ANIMATION_TIME, now);
        canvas->clear(SK_ColorBLACK);
        doc.applyDataOperations(ctx, -2);
        doc.paint(ctx, -2);
    };
    for (int i = 0; i < 10; i++) frame();
    const auto* dim = ctx.getComponentDimension(501);
    if (!dim) { std::fprintf(stderr, "FAIL: no component 501\n"); return 1; }
    const float w0 = dim->w;

    // Swipe left about two pages, slowly, and let it settle on the third.
    doc.touchDown(ctx, 290, 150); frame();
    for (int s = 1; s <= 30; s++) { doc.touchDrag(ctx, 290 - 17.0f * s, 150); frame(); }
    doc.touchUp(ctx, 290 - 17.0f * 30, 150, 0, 0);
    for (int i = 0; i < 400; i++) frame();

    dim = ctx.getComponentDimension(501);
    const float w1 = dim ? dim->w : 0;
    std::fprintf(stderr, "text width before %.1f, after %.1f\n", w0, w1);
    CHECK(w1 > w0 * 3, "the text shows the third, much longer name at its own width");
    return failures ? 1 : 0;
}
