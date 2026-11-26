// What androidx added to the player since ebe7964173e (kobento-ts tests/property_bundle_test.mjs
// writes the fixture, and checks the same values and pixels):
//   MODIFIER_PROPERTY_BUNDLE (95) + PROPERTY_BUNDLE_LOOKUP (96)   0d0f5ab0f6d, 7a96775051d, 70ac8c200f4
//   MODIFIER_CLIP_PATH (109)                                      a34763e67a6
// The lookups mirror androidx's PropertyBundleLookupOperationTest: an implicit lookup (40), a
// missing property (-99), the parent column's property by id (7), their sum (47), a component
// without a bundle (123), a missing component (456), a canvas content reading its canvas (88);
// then the property's variable changes (40 -> 30) and the lookup and the sum follow. The last box
// is red, clipped to the triangle (0,0) (100,0) (0,100).

#include "rccore/WireBuffer.h"
#include "rccore/CoreDocument.h"
#include "rccore/RemoteContext.h"
#include "rccore/operations/LayoutOperations.h"
#include "rcskia/SkiaPaintContext.h"

#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::fprintf(stderr, "FAIL: %s\n", msg); ++failures; } \
    else         { std::fprintf(stderr, "ok:   %s\n", msg); } \
} while (0)

static std::vector<uint8_t> readFile(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

static void collect(rccore::Operation* op, std::vector<rccore::PropertyBundleLookupOp*>& out) {
    if (auto* l = dynamic_cast<rccore::PropertyBundleLookupOp*>(op)) out.push_back(l);
    for (auto* c : op->children()) collect(c, out);
}

int main(int argc, char** argv) {
    std::string dir = (argc > 1) ? argv[1] : "tests/fixtures";
    auto data = readFile(dir + "/property_bundle.rc");
    if (data.empty()) { std::fprintf(stderr, "FAIL: cannot read %s/property_bundle.rc\n", dir.c_str()); return 1; }

    const int W = 300, H = 300;
    rccore::WireBuffer buffer(data.data(), data.size());
    rccore::CoreDocument doc;
    CHECK(doc.initFromBuffer(buffer), "the document parses");
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(W, H));
    rccore::RemoteContext ctx;
    rcskia::SkiaPaintContext paint(ctx, surface->getCanvas());
    ctx.setPaintContext(&paint);
    ctx.setDocument(&doc);
    ctx.mWidth = W; ctx.mHeight = H;
    doc.registerListeners(ctx);
    doc.applyDataOperations(ctx, -1);
    auto frame = [&]() { surface->getCanvas()->clear(SK_ColorTRANSPARENT); doc.paint(ctx, -1); };
    frame();

    // The lookups, in document order: implicit, missing property, explicit parent, no bundle,
    // missing component, canvas. The sum is the integer expression after the third.
    std::vector<rccore::PropertyBundleLookupOp*> lookups;
    for (auto& op : doc.getOperations()) collect(op.get(), lookups);
    CHECK(lookups.size() == 6, "six lookups");
    if (lookups.size() != 6) return 1;
    auto value = [&](int i) { return ctx.getInteger(lookups[i]->valueId); };
    const int sumId = lookups[2]->valueId + 1;   // the fixture allocates it right after that one
    CHECK(value(0) == 40, "a lookup naming no component reads the box's bundle");
    CHECK(value(1) == -99, "a missing property: the default");
    CHECK(value(2) == 7, "a lookup naming the column reads its bundle");
    CHECK(ctx.getInteger(sumId) == 47, "an integer expression reads two lookups");
    CHECK(value(3) == 123, "a component without a bundle: the default");
    CHECK(value(4) == 456, "a missing component: the default");
    CHECK(value(5) == 88, "a canvas content reads its canvas's bundle");

    // The property's variable changes: the lookup follows.
    int source = -1;
    if (auto* b = doc.propertyBundleOf(lookups[0]->componentId)) source = b->variableOf(15);
    CHECK(source != -1, "the box's bundle has property 15");
    ctx.overrideInteger(source, 30);
    frame();
    CHECK(value(0) == 30, "the lookup listens to its property's variable");
    CHECK(ctx.getInteger(sumId) == 37, "and what reads it follows");

    // The clip: the last box, at y = 150.
    SkPixmap pm;
    surface->peekPixels(&pm);
    auto red = [&](int x, int y) { SkColor c = pm.getColor(x, y); return SkColorGetA(c) == 255 && SkColorGetR(c) == 255 && SkColorGetG(c) == 0; };
    auto clear = [&](int x, int y) { return SkColorGetA(pm.getColor(x, y)) == 0; };
    CHECK(red(10, 160) && red(80, 160), "inside the triangle: the background");
    CHECK(clear(80, 230) && clear(40, 240), "below the diagonal: clipped");

    if (failures) std::fprintf(stderr, "%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
