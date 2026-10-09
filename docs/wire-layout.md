# OGSA-TS wire layout (as emitted by the reference stack)

Reverse-engineered from the conformance corpus; proven by
`tools/blueprint` (C# mirror, 6/6 byte-identical) before C++ transcription.
All words little-endian u64. Offsets are in words, relative to the end of
the pointer word (may be negative — truncate the shifted offset to u32,
never sign-extend into the upper half).

## Message framing

- Datagram = `OMT1` + frameType(u8) + 3 reserved bytes + message
- Message (1 segment) = `[u32 0][u32 segWords]` + words — table is an even u32 count (canonical; 1 segment = exactly 1 word, no pad)

## Pointer encoding

| kind | value |
|---|---|
| struct | `(u32)(off << 2) \| dataSize << 32 \| ptrCount << 48` (kind bits 00) |
| list (fixed) | `1 \| (u32)(off << 2) \| ((count << 3) \| sizeCode) << 32` |
| list (composite) | `1 \| (u32)(off << 2) \| (elementWords << 3 \| 7) << 32` — elementWords excludes the tag |
| composite tag | `elemCount << 2 \| dataW << 32 \| ptrW << 48` |
| text | byte list (sizeCode 2), content + NUL, count includes NUL |
| NULL | 0 |

## Struct layouts (data words | pointer slots)

- **DiscoveryFrame** `[schemaVersion u16 | sessionStartUnixUs i64] | [gameName, environment, objects]`
- **Environment** `[airDensity f32, temperature f32][pressure f32, gravity f32][medium u16]`
- **ObjectDescriptor** `[type u16, primary bit@16] | [name, location, typeLabel, referencePoint, namedPoints, drivePoints, channels, values]`
- **NamedPoint** `[] | [name, position]` (position = pointer to 2w Vector3)
- **ChannelDescriptor** `[id u16, rangeTag u16 (1 = range set, ffff = unset)] | [name, unit, range, description]`
- **Range** `[min f64][max f64]`
- **MotionFrame** `[timestamp f64] | [objects]`
- **MotionObject** `[dynamicsTag u16 = ffff] | [name, position, orientation, NULL, NULL, channels]`
- **ChannelValue** `[id u16, valueTag u16 (0 number, 1 boolean), f32/bool @byte4] | [text (NULL unless text variant)]`
- **EventFrame** `[timestamp f64] | [events]`
- **Event** `[id u32, intensity f32][duration f32, dirTag u16 (1 = direction, ffff = none)] | [name, direction, object]`
- **Vector3** `[x f32, y f32][z f32, pad]` — **Quaternion** `[x, y][z, w]`

## Allocation order (bodies appended sequentially, breadth-first)

- DiscoveryFrame: gameName, environment, objects(tag + elements), then
  per-object children: name, location, typeLabel, refPoint.name,
  refPoint-slots, refPoint.position, channels-list(tag + elements), then
  per-channel children: **range, name, unit** (range first!)
- MotionFrame: objects(tag + elements), then per-object: name, position,
  orientation, channels(tag + elements)
- EventFrame: events(tag + elements), then per-event: **direction, name,
  object** (direction first!)

## Quirks (mirrored deliberately)

1. Struct-typed fields (Vector3/Quaternion/NamedPoint/Range) are encoded
   as **pointers**, not inline, by the reference stack (Capnp.Net). This
   deviates from canonical capnp C++ inline-struct layout — cross-library
   compatibility is an open question (KB-tracked).
2. Padding bytes of truncated structs can carry allocator state:
   motion-pose's Vector3 pad is `ffffffff`; others are zero. Receivers
   must ignore pads; emitters matching the corpus mirror them.
3. Unset unions carry discriminant `0xffff`.
4. Enum ordinals matter: `ObjectType.vehicle = 0`, `ContactMedium.asphalt = 1`.
