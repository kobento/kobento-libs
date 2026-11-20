// BinaryDataGolden DOC.rc OUT : BINARY_DATA (41) as the JVM writes and reads it, for the
// players' tests. Each case is the operation's bytes, then what the reference reads back (id,
// type, flags, the data) or REJECT when its reader refuses the payload. DOC.rc is carried as a
// TYPE_RC_DOC blob, as a parent document carries an embedded child.
//   javac -cp <remote-core classes> -d out BinaryDataGolden.java
//   java -cp out:<remote-core classes> BinaryDataGolden root_data.rc binary_data_java.txt
import androidx.compose.remote.core.Operation;
import androidx.compose.remote.core.WireBuffer;
import androidx.compose.remote.core.operations.BinaryData;

import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;

public class BinaryDataGolden {
    static final StringBuilder out = new StringBuilder();

    static byte[] op(int id, int type, int flags, byte[] data) {
        WireBuffer w = new WireBuffer();
        BinaryData.apply(w, id, type, flags, data);
        byte[] b = new byte[w.getSize()];
        System.arraycopy(w.getBuffer(), 0, b, 0, b.length);
        return b;
    }

    static String hex(byte[] b) {
        StringBuilder s = new StringBuilder();
        for (byte x : b) s.append(String.format("%02x", x & 0xFF));
        return s.toString();
    }

    static void emit(String name, byte[] op) {
        out.append("CASE ").append(name).append('\n');
        out.append("HEX ").append(hex(op)).append('\n');
        WireBuffer in = new WireBuffer(op, op.length);
        in.readByte();
        List<Operation> ops = new ArrayList<>();
        try {
            BinaryData.read(in, ops);
        } catch (RuntimeException e) {
            out.append("REJECT ").append(e.getMessage()).append("\nEND\n");
            return;
        }
        BinaryData d = (BinaryData) ops.get(0);
        out.append("ID ").append(d.getId()).append('\n');
        out.append("TYPE ").append(d.getType()).append('\n');
        out.append("FLAGS ").append(d.getFlags()).append('\n');
        out.append("DATA ").append(d.getData().length).append(' ').append(hex(d.getData())).append('\n');
        out.append("END\n");
    }

    public static void main(String[] args) throws Exception {
        byte[] doc = Files.readAllBytes(Paths.get(args[0]));
        emit("raw_bytes", op(7, BinaryData.TYPE_RAW, 0, new byte[] {0, 1, 2, (byte) 0x80, (byte) 0xFF}));
        emit("raw_empty", op(8, BinaryData.TYPE_RAW, 0, new byte[0]));
        emit("rc_doc", op(9, BinaryData.TYPE_RC_DOC, 0, doc));
        // flags is reserved for the type; the reader keeps whatever is there.
        emit("raw_flags", op(10, BinaryData.TYPE_RAW, 3, new byte[] {42}));
        // One byte over the limit is refused (the length field patched, since the reference
        // writer will not produce it).
        byte[] over = op(12, BinaryData.TYPE_RAW, 0, new byte[] {1, 2, 3});
        int at = 1 + 3 * 4;
        int n = 100_001;
        for (int i = 0; i < 4; i++) over[at + i] = (byte) (n >>> (24 - 8 * i));
        emit("reject_over_limit", over);
        Files.writeString(Paths.get(args[1]), out.toString());
    }
}
