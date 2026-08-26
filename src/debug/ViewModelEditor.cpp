#include "debug/ViewModelEditor.h"

#include "raymath.h"
#include "render/ViewModel.h"
#include "rlgl.h"

#include <cstdio>
#include <ctime>

namespace
{
    constexpr float GizmoLength    = 0.22f;     // World units - the weapon sits ~0.5 from the eye
    constexpr float HandleRadius   = 0.035f;
    constexpr int   RingSegments   = 48;
    constexpr float RingPickPixels = 12.0f;     // How near the mouse must be to grab a ring
    constexpr float ScaleStep      = 0.01f;
    constexpr char  DumpFile[]     = "viewmodel_poses.txt";

    const Color AxisColor[3] = { RED, GREEN, BLUE };

    // Closest point on the line (origin + t*axis) to the given ray
    bool ClosestPointOnAxis(Ray ray, Vector3 origin, Vector3 axis, float *t)
    {
        const Vector3 w0 = Vector3Subtract(origin, ray.position);
        const float b = Vector3DotProduct(axis, ray.direction);
        const float denom = 1.0f - b*b;

        if (fabsf(denom) < 1e-5f) return false;     // Looking straight down the axis

        const float d = Vector3DotProduct(axis, w0);
        const float e = Vector3DotProduct(ray.direction, w0);
        *t = (b*e - d)/denom;

        return true;
    }

    // Two unit vectors spanning the plane with this normal
    void PlaneBasis(Vector3 normal, Vector3 *basisX, Vector3 *basisY)
    {
        const Vector3 reference = (fabsf(normal.y) > 0.9f) ? Vector3{ 1.0f, 0.0f, 0.0f }
                                                           : Vector3{ 0.0f, 1.0f, 0.0f };
        *basisX = Vector3Normalize(Vector3CrossProduct(normal, reference));
        *basisY = Vector3CrossProduct(normal, *basisX);
    }

    // Screen space angle of a point about a centre, counter clockwise positive.
    // Screen y grows downward, hence the negation.
    float ScreenAngle(Vector2 point, Vector2 center)
    {
        return atan2f(-(point.y - center.y), point.x - center.x)*RAD2DEG;
    }

    float WrapDegrees(float angle)
    {
        while (angle > 180.0f) angle -= 360.0f;
        while (angle < -180.0f) angle += 360.0f;

        return angle;
    }

    float DistanceToSegment(Vector2 point, Vector2 a, Vector2 b)
    {
        const Vector2 ab = { b.x - a.x, b.y - a.y };
        const float lengthSq = ab.x*ab.x + ab.y*ab.y;

        if (lengthSq < 1e-6f) return Vector2Distance(point, a);

        float t = ((point.x - a.x)*ab.x + (point.y - a.y)*ab.y)/lengthSq;
        t = Clamp(t, 0.0f, 1.0f);

        const Vector2 closest = { a.x + ab.x*t, a.y + ab.y*t };

        return Vector2Distance(point, closest);
    }

    // Rotation that takes DrawCircle3D's default plane (normal +Z) onto `normal`
    void CircleOrientation(Vector3 normal, Vector3 *axis, float *angleDeg)
    {
        const Vector3 zAxis = { 0.0f, 0.0f, 1.0f };
        Vector3 cross = Vector3CrossProduct(zAxis, normal);

        if (Vector3Length(cross) < 1e-5f)
        {
            *axis = { 1.0f, 0.0f, 0.0f };
            *angleDeg = (Vector3DotProduct(zAxis, normal) > 0.0f) ? 0.0f : 180.0f;
            return;
        }

        *axis = Vector3Normalize(cross);
        *angleDeg = acosf(Clamp(Vector3DotProduct(zAxis, normal), -1.0f, 1.0f))*RAD2DEG;
    }
}

// Gizmo axes are the camera's own basis, because the pose is in camera space.
// Translate drives right/up/forward; rotate drives pitch/yaw/roll, whose third
// axis is camera space +Z - the opposite of the way the camera looks.
void ViewModelEditor::Axes(const Camera3D &camera, Vector3 out[3]) const
{
    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    const Vector3 up = Vector3CrossProduct(right, forward);

    out[0] = right;
    out[1] = up;
    out[2] = (mode == Mode::Rotate) ? Vector3Negate(forward) : forward;
}

