#include "render/FpsCamera.h"

#include "raymath.h"

namespace
{
    // A fresh pick every call rather than a smoothed noise function: a shake this
    // short is meant to read as a shock, not a sway, and a random jitter each
    // frame is exactly that. -amount..+amount.
    float RandomSigned(float amount)
    {
        return (GetRandomValue(-1000, 1000)/1000.0f)*amount;
    }
}

FpsCamera::FpsCamera()
{
    camera.fovy = Config::FovDefault;
    camera.projection = CAMERA_PERSPECTIVE;
}

void FpsCamera::ApplyLookInput(Vector2 mouseDelta)
{
    lookRotation.x -= mouseDelta.x*Config::MouseSensitivityX;
    lookRotation.y += mouseDelta.y*Config::MouseSensitivityY;
}

void FpsCamera::SnapTo(Vector3 bodyPosition)
{
    headLerp = Config::StandHeight;
    walkLerp = 0.0f;
    headTimer = 0.0f;
    lean = { 0.0f, 0.0f };

    camera.position = { bodyPosition.x, bodyPosition.y + (Config::BottomHeight + headLerp), bodyPosition.z };
    UpdateView();
}

void FpsCamera::Update(float delta, Vector3 bodyPosition, Vector2 move, bool crouching, bool grounded)
{
    headLerp = Lerp(headLerp, (crouching ? Config::CrouchHeight : Config::StandHeight), Config::CrouchLerpSpeed*delta);

    camera.position = { bodyPosition.x, bodyPosition.y + (Config::BottomHeight + headLerp), bodyPosition.z };

    if (grounded && ((move.x != 0.0f) || (move.y != 0.0f)))
    {
        headTimer += delta*Config::HeadBobSpeed;
        walkLerp = Lerp(walkLerp, 1.0f, Config::WalkLerpSpeed*delta);
        camera.fovy = Lerp(camera.fovy, Config::FovWalk, Config::FovLerpSpeed*delta);
    }
    else
    {
        walkLerp = Lerp(walkLerp, 0.0f, Config::WalkLerpSpeed*delta);
        camera.fovy = Lerp(camera.fovy, Config::FovDefault, Config::FovLerpSpeed*delta);
    }

    lean.x = Lerp(lean.x, move.x*Config::LeanStrafe, Config::LeanLerpSpeed*delta);
    lean.y = Lerp(lean.y, move.y*Config::LeanForward, Config::LeanLerpSpeed*delta);

    UpdateView();

    //------------------------------------------------------------------------------
    // The shake, applied last and never stored back into position or rotation -
    // UpdateView rebuilds both from scratch every frame, so trauma decaying to
    // zero over a couple of frames undoes itself automatically rather than
    // needing to be subtracted back out.
    //------------------------------------------------------------------------------
    shakeTrauma -= Config::CameraShakeDecay*delta;
    if (shakeTrauma < 0.0f) shakeTrauma = 0.0f;
    if (shakeTrauma > 1.0f) shakeTrauma = 1.0f;

    if (shakeTrauma > 0.0f)
    {
        // Squared, so a little trauma is barely felt and a lot of it is genuinely
        // rough - see the note on Config::CameraShakeDecay.
        const float power = shakeTrauma*shakeTrauma;
        const float offset = Config::CameraShakeMaxOffset*power;

        const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
        const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));

        const Vector3 kick = Vector3Add(Vector3Scale(right, RandomSigned(offset)),
                                        Vector3Scale(camera.up, RandomSigned(offset)));

        camera.position = Vector3Add(camera.position, kick);
        camera.target = Vector3Add(camera.target, kick);
    }
}

void FpsCamera::Shake(float trauma)
{
    shakeTrauma += trauma;
    if (shakeTrauma > 1.0f) shakeTrauma = 1.0f;
}

void FpsCamera::BeginDeathFall()
{
    deathPosition = camera.position;
    deathTarget = camera.target;
    deathUp = camera.up;
}

void FpsCamera::UpdateDeathFall(float elapsed)
{
    // Normalised against the whole dying beat - fall and delay and fade together
    // - so the keel-over is still gaining ground as the screen goes black rather
    // than having already settled with the fade barely started.
    float t = elapsed/(Config::DeathFallToFadeDelay + Config::DeathFadeDuration);
    if (t > 1.0f) t = 1.0f;

    // Eased in: a body does not keel over at constant speed, it starts slow and
    // goes
    const float ease = t*t;

    const Vector3 forward = Vector3Normalize(Vector3Subtract(deathTarget, deathPosition));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, deathUp));

    const Vector3 drop = { 0.0f, -Config::DeathFallDrop*ease, 0.0f };
    const Vector3 sway = Vector3Scale(right, Config::DeathFallSway*ease);
    const Vector3 offset = Vector3Add(drop, sway);

    camera.position = Vector3Add(deathPosition, offset);
    camera.target = Vector3Add(deathTarget, offset);

    // Rolled around the view's own forward axis, so it reads as the head tipping
    // toward one shoulder rather than the world tilting under it
    camera.up = Vector3RotateByAxisAngle(deathUp, forward, Config::DeathFallTilt*DEG2RAD*ease);
}

Vector3 FpsCamera::Forward() const
{
    return Vector3Normalize(Vector3Subtract(camera.target, camera.position));
}

Ray FpsCamera::AimRay() const
{
    Ray ray = { 0 };
    ray.position = camera.position;
    ray.direction = Forward();

    return ray;
}

// Rebuild target and up from the accumulated look rotation, plus head animation
void FpsCamera::UpdateView()
{
    const Vector3 up = { 0.0f, 1.0f, 0.0f };
    const Vector3 targetOffset = { 0.0f, 0.0f, -1.0f };

    // Left and right
    Vector3 yaw = Vector3RotateByAxisAngle(targetOffset, up, lookRotation.x);

    // Clamp view up
    float maxAngleUp = Vector3Angle(up, yaw);
    maxAngleUp -= 0.001f;   // Avoid numerical errors
    if (-(lookRotation.y) > maxAngleUp) lookRotation.y = -maxAngleUp;

    // Clamp view down
    float maxAngleDown = Vector3Angle(Vector3Negate(up), yaw);
    maxAngleDown *= -1.0f;  // Downwards angle is negative
    maxAngleDown += 0.001f; // Avoid numerical errors
    if (-(lookRotation.y) < maxAngleDown) lookRotation.y = -maxAngleDown;

    // Up and down
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(yaw, up));

    // Rotate view vector around right axis
    float pitchAngle = -lookRotation.y - lean.y;
    pitchAngle = Clamp(pitchAngle, -PI/2 + 0.0001f, PI/2 - 0.0001f);    // Don't go past straight up or down
    const Vector3 pitch = Vector3RotateByAxisAngle(yaw, right, pitchAngle);

    // Head animation: rotate up direction around forward axis
    const float headSin = sinf(headTimer*PI);
    const float headCos = cosf(headTimer*PI);
    camera.up = Vector3RotateByAxisAngle(up, pitch, headSin*Config::StepRotation + lean.x);

    // Camera bob
    Vector3 bobbing = Vector3Scale(right, headSin*Config::BobSide);
    bobbing.y = fabsf(headCos*Config::BobUp);

    camera.position = Vector3Add(camera.position, Vector3Scale(bobbing, walkLerp));
    camera.target = Vector3Add(camera.position, pitch);
}
