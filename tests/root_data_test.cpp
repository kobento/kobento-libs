// A string changed after the document was loaded keeps its value through later frames and a
// resize (androidx CL 4445989: "Do not re-apply root data operations while animating the root
// bounds"). Java applies a data operation when the document is initialized and then only when
// it is dirty; applying TextData on every frame reloaded the document's string over the edit.
//
// fixtures/root_data.rc (root_data.json): an exported string @label ("initial") shown by a text.
//
// Pure CPU. Returns 0 on success, 1 on any failed assertion.

#include "rccore/WireBuffer.h"
#include "rccore/CoreDocument.h"
#include "rccore/RemoteContext.h"
#include "rccore/operations/DrawOperations.h"
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

static int findNamedId(const std::vector<std::unique_ptr<rccore::Operation>>& ops, const std::string& want) {
    for (const auto& op : ops) {
        if (auto* nv = dynamic_cast<rccore::NamedVariable*>(op.get()))
            if (nv->varName == want) return nv->varId;
        if (!op->mChildren.empty()) { int id = findNamedId(op->mChildren, want); if (id >= 0) return id; }
    }
    return -1;
}

int main(int argc, char** argv) {
    std::string dir = (argc > 1) ? argv[1] : "tests/fixtures";
    auto data = readFile(dir + "/root_data.rc");
    if (data.empty()) { std::fprintf(stderr, "FAIL: cannot read %s/root_data.rc\n", dir.c_str()); return 1; }
    rccore::WireBuffer buffer(data.data(), data.size());
    rccore::CoreDocument doc;
    CHECK(doc.initFromBuffer(buffer), "root_data document parses");

    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(300, 300));
    SkCanvas* canvas = surface->getCanvas();
    rccore::RemoteContext ctx;
    rcskia::SkiaPaintContext paint(ctx, canvas);
    ctx.setPaintContext(&paint);
    ctx.setDocument(&doc);
    ctx.mWidth = 200; ctx.mHeight = 200;
    doc.registerListeners(ctx);
    auto frame = [&] { canvas->clear(SK_ColorWHITE); doc.applyDataOperations(ctx, -2); doc.paint(ctx, -2); };
    frame();

    const int id = findNamedId(doc.getOperations(), "label");
    CHECK(id >= 0, "@label is an exported named variable");
    if (id < 0) return 1;
    CHECK(ctx.getText(id) == "initial", "the string starts as the document has it");

    ctx.loadText(id, "edited");
    frame();
    CHECK(ctx.getText(id) == "edited", "a string changed after the load survives the next frame");

    ctx.mWidth = 300; ctx.mHeight = 300;   // a resize: the root relayouts (and animates its bounds)
    for (int i = 0; i < 5; ++i) frame();
    CHECK(ctx.getText(id) == "edited", "and survives a resize");

    if (failures) std::fprintf(stderr, "%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
