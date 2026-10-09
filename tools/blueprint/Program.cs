using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
#nullable enable
using System.Text;

// Blueprint mirror: reproduce the corpus bytes with explicit word math.
// If all six match, the same formulas transcribe to C++ for ogsa-ts-unreal.
// Usage: dotnet run -- <corpus-vectors-dir>  (default: the standard repo
// sibling of this workspace checkout)
var corpus = args.Length > 0 ? args[0]
    : Path.Combine("..", "..", "..", "OGSA-Telemetry-Standard", "conformance", "vectors");
int diffs = 0; // CI exit code: 0 only when every vector matched

var b = new Builder();
// ---- motion-pose: root(1d,1p) ts, objects: 1 obj {name,pos,orient} ----
double ts = 1.25;
var o = b.MotionFrame(ts);
var e = b.MotionObjectList(o, 1);
b.MotionObject(e, 0, "obj", (1.5f, 0.25f, -3.75f), (0f, 0f, 0f, 1f), null, 0xffffffff);
b.PatchRoot(1,1); Check("motion-pose", b, 1, corpus);

b = new Builder();
o = b.MotionFrame(2.5);
e = b.MotionObjectList(o, 1);
b.MotionObject(e, 0, "demo_car", (-2f, 0.5f, 3f), (0f, 1f, 0f, 0f),
    new (ushort, int, double?, bool?)[] { (0, 0, 3250.5, null), (1, 0, 8.0, null), (2, 1, null, true) });
b.PatchRoot(1,1); Check("motion-channels", b, 1, corpus);

b = new Builder();
o = b.MotionFrame(3.75);
e = b.MotionObjectList(o, 2);
b.MotionObject(e, 0, "player_car", (1f, 0f, 0f), (0f, 0f, 0f, 1f), null);
b.MotionObject(e, 1, "wingman", (-1f, 0.5f, 2.5f), (0f, 1f, 0f, 0f), null);
b.PatchRoot(1,1); Check("motion-multi", b, 1, corpus);

b = new Builder();
b.EventFrame(0.125, 7, "body.impact", 0.75f, 0.25f, (-1f, 0f, 0f));
b.PatchRoot(1,1); Check("event-impact", b, 2, corpus);

Console.WriteLine("done");
for (int i = 20; i < Math.Min(28, b.w.Count); i++) Console.WriteLine($"  my w{i} = {b.w[i]:x16}");
// discovery vectors
b = new Builder();
int dtag = b.DiscoveryFrame(1, 1730000000000000L, "conformance",
    (1.25f, 20f, 1f, 9.5f, 1), 1);
b.Object(dtag, 0, "obj", "local", null, "origin", (0, true), null);
Check("discovery-minimal", b, 0, corpus);
for (int i = 22; i < 28; i++) System.Console.WriteLine("  my w" + i + " = " + b.w[i].ToString("x16"));

b = new Builder();
dtag = b.DiscoveryFrame(1, 1730000000000000L, "conformance",
    (1.25f, 20f, 1f, 9.5f, 1), 1);
b.Object(dtag, 0, "demo_car", "local", "conformance car", "origin", (0, true),
    new (string, string, double?, double?)[] {
        ("engine.rpm", "rpm", 0.0, 9000.0),
        ("vehicle.speed", "m/s", 0.0, 30.0),
        ("lights.headlights", "", null, null) });
Check("discovery-channels", b, 0, corpus);

void Check(string name, Builder b, byte ft, string corpus)
{
    var hex = Convert.ToHexString(b.Wrap(ft)).ToLowerInvariant();
    var want = string.Concat(File.ReadAllLines(Path.Combine(corpus, name + ".hex"))).Trim();
    var ok = hex == want;
    Console.WriteLine($"{name}: {(ok ? "MATCH" : "DIFF")}");
    if (!ok)
    {
        diffs++;
        for (int i = 0; i < Math.Max(hex.Length, want.Length); i += 2)
        {
            var a = hex.Length > i + 1 ? hex.Substring(i, 2) : "--";
            var w = want.Length > i + 1 ? want.Substring(i, 2) : "--";
            if (a != w) { Console.WriteLine($"  byte {i / 2}: got {a} want {w}"); if (i > 120) break; }
        }
    }
}

// CI contract: exit 0 only when every vector matched
if (diffs > 0)
{
    Console.WriteLine($"{diffs} vector(s) DIFF");
    Environment.Exit(1);
}

class Builder
{
    public readonly List<ulong> w = new();
    static ushort F16(float f) => 0;
    static uint F32(float f) => BitConverter.SingleToUInt32Bits(f);

    ulong this[int i] { get => w[i]; set => w[i] = value; }
    int Alloc(int words) { w.AddRange(new ulong[words]); return w.Count - words; }

