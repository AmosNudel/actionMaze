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
    // The band is deliberately narrow (0.8 to 1.9). A school's real spread is in
    // its EFFECT, its reach and its speed, and the multiplier is only there to
    // make the fast ones and the utility ones worth less per hit than the two
    // that are bought to kill things.
    //------------------------------------------------------------------------------
    float damageMult;

    //------------------------------------------------------------------------------
    // Every OTHER living enemy within this of the impact point takes the same
    // blow - and the same effect (see Enemy::ApplyMagicEffect) - the mote's real
    // target did. Applied in ProjectileManager::Advance, the same place NOVA's
    // own burst always was; every school gets one now instead of just that one.
    //
    // Except SPARK, which is ZERO, and that is the whole shape of the table now:
    // seven schools that answer a ROOM and one that answers a BODY. SPARK pays
    // for being alone in that column by critting every time it lands (see
    // Enemy::ApplyMagicEffect's note), so the choice a player makes when they
    // cycle onto it is "this one thing, hard" against "all of them, less".
    //
    // Still set against each other rather than to one shared number: BLAST is the
    // widest because raw reach and raw damage are what it is, NOVA is next
    // because a shove that did not catch a pack would not move a fight, and REND
    // is the tightest of the seven because every body it catches pays the caster
    // health back and a lifesteal with no range limit is a health bar that never
    // empties.
    //
    // Declared AFTER damageMult on purpose - see aoeMaxTargets below, which is
    // now the trailing column that has to stay last.
    //------------------------------------------------------------------------------
    float aoeRadius;

    //------------------------------------------------------------------------------
    // At most this many OTHER bodies the burst may take, nearest first. Zero means
    // no cap, which is what every school but FLAME says.
    //
    // FLAME is the reason this column exists: "it spreads to what is next to it"
    // and "it hits everything in a five unit circle" are the same code and very
    // different spells, and the difference between them is entirely a number. A
    // fire that takes the three nearest bodies reads as catching; the same fire
    // with no cap reads as a second BLAST that happens to be orange.
    //
    // Nearest first rather than whatever the enemy vector happened to hold, so
    // the three it takes are the three the player can see it should have.
    //
    // This is the trailing column and must STAY last, for the reason the note on
    // aoeRadius used to carry: the table below is positional, so a field inserted
    // ahead of another silently shifts every row's remaining values by one - no
    // compile error, just every school reading the wrong number for everything.
    //------------------------------------------------------------------------------
    int aoeMaxTargets;
};

// The table, by index. `Count` entries, in enum order.
const MagicDef &MagicAt(Magic magic);

// Steps the selection by `by` schools, wrapping both ways. What the debug keys and
// any future cycle binding go through, so "past the end" is answered once.
Magic MagicStep(Magic from, int by);
