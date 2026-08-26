#include "render/FpsCamera.h"

#include "raymath.h"

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
