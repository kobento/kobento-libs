// What this player makes of ADD_MESH_2D / BINARY_DATA operations, for the three-way check
// (rcJson tools/xcheck/xcheck_mesh2d.py). Reads the cases file the Java side writes — CASE,
// HEX, VAR lines; its results are ignored — and prints the same format with this player's
// results: REJECT when the reader throws, else CONSUMED and the expansion or the blob.
//
//   mesh2d_dump CASES.txt > OUT.txt
#include "rccore/RemoteContext.h"
#include "rccore/WireBuffer.h"
#include "rccore/operations/BinaryData.h"
#include "rccore/operations/Mesh2D.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static void floats(const char* tag, const std::vector<float>& a) {
    std::printf("%s %zu", tag, a.size());
    for (float f : a) { uint32_t b; std::memcpy(&b, &f, 4); std::printf(" %x", b); }
    std::printf("\n");
}

static void ints(const char* tag, const std::vector<int32_t>& a) {
    std::printf("%s %zu", tag, a.size());
    for (int32_t i : a) std::printf(" %d", i);
    std::printf("\n");
}

static void run(const std::string& name, const std::vector<uint8_t>& op, int varId, float var) {
    std::printf("CASE %s\n", name.c_str());
    rccore::WireBuffer buf(op.data(), op.size());
    int opcode = buf.readByte();
    std::vector<std::unique_ptr<rccore::Operation>> ops;
    try {
        if (opcode == rccore::BinaryData::OP_CODE) rccore::BinaryData::read(buf, ops);
        else rccore::AddMesh2D::read(buf, ops);
    } catch (const std::runtime_error&) {
        std::printf("REJECT\nEND\n");
        return;
    }
    std::printf("CONSUMED %d\n", buf.getIndex());
    if (auto* d = dynamic_cast<rccore::BinaryData*>(ops[0].get())) {
        std::printf("BLOB %d %d %d %zu ", d->id, d->type, d->flags, d->data.size());
        for (uint8_t b : d->data) std::printf("%02x", b);
        std::printf("\nEND\n");
        return;
    }
    auto* mesh = static_cast<rccore::AddMesh2D*>(ops[0].get());
    rccore::RemoteContext ctx;
    if (varId >= 0) ctx.loadFloat(varId, var);
    mesh->updateVariables(ctx);
    mesh->expand(ctx);
    floats("VERTS", mesh->verts);
    floats("UV", mesh->uv);
    ints("COLORS", mesh->colors);
    ints("INDICES", mesh->indices);
    std::printf("END\n");
}

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: mesh2d_dump CASES.txt\n"); return 2; }
    std::ifstream f(argv[1]);
    if (!f) { std::fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    std::string line, name;
    std::vector<uint8_t> op;
    int varId = -1;
    float var = 0;
    while (std::getline(f, line)) {
        std::istringstream in(line);
        std::string tag;
        in >> tag;
        if (tag == "CASE") { in >> name; op.clear(); varId = -1; }
        else if (tag == "HEX") {
            std::string h; in >> h;
            for (size_t i = 0; i + 1 < h.size(); i += 2)
                op.push_back(static_cast<uint8_t>(std::stoul(h.substr(i, 2), nullptr, 16)));
        } else if (tag == "VAR") {
            std::string bits; in >> varId >> bits;
            uint32_t b = static_cast<uint32_t>(std::stoul(bits, nullptr, 16));
            std::memcpy(&var, &b, 4);
        } else if (tag == "END") {
            run(name, op, varId, var);
        }
    }
    return 0;
}
