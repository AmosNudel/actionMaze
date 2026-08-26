#pragma once

#include "raylib.h"

//----------------------------------------------------------------------------------
// A standing column of light, drawn out of the shared soft round glow.
//
// The way down is one. So is every event marker waiting to be walked into. They are
// the same object in different colours at different sizes, and the whole reason this
// is a function rather than two copies of the same loop is that the player learns
// what a pillar of light MEANS once - a thing to walk into - and every one of them
// after that has to keep the promise.
//
// Three parts, all additive, none of them writing depth: a pool on the floor, a
// stack of billboards up the axis, and a ring of motes turning about it. The pool is
// the part that does the most work and the easiest to leave out - without it the
// column is a bright object hanging in a room, and with it the light has somewhere
// to land.
//
// Call inside BeginMode3D. It sets and restores the blend mode and the depth mask
// itself, so callers do not have to know it is additive.
//----------------------------------------------------------------------------------
struct BeamLook
{
    Color colour = WHITE;
    float radius = 1.0f;
    float height = 3.0f;

    // 0 draws nothing, 1 is fully up. Everything scales off it, so a fade is one
    // number rather than a rule per part.
    float lit = 1.0f;

    // Turns forever, and is the caller's to advance. Kept out here because a beam
    // that starts from zero every time it is drawn reads as a machine starting up.
    float spin = 0.0f;

    int motes = 7;              // Circling the column. Zero for a still beam.
    float moteSize = 0.30f;

    // How strongly the column and the pool are added. The column is the dimmer of
    // the two by a distance - it is a haze the pool is seen through.
    int columnAlpha = 120;
    int poolAlpha = 220;
};

void DrawBeam(const Camera3D &camera, const Texture2D &glow, Vector3 at, const BeamLook &look);
