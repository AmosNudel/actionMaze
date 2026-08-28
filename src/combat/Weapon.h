#pragma once

#include "combat/AttackStyle.h"
#include "combat/Modifiers.h"

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
// What a weapon is CALLED, from the name of the file it was cut out of.
//
// The pack names its models by shape and letter - sword_A, hammer_C - and that name
// is an identity: it is what the arsenal keys on, what the stat overrides match
// against, and what WeaponPreview looks the model up by. It is not a name to show
// anybody. A merchant selling "sword_D" is a merchant selling a filename.
//
// So the display name is a second column rather than a rename. The model name stays
// exactly what it was and nothing that keys on it has to change; this is read only
// where a human is going to see the result.
//
// Falls back to the model name itself for anything not in the table, so a weapon
// dropped into the pack tomorrow shows up in the shop under its filename rather
// than under an empty string - visibly unfinished rather than invisibly broken.
//
// Takes and returns a `const char *` rather than a std::string deliberately: the
// fallback hands BACK the caller's own pointer, and a std::string parameter would
// make that a pointer into a temporary that dies at the end of the expression. The
// arsenal stores its names as strings that outlive any call, so this is safe as
// long as the signature stays this shape.
//----------------------------------------------------------------------------------
const char *WeaponDisplayName(const char *modelName);

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
    // A Modifiers, restricted BY CONVENTION to its flat and fraction columns -
    // never `.stat` - see the long note at the top of combat/Modifiers.h. A
    // weapon changes what the numbers it touches DO (a sword hits harder, a
    // shield takes less), not the stats those numbers are derived from: a
    // shield that quietly raised constitution used to read as extra health
    // rather than as what standing behind it is actually worth, which is the
    // one thing this column exists to avoid.
    //------------------------------------------------------------------------------
    Modifiers bonus;

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

    //------------------------------------------------------------------------------
    // How many blows this shield stops before the guard is knocked down.
    //
    // The one number that separates the three shields. Every shield used to stop
    // exactly one blow and then go into recovery, which made "which shield" a
    // question with no answer: they carried identical damage reduction and identical
    // everything else, so the only difference between them was the picture.
    //
    // A charge is spent per blow the guard actually ATE, and the guard drops when
    // the last one goes - see Player::TakeDamageFrom. That makes a heavier shield
    // worth carrying against a PACK specifically, which is the situation a shield is
    // for and the one where a single-blow guard was worth least: three skeletons
    // swinging meant the first was blocked and the next two were not.
    //
    // Zero on everything that is not a shield, and unread there - the block path
    // only ever runs for a hand carrying TagBlocking.
    //------------------------------------------------------------------------------
    int blockCharges = 1;

    //------------------------------------------------------------------------------
    // How much of this weapon's damage also counts toward an enemy's poise meter -
    // see Enemy::TakeDamageFrom and Config::EnemyPoiseRecovery. 1.0 by default, set
    // lower for the Throw style (Config::ThrowPoiseScale).
    //
    // A ranged weapon that filled the meter exactly as fast as a melee one at the
    // same DPS would still stagger a body just as hard from a distance nothing can
    // answer from - which is backwards for the one weapon built to be used that
    // way. Melee weapons leave this at 1.0: closing the distance is already the
    // cost of using one.
    //------------------------------------------------------------------------------
    float poiseScale = 1.0f;

    // What kind of thing this is, for a list rather than for combat - see
    // WeaponTag above.
    unsigned tags = 0;
};

// modelHeight comes from the loaded model's bounding box
WeaponStats StatsFor(const std::string &name, AttackStyle style, float modelHeight);
