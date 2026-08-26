#pragma once

#include "raylib.h"
#include "render/ViewModel.h"

//----------------------------------------------------------------------------------
// In game pose editor for the first person weapons.
//
//   T  toggle the position gizmo (drag the arrows)
//   Y  toggle the rotation gizmo (drag the rings)
//   H  switch which hand the gizmo is attached to
//   G  switch between the rest pose and the end-of-attack pose
//   U  append every weapon's poses to viewmodel_poses.txt
//   [  ]  shrink / grow the pose being edited
//
// A translucent copy of each weapon sits at its end pose while the editor is
// open - drag that one to say where the attack should finish.
//
// While a gizmo is up the mouse cursor comes back and mouse look is suspended,
// so dragging does not spin the view. The gizmo axes are the camera's own right,
// up and forward, because that is the space the pose is expressed in.
//
// Editing a weapon held in its off hand works on the mirrored pose and folds the
// result back into storage, so a weapon still only needs tuning once.
//
// Development tool: it is the only thing in src/debug, and Game touches it in
// four places (member, Update, Draw, DrawUi).
//----------------------------------------------------------------------------------
class ViewModelEditor
{
public:
    void Update(ViewModel &viewModel, const Camera3D &camera);

    // Inside BeginMode3D. Non const because drawing a preview borrows the
    // model's transform, the same way ViewModel::Draw does.
    void Draw(ViewModel &viewModel, const Camera3D &camera) const;
    void DrawUi(const ViewModel &viewModel) const;                          // After EndMode3D

    bool IsActive() const { return mode != Mode::Off; }

    // True while a gizmo is up, plus a couple of frames after closing one:
    // re-grabbing the cursor produces one huge mouse delta that would otherwise
    // snap the view around.
    bool BlocksLook() const { return (mode != Mode::Off) || (lookCooldown > 0); }

private:
    enum class Mode { Off, Translate, Rotate };

    void SetMode(Mode next);
    void Axes(const Camera3D &camera, Vector3 out[3]) const;
    int PickAxis(const ViewModel &viewModel, const Camera3D &camera) const;
    void DumpPoses(const ViewModel &viewModel);

    Mode mode = Mode::Off;
    Hand editHand = Hand::Right;
    PoseSlot editSlot = PoseSlot::Rest;

    int hoveredAxis = -1;
    int draggedAxis = -1;
    Vector3 grabAnchor{};       // Anchor at the moment of grabbing, so dragging cannot feed back
    Vector3 grabAxis{};
    float grabParam = 0.0f;     // Distance along the axis, or screen angle, when grabbed
    float grabValue = 0.0f;     // The pose field's value when grabbed
    float grabSign = 1.0f;      // Which way a screen drag turns this ring

    int savedCount = 0;         // How many times U has been pressed this session
    int lookCooldown = 0;       // Frames of mouse look to swallow after closing a gizmo
};
