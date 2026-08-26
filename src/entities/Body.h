#pragma once

#include "core/Config.h"
#include "raylib.h"

//----------------------------------------------------------------------------------
// Kinematic character body: gravity, acceleration, integration.
//
// The body knows nothing about the world - after Update() the Level applies
// collision response via Level::ResolveBody(). The player and every enemy that
// walks share this, so a change to movement feel lands everywhere at once.
//----------------------------------------------------------------------------------
struct Body
{
    Vector3 position = { 0.0f, 0.0f, 0.0f };
    Vector3 velocity = { 0.0f, 0.0f, 0.0f };
    Vector3 dir      = { 0.0f, 0.0f, 0.0f };  // Smoothed movement direction
    float   radius   = 0.4f;                  // Horizontal extent, used by collision
    float   maxSpeed = Config::MaxSpeed;      // Enemies run their own top speed
    bool    isGrounded = false;

    // yaw:  direction the body is facing, in radians
    // move: x = strafe (+ right), y = forward (+ forward), each in [-1, 1]
    void Update(float delta, float yaw, Vector2 move, bool jumpPressed, bool crouchHold);

    // Stops horizontal motion dead, leaving gravity to carry on.
    //
    // Simply passing no movement is not enough to stand still: `dir` is smoothed
    // toward the input rather than set from it, and Update accelerates along `dir`
    // whatever the input was, so a body that stops asking to move still drifts for
    // a few tenths of a second. That drift is a foot slide on anything playing an
    // animation that is supposed to be planted.
    void Halt()
    {
        velocity.x = 0.0f;
        velocity.z = 0.0f;
        dir = { 0.0f, 0.0f, 0.0f };
    }
};
