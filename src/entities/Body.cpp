#include "entities/Body.h"

#include "core/Config.h"
#include "raymath.h"

void Body::Update(float delta, float yaw, Vector2 move, bool jumpPressed, bool crouchHold)
{
    Vector2 input = { move.x, -move.y };

    if (Config::NormalizeDiagonalInput && (move.x != 0.0f) && (move.y != 0.0f)) input = Vector2Normalize(input);

    if (!isGrounded) velocity.y -= Config::Gravity*delta;

    if (isGrounded && jumpPressed)
    {
        velocity.y = Config::JumpForce;
        isGrounded = false;

        // TODO: jump sound goes here, once AssetManager holds the fx
    }

    const Vector3 front = { sinf(yaw), 0.0f, cosf(yaw) };
    const Vector3 right = { cosf(-yaw), 0.0f, sinf(-yaw) };

    const Vector3 desiredDir = { input.x*right.x + input.y*front.x, 0.0f, input.x*right.z + input.y*front.z };
    dir = Vector3Lerp(dir, desiredDir, Config::Control*delta);

    const float decel = (isGrounded ? Config::Friction : Config::AirDrag);
    Vector3 hvel = { velocity.x*decel, 0.0f, velocity.z*decel };

    if (Vector3Length(hvel) < (maxSpeed*0.01f)) hvel = { 0.0f, 0.0f, 0.0f };

    // This is what creates strafing
    const float speed = Vector3DotProduct(hvel, dir);

    // Whenever the amount of acceleration to add is clamped by the maximum acceleration constant,
    // a Player can make the speed faster by bringing the direction closer to horizontal velocity angle
    // More info here: https://youtu.be/v3zT3Z5apaM?t=165
    const float targetSpeed = (crouchHold ? Config::CrouchSpeed : maxSpeed);
    const float accel = Clamp(targetSpeed - speed, 0.0f, Config::MaxAccel*delta);
    hvel.x += dir.x*accel;
    hvel.z += dir.z*accel;

    velocity.x = hvel.x;
    velocity.z = hvel.z;

    position = Vector3Add(position, Vector3Scale(velocity, delta));
}
