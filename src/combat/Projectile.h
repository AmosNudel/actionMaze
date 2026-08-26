#pragma once

#include "combat/Magic.h"
#include "core/Config.h"
#include "raylib.h"

#include <vector>

class AssetManager;
class Level;
class Player;
class VfxManager;
struct Enemy;

//----------------------------------------------------------------------------------
// Who a shot is looking for.
//
// A side rather than an owner. What a projectile needs to know is what it may
// damage, and an owner pointer answers that only after a lookup - and only while
// the owner is still alive, which for an arrow that outlives its archer is not
// long enough. Two sides is also all a shot ever needs: nothing here friendly
// fires, so "not mine" and "the other lot" are the same set.
//----------------------------------------------------------------------------------
enum class ProjectileSide { AtPlayer, AtEnemies };

//----------------------------------------------------------------------------------
// What a shot looks like in the air.
//
// Two ways to orient one, and the difference is the whole reason this is a struct
// rather than a Model pointer:
//
//   Aligned    the model's own nose axis is rotated onto the velocity every frame.
//              Right for an arrow, which points where it is going by definition.
//
//   Fixed      the model keeps one orientation for its whole flight, captured from
//              somewhere else. Right for a thrown weapon, because the moment it
//              leaves the hand it has to be in exactly the pose the hand was
//              holding it in - snapping it onto the velocity at release is a
//              visible flick, and it is the one frame the player is looking at.
//
// `axis` is which way the model's nose points in its own space, and it is per
// model because the pack does not agree with itself: Skeleton_Arrow is authored
// nose-down -Y, where every weapon the player holds has its tip at +Y.
//
// And a third way, which is no model at all: `magic`. A cast is a MOTE - a round
// glow in the school's colour, with no geometry and therefore no orientation to
// argue about, which bursts into that school's impact sheet wherever it stops.
// When it is set it overrides the model entirely, and everything the shot looks
// like on both halves of its life comes off the one table row.
//----------------------------------------------------------------------------------
struct ProjectileLook
{
    Model *model = nullptr;                     // Null uses the shared arrow
    float scale = 1.0f;
    Vector3 axis = { 0.0f, 1.0f, 0.0f };        // The nose, in model space
    bool fixed = false;                         // Keep `orientation` instead of aligning
    Matrix orientation{};

    // A cast rather than a shot. Points into the static table in Magic.cpp, so it
    // outlives anything that set it and never needs freeing.
    const MagicDef *magic = nullptr;

    //------------------------------------------------------------------------------
    // What the mote bursts into, overriding the school's own sheet. `Count` means
    // "whatever the school says", which is what the player's casts want.
    //
    // The enemy casters do not want it. A school's impact art IS the player's magic
    // - it is the loudest thing on the screen and it is how a cast reads as having
    // landed - and an enemy repainting the same picture every couple of seconds
    // would drown out the player's own. So they burst into a small flash in the
    // mote's colour instead: enough to say a hit landed, not enough to be mistaken
    // for a spell the player cast.
    //------------------------------------------------------------------------------
    VfxKind impactOverride = VfxKind::Count;
    float impactScale = 1.0f;
};

//----------------------------------------------------------------------------------
// Arrows in flight.
//
// Deliberately the simplest thing that reads correctly: a point that travels in a
// straight line until it hits a wall, the floor, a body, or its own lifetime.
// Nothing stops it from above - the levels are open topped.
// No gravity, no drop, no spin - an arrow crossing eleven units at twenty-four a
// second is in the air for under half a second, and over that distance a ballistic
// arc is invisible while being a great deal harder to aim.
//
// What a shot LOOKS like travels with it, because the two sides no longer throw
// the same object: an enemy crossbow sends an arrow, and a thrown dagger has to
// be the dagger that left the hand.
//
// Both sides fire now - enemy crossbows and staves, and the player's own staffs
// and thrown daggers - so every shot carries the side it is looking for. Nothing
// friendly fires, which is what keeps that a single enum rather than a mask.
//
// It moves in substeps because a whole frame's travel is longer than the player is
// wide - 0.4 units against a 0.4 radius at 60fps - and the wall test is a point
// sample of the grid, which would step clean over a one-cell pillar otherwise.
//
// The player test does not depend on them: the arrow is swept as the capsule from
// where it was to where it is, so a body cannot be skipped however long the frame
// ran. Substeps are the wall's problem, not the target's.
//----------------------------------------------------------------------------------
class ProjectileManager
{
public:
    // Optional: without the arrow model shots draw as a debug line and still hit
    void Load(AssetManager &assets);

