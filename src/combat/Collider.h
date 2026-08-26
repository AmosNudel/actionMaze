#pragma once

#include "raylib.h"

//----------------------------------------------------------------------------------
// The one hit volume in the game: a capsule, being the segment a->b fattened by
// `radius`.
//
// Everything that can hit or be hit is expressed as one of these - a standing
// body, a blade mid-swing, an arrow's path across a substep - so there is exactly
// one intersection routine to get right and exactly one shape to draw in the
// debug overlay. A capsule is also the only primitive that sweeps for free: a
// moving sphere is a capsule, and a moving segment is close enough to one that
// the difference never shows at these speeds.
//
// A degenerate capsule (a == b) is a sphere, and every routine here handles it.
//----------------------------------------------------------------------------------
struct Capsule
{
    Vector3 a{};
    Vector3 b{};
    float radius = 0.0f;
};

// A standing body as combat sees it: a vertical capsule from the feet to
// `height`, with the caps tucked inside so the total extent is exactly `height`
// rather than height plus two radii. This is the same shape CombatDebug has
// always drawn for enemies, which is now not a coincidence: the overlay shows the
// volume that is actually tested.
Capsule BodyCapsule(Vector3 feet, float height, float radius);

// Shortest distance between two segments. The whole collision system rests on
// this, so it is the textbook clamped-parameter solve rather than anything
// clever: degenerate segments fall out as the a<=eps / e<=eps branches.
float SegmentDistance(Vector3 p1, Vector3 q1, Vector3 p2, Vector3 q2);

bool CapsulesOverlap(const Capsule &lhs, const Capsule &rhs);

// Where the two capsules touch, near enough - the midpoint of the closest
// approach. Used to place hit feedback where the blade actually met the body
// rather than at the enemy's centre.
Vector3 CapsuleContactPoint(const Capsule &lhs, const Capsule &rhs);

Capsule LerpCapsule(const Capsule &from, const Capsule &to, float t);

//----------------------------------------------------------------------------------
// A capsule that moved. Tests `steps` interpolated positions between where it was
// and where it is, because a blade at the fast part of a swing crosses more than
// its own width in a frame and a single test would pass straight through a body.
//
// This is a sampled sweep, not an analytic one. An analytic capsule-vs-capsule
// sweep is a quartic; sampling four positions costs four cheap segment solves,
// never misses at the speeds anything here actually moves, and can be read.
//----------------------------------------------------------------------------------
bool SweptCapsuleHits(const Capsule &from, const Capsule &to, const Capsule &target, int steps);
