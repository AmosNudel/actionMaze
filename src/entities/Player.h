#pragma once

#include "combat/Attack.h"
#include "combat/AttackStyle.h"
#include "combat/Modifiers.h"
#include "combat/Stats.h"
#include "core/Config.h"
#include "progress/Purse.h"
#include "core/Hand.h"
#include "entities/Body.h"
#include "raylib.h"

struct InputState;

//----------------------------------------------------------------------------------
// The player character: a body, the stats combat acts on, and one attack state
// per hand.
//
// The attack clock lives here rather than in the view model because when a hit
// lands is a gameplay decision. The view model is handed the resulting blend and
// interpolates its poses along it, so the swing you see and the hit you deal are
// reading the same number.
//
// Which weapon is in which hand is still ViewModel's business; Game passes the
// styles down each frame, so nothing here has to know about rendering.
//----------------------------------------------------------------------------------
class Player
{
public:
    void Spawn(Vector3 position);

    //------------------------------------------------------------------------------
    // A whole new character: level 1, no points spent, no mana banked.
    //
    // Deliberately separate from Spawn, which is a PLACEMENT and carries level,
    // experience and spent points across a floor change on purpose - see the note
    // on Game::Descend. This is the one call that actually throws a character away,
    // and it exists for exactly one caller: starting a fresh run after a defeat or
    // a victory screen.
    //------------------------------------------------------------------------------
    void ResetCharacter();

    void Update(float delta, const InputState &input, float yaw);

    // pressed/held and styles are indexed by Hand
    void UpdateAttacks(float delta, const bool pressed[2], const bool held[2], const AttackStyle styles[2]);

    Vector3 Position() const { return body.position; }
    Vector3 EyePosition() const;
    Vector3 Forward() const;    // Horizontal facing, which is all melee cares about
    Vector3 Velocity() const { return body.velocity; }
    float Yaw() const { return yaw; }

    bool IsGrounded() const { return body.isGrounded; }
    bool IsAlive() const { return health > 0; }

    AttackState &Attack(Hand hand) { return attacks[(int)hand]; }
    const AttackState &Attack(Hand hand) const { return attacks[(int)hand]; }
    AttackStyle StyleFor(Hand hand) const { return styles[(int)hand]; }

    // A shield counts as up only near full extension, not on the way there
    bool IsBlocking() const;

    void TakeDamage(int amount);
    void TakeDamageFrom(int amount, Vector3 source);

    // Health back, capped at the pool. What weapon lifesteal pays into, and the
    // one way health is ever gained - so a cap that has just grown from a point of
    // constitution is respected without the caller knowing it moved.
    void Heal(int amount);

    //------------------------------------------------------------------------------
    // Progression
    //------------------------------------------------------------------------------
    // A level gives NOTHING directly. It grants points, and the points are the only
    // source of health, damage, crit or spell power there is. Levelling that
    // happens TO a character is a number going up while the player watches; this is
    // meant to be a decision, which is why the whole of it lands in `statPoints`
    // and stops there until the player spends it.
    //
    // Returns how many levels were gained, so the caller can say so.
    int GainExp(int amount);

    // One point into one stat, if there is one to spend. Returns whether it went
    // in. The health pool follows immediately - a point of constitution that only
    // took effect on the next level-up would be indistinguishable from a bug.
    bool SpendPoint(Stat stat);

    // Refunds every point back and returns the line to neutral. There is no cost
    // and no limit: the stat system is new, the rates will move, and a player who
    // cannot undo a spend under those conditions is being asked to commit to
    // numbers that have not settled.
    void RespecStats();

    // The pool a point of constitution moves. Recomputed rather than stored,
    // because it is the one number that has to agree with `stats` at all times.
    int MaxHealth() const;

    //------------------------------------------------------------------------------
    // The stats the character actually fights with: the spent line, plus whatever
    // both hands are carrying.
    //
    // Everything derived reads THIS and not `stats`, so a weapon's bonus is worth
    // exactly what a spent point is worth and neither has to be handled twice. The
    // character sheet is the one place that wants the two apart, so it can show
    // what was earned against what is being held.
    //------------------------------------------------------------------------------
    StatBlock Fighting() const;

    //------------------------------------------------------------------------------
    // What the hands are carrying, as an offset from neutral. Set once a frame from
    // the loadout; both weapons' bonuses are already summed by the caller.
    //
    // The pool it moves is clamped, never carried up. A point of constitution
    // SPENT grows the health inside the bar with it - see RefreshHealth - but a
    // weapon's constitution must not, or swapping a shield in and out is a heal
    // with a cooldown of one frame.
    //------------------------------------------------------------------------------
    void SetHeldBonus(const StatBlock &bonus);

