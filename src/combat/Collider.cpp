#include "combat/Collider.h"

#include "raymath.h"

namespace
{
    constexpr float Eps = 1e-6f;
}

Capsule BodyCapsule(Vector3 feet, float height, float radius)
{
    // A body shorter than it is wide would otherwise invert its own segment
    const float half = height*0.5f;
    const float cap = (radius < half) ? radius : half;

    Capsule capsule;
    capsule.a = { feet.x, feet.y + cap, feet.z };
    capsule.b = { feet.x, feet.y + height - cap, feet.z };
    capsule.radius = radius;

    return capsule;
}

float SegmentDistance(Vector3 p1, Vector3 q1, Vector3 p2, Vector3 q2)
{
    const Vector3 d1 = Vector3Subtract(q1, p1);
    const Vector3 d2 = Vector3Subtract(q2, p2);
    const Vector3 r  = Vector3Subtract(p1, p2);

    const float a = Vector3DotProduct(d1, d1);   // Squared length of segment 1
    const float e = Vector3DotProduct(d2, d2);   // Squared length of segment 2
    const float f = Vector3DotProduct(d2, r);

    float s = 0.0f;
    float t = 0.0f;

    if ((a <= Eps) && (e <= Eps)) return Vector3Length(r);   // Both are points

    if (a <= Eps)
    {
        t = Clamp(f/e, 0.0f, 1.0f);              // First is a point
    }
    else
    {
        const float c = Vector3DotProduct(d1, r);

        if (e <= Eps)
        {
            s = Clamp(-c/a, 0.0f, 1.0f);         // Second is a point
        }
        else
        {
            const float b = Vector3DotProduct(d1, d2);
            const float denom = a*e - b*b;       // Zero when the two are parallel

            s = (denom > Eps) ? Clamp((b*f - c*e)/denom, 0.0f, 1.0f) : 0.0f;
            t = (b*s + f)/e;

            // t fell off the end of segment 2, so pin it and re-solve for s
            if (t < 0.0f)
            {
                t = 0.0f;
                s = Clamp(-c/a, 0.0f, 1.0f);
            }
            else if (t > 1.0f)
            {
                t = 1.0f;
                s = Clamp((b - c)/a, 0.0f, 1.0f);
            }
        }
    }

    const Vector3 c1 = Vector3Add(p1, Vector3Scale(d1, s));
    const Vector3 c2 = Vector3Add(p2, Vector3Scale(d2, t));

    return Vector3Distance(c1, c2);
}

bool CapsulesOverlap(const Capsule &lhs, const Capsule &rhs)
{
    const float gap = SegmentDistance(lhs.a, lhs.b, rhs.a, rhs.b);

    return gap <= (lhs.radius + rhs.radius);
}

Vector3 CapsuleContactPoint(const Capsule &lhs, const Capsule &rhs)
{
    // Good enough for feedback: the middle of the two spines' closest approach,
    // which sits inside both bodies whenever they actually overlap
    const Vector3 midLhs = Vector3Lerp(lhs.a, lhs.b, 0.5f);
    const Vector3 midRhs = Vector3Lerp(rhs.a, rhs.b, 0.5f);

    return Vector3Lerp(midLhs, midRhs, 0.5f);
}

Capsule LerpCapsule(const Capsule &from, const Capsule &to, float t)
{
    Capsule out;
    out.a = Vector3Lerp(from.a, to.a, t);
    out.b = Vector3Lerp(from.b, to.b, t);
    out.radius = from.radius + (to.radius - from.radius)*t;

    return out;
}

bool SweptCapsuleHits(const Capsule &from, const Capsule &to, const Capsule &target, int steps)
{
    if (steps < 1) steps = 1;

    // Starts at 1 and ends at `steps`: position 0 is where the blade was last
    // frame, which was already tested as that frame's end position
    for (int i = 1; i <= steps; i++)
    {
        if (CapsulesOverlap(LerpCapsule(from, to, (float)i/steps), target)) return true;
    }

    return false;
}
