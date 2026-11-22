// Host actions and named variables as a host sees them (the Navigator convention's needs).
//
// fixtures/host_action_names.rc (from host_action_names.json, by json2rc) has two 100x100
// boxes: the left one sends hostNamedAction "back", the right one "navigate" with "/x". Each
// name is used once, so the converter writes its DATA_TEXT inside the click's action list (as
// the reference does), not in the data pass: the text has to be loaded when the list runs, or
// the action reaches the host with an empty name. It also exports `count` (3), which a host
// finds by name (namedVariableId) and sets (overrideFloat).
//
// Pure CPU (no GPU/GLFW). Returns 0 on success, 1 on any failed assertion.

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
#include <utility>
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

int main(int argc, char** argv) {
    std::string dir = (argc > 1) ? argv[1] : "tests/fixtures";
    auto data = readFile(dir + "/host_action_names.rc");
    if (data.empty()) { std::fprintf(stderr, "FAIL: cannot read %s/host_action_names.rc\n", dir.c_str()); return 1; }

    rccore::WireBuffer buffer(data.data(), data.size());
    rccore::CoreDocument doc;
    CHECK(doc.initFromBuffer(buffer), "the document parses");
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 100));
    SkCanvas* canvas = surface->getCanvas();
    rccore::RemoteContext ctx;
    rcskia::SkiaPaintContext paint(ctx, canvas);
    ctx.setPaintContext(&paint);
    ctx.setDocument(&doc);
    ctx.mWidth = 200; ctx.mHeight = 100;
    std::vector<std::pair<std::string, std::string>> sent;
    ctx.hostNamedAction = [&](const std::string& name, int, const std::string& value) { sent.emplace_back(name, value); };
    doc.registerListeners(ctx);
    doc.applyDataOperations(ctx, -2);
    doc.paint(ctx, -2);

    // Named variables, by name.
    const int count = ctx.namedVariableId("count");
    CHECK(count >= 0, "count is found by name");
    CHECK(ctx.namedVariableType("count") == 1, "count is a float (NAMED_VARIABLE type 1)");
    CHECK(count >= 0 && ctx.getFloat(count) == 3.0f, "count reads 3");
    CHECK(ctx.namedVariableId("nope") == -1, "an unknown name is -1");
    if (count >= 0) ctx.overrideFloat(count, 5.0f);
    doc.applyDataOperations(ctx, -2);
    doc.paint(ctx, -2);
    CHECK(count >= 0 && ctx.getFloat(count) == 5.0f, "a host value sticks through the data pass");

    // Host actions whose names live in their click lists.
    doc.onClick(ctx, 50, 50);
    CHECK(sent.size() == 1 && sent[0].first == "back", "the left box sends \"back\"");
    doc.onClick(ctx, 150, 50);
    CHECK(sent.size() == 2 && sent[1].first == "navigate" && sent[1].second == "/x", "the right box sends \"navigate\" with \"/x\"");

    std::fprintf(stderr, failures ? "\n%d FAILURE(S)\n" : "\nALL PASSED\n", failures);
    return failures ? 1 : 0;
}