    // What magic is multiplied against - the ARCANE counterpart of a weapon's own
    // damage. At neutral arcane this is exactly Config::BaseSpellPower, so a fresh
    // character's spells mean what the table says they mean.
    int SpellPower() const;

    //------------------------------------------------------------------------------
    // Everything the traits are granting, as one summed block.
    //
    // Written by whoever owns a source - today that is the trait loadout and nothing
    // else - and read by everything derived. It is not part of `stats` and not part
    // of `heldBonus`, because it is a third thing: not the character, and not what
    // they are holding, but what they have learned to do.
    //
    // Setting it refreshes the health pool the same way spending a point does, so a
    // trait granting constitution is felt on the frame it is equipped.
    //------------------------------------------------------------------------------
    void SetModifiers(const Modifiers &bonus);
    const Modifiers &Mods() const { return mods; }

    //------------------------------------------------------------------------------
    // Mana
    //------------------------------------------------------------------------------
    // The pool a cast is paid out of. It refills by KILLING - see the long note in
    // progress/Spellbook.h - and carries across floors, because a pool emptied by the
    // portal would make every kill before descending a waste.
    //
    // MaxMana is recomputed rather than stored, for the same reason MaxHealth is: it
    // is the one number that has to agree with the stat line at all times.
    int MaxMana() const;

    // Takes `cost` if it is there, and returns whether it was. One function rather
    // than a check and a subtraction at every call site.
    bool SpendMana(int cost);

    //------------------------------------------------------------------------------
    // Credits kills into the pool.
    //
    // Weapon kills pay outright; spell kills pay at a fraction, and the REMAINDER is
    // carried rather than dropped. Rounding each cast's kills down on its own would
    // mean a run of single kills paid nothing at all, which is the difference between
    // a slow refill and no refill.
    //------------------------------------------------------------------------------
    void CreditWeaponKills(int kills);
    void CreditSpellKills(int kills);

    // The weapon's own damage, raised by ARMS. The base is the weapon's, not the
    // player's, so a dagger stays a dagger in the hands of a strong character -
    // arms scales what you are holding rather than replacing it.
    int WeaponDamage(int weaponDamage) const;

    // Public so the Level can resolve collision against it in place
    Body body;

    int health = Config::PlayerMaxHealth;
    // The live pool. Kept as a field rather than only as MaxHealth() so the HUD and
    // the damage path read one number, and refreshed whenever `stats` moves.
    int maxHealth = Config::PlayerMaxHealth;

    // The four. Public because the character sheet edits them through SpendPoint
    // and reads them directly for every derived row it prints.
    StatBlock stats;

    // What both hands add, as an offset from neutral. Not part of `stats` because
    // it is not the character - it is what they picked up, and it goes away when
    // they put it down.
    StatBlock heldBonus = { 0, 0, 0, 0 };

    int level = 1;
    int exp = 0;
    int expToNext = Config::PlayerExpFirstLevel;
    int statPoints = 0;

    // What the three vendors trade in. Public for the same reason the stats are: the
    // shop page reads every column of it directly to draw the price rows.
    Purse purse;

    // What is left of the pool. Public alongside health for the HUD, and moved only
    // through SpendMana and the credit functions above.
    int mana = 0;

private:
    // Brings `maxHealth` back in line with `stats`, carrying the current health up
    // with it by however much the pool grew. Growing the pool without growing the
    // health in it means a point of constitution makes the bar longer and the
    // character no safer, which is the opposite of what it was spent for.
    void RefreshHealth();

    // What the traits are granting. Derived - see SetModifiers - and never written
    // from anywhere but there.
    Modifiers mods;

    //------------------------------------------------------------------------------
    // Spell kills that have not yet added up to a whole point of mana.
    //
    // Carried between calls rather than rounded away. At one mana per two spell
    // kills, rounding each cast down on its own would mean a caster who kills one
    // body at a time is paid nothing, ever - which is the exact build the half-rate
    // refill was added to stop starving.
    //------------------------------------------------------------------------------
    int spellKillCarry = 0;

    AttackState attacks[2];
    AttackStyle styles[2] = { AttackStyle::Swing, AttackStyle::Swing };
    float yaw = 0.0f;
};
