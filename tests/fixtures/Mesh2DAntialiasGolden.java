// Mesh2DAntialiasGolden OUT : AddMesh2D FLAG_ANTIALIAS on the JVM, for the players' tests.
//
// Each case is an ADD_MESH_2D operation as the reference writes it, then either what the
// reference expands it to (verts, uv, colors, indices: the body followed by the skirt) or
// REJECT when its reader refuses the payload. The players read the same bytes and must expand
// them to the same arrays. Build against remote-core with its test HeadlessRemoteContext:
//   javac -cp <remote-core classes>:<jspecify> -d out HeadlessRemoteContext.java Mesh2DAntialiasGolden.java
//   java -cp out:<remote-core classes> Mesh2DAntialiasGolden mesh2d_antialias_java.txt
import androidx.compose.remote.core.Operation;
import androidx.compose.remote.core.WireBuffer;
import androidx.compose.remote.core.operations.AddMesh2D;
import androidx.compose.remote.core.operations.Utils;
import androidx.compose.remote.core.operations.utilities.AnimatedFloatExpression;
import androidx.compose.remote.core.operations.utilities.Mesh2DGenerator;
import androidx.compose.remote.core.testing.HeadlessRemoteContext;

import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;

public class Mesh2DAntialiasGolden {
    static final float U = AnimatedFloatExpression.VAR1;
    static final float V = AnimatedFloatExpression.VAR2;
    static final float ADD = AnimatedFloatExpression.ADD;
    static final float MUL = AnimatedFloatExpression.MUL;
    static final float SIN = AnimatedFloatExpression.SIN;
    static final int WIDTH_VAR = 42;          // the variable a case's width may name
    static final float WIDTH_VAR_VALUE = 3f;

    static final StringBuilder out = new StringBuilder();

    static float[] affine(float var, float scale, float offset) {
        return new float[] {var, scale, MUL, offset, ADD};
    }

    static float[] k(float value) {
        return new float[] {value};
    }

    /** x, y (null for the layout default), and a colour, which a skirt needs. */
    static float[][] groups(float[] x, float[] y, float[] r, float[] g, float[] b) {
        return new float[][] {x, y, null, null, k(1f), r, g, b, null};
    }

    static byte[] expression(int layout, int u, int v, int flags, float[][] groups, float width) {
        WireBuffer w = new WireBuffer();
        AddMesh2D.apply(w, 1, AddMesh2D.TYPE_EXPRESSION, layout, u, v, flags, 0, groups, null,
                null, null, null, width);
        return bytes(w);
    }

    /** A u x v grid of literal vertices from (10, 20), 7 apart, in colours that vary. */
    static byte[] literal(int type, int u, int v, float width, boolean withUv) {
        int n = u * v;
        float[] verts = new float[n * 2];
        float[] uv = withUv ? new float[n * 2] : null;
        int[] colors = new int[n];
        for (int j = 0; j < v; j++) {
            for (int i = 0; i < u; i++) {
                int p = j * u + i;
                verts[p * 2] = 10f + 7f * i + 0.5f * j;
                verts[p * 2 + 1] = 20f + 7f * j;
                if (uv != null) {
                    uv[p * 2] = i / (float) (u - 1);
                    uv[p * 2 + 1] = j / (float) (v - 1);
                }
                colors[p] = 0xFF000000 | (i * 40 << 16) | (j * 50 << 8) | 0x33;
            }
        }
        int[] indices = new int[Mesh2DGenerator.indexCount(Mesh2DGenerator.LAYOUT_GRID, u, v)];
        Mesh2DGenerator.generateIndices(Mesh2DGenerator.LAYOUT_GRID, u, v, indices);
        WireBuffer w = new WireBuffer();
        AddMesh2D.apply(w, 1, type, Mesh2DGenerator.LAYOUT_GRID, u, v, AddMesh2D.FLAG_ANTIALIAS,
                0, null, indices, verts, uv, colors, width);
        return bytes(w);
    }

