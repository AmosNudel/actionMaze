#include "combat/Magic.h"

namespace
{
    //------------------------------------------------------------------------------
    // The table, in Magic order.
    //
    // The columns are set against each other rather than against absolute numbers:
    // the eight are meant to be visibly different in the air and on impact, and the
    // damage spread pays for the reach and the size.
    //
    //   Flame   the ordinary one - middling everything, a wide burn where it lands
    //   Spark   fast and small. Least damage per hit, arrives before you can move
    //   Toxin   slow, fat, green. The mote is the biggest and the easiest to dodge
    //   Blast   slow and heavy. The widest impact on the table
    //   Splash  quick and cold, a small tight burst
    //   Flash   the snap - fastest, smallest impact, hits once and is gone
    //   Nova    a slow heavy flare, the wide flash
    //   Rend    close and physical, drawn as matter rather than light
    //
    // Damage is a multiple of SPELL POWER rather than a figure - see the note on
    // MagicDef::damageMult. The two ends of the band are the two ends of the speed
    // range and the pairing is not a coincidence: SPARK arrives before the player
    // can move and is worth 0.8, BLAST can be walked out of and is worth 1.7. What
    // a school costs to land is what it is paid for landing.
    //
    // Speed is the readability dial and not just a stat: below about 18 units a
    // second the mote is a thing you watch cross the room, and above about 40 it is
    // a line that was already there. Both are legitimate - Toxin is meant to be
    // watched and Flash is meant to be instant - and everything in between reads as
    // one or the other depending which end it is nearer.
    //
    // The impact sizes are set against an enemy standing about two units tall: 1.6
    // is a hit ON one, and 3.4 is a blast that covers it and whatever it was next
    // to. NOVA alone has a real area of effect (Config::NovaRadius, applied where a
    // mote of it lands - see ProjectileManager::Advance); everywhere else a wide
    // impact is still a picture of force and not a promise about who was caught in
    // it, which is worth being honest about before someone tunes BLAST expecting
    // the second thing.
    //
    // Elements and resistances were the plan here once - see the older note in
    // combat/Stats.h - and are not any more: an enemy never answered one
    // differently to another, so a resistance table would have changed nothing a
    // player could feel. What each school does INSTEAD is its own effect, in
    // `school` below and applied through Enemy::ApplyMagicEffect: SPARK always
    // crits, FLAME burns and can jump once to a neighbour, TOXIN stacks into a
    // panic, BLAST shoves, SPLASH chills, FLASH blinds, REND bleeds. That is a
    // difference a player watching the fight can actually see.
    //------------------------------------------------------------------------------
    constexpr MagicDef Table[(int)Magic::Count] =
    {
        // name      school           colour                        impact              tint    mote  impact speed mult
        { "FLAME",  Magic::Flame,  { 255, 110,  40, 255 }, VfxKind::Flame,     WHITE,          0.13f, 2.6f, 26.0f, 1.10f },
        { "SPARK",  Magic::Spark,  { 150, 170, 255, 255 }, VfxKind::Lightning, WHITE,          0.10f, 2.2f, 44.0f, 0.80f },
        { "TOXIN",  Magic::Toxin,  { 130, 225,  95, 255 }, VfxKind::Poison,    WHITE,          0.17f, 3.0f, 17.0f, 0.90f },
        { "BLAST",  Magic::Blast,  { 255, 145,  55, 255 }, VfxKind::Explosion, WHITE,          0.16f, 3.4f, 20.0f, 1.70f },
        { "SPLASH", Magic::Splash, {  90, 205, 240, 255 }, VfxKind::Splash,    { 150, 220, 255, 255 }, 0.12f, 2.0f, 32.0f, 1.00f },
        { "FLASH",  Magic::Flash,  { 255, 225, 140, 255 }, VfxKind::Muzzle,    WHITE,          0.10f, 1.6f, 50.0f, 1.20f },
        { "NOVA",   Magic::Nova,   { 255, 195,  90, 255 }, VfxKind::MuzzleBig, WHITE,          0.15f, 3.2f, 22.0f, 1.40f },
        // The one drawn as matter. Its sheet is blood, which is already red, so the
        // tint stays white and the mote carries the colour on its own.
        { "REND",   Magic::Rend,   { 210,  55,  60, 255 }, VfxKind::Blood,     WHITE,          0.14f, 2.0f, 28.0f, 1.50f },
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
