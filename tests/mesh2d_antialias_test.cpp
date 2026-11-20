// AddMesh2D FLAG_ANTIALIAS against the Java reference. The fixture holds one ADD_MESH_2D per
// case as the reference writes it, and what the reference expands it to: the body followed by
// the skirt (fixtures/Mesh2DAntialiasGolden.java). A case marked REJECT is one the reference's
// reader refuses; here the reader must throw, which readOperationTree turns into a failed load.
//
// Indices and colours must match exactly. Positions are float arithmetic done in a different
// order in places, so they get a small tolerance; the largest difference is printed.
//
//   mesh2d_antialias_test <fixtures>/mesh2d_antialias_java.txt
#include "rccore/Loom.h"
#include "rccore/RemoteContext.h"
#include "rccore/WireBuffer.h"
#include "rccore/operations/Mesh2D.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

struct Case {
    std::string name;
    std::vector<uint8_t> op;
    int varId = -1;
    float varValue = 0;
    bool reject = false;
    std::vector<float> verts, uv;
    std::vector<int32_t> colors, indices;
};

static float bitsToFloat(uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; }

static std::vector<Case> load(const char* path) {
    std::ifstream f(path);
    std::vector<Case> cases;
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream in(line);
        std::string tag;
        in >> tag;
        if (tag == "CASE") { cases.emplace_back(); in >> cases.back().name; continue; }
        if (cases.empty()) continue;
        Case& c = cases.back();
        if (tag == "HEX") {
            std::string hex; in >> hex;
            for (size_t i = 0; i + 1 < hex.size(); i += 2)
                c.op.push_back(static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
        } else if (tag == "VAR") {
            in >> c.varId >> c.varValue;
        } else if (tag == "REJECT") {
            c.reject = true;
        } else if (tag == "VERTS" || tag == "UV") {
            size_t n; in >> n;
            auto& dst = tag == "VERTS" ? c.verts : c.uv;
            for (size_t i = 0; i < n; i++) { std::string h; in >> h; dst.push_back(bitsToFloat(std::stoul(h, nullptr, 16))); }
        } else if (tag == "COLORS" || tag == "INDICES") {
            size_t n; in >> n;
            auto& dst = tag == "COLORS" ? c.colors : c.indices;
            for (size_t i = 0; i < n; i++) { long long v; in >> v; dst.push_back(static_cast<int32_t>(v)); }
        }
    }
    return cases;
}

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: mesh2d_antialias_test FILE.txt\n"); return 2; }
    std::vector<Case> cases = load(argv[1]);
    if (cases.empty()) { std::fprintf(stderr, "no cases in %s\n", argv[1]); return 1; }
    int bad = 0;
    for (const Case& c : cases) {
        rccore::WireBuffer buf(c.op.data(), c.op.size());
        buf.readByte();
        std::vector<std::unique_ptr<rccore::Operation>> ops;
        bool threw = false;
        try { rccore::AddMesh2D::read(buf, ops); } catch (const std::runtime_error&) { threw = true; }
        if (c.reject) {
            // And a document holding it fails to load, rather than the throw escaping.
            rccore::WireBuffer whole(c.op.data(), c.op.size());
            std::vector<std::unique_ptr<rccore::Operation>> tree;
            bool loaded = rccore::readOperationTree(whole, tree, nullptr);
            if (!threw) { std::fprintf(stderr, "FAIL %s: read accepted a mesh the reference refuses\n", c.name.c_str()); bad++; }
            else if (loaded) { std::fprintf(stderr, "FAIL %s: the document still loaded\n", c.name.c_str()); bad++; }
            else std::fprintf(stderr, "ok   %s: refused, and the document with it\n", c.name.c_str());
            continue;
        }
        if (threw || ops.size() != 1) { std::fprintf(stderr, "FAIL %s: read refused it\n", c.name.c_str()); bad++; continue; }
        if (buf.available()) { std::fprintf(stderr, "FAIL %s: read stopped at %d of %zu bytes\n", c.name.c_str(), buf.getIndex(), c.op.size()); bad++; continue; }
        auto* mesh = static_cast<rccore::AddMesh2D*>(ops[0].get());
        rccore::RemoteContext ctx;
        if (c.varId >= 0) ctx.loadFloat(c.varId, c.varValue);
        mesh->updateVariables(ctx);
        mesh->expand(ctx);

        std::string why;
        float worst = 0;
        auto near = [&](const std::vector<float>& got, const std::vector<float>& want, const char* what) {
            if (got.size() != want.size()) { why += std::string(what) + " size " + std::to_string(got.size()) + " != " + std::to_string(want.size()) + "; "; return; }
            for (size_t i = 0; i < got.size(); i++) {
                float d = std::fabs(got[i] - want[i]);
                worst = std::max(worst, d);
                if (!(d <= 1e-4f * std::max(1.0f, std::fabs(want[i])))) {
                    why += std::string(what) + "[" + std::to_string(i) + "] " + std::to_string(got[i]) + " != " + std::to_string(want[i]) + "; ";
                    return;
                }
            }
        };
        auto same = [&](const std::vector<int32_t>& got, const std::vector<int32_t>& want, const char* what) {
            if (got != want) why += std::string(what) + " differ (" + std::to_string(got.size()) + " vs " + std::to_string(want.size()) + "); ";
        };
        near(mesh->verts, c.verts, "verts");
        near(mesh->uv, c.uv, "uv");
        same(mesh->colors, c.colors, "colors");
        same(mesh->indices, c.indices, "indices");
        if (!why.empty()) { std::fprintf(stderr, "FAIL %s: %s\n", c.name.c_str(), why.c_str()); bad++; }
        else std::fprintf(stderr, "ok   %s: %zu vertices, %zu indices, max |dpos| %.3g\n", c.name.c_str(), c.verts.size() / 2, c.indices.size(), worst);
    }
    std::fprintf(stderr, "%zu cases, %d failed\n", cases.size(), bad);
    return bad ? 1 : 0;
}
