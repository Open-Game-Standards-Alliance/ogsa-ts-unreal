// OgsaTelemetryFrame.h — Unreal Engine → OGSA-TS frame conversion.
//
// UE is left-handed, X-forward, Y-right, Z-up. OGSA-TS is left-handed,
// Z-forward, X-right, Y-up. The map is the axis permutation
//   (x, y, z)_std = (y, z, z->... )  — concretely: std.X = ue.Y,
//   std.Y = ue.Z, std.Z = ue.X  (a cyclic permutation, even => handedness
//   preserved, both left-handed).
// For orientation, that permutation is the rotation P with quaternion
// q_p = (x=0.5, y=0.5, z=0.5, w=0.5) (120° about (1,1,1)/√3), and the
// converted rotation is the similarity transform
//   q_std = q_p · q_ue · q_p⁻¹.
// Verified in Test/main.cpp: identity stays identity; a UE yaw about Z-up
// maps to a standard yaw about Y with equal angle and sign.
#pragma once
#include "OgsaTelemetryWire.h"

namespace ogsa::telemetry
{

inline Vec3 ToStandard(const Vec3& ue) { return { ue.Y, ue.Z, ue.X }; }

inline Quat Multiply(const Quat& a, const Quat& b)
{
    // (w1,v1)*(w2,v2) with Hamilton product
    float aw = a.W, bw = b.W;
    float ax = a.X, ay = a.Y, az = a.Z;
    float bx = b.X, by = b.Y, bz = b.Z;
    return {
        aw * bx + ax * bw + ay * bz - az * by,
        aw * by - ax * bz + ay * bw + az * bx,
        aw * bz + ax * by - ay * bx + az * bw,
        aw * bw - ax * bx - ay * by - az * bz,
    };
}

inline Quat ToStandard(const Quat& ue)
{
    static constexpr Quat Qp{ 0.5f, 0.5f, 0.5f, 0.5f };
    static constexpr Quat QpInv{ -0.5f, -0.5f, -0.5f, 0.5f };
    return Multiply(Multiply(Qp, ue), QpInv);
}

// rotate v by q (v' = q v q*)
inline Vec3 Rotate(const Quat& q, const Vec3& v)
{
    Quat p{ v.X, v.Y, v.Z, 0.0f };
    Quat qc{ -q.X, -q.Y, -q.Z, q.W };
    Quat r = Multiply(Multiply(q, p), qc);
    return { r.X, r.Y, r.Z };
}

} // namespace ogsa::telemetry
