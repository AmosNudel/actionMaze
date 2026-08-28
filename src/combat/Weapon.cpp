#include "combat/Weapon.h"

#include "core/Config.h"
#include <cstring>

namespace
{
    //------------------------------------------------------------------------------
    // Per weapon overrides, for the ones the derived numbers get wrong - and, since
    // there is no armour in this game, for what carrying the thing is worth.
    //
    // First match wins, so more specific prefixes come first: the same shape as the
    // style rules in ViewModel.cpp.
    //
    // --- What the columns are for ---------------------------------------------
    // reach and damage scale the figures derived from the model's own measured
    // height, so they are corrections to a guess. `bonus` and the four behaviours
    // are not corrections: they are the weapon's IDENTITY, and they are what makes
    // choosing one a build decision rather than a preference about silhouette.
    //
    // --- How the four behaviours are spread ------------------------------------
    // Deliberately, and never more than one per weapon. A weapon with three
    // behaviours is not three times as interesting, it is a weapon with no
    // character - the player cannot tell which of them is doing the work, so none
    // of them is a reason to carry it.
    //
    //   light and quick   crit. It finds the gap; it does not make one.
    //   heavy and slow    stun, or knockback. It takes the answer away.
    //   the greatsword    lifesteal. The only weapon that pays you to stay in.
    //   the ranged pair   nothing but arcane. Their behaviour is the magic.
    //
    // The bonuses run the same way and are set against each other rather than
    // against absolutes: nothing here is worth more than about a fifth of what
    // a whole level's own spending buys, because a weapon that outweighs a
    // level of points is a weapon nobody chooses against.
    //
    // `damageDealt`, `spellPower`, `flatMana` and `damageTaken` all land in
    // WeaponStats::bonus, a Modifiers - see the note there for why every one of
    // them is a fraction or a flat figure and never a stat point: a weapon
    // changes what a number DOES while it is in your hand, not the stat that
    // number is derived from, so putting it down leaves nothing behind for a
    // stat page to explain.
    //------------------------------------------------------------------------------
    struct StatOverride
    {
        const char *prefix;
        float reachScale;
        float damageScale;
        float damageDealt;    // Fraction, melee/ranged - see Modifiers::damageDealt
        float spellPower;     // Fraction, casters only - see Modifiers::spellPower
        int   flatMana;       // Pool, casters only - see Modifiers::flatMana
        float damageTaken;    // Fraction, negative is safer - shield only
        float lifesteal;
        float critBonus;
        float stun;
        float knockback;

        // Shields only - see WeaponStats::blockCharges. 0 on every other row, which
        // the reader below turns into the default of 1.
        int   blockCharges;
    };

