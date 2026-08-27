#pragma once

#include "combat/Attack.h"
#include "combat/AttackStyle.h"
#include "combat/Buff.h"
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

    // Damage from somewhere a raised shield can be pointed at. Returns whether
    // this blow was PARRIED - caught in the instant the shield went up rather
    // than absorbed after a wait - which is the caller's cue to punish whatever
    // threw it (see EnemyManager::LandMelee). `melee` gates that: a parry is a
    // timed read of a swing, not something a shield does to an arrow, so a
    // ranged hit can still be blocked but never parried.
    bool TakeDamageFrom(int amount, Vector3 source, bool melee);

    //------------------------------------------------------------------------------
    // What the last landed blow looked like, for anyone drawing feedback about it.
    //
    // Player only remembers WHERE and WHEN, in world space and a raw age - it has
    // no idea which way the camera is looking, so turning that into a screen
    // position is the HUD's job (see Hud::DrawHurtIndicator) and not this class's.
    // `hitDirectional` is false for a blow with no meaningful source, like a Seal
    // bolt falling from directly overhead - TakeDamage(int) alone cannot say a
    // direction, and should not invent one.
    //------------------------------------------------------------------------------
    Vector3 lastHitFrom = { 0.0f, 0.0f, 0.0f };
    bool lastHitDirectional = false;
    float lastHitAge = 1e9f;    // Seconds since a blow last actually landed

    // Edge-triggered: true for exactly the frame a blow landed, then cleared by
    // whoever reads it. The same idiom Enemy::raidHitPending already uses - a
    // body sets a flag when something happens and forgets it, and the system that
    // cares (here, the camera shake) clears it once it has acted. `lastHitAge`
    // above cannot serve this job on its own: it is a fading readout for the HUD,
    // and a shake gated on "age is small" would refire every frame the age
    // happened to still be under some threshold rather than once per blow.
    bool hitPending = false;

    // Health back, capped at the pool. What weapon lifesteal pays into, and the
    // one way health is ever gained - so a cap that has just grown from a point of
    // constitution is respected without the caller knowing it moved.
    void Heal(int amount);

    // Mana back, capped at the pool - the direct counterpart to Heal, and What a
    // mana pickup pays into. Not the same path as CreditWeaponKills/
    // CreditSpellKills below: those are earned by killing and carry a rate
    // (Config::ManaPerKill) that a trait can modify, where this is a flat amount
    // handed over outright by something the player walked onto.
    void GiveMana(int amount);

    //------------------------------------------------------------------------------
    // A temporary buff - a Pickup's grant, see world/Pickup.h and combat/Buff.h
    // for the table this reads. Kept apart from `mods` even though both end up
    // added into combat the same way: `mods` is rebuilt from the trait loadout
    // whenever it changes (Game::RefreshModifiers) and a buff would be silently
    // erased by the next equip. Everything derived reads the two summed together
    // - see the private Combined().
    //
    // Takes the KIND rather than a Modifiers directly, and looks the row up
    // itself - Player is where the buff's clock lives, so it is also where
    // "what BASTION grants" and "how long BASTION runs" belong, rather than
    // splitting the two between a caller that read the table and a Player that
    // only knows the numbers it was handed.
    //
    // A second ApplyBuff REPLACES whatever was running rather than stacking,
    // both the effect and the clock: two pickups grabbed in quick succession are
    // one buff refreshed, not a stronger one, which is what keeps a corridor of
    // them from being a build in its own right.
    //------------------------------------------------------------------------------
    void ApplyBuff(BuffKind kind);

    float BuffTimeLeft() const { return buffTimeLeft; }
    bool HasBuff() const { return buffTimeLeft > 0.0f; }
    BuffKind ActiveBuff() const { return activeBuff; }

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

    // Undoes exactly the points named in `pending` - not a full respec, just
    // whatever a caller is tracking as not yet confirmed. See CharacterSheet's
    // CANCEL, which is what this exists for: a level's points go into `stats`
    // the moment they are clicked, for the same reason SpendPoint always has -
    // a point that only took effect once "confirmed" would be indistinguishable
    // from a bug - and CANCEL is what takes back exactly that click's worth
    // rather than the whole build.
    void RevertPoints(const StatBlock &pending);

    // The pool a point of constitution moves. Recomputed rather than stored,
    // because it is the one number that has to agree with `stats` at all times.
    int MaxHealth() const;

    //------------------------------------------------------------------------------
    // The stats the character actually fights with: the spent line, with the
    // trait loadout's own point bonuses and conversions applied on top.
    //
    // Held gear does NOT appear here any more - see the note on SetGearMods.
    // What a weapon is worth lives in Combined() instead, read directly by
    // whatever it actually changes (WeaponDamage, SpellPower, TakeDamage), so
    // this stays exactly the answer to "what did the character build", which is
    // the one thing the character sheet's STATS tab wants and the one thing a
    // weapon in the hand must never be allowed to answer for.
    //------------------------------------------------------------------------------
    StatBlock Fighting() const;

    //------------------------------------------------------------------------------
    // What both hands are granting right now, as a Modifiers - see the long note
    // at the top of combat/Modifiers.h and on WeaponStats::bonus for why this is
    // flat figures and fractions and never a stat point. Set once a frame from
    // the loadout; both weapons' bonuses, and whatever their forge levels add on
    // top, are already summed by the caller.
    //
    // Whatever pool a column here touches is clamped down, never carried up -
    // the same rule a shield's old constitution point always followed, now
    // applied to (for instance) a staff's mana pool instead: putting the weapon
    // away shortens the bar, picking it up lengthens it and leaves the current
    // amount where it was, which is the only version of this a player cannot
    // farm by cycling the mouse wheel.
    //------------------------------------------------------------------------------
    void SetGearMods(const Modifiers &bonus);

    // What magic is multiplied against - the ARCANE counterpart of a weapon's own
    // damage. At neutral arcane this is exactly Config::BaseSpellPower, so a fresh
    // character's spells mean what the table says they mean.
    int SpellPower() const;

    //------------------------------------------------------------------------------
    // Everything the traits are granting, as one summed block.
    //
    // Written by whoever owns a source - today that is the trait loadout and nothing
    // else - and read by everything derived. It is not part of `stats` and not part
    // of `gearMods`, because it is a third thing: not the character, and not what
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

    // What both hands are granting - see SetGearMods. Not part of `stats` because
    // it is not the character - it is what they picked up, and it goes away when
    // they put it down.
    Modifiers gearMods;

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
    // Forces a raised (or still raising, or lowering) shield down immediately and
    // starts its recovery timer - see the note on blockCooldown. Called the
    // instant a shield actually stops something, regardless of whether the
    // button is still held: the timer is what makes blocking again take a
    // moment, not letting go of the button.
    void DropBlock();

    // Brings `maxHealth` back in line with `stats`, carrying the current health up
    // with it by however much the pool grew. Growing the pool without growing the
    // health in it means a point of constitution makes the bar longer and the
    // character no safer, which is the opposite of what it was spent for.
    void RefreshHealth();

    // What the traits are granting. Derived - see SetModifiers - and never written
    // from anywhere but there.
    Modifiers mods;

    // What a Buff pickup is granting right now, and how much longer it runs -
    // see ApplyBuff. Zero and empty is the neutral state; ticked down in Update
    // and cleared the frame it reaches zero, rather than left sitting at zero
    // and checked with HasBuff every time, so a stale, spent Modifiers is never
    // one frame away from silently being summed in again.
    Modifiers buffMods;
    float buffTimeLeft = 0.0f;
    BuffKind activeBuff = BuffKind::Might;   // Meaningless while buffTimeLeft is 0

    // What combat actually reads: the traits, the running buff and whatever
    // both hands are granting, summed. Every derived figure below goes through
    // this and never through `mods` alone - see the class note on ApplyBuff for
    // why the three are kept as separate fields right up until here.
    Modifiers Combined() const;

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

    // Seconds left before a dropped shield can be raised again - zero means it
    // is free to go up. Set by DropBlock the instant a blow lands on it, so
    // holding the button back down does not just raise it a second time.
    float blockCooldown = 0.0f;

    // Seconds the shield has been continuously up (IsBlocking() true) since it
    // last went up. What TakeDamageFrom reads against Config::ParryWindow to
    // tell a parry from an ordinary block: small means the blow landed right as
    // the shield went up, which is the timed read a parry is supposed to reward.
    float blockActiveTime = 0.0f;
};
