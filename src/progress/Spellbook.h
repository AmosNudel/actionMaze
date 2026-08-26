#pragma once

#include "combat/MagicKind.h"
#include "combat/Modifiers.h"

//----------------------------------------------------------------------------------
// Which schools the player has, how far each has been empowered, and what a cast
// costs.
//
// The number keys used to reach all eight from the first frame, and the header on
// InputState says in as many words that it was a debug binding standing in for a
// spellbook. This is the spellbook. A run starts with one school and the mystic sells
// the rest.
//
// --- Mana, and why it comes from killing --------------------------------------------
// Without a cost, standing at the back of a room cycling motes is strictly better than
// closing with anything, and the schools stop being a decision. So a cast is paid for,
// and the pool refills by KILLING - mostly by killing with a weapon.
//
// The invariant, carried over from the mobile game and worth writing down because it
// is the thing a new school can quietly break: A SPELL CAN NEVER FUND ITSELF. A mote
// kills at most one body, and a spell kill pays less than a cast costs. Any change to
// Config::ManaPerSpellKill or to a school's cost has to be checked against that, or
// the pool becomes a perpetual motion machine and melee becomes pointless.
//
// Spell kills pay at all - rather than nothing - because schools key off ARCANE alone.
// A character who spent everything on arcane has strong spells and a base-10 sword, so
// under a rule of "only weapons pay" that build starves itself and arcane is a trap
// wearing the costume of a choice.
//
// ARCANE also raises the POOL, which is a reservoir and not a faucet: a caster banks
// more casts by fighting for them.
//
// --- Empowering ---------------------------------------------------------------------
// The mystic raises a school's empower level. Each level adds a fraction of that
// school's own damage multiplier and widens its impact a little. Same rule as the
// forge: it makes the SPELL better, never the character, so the curve stays linear.
//----------------------------------------------------------------------------------

constexpr int SpellEmpowerMax = 5;

// Of the school's own damage multiplier, per level. Additive against the table figure.
constexpr float SpellEmpowerDamage = 0.14f;

// Of the school's own impact size, per level. Small, and deliberately: the impact is
// a picture of force rather than an area of effect, so growing it is feedback that a
// spell got stronger and not a second, hidden bonus.
constexpr float SpellEmpowerSize = 0.06f;

class Spellbook
{
public:
    // A fresh book: one school, nothing empowered, and no mana until something dies.
    void Reset(Magic starting);

    bool Owns(Magic magic) const;
    void Give(Magic magic);

    int Empower(Magic magic) const;
    bool CanEmpower(Magic magic) const;
    void RaiseEmpower(Magic magic);

    // Gems. Both climb with the school's own damage multiplier, so the heavy end of
    // the table costs more to buy and more to deepen.
    int Price(Magic magic) const;
    int EmpowerPrice(Magic magic) const;

    // Multipliers from the empower level: 1.0 at zero, applied to the SCHOOL's own
    // figures rather than to the caster's.
    float DamageMult(Magic magic) const;
    float SizeMult(Magic magic) const;

    // What one cast of `magic` costs, after `mods`. Floored at 1: a school made free
    // by enough cooldown reduction would take the whole system with it.
    int CostOf(Magic magic, const Modifiers &mods) const;

    int OwnedCount() const;

    //------------------------------------------------------------------------------
    // The next owned school at or after `from`, cycling in `step`.
    //
    // Unlike the weapon wheel there is no "none" slot: a caster always has a school
    // selected, because selecting one costs nothing and casting is what costs. It
    // returns `from` unchanged when the player owns exactly one, which is what a
    // cycle key should do rather than silently unselecting it.
    //------------------------------------------------------------------------------
    Magic NextOwned(Magic from, int step) const;

private:
    unsigned char owned[(int)Magic::Count] = { 0 };
    unsigned char empower[(int)Magic::Count] = { 0 };
};