    const StatOverride Overrides[] =
    {
        // Fast and short: it is a throwing knife, and it is the crit weapon.
        // The flat chance is its whole identity - nothing else on this row.
        { "dagger_", 0.85f, 0.75f, 0.00f, 0.00f, 0, 0.00f, 0.00f, 0.10f, 0.0f, 0.0f, 0 },

        // Slow, heavy, and it has the length. The reach IS its identity, so what
        // it carries beyond that is a flat bite on top rather than a behaviour
        // that would compete with standing further away than everyone.
        { "halberd", 1.15f, 1.30f, 0.12f, 0.00f, 0, 0.00f, 0.00f, 0.00f, 0.0f, 5.0f, 0 },

        // The greatsword, and the one weapon that pays you to stay in. Ten percent
        // lifesteal against a swing that already hits hardest is a real fraction of
        // what a fight deals back - enough to build around, nowhere near enough to
        // stand still in, and it returns nothing at all on the swings that miss.
        { "sword_E", 1.05f, 1.25f, 0.16f, 0.00f, 0, 0.00f, 0.10f, 0.00f, 0.0f, 0.0f, 0 },

        // Short haul, heavy head, and the only thing here that stops an enemy
        // outright. Six tenths of a second is most of a Warrior's gap between
        // swings: it does not kill anything, it decides who swings next.
        { "hammer_", 0.95f, 1.20f, 0.08f, 0.00f, 0, 0.00f, 0.00f, 0.00f, 0.6f, 4.0f, 0 },

        // The middle of the table on every axis, which is what an axe is for. A
        // little of the shove and a little of the bite, none of the rest.
        { "axe_",    1.00f, 1.10f, 0.08f, 0.00f, 0, 0.00f, 0.00f, 0.00f, 0.0f, 3.0f, 0 },

        // The spear: reach without the halberd's weight. Its trade is that it
        // thrusts, and a thrust catches one body where a sweep catches three.
        { "spear_",  1.10f, 1.00f, 0.04f, 0.00f, 0, 0.00f, 0.00f, 0.04f, 0.0f, 0.0f, 0 },

        // The rapier. Thrust, quick, and the second crit weapon - lighter on the
        // flat chance than the dagger, so it opens a build the dagger already owns
        // rather than competing with it for the same one.
        { "sword_D", 1.00f, 0.95f, 0.04f, 0.00f, 0, 0.00f, 0.00f, 0.06f, 0.0f, 0.0f, 0 },

        //----------------------------------------------------------------------
        // The casters. Arcane and nothing else, and the reason is the same reason
        // the dagger has no lifesteal: their behaviour IS the magic, and a staff
        // that also stunned would be answering a question the school already
        // answers.
        //
        // The staff outweighs the wand on both figures - it is what the school's
        // reservoir is FOR, and the wand's own compensation is that it is quick
        // enough to hold beside a shield (see IsTwoHanded, and TagTwoHanded's
        // enforcement in Game::UpdateWorld) where the staff, now one-handed too,
        // no longer has to give up an off hand to be worth carrying at all.
        //----------------------------------------------------------------------
        { "staff_",  1.00f, 1.00f, 0.00f, 0.24f, 5, 0.00f, 0.00f, 0.00f, 0.0f, 0.0f, 0 },
        { "wand_",   1.00f, 1.00f, 0.00f, 0.16f, 4, 0.00f, 0.00f, 0.00f, 0.0f, 0.0f, 0 },

        // A shield is not a weapon and has no damage worth scaling, so what it
        // carries is what standing behind it is worth - not a bigger health pool,
        // less of every blow actually landing. See the note on Modifiers::
        // damageTaken and the long one at the top of Modifiers.h for why this is
        // no longer a stat.
        //----------------------------------------------------------------------
        // The three shields, and the one column that tells them apart.
        //
        // They shared a single row until now - identical reduction, identical
        // everything - so "which shield" was a question about the picture. What
        // separates them is how many blows the guard eats before it is knocked
        // down (see WeaponStats::blockCharges), which is the stat that matters in
        // the situation a shield is actually for: a pack, where a one-blow guard
        // stops the first skeleton and neither of the two behind it.
        //
        // The reduction climbs with the charges rather than trading against them.
        // A heavier shield is simply a better shield and is priced as one - the
        // trade is that it costs a whole hand either way, and against a single
        // target the extra charges do nothing at all.
        //
        // These sit ABOVE the shared "shield_" row because FindOverride takes the
        // first prefix that matches.
        //----------------------------------------------------------------------
        { "shield_A", 1.00f, 1.00f, 0.00f, 0.00f, 0, -0.15f, 0.00f, 0.00f, 0.0f, 0.0f, 1 },
        { "shield_B", 1.00f, 1.00f, 0.00f, 0.00f, 0, -0.22f, 0.00f, 0.00f, 0.0f, 0.0f, 2 },
        { "shield_C", 1.00f, 1.00f, 0.00f, 0.00f, 0, -0.30f, 0.00f, 0.00f, 0.0f, 0.0f, 3 },

        // Anything else named shield_, so a fourth dropped into the pack is a
        // working shield on the day it arrives rather than a weapon with no row.
        { "shield_", 1.00f, 1.00f, 0.00f, 0.00f, 0, -0.15f, 0.00f, 0.00f, 0.0f, 0.0f, 1 },
    };

