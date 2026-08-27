#pragma once

#include "combat/AttackStyle.h"
#include "combat/StatBlock.h"

#include <string>

//----------------------------------------------------------------------------------
// What KIND of thing a weapon is, for anyone showing a list of them - the shop, the
// character page's Inventory tab - rather than for combat, which already has style
// and the four behaviours for that.
//
// A mask rather than one enum because a few weapons are legitimately more than one
// of these at once: a wand is both Casting and Ranged, a dagger is both OneHanded
// and Thrown.
//----------------------------------------------------------------------------------
enum WeaponTag : unsigned
{
    TagOneHanded = 1u << 0,
    TagTwoHanded = 1u << 1,
    TagCasting   = 1u << 2,
    TagRanged    = 1u << 3,
    TagThrown    = 1u << 4,
    TagBlocking  = 1u << 5,
};

// Compact, in a fixed order, for a shop row or an inventory line - "1H  CAST" or
// "2H  RANGED  THROWN". By value and not into a shared buffer - see the note on
// ShopScreen::Row for exactly the bug that shape caused once already.
std::string WeaponTagsText(unsigned tags);

//----------------------------------------------------------------------------------
// What a weapon does, as opposed to what it looks like.
//
// Reach is how far from the eye the tip of the blade ends up. It is not where the
// view model is drawn - the weapons render as miniatures about half a unit from
// the camera, well inside the player's own collision radius - it is how far
// ViewModel::BladeFor magnifies that drawn pose to build the hit capsule. So the
// swing's direction and arc come from the animation and only its length comes
// from here.
//
// The default is derived from the model's measured height, which the view model
// already knows, so a dagger does not reach as far as a greatsword without
// anyone hand authoring twenty one numbers.
//----------------------------------------------------------------------------------
struct WeaponStats
{
    float reach = 2.0f;         // World units from the eye to the tip at full extension
    float bladeRadius = 0.12f;  // How fat the blade is as a hit volume
    int damage = 20;

    // The stroke's live window, in attack blend. Outside it the blade is drawn but
    // does not cut: below `liveFrom` the weapon is still coming up out of rest, and
    // a weapon that hit from the first frame of the animation would land blows with
    // its own grip. There is no window on the way back - one swing, one direction.
    float liveFrom = 0.25f;
    float liveTo = 1.0f;

    // Two flags rather than one, because "not melee" is not the same as "ranged":
    // an empty hand and a shield are neither. Inferring one from the other is how
    // bare hands end up casting spells.
    bool melee = true;
    bool ranged = false;

    // For the Cast and Throw styles. Ignored entirely unless `ranged`.
    //
    // `releaseAt` is the same idea as the enemies' EnemyShootRelease: a blend, not
    // a time, so the bolt leaves when the staff is actually pointing somewhere
    // rather than on the frame the button went down. Below it the weapon is still
    // coming up out of rest.
    float projectileSpeed = 22.0f;
    float releaseAt = 0.65f;

    //------------------------------------------------------------------------------
    // What holding this weapon is worth, over and above what it does when it lands.
    //
    // This is where armour would be in a game that had any. It does not, and is not
    // getting any: an FPS whose whole loadout is two visible hands has nowhere to
    // put a chest piece the player cannot see, and a stat that comes from a slot
    // nobody looks at is a spreadsheet entry rather than a decision. So the weapon
    // carries it. Picking up the greatsword IS the build choice, and it is one the
    // player can see in their own hands from the frame they make it.
    //
    // Both hands count. Two weapons held are two sets of bonuses, which is what
    // makes the off hand a real slot rather than a place to keep a shield.
    //
    // The bonus is expressed as an OFFSET from neutral, not as a stat line: a
    // weapon giving `+4 arms` is +4 whatever the character underneath it is, and
    // adding blocks would make two weapons of +10 arms into a character of 20 arms
    // rather than one of +20.
    //------------------------------------------------------------------------------
    StatBlock bonus = { 0, 0, 0, 0 };

    //------------------------------------------------------------------------------
    // Behaviours.
    //
    // All four are OFF at zero, so a weapon that wants none of them leaves them
    // zero and nothing downstream branches on which weapon it is. A new behaviour
    // belongs here as another column with the same property, never as a special
    // case at the call site - that is the rule the whole table is built on.
    //
    // They are the reason a weapon is more than its damage number. Two weapons at
    // the same damage and the same reach are the same weapon; one that drinks and
    // one that staggers are two different fights.
    //------------------------------------------------------------------------------

    // Fraction of the damage this weapon deals, returned as health. The sustain
    // build: it makes wading in the way you stay alive rather than something you
    // survive, and it is worth nothing at all if you are not connecting.
    float lifesteal = 0.0f;

    // Added flat to the crit chance while this weapon is the one striking. On the
    // weapon rather than on the character because it is the light, quick ones that
    // carry it - a knife finds the gap, a hammer makes its own.
    float critBonus = 0.0f;

    // Seconds an enemy struck by this is frozen for. What the heavy end of the
    // table trades its speed for: a hammer that lands takes the answer away.
    float stun = 0.0f;

    // World units a second of shove, along the blow. Reads as weight, and it is
    // the crowd answer - a weapon that pushes buys the room to swing again.
    float knockback = 0.0f;

    // What kind of thing this is, for a list rather than for combat - see
    // WeaponTag above.
    unsigned tags = 0;
};

// modelHeight comes from the loaded model's bounding box
WeaponStats StatsFor(const std::string &name, AttackStyle style, float modelHeight);