    /** A 3x2 grid whose top middle vertex is pulled 300 up into a spike: its tip is about 19
     *  degrees, so the skirt there hits the miter limit (no rounder corner reaches it). */
    static byte[] spike() {
        float[] verts = {0f, 0f, 50f, -300f, 100f, 0f, 0f, 10f, 50f, 10f, 100f, 10f};
        int[] colors = {0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0xFF808080, 0xFF101010, 0xFFFFFFFF};
        int[] indices = new int[Mesh2DGenerator.indexCount(Mesh2DGenerator.LAYOUT_GRID, 3, 2)];
        Mesh2DGenerator.generateIndices(Mesh2DGenerator.LAYOUT_GRID, 3, 2, indices);
        WireBuffer w = new WireBuffer();
        AddMesh2D.apply(w, 1, AddMesh2D.TYPE_VALUES, Mesh2DGenerator.LAYOUT_GRID, 3, 2,
                AddMesh2D.FLAG_ANTIALIAS, 0, null, indices, verts, null, colors, 2f);
        return bytes(w);
    }

    static byte[] bytes(WireBuffer w) {
        byte[] b = new byte[w.getSize()];
        System.arraycopy(w.getBuffer(), 0, b, 0, b.length);
        return b;
    }

    /** The flags word of an ADD_MESH_2D: after the opcode and five ints. */
    static final int FLAGS_AT = 1 + 5 * 4;

    /** Set FLAG_ANTIALIAS on a mesh written without it and append a width, as a hostile or
     *  buggy writer would; the reference writer refuses to produce these itself. */
    static byte[] forceFlag(byte[] op, float width) {
        byte[] b = new byte[op.length + 4];
        System.arraycopy(op, 0, b, 0, op.length);
        b[FLAGS_AT + 2] |= 0x01;   // bit 8 of the big-endian int
        int bits = Float.floatToRawIntBits(width);
        for (int i = 0; i < 4; i++) b[op.length + i] = (byte) (bits >>> (24 - 8 * i));
        return b;
    }

    static void emit(String name, byte[] op, boolean useVar) {
        out.append("CASE ").append(name).append('\n');
        out.append("HEX ");
        for (byte x : op) out.append(String.format("%02x", x & 0xFF));
        out.append('\n');
        if (useVar) out.append("VAR ").append(WIDTH_VAR).append(' ').append(WIDTH_VAR_VALUE).append('\n');
        WireBuffer in = new WireBuffer(op, op.length);
        in.readByte();
        List<Operation> ops = new ArrayList<>();
        try {
            AddMesh2D.read(in, ops);
        } catch (RuntimeException e) {
            out.append("REJECT ").append(e.getMessage()).append("\nEND\n");
            return;
        }
        AddMesh2D mesh = (AddMesh2D) ops.get(0);
        HeadlessRemoteContext context = new HeadlessRemoteContext();
        if (useVar) context.loadFloat(WIDTH_VAR, WIDTH_VAR_VALUE);
        mesh.updateVariables(context);
        mesh.expand(context);
        floats("VERTS", mesh.getVerts());
        floats("UV", mesh.getUv());
        ints("COLORS", mesh.getColors());
        ints("INDICES", mesh.getIndices());
        out.append("END\n");
    }

    static void floats(String tag, float[] a) {
        out.append(tag).append(' ').append(a.length);
        for (float f : a) out.append(' ').append(Integer.toHexString(Float.floatToRawIntBits(f)));
        out.append('\n');
    }

    static void ints(String tag, int[] a) {
        out.append(tag).append(' ').append(a.length);
        for (int i : a) out.append(' ').append(i);
        out.append('\n');
    }