    //------------------------------------------------------------------------------
    // Model name to display name - see the note on WeaponDisplayName.
    //
    // EXACT names rather than prefixes, unlike the stat table above: the whole point
    // is to tell sword_A from sword_D, and a prefix match would give all five swords
    // the same word. Each name says what the SHAPE is, so a player reading the shop
    // knows what they are about to hold before the icon finishes turning.
    //
    // They also carry the weapon's identity where it has one, because the stat table
    // gave several of these a behaviour and the name is the only place the player
    // meets it first: the rapier and the dirk are the two crit weapons, the reaver
    // is the one that drinks, the mauls are the ones that stun.
    //------------------------------------------------------------------------------
    struct DisplayName
    {
        const char *model;
        const char *shown;
    };

    const DisplayName DisplayNames[] =
    {
        // Swords. A through C are the plain line; D is the rapier and E the
        // greatsword, which is where the stat table splits them too.
        { "sword_A", "Arming Sword" },
        { "sword_B", "Broadsword" },
        { "sword_C", "Falchion" },
        { "sword_D", "Rapier" },
        { "sword_E", "Reaver" },

        { "axe_A",   "Hatchet" },
        { "axe_B",   "War Axe" },
        { "axe_C",   "Bearded Axe" },

        { "hammer_A", "Mallet" },
        { "hammer_B", "War Maul" },
        { "hammer_C", "Skullbreaker" },

        { "dagger_A", "Dirk" },
        { "dagger_B", "Stiletto" },

        { "spear_A", "Pike" },
        { "halberd", "Halberd" },

        // The casters
        { "staff_A", "Oaken Stave" },
        { "staff_B", "Runed Stave" },
        { "wand_A",  "Ashwood Wand" },

        // The shields, named for what separates them - see the stat rows above.
        // Buckler, kite, tower: smallest to largest, which is also one charge to
        // three, so the name is the stat.
        { "shield_A", "Buckler" },
        { "shield_B", "Kite Shield" },
        { "shield_C", "Tower Shield" },
    };

    const StatOverride *FindOverride(const std::string &name)
    {
        for (const StatOverride &entry : Overrides)
        {
            const std::string prefix = entry.prefix;
            if (name.compare(0, prefix.size(), prefix) == 0) return &entry;
        }

        return nullptr;
    }

    //------------------------------------------------------------------------------
    // Which weapons take both hands.
    //
    // Explicit names rather than a derived rule, matched the same way
    // FindOverride matches its prefixes - there is no property of a weapon's
    // damage or reach that reliably says "two-handed", and this pack's own answer
    // (the greatsword, the halberd, the spear, both staves) is short enough to
    // just write down.
    //------------------------------------------------------------------------------
    bool IsTwoHanded(const std::string &name)
    {
        // The staff is NOT here, deliberately - unlike the halberd, the spear
        // and the greatsword, its whole identity is the spell leaving it, not
        // the weight of the thing itself, and a caster giving up a shield to
        // hold one is a worse trade than the wand already offers for the same
        // reach. See Game::UpdateWorld's wheel handling for what this list
        // actually enforces: a hand cannot hold anything alongside whatever is
        // named here.
        constexpr const char *TwoHanded[] = { "halberd", "spear_", "sword_E" };

        for (const char *prefix : TwoHanded)
        {
            if (name.compare(0, std::string(prefix).size(), prefix) == 0) return true;
        }

        return false;
    }
}

const char *WeaponDisplayName(const char *modelName)
{
    if (modelName == nullptr) return "";

    for (const DisplayName &entry : DisplayNames)
    {
        if (strcmp(modelName, entry.model) == 0) return entry.shown;
    }

    // Not in the table - see the note on the declaration. Showing the filename is
    // the honest answer: it is wrong in a way somebody will notice and fix.
    return modelName;
}

