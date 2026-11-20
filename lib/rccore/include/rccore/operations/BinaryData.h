#pragma once
// BINARY_DATA (41): an opaque blob stored in the document under an id.
//
// Ported from androidx remote-core BinaryData.java (2026-11). Wire format: id, type, flags,
// then the bytes as a length-prefixed buffer of at most MAX_BINARY_DATA. `type` says what the
// bytes are; `flags` is reserved for the type and 0 today. The blob is registered with
// putObject so other operations can find it by id: an embedded document (LAYOUT_RC_DOC) whose
// url id names a TYPE_RC_DOC blob plays those bytes rather than asking the host to resolve the
// url, which lets a parent carry its children in its own buffer.

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "rccore/Operation.h"
#include "rccore/RemoteContext.h"
#include "rccore/WireBuffer.h"

namespace rccore {

class BinaryData : public Operation {
  public:
    static constexpr int OP_CODE = 41;
    /** Uninterpreted bytes. */
    static constexpr int TYPE_RAW = 0;
    /** A complete RemoteCompose document (header included), playable on its own. */
    static constexpr int TYPE_RC_DOC = 1;
    /** Limits.MAX_BINARY_DATA. */
    static constexpr int MAX_BINARY_DATA = 100000;

    int id = 0, type = TYPE_RAW, flags = 0;
    std::vector<uint8_t> data;

    std::string name() const override { return "BINARY_DATA"; }
    int opcode() const override { return OP_CODE; }
    std::vector<Field> fields() const override {
        return {
            {"id", "INT", std::to_string(id)},
            {"type", "INT", std::to_string(type)},
            {"flags", "INT", std::to_string(flags)},
            {"length", "INT", std::to_string(data.size())},
        };
    }

    static void read(WireBuffer& buffer, std::vector<std::unique_ptr<Operation>>& ops) {
        auto op = std::make_unique<BinaryData>();
        op->id = buffer.readId();
        op->type = buffer.readInt();
        op->flags = buffer.readInt();
        // readBuffer(Limits.MAX_BINARY_DATA): the reference refuses a larger blob, so a
        // document carrying one fails to load here as it does there.
        int count = buffer.peekInt();
        if (count < 0 || count > MAX_BINARY_DATA) {
            throw std::runtime_error("BINARY_DATA of " + std::to_string(count)
                                     + " bytes exceeds MAX_BINARY_DATA");
        }
        op->data = buffer.readBuffer();
        ops.push_back(std::move(op));
    }

    void apply(RemoteContext& context) override { context.putObject(id, this); }
};

}  // namespace rccore
