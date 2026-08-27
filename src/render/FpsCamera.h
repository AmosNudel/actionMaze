#pragma once

#include "core/Config.h"
#include "raylib.h"

//----------------------------------------------------------------------------------
// First person camera: mouse look, crouch height, head bob and lean.
//
// It owns the view only - it never moves the body. Feed it the body position
// each frame and read AimRay() back for melee and ranged hit tests.
//----------------------------------------------------------------------------------
class FpsCamera
{
public:
    FpsCamera();

    // Accumulate mouse look. Call before the body updates, so the body turns
    // toward where the player is already looking this frame.
    void ApplyLookInput(Vector2 mouseDelta);

    // Place the camera without any smoothing (spawn, teleport, level change)
    void SnapTo(Vector3 bodyPosition);

    void Update(float delta, Vector3 bodyPosition, Vector2 move, bool crouching, bool grounded);

    //------------------------------------------------------------------------------
    // A jolt to the view: a blow landing, a crit going out. Adds to `trauma`
    // rather than setting it, so a second hit a frame after the first makes the
    // shake worse instead of just restarting it - see Config::CameraShakeDecay.
    //
    // Purely cosmetic and applied AFTER UpdateView has already placed the camera
    // from the body and the look rotation, so it never touches `lookRotation` -
    // the aim the player is actually tracking is untouched by how hard the
    // screen happens to be shaking.
    //------------------------------------------------------------------------------
    void Shake(float trauma);

    const Camera3D &Get() const { return camera; }
    float Yaw() const { return lookRotation.x; }

    // Walk cycle, for anything that needs to sway in step with the head
    float BobPhase() const { return headTimer; }
    float WalkAmount() const { return walkLerp; }
    Vector3 Forward() const;
    Ray AimRay() const;

private:
    void UpdateView();

    Camera3D camera{};
    Vector2 lookRotation = { 0.0f, 0.0f };
    Vector2 lean = { 0.0f, 0.0f };
    float headTimer = 0.0f;
    float walkLerp = 0.0f;
    float headLerp = Config::StandHeight;

    // How rough the view is right now, 0 (still) to 1 (as rough as it gets) -
    // see Shake and Config::CameraShakeDecay.
    float shakeTrauma = 0.0f;
};