void ViewModelEditor::SetMode(Mode next)
{
    mode = next;
    draggedAxis = -1;
    hoveredAxis = -1;

    if (mode == Mode::Off)
    {
        DisableCursor();
        lookCooldown = 2;
    }
    else EnableCursor();
}

void ViewModelEditor::Update(ViewModel &viewModel, const Camera3D &camera)
{
    if (lookCooldown > 0) lookCooldown--;

    if (IsKeyPressed(KEY_T)) SetMode((mode == Mode::Translate) ? Mode::Off : Mode::Translate);
    if (IsKeyPressed(KEY_Y)) SetMode((mode == Mode::Rotate) ? Mode::Off : Mode::Rotate);
    if (IsKeyPressed(KEY_U)) DumpPoses(viewModel);

    if (mode == Mode::Off) return;

    if (IsKeyPressed(KEY_H))
    {
        editHand = (editHand == Hand::Right) ? Hand::Left : Hand::Right;
        draggedAxis = -1;
    }

    if (IsKeyPressed(KEY_G))
    {
        editSlot = (editSlot == PoseSlot::Rest) ? PoseSlot::End : PoseSlot::Rest;
        draggedAxis = -1;
    }

    if (!viewModel.HasWeapon(editHand)) return;

    // Worked on by value and written back at the end
    ViewModelPose pose = viewModel.PoseFor(editHand, editSlot);

    if (IsKeyPressed(KEY_LEFT_BRACKET)) pose.scale = fmaxf(0.01f, pose.scale - ScaleStep);
    if (IsKeyPressed(KEY_RIGHT_BRACKET)) pose.scale += ScaleStep;

    Vector3 axes[3];
    Axes(camera, axes);

    const Vector3 anchor = viewModel.AnchorPoint(editHand, editSlot, camera);
    const Ray ray = GetScreenToWorldRay(GetMousePosition(), camera);

    // Grab
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        draggedAxis = PickAxis(viewModel, camera);

        if (draggedAxis >= 0)
        {
            grabAnchor = anchor;
            grabAxis = axes[draggedAxis];

            if (mode == Mode::Translate)
            {
                float t = 0.0f;
                ClosestPointOnAxis(ray, grabAnchor, grabAxis, &t);
                grabParam = t;
                grabValue = (draggedAxis == 0) ? pose.right : ((draggedAxis == 1) ? pose.up : pose.forward);
            }
            else
            {
                grabParam = ScreenAngle(GetMousePosition(), GetWorldToScreen(grabAnchor, camera));

                // Which way a screen space drag turns the ring depends on the
                // side it is seen from. The left hand sits on the far side of
                // the view axis from the right, so without this its rings turn
                // backwards - which is what made them feel broken.
                const Vector3 toAnchor = Vector3Normalize(Vector3Subtract(grabAnchor, camera.position));
                grabSign = (Vector3DotProduct(grabAxis, toAnchor) < 0.0f) ? 1.0f : -1.0f;

                grabValue = (draggedAxis == 0) ? pose.pitch : ((draggedAxis == 1) ? pose.yaw : pose.roll);
            }
        }
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) draggedAxis = -1;

    // Drag
    if ((draggedAxis >= 0) && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        if (mode == Mode::Translate)
        {
            float t = 0.0f;
            if (ClosestPointOnAxis(ray, grabAnchor, grabAxis, &t))
            {
                const float value = grabValue + (t - grabParam);

                if (draggedAxis == 0) pose.right = value;
                else if (draggedAxis == 1) pose.up = value;
                else pose.forward = value;
            }
        }
        else
        {
            const Vector2 anchorScreen = GetWorldToScreen(grabAnchor, camera);

            // Keep the drag continuous as it crosses the atan2 seam
            const float delta = WrapDegrees(ScreenAngle(GetMousePosition(), anchorScreen) - grabParam);
            const float value = grabValue + delta*grabSign;

            if (draggedAxis == 0) pose.pitch = value;
            else if (draggedAxis == 1) pose.yaw = value;
            else pose.roll = value;
        }
    }
    else hoveredAxis = PickAxis(viewModel, camera);

    viewModel.SetPoseFor(editHand, editSlot, pose);
}

