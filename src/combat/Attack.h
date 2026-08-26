#pragma once

#include "combat/AttackStyle.h"
#include "combat/Collider.h"
#include "combat/StatBlock.h"
#include "combat/Weapon.h"
#include "core/Config.h"
#include "raylib.h"

#include <vector>

struct Enemy;
class Level;

//----------------------------------------------------------------------------------
// One hand's attack, as a state machine.
//
// Idle -> Out -> (Hold) -> Back -> Idle. `blend` runs 0 at rest to 1 at full
// extension and is what the view model interpolates its poses along, so the
// animation and the hit are reading the same number rather than two clocks that
// can drift apart.
//----------------------------------------------------------------------------------
struct AttackState
{
    enum class Phase { Idle, Out, Hold, Back };

    Phase phase = Phase::Idle;
    float timer = 0.0f;
    float blend = 0.0f;
    bool startedThisFrame = false;  // For the view model's per swing variation

    // A ranged attack has begun but the bolt or knife has not left yet. The
    // release point is a threshold rather than an event, so without this every
    // frame past it would fire again - the same guard the enemies' shots use.
    bool shotPending = false;

    // Who this stroke has already cut. A cone resolved on one frame and needed
    // nothing but a bool; a swept blade is tested every frame it is live and stays
    // inside a body for several of them, so the swing has to remember what it has
    // touched or one stroke bills an enemy four times over.
    //
    // Enemy ids, not indices: RemoveDead compacts the vector between frames of the
    // same swing, and an index that meant one skeleton on Tuesday means its
    // neighbour on Wednesday.
    int hitIds[Config::MaxHitsPerSwing] = {};
    int hitCount = 0;

    bool IsAttacking() const { return phase != Phase::Idle; }

    // True while a shield is actually up, as opposed to on its way there
    bool IsHolding() const { return phase == Phase::Hold; }

    // Whether this stroke is allowed to damage `enemyId` right now
    bool CanHit(int enemyId) const;
    void NoteHit(int enemyId);

    void Update(float delta, bool pressed, bool held, AttackStyle style);
    void Reset();
};

//----------------------------------------------------------------------------------
// Melee resolution: a swept blade, not a cone.
//
// `from` and `to` are the weapon's hit capsule where it was last frame and where
// it is now, as ViewModel::BladeFor builds them, and the sweep between the two is
// what gets tested. Damage lands when and where the blade passes through the body
// - so a swing that visibly misses misses, which is the entire reason this is not
// a cone from the eye any more. A cone ignores where the weapon is pointing, and
// what that bought in forgiveness it paid for in hits that connected from a metre
// wide of the target.
//
// Called every frame the stroke is live rather than once at a fixed blend, and it
// is `state`'s hit list, not a one-shot flag, that keeps one stroke to one hit per
// enemy.
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// What one frame of a stroke actually did.
//
// The damage total is here because LIFESTEAL needs it and only the caller can pay
// it - a weapon that drinks has to know what it drank, and the sweep is the only
// thing that knows. Returning it beats handing this function a Player: the sweep
// would then need the whole of one to give back a fraction of a number.
//
// It is the damage RESOLVED, after the crit roll and after whatever the guard
// turned away, because that is what actually landed. Lifesteal off the raw figure
// would pay full price for a blow a shield ate.
//----------------------------------------------------------------------------------
struct MeleeResult
{
    int hits = 0;
    int damage = 0;
};

// `attacker` is the stats the blow is struck with - the player's own line plus what
// both hands are carrying - and is what the crit is rolled and multiplied against.
MeleeResult SweepMelee(const Capsule &from, const Capsule &to, Vector3 eye,
                       const WeaponStats &stats, const StatBlock &attacker,
                       const Level &level, std::vector<Enemy> &enemies, AttackState &state);

//----------------------------------------------------------------------------------
// Is this stroke actually cutting right now?
//
// A blade capsule exists on every frame of every weapon, swinging or not - it is
// where the model is, not what it is doing. Anything that reacts to being struck
// has to ask this first, or it reacts to being walked at.
//----------------------------------------------------------------------------------
bool MeleeIsLive(const WeaponStats &stats, const AttackState &state);

// Still shared by blocking on both sides: is `target` inside a horizontal cone of
// `arcDegrees` about `forward`, within `reach`? A guard is a direction you are
// facing rather than a volume you swing, so a cone is the right shape for it.
bool InCone(Vector3 origin, Vector3 forward, Vector3 target, float reach, float arcDegrees);