    // pointer value constructors (offset = bodyIdx - ptrIdx - 1)
    static ulong StructPtr(int ptrIdx, int bodyIdx, int dataW, int ptrW) =>
        (ulong)(uint)((bodyIdx - ptrIdx - 1) << 2) | ((ulong)dataW << 32) | ((ulong)ptrW << 48);
    static ulong TextPtr(int ptrIdx, int bodyIdx, int byteCount) =>   // kind 1, size code 2
        1ul | (ulong)(uint)((bodyIdx - ptrIdx - 1) << 2) | ((ulong)byteCount << 35) | (2ul << 32);
    static ulong StructListPtr(int ptrIdx, int bodyIdx, int elemWords) => // kind 1, code 7
        1ul | (ulong)(uint)((bodyIdx - ptrIdx - 1) << 2) | ((ulong)elemWords << 35) | (7ul << 32);
    static ulong Tag(int elems, int dataW, int ptrW) =>
        (ulong)elems << 2 | ((ulong)dataW << 32) | ((ulong)ptrW << 48);

    int Text(string s)
    {
        var bytes = Encoding.UTF8.GetBytes(s).Append((byte)0).ToArray();
        var words = (bytes.Length + 7) / 8;
        int at = Alloc(words);
        for (int i = 0; i < bytes.Length; i++)
            w[at + i / 8] |= (ulong)bytes[i] << (8 * (i % 8));
        return at;
    }
    int Vec3((float x, float y, float z) v, ulong pad = 0) { int a = Alloc(2); w[a] = F32(v.x) | (ulong)F32(v.y) << 32; w[a + 1] = F32(v.z) | pad << 32; return a; }

    public byte[] Datagram()
    {
        // OMT1 + type set later; here: motion type assumed set by caller via WithEnvelope
        throw new NotSupportedException();
    }
    public byte[] Wrap(byte frameType)
    {
        var msg = new List<byte>();
        msg.AddRange(BitConverter.GetBytes(0u));
        msg.AddRange(BitConverter.GetBytes((uint)w.Count));
        // canonical: 1-segment table is exactly 1 word (no pad)
        foreach (var x in w) msg.AddRange(BitConverter.GetBytes(x));
        var d = new byte[8 + msg.Count];
        d[0] = (byte)'O'; d[1] = (byte)'M'; d[2] = (byte)'T'; d[3] = (byte)'1'; d[4] = frameType;
        msg.CopyTo(d, 8);
        return d;
    }

