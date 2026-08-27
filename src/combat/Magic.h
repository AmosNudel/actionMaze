#pragma once

#include "combat/MagicKind.h"
#include "raylib.h"
#include "render/Vfx.h"

//----------------------------------------------------------------------------------
// The magic the player casts, one entry per school.
//
// Everything a school IS lives in the table in Magic.cpp: what colour it flies as,
// what it looks like when it lands, how fast it travels and how hard it hits.
// Nothing outside that table branches on which magic it is, so adding a ninth is a
// row and not a change - which is the whole reason the mote and the impact are one
// struct rather than two parallel lists that can drift apart.
//
// The order is the debug key order: School 0 is the 1 key, and so on up to 8. That
// coupling is deliberate and is the reason this enum is contiguous from zero.
//----------------------------------------------------------------------------------
// The Magic enum itself is in combat/MagicKind.h - see the note there for why the
// names and the table are in different files.

//----------------------------------------------------------------------------------
// One school.
//
// Two colours, and the difference between them is the difference between the two
// halves of a cast. `colour` is the mote in flight and the light it is made of;
// `impactTint` is what the impact sheet is multiplied by when it lands. They are
// not the same value because the sheets are not neutral: the explosion art is
// already orange and the poison art already green, so tinting them with the mote's
// own colour a second time doubles the hue and darkens the frame. A sheet drawn in
// its school's colour takes WHITE and stays as authored; only the ones borrowed
// from a school they were not drawn for carry a real tint.
//----------------------------------------------------------------------------------
struct MagicDef
{
    const char *name;       // Shown on the debug readout, so keep it short

    // Which row this is. A plain mirror of the table's own position - the enum is
    // contiguous and the table is indexed by it - but the enemy hit path only ever
    // holds a `const MagicDef *` (see ProjectileLook::magic), and a pointer into a
    // file-local table cannot be turned back into an index from outside Magic.cpp.
    // Carrying the answer here is one field rather than exposing the table.
    Magic school;

    Color colour;           // The mote in flight
    VfxKind impact;         // The sheet that plays where it lands
    Color impactTint;       // What that sheet is multiplied by

    float moteRadius;       // World units, the bright core; the glow is drawn wider
    float impactSize;       // World units across the impact billboard is drawn
    float speed;            // World units a second in flight

    //------------------------------------------------------------------------------
    // Every OTHER living enemy within this of the impact point takes the same
    // blow - and the same effect (see Enemy::ApplyMagicEffect) - the mote's real
    // target did. Applied in ProjectileManager::Advance, the same place NOVA's
    // own burst always was; every school gets one now instead of just that one.
    //
    // Still set against each other rather than to one shared number: NOVA stays
    // the widest because "the one real area of effect" is still its whole
    // identity, and FLASH/SPARK stay tight because a school that is fast and
    // small in every other column reading as fast and small here too is the
    // point, not an oversight.
    //------------------------------------------------------------------------------
    float aoeRadius;

    //------------------------------------------------------------------------------
    // Multiple of the caster's SPELL POWER, not a damage figure.
    //
    // Spell power is arcane and only arcane (see Player::SpellPower), and that is
    // the one thing to know about balancing magic: a character that put its points
    // into arms has a strong weapon and ordinary spells, and one that put them into
    // arcane has the reverse. At neutral arcane spell power is exactly
    // Config::BaseSpellPower, so every multiplier here means what it says at level
    // one, and it goes on meaning it for the whole run - because enemy health grows
    // linearly with rank and so does the arcane line underneath this.
    //
    // The band is deliberately narrow (0.8 to 1.7). A school's real spread is in
    // its SPEED and its impact, and the multiplier is only there to make the fast
    // ones worth less per hit than the slow ones.
    //------------------------------------------------------------------------------
    float damageMult;
};

// The table, by index. `Count` entries, in enum order.
const MagicDef &MagicAt(Magic magic);

// Steps the selection by `by` schools, wrapping both ways. What the debug keys and
// any future cycle binding go through, so "past the end" is answered once.
Magic MagicStep(Magic from, int by);
