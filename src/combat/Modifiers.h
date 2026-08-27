#pragma once

#include "combat/StatBlock.h"

//----------------------------------------------------------------------------------
// One struct for every bonus anything can grant the player.
//
// Weapon upgrades, spell upgrades and traits all do the same job: they sit on top of
// the character and change a number. Written as three separate mechanisms they would
// need three sets of hooks in combat, three places to remember when a new bonus is
// invented, and three chances to get the sum wrong. So there is ONE struct, every
// source fills one in, and Player adds them up.
//
// Adding a new kind of bonus is a field here plus the one place that reads it. It is
// never a branch on which trait granted it - the same property the magic table and
// the room table are built on.
//
// --- POINTS vs ABSOLUTE, and why the split is load-bearing -------------------------
// Stats.h is explicit that a PERMANENT bonus which multiplies an already-climbing
// figure makes the player quadratic against linear enemy health - the exact failure
// the enemy rank system exists to prevent. So two kinds of source use two columns:
//
//   traits and points   -> `stat`, in stat POINTS. A point is a percentage of the
//                          character's BASE line, added and never compounded, so the
//                          curve stays linear. Earned by levelling or bought from
//                          the captain - a permanent, deliberate build choice.
//   anything conditional -> the flat and fraction columns, worked out from the
//                          player's CURRENT line at the moment they apply. A spike on
//                          a duty cycle preserves the shape while still being worth
//                          having late - and this is the ONLY column a WEAPON'S OWN
//                          bonus is allowed to use (see Weapon.cpp's Overrides
//                          table): a sword should make blows hit harder while it is
//                          in your hand, not quietly raise your arms as though you
//                          had spent a point on it, which is a bonus indistinguishable
//                          from a stat point except that it vanishes the moment the
//                          weapon is put down - the exact confusion a shield's old
//                          +constitution used to cause, reading as a bigger health
//                          pool rather than as what standing behind it is actually
//                          worth. A buff pickup is the other tenant of this column,
//                          for the identical reason - see combat/Buff.h.
//
// Everything else here is a fraction ADDED to a fraction, never multiplied together,
// so two sources of +10% give 20% and not 21%.
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// One "stat A gains a percentage of stat B" rule.
//
// Traits grant these, and they are applied AFTER every point bonus has been summed -
// so "25% of arcane" means 25% of the arcane the character actually ended up with,
// weapons and upgrades included. That ordering is the whole value of the rule: a
// conversion that read the raw spent line would be a flat bonus wearing a costume.
//----------------------------------------------------------------------------------
struct StatConvert
{
    unsigned char from = 0;     // A Stat, read
    unsigned char to = 0;       // A Stat, credited
    float frac = 0.0f;          // How much of `from` is added to `to`
};

constexpr int MaxStatConverts = 4;

struct Modifiers
{
    // --- Permanent sources: stat points -------------------------------------------
    // Added to the spent line before anything derives from it. Zeroed rather than
    // neutral: this is an OFFSET, and a default StatBlock is 10s.
    StatBlock stat = { 0, 0, 0, 0 };

    // --- Absolute amounts ---------------------------------------------------------
    int flatHealth = 0;
    int flatDamage = 0;
    int flatSpell = 0;
    int flatMana = 0;

    // --- Fractions. 0 = no change, 0.25 = +25% ------------------------------------
    float attackSpeed = 0.0f;       // Swing and cast rate
    float moveSpeed = 0.0f;
    float critChance = 0.0f;        // Added FLAT to the rolled chance, not scaled
    float lifesteal = 0.0f;         // Of damage dealt, returned as health
    float spellPower = 0.0f;        // On top of what arcane already bought

    // NEGATIVE is cheaper. Written that way round so that every fraction in this
    // struct means "more of the thing named" and a reader never has to remember
    // which one is inverted.
    float manaCost = 0.0f;

    int manaPerKill = 0;            // On top of Config::ManaPerKill

    //------------------------------------------------------------------------------
    // Conversions, applied last. See StatConvert.
    //
    // A fixed array rather than a vector: a Modifiers is summed every time anything
    // is equipped and lives on the Player by value, and four is more than the trait
    // table will ever want. Converts past the fourth are dropped rather than
    // reallocating, which is a limit worth having written down.
    //------------------------------------------------------------------------------
    StatConvert convert[MaxStatConverts];
    int convertCount = 0;

    //------------------------------------------------------------------------------
    // Gear's own two columns - added last and kept apart from the fractions
    // above them on purpose: those are all trait territory already, exercised
    // by the positional table in progress/Traits.cpp, and a field inserted
    // between two existing ones there silently reinterprets every row after it
    // rather than failing to compile. New fields belong at the end for exactly
    // that reason, this pair included.
    //------------------------------------------------------------------------------

    // The melee/ranged counterpart of spellPower - on top of what arms already
    // bought. Exists so a weapon's own bonus (see Weapon.cpp's Overrides table)
    // can say "hits harder" directly rather than through a stat point that
    // would also move whatever else that stat happens to touch - see the long
    // note above on why gear works this way and traits do not.
    float damageDealt = 0.0f;

    // Fraction MORE damage taken - negative is safer, the same convention as
    // manaCost above. What a shield grants instead of constitution: a bigger
    // health pool is not what standing behind one is worth, less of every
    // blow actually landing is.
    float damageTaken = 0.0f;
};

// a + b, column by column. Fractions add rather than compound - see the note above.
Modifiers ModifiersAdd(const Modifiers &a, const Modifiers &b);

// The spent line plus every point bonus, with the conversions applied on top. The one
// place the two kinds of stat source are combined, so nothing downstream has to know
// there were two.
StatBlock ApplyModifiers(const StatBlock &spent, const Modifiers &mods);

// A one-line summary of everything a Modifiers grants ("+4 ARMS  +8% crit"), for the
// trait list and the upgrade rows. Static buffer, valid until the next call - the
// same idiom TextFormat uses, and for the same reason.
const char *ModifiersText(const Modifiers &mods);
