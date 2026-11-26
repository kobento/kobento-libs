// Compressed documents (Header.COMPRESS = DEFLATE). analog-clock.deflate.rc was compressed by
// androidx's Header.compressDocument before the stream marker existed; analog-clock.marked.rc is
// the same stream behind Header.COMPRESSED_STREAM_MARKER (0x77), as compressDocument writes it
// since the 3D change (4256202). Both decompress to analog-clock.rc byte for byte, and load.
// (kobento-ts tests/compression_test.mjs writes the marked fixture.)

#include "rccore/Compression.h"
#include "rccore/CoreDocument.h"
#include "rccore/WireBuffer.h"

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

int main(int argc, char** argv) {
    std::string dir = (argc > 1) ? argv[1] : "tests/fixtures";
    auto plain = readFile(dir + "/analog-clock.rc");
    auto old = readFile(dir + "/analog-clock.deflate.rc");
    auto marked = readFile(dir + "/analog-clock.marked.rc");
    if (plain.empty() || old.empty() || marked.empty()) { std::fprintf(stderr, "FAIL: missing fixtures in %s\n", dir.c_str()); return 1; }

    CHECK(!rccore::isCompressed(plain.data(), plain.size()), "the original is not compressed");
    CHECK(rccore::isCompressed(old.data(), old.size()) && rccore::isCompressed(marked.data(), marked.size()), "both compressed forms are");
    std::vector<uint8_t> out;
    rccore::decompressDocument(old.data(), old.size(), out);
    CHECK(out == plain, "a stream right after the header decompresses to the original");
    rccore::decompressDocument(marked.data(), marked.size(), out);
    CHECK(out == plain, "a stream after the marker decompresses to the original");

    auto count = [](std::vector<uint8_t>& bytes) {
        rccore::WireBuffer buffer(bytes.data(), bytes.size());
        rccore::CoreDocument doc;
        return doc.initFromBuffer(buffer) ? (int) doc.getOperations().size() : -1;
    };
    int n = count(plain);
    CHECK(n > 0 && count(marked) == n, "the marked document loads as the original");

    if (failures) std::fprintf(stderr, "%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
