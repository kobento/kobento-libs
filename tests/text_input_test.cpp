// LAYOUT_TEXT_INPUT (TextInputLayout, androidx 7f6f9150bc8) reads as a box, and an empty string
// shows the layout's hint. This engine has no editor, so nothing else changes.
//
// fixtures/text_input.rc (from text_input.json) stacks four 60-tall fields on black:
//   0 editableText "@name", empty, hint "Your name"   -> the hint, in the text's style at 45%
//   1 textInput "@email" { row { icon, text } }, empty -> the hint, after the icon
//   2 a text "@city" = "Paris" (json2rc's modifier form, rewritten as a layout) -> the text
//   3 textInput "@code" { box } on white, empty       -> the hint at the content origin, grey
//
//   text_input_test <fixtures dir>
#include "rccore/WireBuffer.h"
#include "rccore/CoreDocument.h"
#include "rccore/RemoteContext.h"
#include "rcskia/SkiaPaintContext.h"

#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"

#include <algorithm>
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
    std::string path = dir + "/text_input.rc";
    std::ifstream f(path, std::ios::binary);
    std::vector<uint8_t> data{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
    if (data.empty()) { std::fprintf(stderr, "FAIL: cannot read %s\n", path.c_str()); return 1; }

    rccore::WireBuffer buffer(data.data(), data.size());
    rccore::CoreDocument doc;
    if (!doc.initFromBuffer(buffer)) { std::fprintf(stderr, "FAIL: fixture does not parse\n"); return 1; }

    const int W = doc.getWidth(), H = doc.getHeight();
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(W, H));
    SkCanvas* canvas = surface->getCanvas();
    rccore::RemoteContext ctx;
    rcskia::SkiaPaintContext paint(ctx, canvas);
    ctx.setPaintContext(&paint);
    ctx.setDocument(&doc);
    ctx.mWidth = static_cast<float>(W);
    ctx.mHeight = static_cast<float>(H);
    doc.registerListeners(ctx);
    canvas->clear(SK_ColorBLACK);
    doc.applyDataOperations(ctx, -2);
    doc.paint(ctx, -2);

    std::vector<uint32_t> px(static_cast<size_t>(W) * H, 0);
    surface->readPixels(SkImageInfo::MakeN32Premul(W, H), px.data(), W * 4, 0, 0);
    // Pixels of field i whose channel sum is in (lo, hi), from x0 on; and the brightest.
    auto count = [&](int i, int lo, int hi, int x0, int* brightest) {
        int n = 0, mx = 0;
        for (int y = i * 60; y < i * 60 + 60 && y < H; ++y) {
            for (int x = x0; x < W; ++x) {
                uint32_t c = px[static_cast<size_t>(y) * W + x];
                int sum = (c & 0xff) + ((c >> 8) & 0xff) + ((c >> 16) & 0xff);
                if (sum > lo && sum < hi) { ++n; mx = std::max(mx, sum); }
            }
        }
        if (brightest) *brightest = mx;
        return n;
    };
    int max0 = 0, max1 = 0, max2 = 0;
    CHECK(count(0, 60, 800, 0, &max0) > 30, "field 0: an empty editable text draws its hint");
    CHECK(max0 < 500, "field 0: the hint is dimmed (45% of white)");
    CHECK(count(1, 60, 800, 40, &max1) > 30 && max1 < 500, "field 1: the hint of a text nested in a row");
    CHECK(count(2, 60, 800, 0, &max2) > 30 && max2 > 600, "field 2: a set string draws in full colour");
    CHECK(count(3, 100, 600, 0, nullptr) > 30, "field 3: no bound text, the hint at the content origin");

    std::fprintf(stderr, failures ? "\n%d FAILURE(S)\n" : "\nALL PASSED\n", failures);
    return failures ? 1 : 0;
}
