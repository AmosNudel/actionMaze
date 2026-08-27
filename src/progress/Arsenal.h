#pragma once

#include "combat/Modifiers.h"

#include <string>
#include <vector>

//----------------------------------------------------------------------------------
// Which weapons the player owns, and how far each has been forged.
//
// The view model loads every model in assets/models/weapons and used to let the wheel
// reach all of them from the first frame. That was right while the weapons were being
// tuned and is wrong now that there is a merchant: a shop that sells what you already
// have is a shop with nothing in it.
//
// So the wheel cycles the OWNED set. A run starts with one weapon (see
// Config::StartingWeapon) and the rest are bought.
//
// --- Why this is not part of ViewModel ---------------------------------------------
// ViewModel owns the models, the poses and the hit capsules - what a weapon LOOKS and
// swings like. What the player has paid for is a different question with a different
// lifetime: it survives a floor change, it is written by a shop that knows nothing
// about rendering, and it would still mean something in a build with no view model at
// all. The two are joined by index and nothing else.
//
// --- Forging ------------------------------------------------------------------------
// The merchant raises a weapon's forge level, and each level adds a fraction of the
// weapon's OWN damage plus a point of arms while it is held. Not a flat number and not
// a multiplier on the character: a forged dagger is a better dagger, and it is better
// by the same amount at level 40 as at level 1 - which is the rule Stats.h lays down
// and the reason the enemy rank curve still means anything.
//----------------------------------------------------------------------------------

// How many weapons this can track. The pack ships about twenty; the ceiling is here so
// the owned set is a fixed array on the Player rather than an allocation per run.
constexpr int MaxWeapons = 64;

// How many times one weapon may be forged, and what each level buys.
constexpr int WeaponForgeMax = 5;

// Of the weapon's own damage, per level. Additive against the table figure - five
// levels is +60% of what the weapon started at, never compounded.
constexpr float WeaponForgeDamage = 0.12f;

// Arms granted per forge level while the weapon is held. A forged weapon is worth
// carrying for what it does to the character as well as for what it does on impact,
// which is what stops forging being a damage slider.
constexpr int WeaponForgeArms = 1;

//----------------------------------------------------------------------------------
// One weapon as the arsenal needs to see it.
//
// Three fields out of what the view model and WeaponStats already know, rather than
// either type itself: the arsenal has no use for a model, a pose or a hit capsule,
// and taking a ViewModel here would join the shop to the renderer for the sake of one
// integer.
//----------------------------------------------------------------------------------
struct WeaponListing
{
    const char *name = "";
    int damage = 0;
    float reach = 0.0f;

    // From WeaponStats::tags (combat/Weapon.h) - what KIND of weapon this is, so
    // the shop and the arsenal can answer "is there a castable one" without
    // reaching back into the view model that only lives in Game.
    unsigned tags = 0;
};

class Arsenal
{
public:
    //------------------------------------------------------------------------------
    // Sized to the view model's weapon list and given the starting kit.
    //
    // `startingName` is matched case-insensitively against the loaded names and is a
    // SUBSTRING match, because the files are named "sword_1h" and the constant that
    // names one should not have to know about the suffix. Nothing matching means the
    // first weapon in the list, so a renamed asset folder starts the player armed
    // rather than empty-handed.
    //------------------------------------------------------------------------------
    void Reset(const std::vector<WeaponListing> &weapons, const char *startingName);

    int Count() const { return (int)prices.size(); }

    // The weapon's own name, as the model file gave it. Copied at Reset rather than
    // held as a pointer into the view model's list: the shop outlives any one frame
    // and the arsenal has no business keeping the renderer alive.
    const char *NameAt(int index) const;

    bool Owns(int index) const;
    void Give(int index);

    //------------------------------------------------------------------------------
    // Limited stock: which UNOWNED weapons the merchant is actually selling this
    // floor.
    //
    // An owned weapon needs no offer - it shows as Upgrade regardless, and putting
    // it in the offered set as well would double-count it for no reason. Rerolled
    // once per floor (see Game::StartNewRun / Game::Descend), so a weapon the
    // player wants but was not offered is a reason to come back down a floor and
    // check the counter again, rather than a permanent lockout.
    //------------------------------------------------------------------------------
    bool IsOffered(int index) const;

    // What kind of weapon this is - see WeaponListing::tags / combat/Weapon.h's
    // WeaponTag.
    unsigned TagsAt(int index) const;

    // The weapon's own table figures, for the Inventory tab - see CharacterSheet.
    // Not used by combat, which reads WeaponStats fresh every frame through
    // Game::RefreshLoadout; these are a copy for display only.
    int DamageAt(int index) const;
    float ReachAt(int index) const;

    //------------------------------------------------------------------------------
    // Clears every offer, then marks up to `count` unowned weapons offered at
    // random. Fewer than `count` unowned weapons simply offers all of them.
    //
    // `guaranteeTag`, when non-zero, is offered FIRST: one random unowned weapon
    // carrying every bit in it (if one exists) is placed before the rest of the
    // count is filled at random, so a floor can promise "there is always a
    // castable weapon here" without that promise being a coin flip against
    // `count` random picks.
    //------------------------------------------------------------------------------
    void RerollOffers(int count, unsigned guaranteeTag = 0);

    int Forge(int index) const;
    bool CanForge(int index) const;
    void RaiseForge(int index);

    // What the merchant asks. The list price is rolled per weapon at Reset from its
    // own damage and reach, so a greatsword costs more than a knife without anybody
    // hand-authoring twenty numbers.
    int Price(int index) const;
    int ForgePrice(int index) const;

    // Damage multiplier from the forge level: 1.0 at zero. Applied to the WEAPON's
    // own figure, never to the character's.
    float DamageMult(int index) const;

    // What holding this weapon is worth on top of its own table bonus. Only the forge
    // levels - the weapon's authored StatBlock is WeaponStats' business, and summing
    // the two here would be the same bonus counted twice.
    Modifiers HeldBonus(int index) const;

    // How many are owned, for the shop's "you have everything" line
    int OwnedCount() const;

    //------------------------------------------------------------------------------
    // The next owned weapon at or after `from`, cycling in `step`, or -1 when the
    // player owns nothing.
    //
    // -1 is a legitimate value and means the empty hand, which is a real loadout
    // choice: a shield needs a free hand and so does a spell. So the cycle runs over
    // `Count()` slots plus one, and the extra one is nothing at all.
    //------------------------------------------------------------------------------
    int NextOwned(int from, int step) const;

private:
    // Indexed by the view model's weapon index. Parallel arrays rather than a struct
    // per weapon because two of the three are written by the shop and read by the
    // wheel, and the third never changes after Reset.
    std::vector<unsigned char> owned;
    std::vector<unsigned char> offered;
    std::vector<unsigned char> forge;
    std::vector<int> prices;
    std::vector<std::string> names;
    std::vector<unsigned> tags;
    std::vector<int> damages;
    std::vector<float> reaches;
};