    public static void main(String[] args) throws Exception {
        int F = AddMesh2D.FLAG_ANTIALIAS;
        float[] red = k(1f), zero = k(0f);
        float[] ramp = affine(U, 1f, 0f);

        emit("grid_red_square", expression(Mesh2DGenerator.LAYOUT_GRID, 8, 8, F,
                groups(affine(U, 64f, 32f), affine(V, 64f, 32f), red, zero, zero), 1f), false);
        // Mirrored in x: the skirt has to find the outside from the body, not the winding.
        emit("grid_mirrored_ramp", expression(Mesh2DGenerator.LAYOUT_GRID, 5, 4, F,
                groups(affine(U, -50f, 80f), affine(V, 30f, 5f), ramp, k(0.5f), zero), 2.5f), false);
        emit("grid_wavy", expression(Mesh2DGenerator.LAYOUT_GRID, 6, 3, F,
                groups(affine(U, 90f, 0f), new float[] {V, 20f, MUL, U, 6f, MUL, SIN, 4f, MUL, ADD},
                        zero, ramp, k(1f)), 1.25f), false);
        emit("polar_default", expression(Mesh2DGenerator.LAYOUT_POLAR, 12, 4, F,
                groups(null, null, k(0.2f), k(0.4f), k(0.9f)), 0.05f), false);
        emit("ring_default", expression(Mesh2DGenerator.LAYOUT_RING, 16, 3, F,
                groups(null, null, ramp, zero, k(1f)), 0.1f), false);
        emit("fan_default", expression(Mesh2DGenerator.LAYOUT_FAN, 10, 2, F,
                groups(null, null, red, red, zero), 0.1f), false);
        emit("strip", expression(Mesh2DGenerator.LAYOUT_STRIP, 10, 2, F,
                groups(affine(U, 100f, 0f), affine(V, 8f, 40f), zero, red, zero), 1f), false);
        emit("width_variable", expression(Mesh2DGenerator.LAYOUT_GRID, 4, 4, F,
                groups(affine(U, 40f, 0f), affine(V, 40f, 0f), red, red, red),
                Utils.asNan(WIDTH_VAR)), true);
        emit("width_zero", expression(Mesh2DGenerator.LAYOUT_GRID, 3, 3, F,
                groups(affine(U, 40f, 0f), affine(V, 40f, 0f), red, zero, red), 0f), false);
        emit("values_grid_uv", literal(AddMesh2D.TYPE_VALUES, 4, 3, 1.5f, true), false);
        emit("values_grid_no_uv", literal(AddMesh2D.TYPE_VALUES, 3, 3, 2f, false), false);
        emit("f16_grid_uv", literal(AddMesh2D.TYPE_F16_VALUES, 5, 2, 1f, true), false);
        emit("values_spike_miter", spike(), false);
        // Without the flag: the same bytes as before the flag existed, and no skirt.
        emit("grid_no_flag", expression(Mesh2DGenerator.LAYOUT_GRID, 3, 3, 0,
                groups(affine(U, 40f, 0f), affine(V, 40f, 0f), red, zero, red), 1f), false);

        // The reader refuses these.
        WireBuffer spline = new WireBuffer();
        AddMesh2D.applyPathSplineStrip(spline, 1, 8, 7, new float[] {4f, 10f}, new float[0]);
        emit("reject_spline_strip", forceFlag(bytes(spline), 1f), false);
        emit("reject_no_colour", forceFlag(expression(Mesh2DGenerator.LAYOUT_GRID, 4, 4, 0,
                new float[][] {affine(U, 40f, 0f), affine(V, 40f, 0f), null, null, null, null,
                        null, null, null}, 1f), 1f), false);
        emit("reject_negative_width", forceFlag(expression(Mesh2DGenerator.LAYOUT_GRID, 4, 4, 0,
                groups(affine(U, 40f, 0f), affine(V, 40f, 0f), red, red, red), 1f), -2f), false);
        emit("reject_no_edges", forceFlag(expression(Mesh2DGenerator.LAYOUT_GRID, 1, 8, 0,
                groups(affine(U, 40f, 0f), affine(V, 40f, 0f), red, red, red), 1f), 1f), false);

        Files.writeString(Paths.get(args[0]), out.toString());
    }
}
