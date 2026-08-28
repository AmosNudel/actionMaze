#include "combat/Magic.h"

namespace
{
    //------------------------------------------------------------------------------
    // The table, in Magic order.
    //
    // Every school has ONE thing it is for, and the columns are set to make that
    // thing legible rather than to hit an absolute number:
    //
    //   Flame   spreads. The burst is capped at three bodies, so it CATCHES on
    //           what is next to the target rather than filling a circle, and
    //           every burning body keeps the fire drawn on it while it ticks
    //   Spark   the one single target school - no burst at all, and every hit of
    //           it is a critical strike. What you cycle to for one hard problem
    //   Toxin   the long one. Ten seconds of poison on everyone it caught, which
    //           nothing else on the table comes near
    //   Blast   the kill button. Widest burst, hardest hit, and it costs the most
    //           of any school to cast because of both
    //   Splash  strips defences. Everyone it caught takes MORE from everything -
    //           the sword, the burn, the next cast - for six seconds
    //   Flash   blinds. Everyone it caught loses the player entirely
    //   Nova    moves the room. A hard shove out of the impact, and it interrupts
    //           whatever the shove caught them doing
    //   Rend    feeds the caster. Every body it damages pays a share of that back
    //           as health, so a rend into a pack is a heal
    //
    // Damage is a multiple of SPELL POWER rather than a figure - see the note on
    // MagicDef::damageMult - and the spread of it now follows what a school DOES
    // rather than how fast it flies. BLAST and REND are the two bought to kill and
    // sit at the top; the four utility schools sit near 1.0 because a blind or a
    // sunder that also hit hardest would make the effect a bonus rather than the
    // reason; SPARK sits at the bottom because it crits every time and a
    // guaranteed critical on top of a high multiplier is the same number twice.
    //
    // What a school costs to cast comes straight off that column (see
    // Spellbook::CostOf), which is the balance: BLAST is the most expensive thing
    // in the book and SPARK is nearly the cheapest, so a pool holds about half as
    // many of the first as the second.
    //
    // Speed is the readability dial and not just a stat: below about 18 units a
    // second the mote is a thing you watch cross the room, and above about 40 it is
    // a line that was already there. Both are legitimate - Toxin is meant to be
    // watched and Spark is meant to be instant - and everything in between reads as
    // one or the other depending which end it is nearer.
    //
    // impactSize and aoeRadius are kept equal on every row on purpose. The picture
    // IS the area now: an impact drawn smaller than the burst is a spell that
    // quietly hits things it never appeared to touch, and one drawn larger is a
    // spell the player will swear missed. SPARK is the exception at both ends - it
    // has no burst, so its impact is sized to the one body it hit.
    //
    // Elements and resistances were the plan here once - see the older note in
    // combat/Stats.h - and are not any more: an enemy never answered one
    // differently to another, so a resistance table would have changed nothing a
    // player could feel. What each school does INSTEAD is its own effect, applied
    // through Enemy::ApplyMagicEffect, or - for the three that need something an
    // enemy does not have - in ProjectileManager::Advance: NOVA's shove wants the
    // impact point, REND's lifesteal wants the player, and SPARK's crit was
    // already rolled at the cast.
    //------------------------------------------------------------------------------
    constexpr MagicDef Table[(int)Magic::Count] =
    {
        // name       school          colour                     impact              tint                     mote  impact speed mult   aoe  cap
        { "FLAME",  Magic::Flame,  { 255, 110,  40, 255 }, VfxKind::Flame,     WHITE,                  0.13f, 5.0f, 26.0f, 1.05f, 5.0f, 3 },
        { "SPARK",  Magic::Spark,  { 150, 170, 255, 255 }, VfxKind::Lightning, WHITE,                  0.10f, 2.4f, 46.0f, 0.80f, 0.0f, 0 },
        { "TOXIN",  Magic::Toxin,  { 130, 225,  95, 255 }, VfxKind::Poison,    WHITE,                  0.17f, 4.6f, 17.0f, 0.90f, 4.6f, 0 },
        { "BLAST",  Magic::Blast,  { 255, 145,  55, 255 }, VfxKind::Explosion, WHITE,                  0.16f, 6.5f, 20.0f, 1.90f, 6.5f, 0 },
        { "SPLASH", Magic::Splash, {  90, 205, 240, 255 }, VfxKind::Splash,    { 150, 220, 255, 255 }, 0.12f, 4.6f, 32.0f, 0.95f, 4.6f, 0 },
        { "FLASH",  Magic::Flash,  { 255, 225, 140, 255 }, VfxKind::Muzzle,    WHITE,                  0.10f, 4.2f, 50.0f, 0.85f, 4.2f, 0 },
        { "NOVA",   Magic::Nova,   { 255, 195,  90, 255 }, VfxKind::MuzzleBig, WHITE,                  0.15f, 5.8f, 22.0f, 1.10f, 5.8f, 0 },
        // The one drawn as matter. Its sheet is blood, which is already red, so the
        // tint stays white and the mote carries the colour on its own. The tightest
        // burst of the seven - see the note on MagicDef::aoeRadius for why a
        // lifesteal in particular cannot also have reach.
        { "REND",   Magic::Rend,   { 210,  55,  60, 255 }, VfxKind::Blood,     WHITE,                  0.14f, 3.6f, 28.0f, 1.60f, 3.6f, 0 },
    };
}

const MagicDef &MagicAt(Magic magic)
{
    const int index = (int)magic;

    // Clamped rather than asserted: a bad index here is a debug key or a saved
    // selection that outlived a table edit, and the answer to both is the first
    // school rather than the frame going down
    if (index < 0 || index >= (int)Magic::Count) return Table[0];

    return Table[index];
}

Magic MagicStep(Magic from, int by)
{
    constexpr int count = (int)Magic::Count;

    // The C remainder is negative for a negative left side, so it is brought back
    // round before it is used - stepping down off the first school lands on the last
    int index = ((int)from + by) % count;
    if (index < 0) index += count;

    return (Magic)index;
}
