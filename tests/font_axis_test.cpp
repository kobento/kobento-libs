// Font variation axes on a text (CoreText fontAxis / fontAxisValues, here from a text style:
// PaintBundle FONT_AXIS, Paint.setFontVariationSettings on Android). tests/fixtures/font_axis.json
// (json2rc -> font_axis.rc): "Variable" three times in Skia, macOS's variable font, at width
// (wdth) 0.62, at its default, and at 1.3. Measured and painted with the axes, the three texts
// get narrower and wider boxes. Skipped where the Skia font is not installed.

#include "rccore/WireBuffer.h"
#include "rccore/CoreDocument.h"
#include "rccore/RemoteContext.h"
#include "rccore/operations/LayoutOperations.h"
#include "rcskia/SkiaPaintContext.h"

#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkTypeface.h"
#include "include/ports/SkFontMgr_mac_ct.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::fprintf(stderr, "FAIL: %s\n", msg); ++failures; } \
    else         { std::fprintf(stderr, "ok:   %s\n", msg); } \
} while (0)

static std::vector<uint8_t> readFile(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

static void collect(rccore::Operation* op, std::vector<rccore::CoreTextOp*>& out) {
    if (auto* t = dynamic_cast<rccore::CoreTextOp*>(op)) out.push_back(t);
    for (auto* c : op->children()) collect(c, out);
}

int main(int argc, char** argv) {
    std::string dir = (argc > 1) ? argv[1] : "tests/fixtures";
    {
        auto mgr = SkFontMgr_New_CoreText(nullptr);
        sk_sp<SkTypeface> skia(mgr ? mgr->matchFamilyStyle("Skia", SkFontStyle()) : nullptr);
        if (!skia || skia->getVariationDesignParameters({}) <= 0) {
            std::fprintf(stderr, "skipped: no variable Skia font here\n");
            return 0;
        }
    }
    auto data = readFile(dir + "/font_axis.rc");
    if (data.empty()) { std::fprintf(stderr, "FAIL: cannot read %s/font_axis.rc\n", dir.c_str()); return 1; }

    rccore::WireBuffer buffer(data.data(), data.size());
    rccore::CoreDocument doc;
    CHECK(doc.initFromBuffer(buffer), "the document parses");
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(400, 200));
    rccore::RemoteContext ctx;
    rcskia::SkiaPaintContext paint(ctx, surface->getCanvas());
    ctx.setPaintContext(&paint);
    ctx.setDocument(&doc);
    ctx.mWidth = 400; ctx.mHeight = 200;
    doc.registerListeners(ctx);
    doc.applyDataOperations(ctx, -2);
    doc.paint(ctx, -2);

    std::vector<rccore::CoreTextOp*> texts;
    for (auto& op : doc.getOperations()) collect(op.get(), texts);
    CHECK(texts.size() == 3, "three texts");
    if (texts.size() != 3) return 1;
    CHECK(texts[0]->fontAxis.size() == 1 && texts[2]->fontAxis.size() == 1 && texts[1]->fontAxis.empty(),
          "the axes come from the styles");
    CHECK(ctx.getText(texts[0]->fontAxis[0]) == "wdth", "the axis tag is a text");

    float w[3];
    for (int i = 0; i < 3; i++) {
        auto* dim = ctx.getComponentDimension(texts[i]->componentId);
        w[i] = dim ? dim->w : -1;
    }
    std::fprintf(stderr, "widths: narrow %.1f, default %.1f, wide %.1f\n", w[0], w[1], w[2]);
    CHECK(w[0] > 0 && w[0] < w[1] * 0.9f, "wdth 0.62 is narrower than the default");
    CHECK(w[2] > w[1] * 1.1f, "wdth 1.3 is wider than the default");

    // Painted with them too: the ink of each text spans as its box does.
    SkPixmap pm;
    surface->peekPixels(&pm);
    float ink[3];
    for (int i = 0; i < 3; i++) {
        auto* dim = ctx.getComponentDimension(texts[i]->componentId);
        int minX = 400, maxX = -1;
        for (int y = int(dim->y); y < int(dim->y + dim->h) && y < 200; y++)
            for (int x = 0; x < 400; x++)
                if (SkColorGetA(pm.getColor(x, y)) > 128) { minX = std::min(minX, x); maxX = std::max(maxX, x); }
        ink[i] = maxX >= minX ? float(maxX - minX) : -1;
    }
    std::fprintf(stderr, "ink: narrow %.0f, default %.0f, wide %.0f\n", ink[0], ink[1], ink[2]);
    CHECK(ink[0] > 0 && ink[0] < ink[1] * 0.9f && ink[2] > ink[1] * 1.1f, "painted narrower and wider");

    if (failures) std::fprintf(stderr, "%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