std::string WeaponTagsText(unsigned tags)
{
    std::string out;

    auto append = [&](const char *word)
    {
        if (!out.empty()) out += "  ";
        out += word;
    };

    if (tags & TagOneHanded) append("1H");
    if (tags & TagTwoHanded) append("2H");
    if (tags & TagCasting)   append("CAST");
    if (tags & TagRanged)    append("RANGED");
    if (tags & TagThrown)    append("THROWN");
    if (tags & TagBlocking)  append("BLOCK");

    return out;
}

WeaponStats StatsFor(const std::string &name, AttackStyle style, float modelHeight)
{
    WeaponStats stats;
    stats.melee = IsMeleeStyle(style);
    stats.liveFrom = Config::MeleeLiveFrom;
    stats.liveTo = Config::MeleeLiveTo;

    // Longer weapons reach further, straight off the measured model
    stats.reach = Config::MeleeBaseReach + modelHeight*Config::MeleeReachPerUnit;

    // Fatter for a bigger weapon, so a poleaxe is not as easy to thread past as a
    // knife. The swing's width now comes from the blade's own thickness sweeping
    // through space, which is why there is no arc angle here any more.
    stats.bladeRadius = Config::MeleeBladeRadius*(0.75f + modelHeight*0.25f);

    switch (style)
    {
        case AttackStyle::Thrust:
            stats.reach *= Config::ThrustReachBonus;    // A lunge extends
            stats.damage = Config::ThrustDamage;
            break;

        // The ranged pair. `reach` survives as the muzzle offset rather than as a
        // hit distance - it is how far down the weapon's own axis the shot leaves,
        // which is what puts a bolt at the end of the staff instead of at the eye.
        case AttackStyle::Cast:
            stats.ranged = true;
            stats.damage = Config::CastDamage;
            stats.projectileSpeed = Config::CastSpeed;
            stats.releaseAt = Config::CastReleaseAt;
            stats.tags |= TagCasting | TagRanged;
            break;

        case AttackStyle::Throw:
            stats.ranged = true;
            stats.damage = Config::ThrowDamage;
            stats.projectileSpeed = Config::ThrowSpeed;
            stats.releaseAt = Config::ThrowReleaseAt;
            stats.tags |= TagThrown | TagRanged;
            stats.poiseScale = Config::ThrowPoiseScale;
            break;

        case AttackStyle::Block:
            stats.damage = Config::SwingDamage;
            stats.tags |= TagBlocking;
            break;

        default:
            stats.damage = Config::SwingDamage;
            break;
    }

    // One-handed unless the pack's own two-handed weapons say otherwise - see the
    // note on IsTwoHanded.
    stats.tags |= IsTwoHanded(name) ? TagTwoHanded : TagOneHanded;

    const StatOverride *tweak = FindOverride(name);
    if (tweak != nullptr)
    {
        stats.reach *= tweak->reachScale;
        stats.damage = (int)(stats.damage*tweak->damageScale);

        stats.bonus.damageDealt = tweak->damageDealt;
        stats.bonus.spellPower = tweak->spellPower;
        stats.bonus.flatMana = tweak->flatMana;
        stats.bonus.damageTaken = tweak->damageTaken;

    // Zero on every non-shield row, which is not a legal charge count - a shield
    // that stopped nothing would be a hand given up for a damage reduction alone
    if (tweak->blockCharges > 0) stats.blockCharges = tweak->blockCharges;

        stats.lifesteal = tweak->lifesteal;
        stats.critBonus = tweak->critBonus;
        stats.stun = tweak->stun;
        stats.knockback = tweak->knockback;
    }

    // Last, so nothing above can leave a weapon unable to answer an enemy that is
    // already hitting it - not the thrust bonus, not a per-weapon override
    if (stats.melee && (stats.reach < Config::MeleeMinReach)) stats.reach = Config::MeleeMinReach;

    return stats;
}