// Returns 0/1/2 for the axis under the mouse, or -1
int ViewModelEditor::PickAxis(const ViewModel &viewModel, const Camera3D &camera) const
{
    Vector3 axes[3];
    Axes(camera, axes);

    const Vector3 anchor = viewModel.AnchorPoint(editHand, editSlot, camera);
    const Ray ray = GetScreenToWorldRay(GetMousePosition(), camera);
    const Vector2 mouse = GetMousePosition();
    const Vector3 viewDir = Vector3Normalize(Vector3Subtract(camera.target, camera.position));

    int best = -1;
    float bestDistance = 0.0f;      // World units for arrows, pixels for rings

    for (int i = 0; i < 3; i++)
    {
        if (mode == Mode::Translate)
        {
            const Vector3 tip = Vector3Add(anchor, Vector3Scale(axes[i], GizmoLength));
            const RayCollision hit = GetRayCollisionSphere(ray, tip, HandleRadius);

            if (hit.hit && ((best < 0) || (hit.distance < bestDistance)))
            {
                best = i;
                bestDistance = hit.distance;
            }
        }
        else
        {
            // Rings are picked in screen space, against the shape actually drawn.
            // Intersecting the mouse ray with the ring's plane fails outright
            // when the ring is seen edge on, which the yaw ring usually is.
            Vector3 basisX{}, basisY{};
            PlaneBasis(axes[i], &basisX, &basisY);

            float closest = -1.0f;
            Vector2 previous{};
            bool havePrevious = false;

            for (int s = 0; s <= RingSegments; s++)
            {
                const float angle = (2.0f*PI*s)/RingSegments;
                const Vector3 offset = Vector3Add(Vector3Scale(basisX, cosf(angle)*GizmoLength),
                                                  Vector3Scale(basisY, sinf(angle)*GizmoLength));
                const Vector3 point = Vector3Add(anchor, offset);

                // Anything behind the eye does not project sensibly
                if (Vector3DotProduct(Vector3Subtract(point, camera.position), viewDir) <= 0.0f)
                {
                    havePrevious = false;
                    continue;
                }

                const Vector2 screen = GetWorldToScreen(point, camera);

                if (havePrevious)
                {
                    const float distance = DistanceToSegment(mouse, previous, screen);
                    if ((closest < 0.0f) || (distance < closest)) closest = distance;
                }

                previous = screen;
                havePrevious = true;
            }

            if ((closest >= 0.0f) && (closest < RingPickPixels) && ((best < 0) || (closest < bestDistance)))
            {
                best = i;
                bestDistance = closest;
            }
        }
    }

    return best;
}

void ViewModelEditor::Draw(ViewModel &viewModel, const Camera3D &camera) const
{
    if (mode == Mode::Off) return;

    // A ghost of where each attack finishes. This is the thing to drag when the
    // gizmo is on the end pose - the solid weapon stays at rest behind it.
    for (int h = 0; h < 2; h++)
    {
        const Hand hand = (Hand)h;
        if (!viewModel.HasWeapon(hand)) continue;

        const bool editing = (hand == editHand) && (editSlot == PoseSlot::End);
        viewModel.DrawPosePreview(hand, PoseSlot::End, camera, Fade(editing ? YELLOW : SKYBLUE, 0.45f));
    }

    if (!viewModel.HasWeapon(editHand)) return;

    Vector3 axes[3];
    Axes(camera, axes);

    const Vector3 anchor = viewModel.AnchorPoint(editHand, editSlot, camera);
    const int highlighted = (draggedAxis >= 0) ? draggedAxis : hoveredAxis;

    rlDrawRenderBatchActive();
    rlDisableDepthTest();       // The gizmo is a tool: never let the weapon hide it

    for (int i = 0; i < 3; i++)
    {
        const Color color = (i == highlighted) ? YELLOW : AxisColor[i];

        if (mode == Mode::Translate)
        {
            const Vector3 tip = Vector3Add(anchor, Vector3Scale(axes[i], GizmoLength));

            DrawLine3D(anchor, tip, color);
            DrawSphere(tip, HandleRadius, color);
        }
        else
        {
            Vector3 circleAxis{};
            float circleAngle = 0.0f;
            CircleOrientation(axes[i], &circleAxis, &circleAngle);

            DrawCircle3D(anchor, GizmoLength, circleAxis, circleAngle, color);
            DrawCircle3D(anchor, GizmoLength - 0.004f, circleAxis, circleAngle, color);
            DrawCircle3D(anchor, GizmoLength + 0.004f, circleAxis, circleAngle, color);
        }
    }

    DrawSphere(anchor, 0.012f, WHITE);

    rlDrawRenderBatchActive();
    rlEnableDepthTest();
}