    // ---- frames ----
    public void PatchRoot(int dw, int pw) { w[0] = ((ulong)dw << 32) | ((ulong)pw << 48); }
    public int MotionFrame(double ts)
    {
        w.Add(0); // root ptr, patched at end
        int body = Alloc(2); // 1 data + 1 ptr
        w[body] = (ulong)BitConverter.DoubleToInt64Bits(ts);
        return body; // ptr word at body+1
    }
    public int MotionObjectList(int frameBody, int count)
    {
        int per = 7; // 1d + 6p
        int tag = Alloc(1 + count * per);
        w[tag] = Tag(count, 1, 6);
        w[frameBody + 1] = StructListPtr(frameBody + 1, tag, count * per);
        return tag;
    }
    public void MotionObject(int tag, int index, string name,
        (float, float, float) pos, (float, float, float, float) quat,
        (ushort id, int tag2, double? num, bool? boolean)[]? channels, ulong posPad = 0)
    {
        int e = tag + 1 + index * 7;
        w[e] = 0xffff;                    // dynamics union unset
        // slots 3,4,5(N) — channels at slot 5
        int nameAt = Text(name);
        w[e + 1] = TextPtr(e + 1, nameAt, Encoding.UTF8.GetByteCount(name) + 1);
        int posAt = Vec3(pos, posPad);
        w[e + 2] = StructPtr(e + 2, posAt, 2, 0);
        int qAt = Alloc(2);
        w[qAt] = F32(quat.Item1) | (ulong)F32(quat.Item2) << 32;
        w[qAt + 1] = F32(quat.Item3) | (ulong)F32(quat.Item4) << 32;
        w[e + 3] = StructPtr(e + 3, qAt, 2, 0); // slot2 = orientation
        w[e + 4] = 0; w[e + 5] = 0;
        if (channels != null)
        {
            int ltag = Alloc(1 + channels.Length * 2);
            w[ltag] = Tag(channels.Length, 1, 1);
            for (int i = 0; i < channels.Length; i++)
            {
                var c = channels[i];
                int ce = ltag + 1 + i * 2;
                w[ce] = c.id | (ulong)(ushort)c.tag2 << 16
                      | (ulong)(c.num.HasValue ? F32((float)c.num.Value) : c.boolean == true ? 1u : 0u) << 32;
            }
            w[e + 6] = StructListPtr(e + 6, ltag, channels.Length * 2);
        }
        else w[e + 6] = 0;
    }
    public void EventFrame(double ts, uint id, string name, float intensity, float duration, (float, float, float) dir)
    {
        w.Add(0);
        int body = Alloc(2);
        w[body] = (ulong)BitConverter.DoubleToInt64Bits(ts);
        int tag = Alloc(1 + 5); // 1 event: 2d + 3p
        w[body + 1] = StructListPtr(body + 1, tag, 5);
        w[tag] = Tag(1, 2, 3);
        int e = tag + 1;
        w[e] = id | (ulong)F32(intensity) << 32;
        w[e + 1] = F32(duration) | (1ul << 32); // direction union tag = 1
        int dirAt = Vec3(dir);
        int nameAt = Text(name);
        int objAt = Text("");
        w[e + 2] = TextPtr(e + 2, nameAt, Encoding.UTF8.GetByteCount(name) + 1); // slot0 name
        w[e + 3] = StructPtr(e + 3, dirAt, 2, 0);   // slot1 = direction
        w[e + 4] = TextPtr(e + 4, objAt, 1); // slot2 object ""
    }
    public int DiscoveryFrame(uint schemaVer, long sessionUs, string gameName,
        (float airD, float temp, float press, float grav, ushort medium) env,
        int objectCount)
    {
        w.Add(0);
        int body = Alloc(5);
        w[body] = schemaVer;
        w[body + 1] = (ulong)sessionUs;
        int g = Text(gameName);
        w[body + 2] = TextPtr(body + 2, g, Encoding.UTF8.GetByteCount(gameName) + 1);
        int envAt = Alloc(3);
        w[envAt] = F32(env.airD) | (ulong)F32(env.temp) << 32;
        w[envAt + 1] = F32(env.press) | (ulong)F32(env.grav) << 32;
        w[envAt + 2] = env.medium;
        w[body + 3] = StructPtr(body + 3, envAt, 3, 0);
        int tag = Alloc(1 + objectCount * 9);
        w[tag] = Tag(objectCount, 1, 8);
        w[body + 4] = StructListPtr(body + 4, tag, objectCount * 9);
        PatchRoot(2, 3);
        return tag;
    }
    public void Object(int tag, int index, string name, string location, string? typeLabel,
        string refName, (ushort type, bool primary) meta,
        (string name, string unit, double? min, double? max)[]? channels)
    {
        int o = tag + 1 + index * 9;
        w[o] = meta.type | (meta.primary ? 1ul << 16 : 0);
        int nameAt = Text(name);
        w[o + 1] = TextPtr(o + 1, nameAt, Encoding.UTF8.GetByteCount(name) + 1);
        int locAt = Text(location);
        w[o + 2] = TextPtr(o + 2, locAt, Encoding.UTF8.GetByteCount(location) + 1);
        if (typeLabel != null)
        {
            int tl = Text(typeLabel);
            w[o + 3] = TextPtr(o + 3, tl, Encoding.UTF8.GetByteCount(typeLabel) + 1);
        }
        else w[o + 3] = 0;
        int rnAt = Text(refName);
        int rpAt = Alloc(2);
        w[rpAt] = TextPtr(rpAt, rnAt, Encoding.UTF8.GetByteCount(refName) + 1);
        int rpPos = Vec3((0f, 0f, 0f));
        w[rpAt + 1] = StructPtr(rpAt + 1, rpPos, 2, 0);
        w[o + 4] = StructPtr(o + 4, rpAt, 0, 2);
        w[o + 5] = 0; w[o + 6] = 0; w[o + 8] = 0;
        if (channels != null)
        {
            int ltag = Alloc(1 + channels.Length * 5);
            w[ltag] = Tag(channels.Length, 1, 4);
            for (int i = 0; i < channels.Length; i++)
            {
                int c = ltag + 1 + i * 5;
                var ch = channels[i];
                w[c] = (uint)i | (ch.min.HasValue ? 1ul : 0xfffful) << 16;
            }
            for (int i = 0; i < channels.Length; i++)
            {
                int c = ltag + 1 + i * 5;
                var ch = channels[i];
                if (ch.min.HasValue)
                {
                    int r = Alloc(2);
                    w[r] = (ulong)BitConverter.DoubleToInt64Bits(ch.min.Value);
                    w[r + 1] = (ulong)BitConverter.DoubleToInt64Bits(ch.max!.Value);
                    w[c + 3] = StructPtr(c + 3, r, 2, 0);
                }
                int nAt = Text(ch.name);
                w[c + 1] = TextPtr(c + 1, nAt, Encoding.UTF8.GetByteCount(ch.name) + 1);
                int uAt = Text(ch.unit);
                w[c + 2] = TextPtr(c + 2, uAt, Encoding.UTF8.GetByteCount(ch.unit) + 1);
            }
            w[o + 7] = StructListPtr(o + 7, ltag, channels.Length * 5);
        }
        else w[o + 7] = 0;
    }
}