    // `direction` need not be normalised; a zero direction is ignored. The default
    // look is the shared arrow, aligned to its flight - what an enemy crossbow
    // wants, and what a staff's bolt borrows until there is a spell model for it.
    void Spawn(Vector3 from, Vector3 direction, float speed, int damage, ProjectileSide side,
               ProjectileLook look = ProjectileLook{});

    // `vfx` is where a mote goes when it stops. Passed in rather than held because
    // the effect pool is shared with everything else that flashes, and a projectile
    // manager that owned one would be the only thing able to use it.
    void Update(float delta, Level &level, Player &player, std::vector<Enemy> &enemies,
                VfxManager &vfx);

    // Motes are billboards, so the draw needs to know which way the player is
    // looking. Arrows do not, and ignore it.
    void Draw(const Camera3D &camera) const;

    void Clear();
    int Count() const { return (int)shots.size(); }

private:
    struct Shot
    {
        Vector3 position{};
        Vector3 velocity{};
        // Where it was fired from, kept for the whole flight. What an enemy hit
        // from cover walks to when it goes looking - the contact point on its own
        // chest says which way to flinch and nothing about where the shooter is.
        Vector3 origin{};
        float life = 0.0f;      // Counts down; a shot that hits nothing expires
        int damage = 0;
        ProjectileSide side = ProjectileSide::AtPlayer;
        ProjectileLook look;    // Resolved at spawn: never carries a null model

        // Landed in geometry and stopped there. Still drawn, no longer moving, and
        // no longer able to hit anything - a stuck arrow is scenery.
        bool stuck = false;
        // Which door leaf it is stuck in, or -1 for stone. A leaf swings, and an
        // arrow that opened one has to travel with it rather than hang in the air
        // the door used to fill.
        int stuckDoor = -1;
        // Its pose in that leaf's own space, so the world pose is one multiply by
        // whatever the door has swung to since
        Matrix stuckLocal{};
    };

    // One substep. Returns false when the shot is finished travelling - either
    // spent, or stopped somewhere it will now sit.
    bool Advance(Shot &shot, float step, Level &level, Player &player,
                 std::vector<Enemy> &enemies, VfxManager &vfx) const;

    // The motes, drawn as camera-facing billboards ahead of the geometry pass
    void DrawMotes(const Camera3D &camera) const;

    // A mote reaching the end of its flight. Plays its school's impact sheet at
    // `at` and marks the shot spent - a mote never sticks, because there is no
    // object to leave behind: it is the spell arriving, and once it has arrived
    // the picture of it arriving is the whole of what is left.
    void Burst(Shot &shot, Vector3 at, VfxManager &vfx) const;

    // Plant a shot where it stopped. `at` is the last point it was still clear;
    // it is driven a little further in so the head is under the surface rather
    // than resting on it. `door` is the leaf it landed in, or -1 for stone.
    void Stick(Shot &shot, Vector3 at, const Level &level, int door) const;

    std::vector<Shot> shots;
    Model *arrow = nullptr;     // Owned by the AssetManager, null when missing

    // The mote's texture: one soft round light, tinted per school at the draw.
    // Generated, shared with the portal, and owned by the AssetManager - see
    // render/Glow.h. Null until Load, which is the only reason DrawMotes checks.
    Texture2D *glow = nullptr;
};
