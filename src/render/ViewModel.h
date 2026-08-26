#pragma once

#include "combat/AttackStyle.h"
#include "combat/Collider.h"
#include "core/Hand.h"
#include "raylib.h"

#include <string>
#include <vector>

class AssetManager;

// Every weapon is posed twice: where it rests, and where it wants to be at full
// extension. The attack is the trip between them and back.
enum class PoseSlot { Rest = 0, End = 1 };

//----------------------------------------------------------------------------------
// Where a held weapon sits, expressed in camera space so it is independent of
// where the player is looking. Distances are world units, angles degrees.
//
// The crosshair is the camera's forward axis, so "swinging at the crosshair" is
// just an end pose whose business end is near the centre of the screen - which
// is why long weapons need less travel than short ones.
//----------------------------------------------------------------------------------
struct ViewModelPose
{
    float right   =  0.28f;   // + toward the right hand, - toward the left
    float up      = -0.26f;   // - drops it below the camera (there are no arms)
    float forward =  0.55f;   // + pushes it away from the eye
    float pitch   = -78.0f;   // -90 points the blade straight down the view axis
    float yaw     =   6.0f;
    float roll    =   8.0f;
    float scale   =   0.45f;
};

// The same placement in the other hand. Mirroring across the camera's YZ plane
// flips the sideways offset and reverses the two rotations whose axes lie in
// that plane; pitch is about the mirror axis itself, so it survives untouched.
//
// This is only ever a starting point: a weapon whose off hand pose has been
// tuned keeps that pose, because a mirrored grip is frequently not the grip you
// want once you see it.
ViewModelPose MirrorPose(const ViewModelPose &pose);

// Angles interpolate raw rather than by the shortest route, because the stored
// numbers already describe the arc that was dragged
ViewModelPose LerpPose(const ViewModelPose &from, const ViewModelPose &to, float t);

//----------------------------------------------------------------------------------
// A small random nudge rolled once per attack and added to the end pose, so
// repeated swings do not trace an identical arc. Kept off the stored poses: the
// editor and the dump always see the authored end pose, not a jittered one.
//----------------------------------------------------------------------------------
struct PoseVariation
{
    float right = 0.0f;
    float up = 0.0f;
    float forward = 0.0f;
    float pitch = 0.0f;
    float yaw = 0.0f;
    float roll = 0.0f;
};

//----------------------------------------------------------------------------------
// Everything the view model needs from a frame. Bundled because it needs input,
// timing and the camera's walk cycle, and a seven argument Update is no use to
// anyone.
//----------------------------------------------------------------------------------
struct ViewModelInput
{
    float delta = 0.0f;

    // The wheel used to live here and cycle a hand's weapon. It does not any more:
    // which weapons the player may reach is a fact about the run (see Arsenal), so
    // the caller decides and calls SetSlot. Both fields are kept as dead weight only
    // long enough for this note - see Game::UpdateWorld for where the cycle went.
    float wheel = 0.0f;
    bool offhand = false;
    float bobPhase = 0.0f;                  // From FpsCamera, so sway stays in step
    float walkAmount = 0.0f;

    // Driven by Player's attack states, indexed by Hand. The view model no longer
    // runs the clock: animation and hit detection read the same blend, so they
    // cannot drift apart.
    float blend[2] = { 0.0f, 0.0f };        // 0 at rest, 1 at full extension
    bool attackStarted[2] = { false, false };
};

//----------------------------------------------------------------------------------
// The first person weapons: one per hand, drawn just below the camera.
//
// Every weapon stores a rest pose and an end pose per hand, and all four are
// independent - editing one never disturbs the others. An untuned hand starts as
// the mirror of the tuned one; an untuned end pose is generated from the rest
// pose and the attack style, so there is always something to swing at and refine.
//
// It owns the models, the poses and the weapon list. The attack clock lives in
// Player, because when a hit lands is a gameplay decision; this only reads the
// blend it is given and interpolates along it.
//
// Drawn into its own render target rather than into the world pass. A held
// weapon sits about half a unit from the eye, well inside anything the world can
// legitimately put there - a wall, an enemy's chest - so sharing a depth buffer
// with the world means being sliced by it. Its own target means its own depth
// buffer: the two hands still occlude each other correctly, and nothing outside
// can reach in.
//----------------------------------------------------------------------------------
class ViewModel
{
public:
    void Load(AssetManager &assets);
    void Unload();
    void Update(const ViewModelInput &in);

    // The weapon in this hand has just left it. Told directly rather than through
    // ViewModelInput because the release is detected after Update has run - the
    // muzzle it fires from is this frame's pose - and a flag would arrive a frame
    // late, which is one frame of a knife being in two places at once.
    void NoteThrown(Hand hand);

    // The isolated pass. Everything held in the hands is drawn between these,
    // then Composite() lays the result over the finished world.
    void BeginPass(const Camera3D &camera);
    void EndPass();
    void Composite() const;

    // Call between BeginPass and EndPass
    void Draw(const Camera3D &camera);

    // A translucent copy of one stored pose, for the editor to show and drag
    void DrawPosePreview(Hand hand, PoseSlot slot, const Camera3D &camera, Color tint);

