// BINARY_DATA (41) against the Java reference. The fixture holds each operation as the
// reference writes it and what the reference reads back, or REJECT when its reader refuses it
// (fixtures/BinaryDataGolden.java). Each case is read through readOperationTree, as a document
// is, and applied: the blob must then be the context's object under its id.
//
// The size limit is also checked here with a blob of each size, since the fixture's refused
// case has a length longer than its buffer and is refused for that reason as well.
//
//   binary_data_test <fixtures>/binary_data_java.txt
#include "rccore/Loom.h"
#include "rccore/RemoteContext.h"
#include "rccore/WireBuffer.h"
#include "rccore/operations/BinaryData.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static std::vector<uint8_t> unhex(const std::string& hex) {
    std::vector<uint8_t> out;
    for (size_t i = 0; i + 1 < hex.size(); i += 2)
        out.push_back(static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    return out;
}

/** Read `op` as a document body; the one BinaryData it holds, or null if the load failed. */
static rccore::BinaryData* load(const std::vector<uint8_t>& op,
                                std::vector<std::unique_ptr<rccore::Operation>>& tree) {
    rccore::WireBuffer buf(op.data(), op.size());
    if (!rccore::readOperationTree(buf, tree, nullptr) || tree.size() != 1) return nullptr;
    return dynamic_cast<rccore::BinaryData*>(tree[0].get());
}

static std::vector<uint8_t> blob(int length) {
    std::vector<uint8_t> op = {41, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < 4; i++) op.push_back(static_cast<uint8_t>(length >> (24 - 8 * i)));
    op.resize(op.size() + static_cast<size_t>(length), 0xAB);
    return op;
}

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: binary_data_test FILE.txt\n"); return 2; }
    std::ifstream f(argv[1]);
    std::string line, name;
    std::vector<uint8_t> op, data;
    int id = 0, type = 0, flags = 0, cases = 0, bad = 0;
    bool reject = false;
    while (std::getline(f, line)) {
        std::istringstream in(line);
        std::string tag;
        in >> tag;
        if (tag == "CASE") { in >> name; reject = false; data.clear(); }
        else if (tag == "HEX") { std::string h; in >> h; op = unhex(h); }
        else if (tag == "REJECT") reject = true;
        else if (tag == "ID") in >> id;
        else if (tag == "TYPE") in >> type;
        else if (tag == "FLAGS") in >> flags;
        else if (tag == "DATA") { size_t n; std::string h; in >> n >> h; data = unhex(h); }
        else if (tag == "END") {
            cases++;
            std::vector<std::unique_ptr<rccore::Operation>> tree;
            rccore::BinaryData* d = load(op, tree);
            if (reject) {
                if (d) { std::fprintf(stderr, "FAIL %s: loaded a blob the reference refuses\n", name.c_str()); bad++; }
                else std::fprintf(stderr, "ok   %s: refused\n", name.c_str());
                continue;
            }
            if (!d) { std::fprintf(stderr, "FAIL %s: did not load\n", name.c_str()); bad++; continue; }
            rccore::RemoteContext ctx;
            d->apply(ctx);
            if (d->id != id || d->type != type || d->flags != flags || d->data != data) {
                std::fprintf(stderr, "FAIL %s: read id %d type %d flags %d, %zu bytes\n", name.c_str(),
                             d->id, d->type, d->flags, d->data.size());
                bad++;
            } else if (ctx.getObject(id) != d) {
                std::fprintf(stderr, "FAIL %s: not stored under id %d\n", name.c_str(), id);
                bad++;
            } else {
                std::fprintf(stderr, "ok   %s: %zu bytes under id %d\n", name.c_str(), data.size(), id);
            }
        }
    }
    if (cases == 0) { std::fprintf(stderr, "no cases in %s\n", argv[1]); return 1; }

    std::vector<std::unique_ptr<rccore::Operation>> atLimit, overLimit;
    bool atOk = load(blob(rccore::BinaryData::MAX_BINARY_DATA), atLimit) != nullptr;
    bool overOk = load(blob(rccore::BinaryData::MAX_BINARY_DATA + 1), overLimit) != nullptr;
    if (!atOk) { std::fprintf(stderr, "FAIL a blob of exactly MAX_BINARY_DATA was refused\n"); bad++; }
    if (overOk) { std::fprintf(stderr, "FAIL a blob one byte over MAX_BINARY_DATA loaded\n"); bad++; }
    if (atOk && !overOk) std::fprintf(stderr, "ok   the limit: %d loads, one more is refused\n", rccore::BinaryData::MAX_BINARY_DATA);

    std::fprintf(stderr, "%d cases, %d failed\n", cases + 1, bad);
    return bad ? 1 : 0;
}