void ViewModelEditor::DrawUi(const ViewModel &viewModel) const
{
    if (mode == Mode::Off) return;

    const char *title = (mode == Mode::Translate) ? "POSITION GIZMO  (T)" : "ROTATION GIZMO  (Y)";
    const char *labels = (mode == Mode::Translate) ? "red right   green up   blue forward"
                                                   : "red pitch   green yaw   blue roll";
    const char *handName = (editHand == Hand::Right) ? "RIGHT" : "LEFT";
    const bool onEnd = (editSlot == PoseSlot::End);

    const int width = 300;
    const int height = 122;
    const int x = GetScreenWidth() - width - 10;
    const int y = 10;

    DrawRectangle(x, y, width, height, Fade(BLACK, 0.6f));
    DrawRectangleLines(x, y, width, height, YELLOW);

    DrawText(title, x + 10, y + 8, 10, YELLOW);
    DrawText(labels, x + 10, y + 24, 10, RAYWHITE);
    DrawText(TextFormat("editing: %s hand - %s", handName, viewModel.NameFor(editHand)),
             x + 10, y + 40, 10, viewModel.HasWeapon(editHand) ? RAYWHITE : GRAY);
    DrawText(TextFormat("pose: %s   attack: %s", onEnd ? "END (the ghost)" : "REST",
                        AttackStyleName(viewModel.StyleFor(editHand))),
             x + 10, y + 56, 10, onEnd ? YELLOW : RAYWHITE);
    DrawText("G rest/end   H hand   wheel equips", x + 10, y + 72, 10, GRAY);
    DrawText(TextFormat("[ ] scale   U saves (%i)", savedCount), x + 10, y + 88, 10, GRAY);
    DrawText("attacks are suppressed while a gizmo is up", x + 10, y + 104, 10, GRAY);
}

// Appends the whole table so nothing tuned earlier in the session is lost
void ViewModelEditor::DumpPoses(const ViewModel &viewModel)
{
    FILE *file = fopen(DumpFile, "a");

    if (file == NULL)
    {
        TraceLog(LOG_WARNING, "VIEWMODELEDITOR: Could not open %s for writing", DumpFile);
        return;
    }

    const time_t now = time(NULL);
    char stamp[32] = { 0 };
    strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    fprintf(file, "\n// ---- viewmodel poses, %s ----\n", stamp);
    fprintf(file, "// Rest block goes in TunedRestPoses, end block in TunedEndPoses\n");
    fprintf(file, "// (src/render/ViewModel.cpp)\n");

    const Hand hands[2] = { Hand::Right, Hand::Left };
    const char *handNames[2] = { "Hand::Right", "Hand::Left " };
    const PoseSlot slotOrder[2] = { PoseSlot::Rest, PoseSlot::End };
    const char *slotNames[2] = { "rest", "end" };

    for (int s = 0; s < 2; s++)
    {
        fprintf(file, "\n// -- %s poses --\n", slotNames[s]);

        for (int h = 0; h < 2; h++)
        {
            for (int i = 0; i < viewModel.Count(); i++)
            {
                const ViewModelPose &pose = viewModel.PoseAt(i, hands[h], slotOrder[s]);

                fprintf(file, "    { \"%s\", %s, { %.3ff, %.3ff, %.3ff, %.1ff, %.1ff, %.1ff, %.3ff } },\n",
                        viewModel.NameAt(i), handNames[h], pose.right, pose.up, pose.forward,
                        pose.pitch, pose.yaw, pose.roll, pose.scale);
            }
        }
    }

    fprintf(file, "\n");

    fclose(file);
    savedCount++;

    TraceLog(LOG_INFO, "VIEWMODELEDITOR: Wrote %i poses to %s", viewModel.Count()*4, DumpFile);
}