    // Where a pose hangs in the world, given the camera it is attached to
    Vector3 AnchorPoint(Hand hand, PoseSlot slot, const Camera3D &camera) const;

    //------------------------------------------------------------------------------
    // The weapon as combat sees it: a capsule down the blade, in world space,
    // built from the pose being drawn this frame.
    //
    // The drawn weapon is a miniature about half a unit from the eye, so its own
    // geometry is inside the player's collision radius and worthless as a hit
    // volume. This takes the same pose - the same rest-to-end interpolation, the
    // same per-swing variation, the same sway - and magnifies it about the eye
    // until the tip sits at `reach`. Direction, arc and timing come straight off
    // the authored animation; only the length is a gameplay number.
    //
    // That is the whole point of the shape: where the weapon is pointing is what
    // decides the hit, so a swing that visibly misses does miss.
    //
    // Returns a zero-radius capsule for an empty hand - test HasWeapon first.
    Capsule BladeFor(Hand hand, const Camera3D &camera, float reach, float bladeRadius) const;

    // The model in a hand, for anything that has to draw that weapon somewhere
    // other than the hand - a thrown dagger is the dagger that left it, not a
    // stand-in. Owned by the AssetManager; null for an empty hand.
    Model *ModelFor(Hand hand) const;

    // The held weapon's orientation in world space this frame - rotation only, no
    // scale and no position. A thrown weapon flies with this frozen at the moment
    // it left, so the copy in the air starts in exactly the pose the hand was
    // holding, and the handover is invisible.
    Matrix WorldOrientation(Hand hand, const Camera3D &camera) const;

    //------------------------------------------------------------------------------
    // Put weapon `index` in this hand, or -1 for an empty one.
    //
    // The cycling itself moved OUT of Update when the weapons stopped being free:
    // which ones the player may reach is a fact about the run, and this class is a
    // renderer. It is handed the answer now rather than the wheel.
    //
    // Out-of-range means empty rather than a clamp. A caller asking for a weapon that
    // does not exist has already gone wrong, and an empty hand is the one outcome
    // that cannot be mistaken for having worked.
    //------------------------------------------------------------------------------
    void SetSlot(Hand hand, int index);

    bool HasWeapon(Hand hand) const { return SlotIndex(hand) >= 0; }
    const char *NameFor(Hand hand) const { return NameAt(SlotIndex(hand)); }
    float HeightFor(Hand hand) const { return HeightAt(SlotIndex(hand)); }
    int SlotIndex(Hand hand) const { return slots[(int)hand]; }
    AttackStyle StyleFor(Hand hand) const;
    AttackStyle StyleAt(int i) const;
    float AttackBlend(Hand hand) const { return blends[(int)hand]; }

    const ViewModelPose &PoseFor(Hand hand, PoseSlot slot) const;
    void SetPoseFor(Hand hand, PoseSlot slot, const ViewModelPose &pose);

    // The whole list, for the pose dump
    int Count() const { return (int)weapons.size(); }
    const char *NameAt(int i) const;
    float HeightAt(int i) const;
    const ViewModelPose &PoseAt(int i, Hand hand, PoseSlot slot) const;

private:
    struct Weapon
    {
        std::string name;
        Model *model = nullptr;         // Owned by the AssetManager
        float height = 0.0f;
        // The blade's extent along the model's own Y axis, kept as the raw
        // bounding box rather than just the height: a weapon whose grip sits below
        // the origin needs both ends to place the capsule, and `height` alone
        // cannot say where the hilt is.
        float bladeMin = 0.0f;
        float bladeMax = 0.0f;
        AttackStyle style = AttackStyle::Swing;
        ViewModelPose poses[2][2];      // [hand][slot], all four independent
    };

    // The pose-to-camera-space transform Draw and BladeFor both need, so the hit
    // volume can never be built from different maths than the picture
    static Matrix PoseMatrix(const ViewModelPose &pose);

    ViewModelPose LivePose(Hand hand) const;    // Rest, blended toward End, plus sway
    void DrawWeapon(int i, const ViewModelPose &pose, const Camera3D &camera, Color tint);

    // Walk bob and idle breathing, applied at draw time only: the stored poses
    // stay clean so the editor's gizmos have something steady to attach to
    ViewModelPose ApplySway(const ViewModelPose &pose, float walkFade, float idlePhase) const;

    RenderTexture2D target = { 0 };
    std::vector<Weapon> weapons;
    ViewModelPose fallbackPose;     // Returned when a hand is empty
    int slots[2] = { 0, -1 };       // Right, Left. -1 is an empty hand
    float blends[2] = { 0.0f, 0.0f };
    PoseVariation variations[2];    // Rolled when an attack starts

    // The empty-handed moment after a throw and the slide back in. While this is
    // running the hand ignores its attack blend entirely: the stroke that threw is
    // over, and what is being drawn is the next weapon arriving, not the old one
    // returning from full extension.
    bool throwing[2] = { false, false };
    float throwTimer[2] = { 0.0f, 0.0f };

    bool ThrowHidden(Hand hand) const;
    float bobPhase = 0.0f;
    float bobAmount = 0.0f;         // 0 standing, 1 walking
    float idleTimer = 0.0f;         // Free running, drives the breathing
};
