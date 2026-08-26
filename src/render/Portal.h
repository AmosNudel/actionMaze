#pragma once

#include "raylib.h"

class AssetManager;

//----------------------------------------------------------------------------------
// The way down, drawn as light.
//
// No model and no sprite sheet, deliberately. The dungeon pack has no portal in it,
// and the one piece of portal art to hand is a 32x32 six-frame strip - pixel art,
// which in a room built out of five-thousand-triangle brickwork reads as a mistake
// rather than as a style. So it is built out of the same soft round light the magic
// motes are made of: a standing column of glow, a ring of motes turning about it,
// and a pool on the floor underneath.
//
// That also means it costs nothing to ship, has no licence attached, and looks the
// same at any resolution.
//
// It is only ever in one of two states and the transition between them matters more
// than either: SHUT, which draws nothing at all, and OPEN, which fades up over a
// second or so. A portal that snapped into existence at the far end of the map would
// be a thing the player only ever finds already there.
//----------------------------------------------------------------------------------
class Portal
{
public:
    void Load(AssetManager &assets);

    // Where it stands, on the floor. Set once per level, from Level::PortalPoint.
    void PlaceAt(Vector3 at);

    // Shut again, and instantly - what a new floor calls. Distinct from Open(false)
    // for exactly one reason: a rebuilt floor must not fade its portal out, because
    // there is nothing there to fade.
    void Reset();

    // Raise it. Idempotent: calling it every frame while the floor is cleared is
    // the intended usage, and only the first call starts the fade.
    void Open();

    void Update(float delta);

    // Inside BeginMode3D, after the world. Additive and depth-tested, so it is
    // hidden by the walls between the player and it but glows over everything in
    // the room it stands in.
    void Draw(const Camera3D &camera) const;

    bool IsOpen() const { return open; }

    // Is `point` inside the pillar of light? Tested as a vertical cylinder about
    // the portal's own axis, ignoring height - a portal you can miss by jumping is
    // a portal with a bug in it.
    bool Contains(Vector3 point) const;

private:
    Texture2D *glow = nullptr;      // Shared, owned by the AssetManager

    Vector3 at{};
    bool open = false;

    // 0 shut, 1 fully up. Everything drawn scales off it, so the fade is one
    // number rather than a rule per element.
    float lit = 0.0f;

    // Turns forever, whether the portal is up or not. Kept running rather than
    // reset on open so that the ring is never caught at the same angle twice - and
    // because a rotation that starts from zero every time reads as a machine
    // starting up, which this is not.
    float spin = 0.0f;
};
