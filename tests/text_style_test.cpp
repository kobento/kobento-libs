// A text that names a TextStyle (P_PARENT_ID on CORE_TEXT) takes from it every value it leaves
// at its default, and a style takes what it leaves unset from its parent style, as Java's
// CoreText.applyStyle / TextStyle.apply. The player read TEXT_STYLE but never applied it, and
// skipped the text's style id, so a styled text drew at the default size and colour.
//
// fixtures/text_style.rc is written by kobento-ts tests/text_style_test.mjs (WRITE_FIXTURE=...)
// from json2rc's resources.textStyles: "title" (red, size 30, strikethrough) and "sub"
// (underline, parent "title"), then four texts: "Parent" (title), "Child" (sub), "Own colour"
// (title, but blue and size 12), "No style".
//
// Pure CPU (no GPU/GLFW). Returns 0 on success, 1 on any failed assertion.

#include "rccore/WireBuffer.h"
#include "rccore/CoreDocument.h"
#include "rccore/RemoteContext.h"
#include "rccore/operations/LayoutOperations.h"
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
    auto data = readFile(dir + "/text_style.rc");
    if (data.empty()) { std::fprintf(stderr, "FAIL: cannot read %s/text_style.rc\n", dir.c_str()); return 1; }

    rccore::WireBuffer buffer(data.data(), data.size());
    rccore::CoreDocument doc;
    CHECK(doc.initFromBuffer(buffer), "the document parses");
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(300, 200));
    rccore::RemoteContext ctx;
    rcskia::SkiaPaintContext paint(ctx, surface->getCanvas());
    ctx.setPaintContext(&paint);
    ctx.setDocument(&doc);
    ctx.mWidth = 300; ctx.mHeight = 200;
    doc.registerListeners(ctx);
    doc.applyDataOperations(ctx, -2);
    doc.paint(ctx, -2);

    std::vector<rccore::CoreTextOp*> texts;
    for (auto& op : doc.getOperations()) collect(op.get(), texts);
    CHECK(texts.size() == 4, "four texts");
    if (texts.size() != 4) return 1;
    const int RED = static_cast<int>(0xFFFF0000), BLUE = static_cast<int>(0xFF0000FF);
    auto* parent = texts[0]; auto* child = texts[1]; auto* own = texts[2]; auto* none = texts[3];
    CHECK(parent->fontSize == 30 && parent->color == RED && parent->strikethrough && !parent->underline,
          "the style's size, colour and strikethrough");
    CHECK(child->fontSize == 30 && child->color == RED && child->strikethrough && child->underline,
          "its own underline, the rest from its parent");
    CHECK(own->fontSize == 12 && own->color == BLUE && own->strikethrough,
          "the text's own size and colour win");
    CHECK(none->fontSize == 12 && none->color == static_cast<int>(0xFF000000) && !none->strikethrough,
          "an unstyled text is untouched");

    // And it paints that way: red ink in the top rows, where "Parent" is.
    SkPixmap pm;
    surface->peekPixels(&pm);
    int red = 0;
    for (int y = 0; y < 40; y++)
        for (int x = 0; x < 300; x++) {
            SkColor c = pm.getColor(x, y);
            if (SkColorGetR(c) > 180 && SkColorGetG(c) < 90 && SkColorGetB(c) < 90) red++;
        }
    CHECK(red > 100, "the styled text paints red");

    return failures ? 1 : 0;
}
