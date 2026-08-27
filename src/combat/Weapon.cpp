#include "combat/Weapon.h"

#include "core/Config.h"

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
    // The stat bonuses run the same way and are set against each other rather than
    // against absolutes: nothing here gives more than +6, because +6 arms is worth
    // about a quarter of a level's whole budget and a weapon that outweighs two
    // levels of spending is a weapon nobody chooses against.
    //------------------------------------------------------------------------------
    struct StatOverride
    {
        const char *prefix;
        float reachScale;
        float damageScale;
        StatBlock bonus;            // Offsets from neutral, NOT a stat line
        float lifesteal;
        float critBonus;
        float stun;
        float knockback;
    };

    const StatOverride Overrides[] =
    {
        // Fast and short: it is a throwing knife, and it is the crit weapon. The
        // skill bonus and the flat chance on top are the same idea said twice,
        // which is on purpose - a dagger build should feel like it is happening.
        { "dagger_", 0.85f, 0.75f, { 0, 0, 6, 0 }, 0.00f, 0.08f, 0.0f, 0.0f },

        // Slow, heavy, and it has the length. The reach IS its identity, so what it
        // carries is the reach and a little weight behind it rather than a
        // behaviour that would compete with standing further away than everyone.
        { "halberd", 1.15f, 1.30f, { 2, 3, 0, 0 }, 0.00f, 0.00f, 0.0f, 5.0f },

        // The greatsword, and the one weapon that pays you to stay in. Ten percent
        // against a swing that already hits hardest is a real fraction of what a
        // fight deals back - enough to build around, nowhere near enough to stand
        // still in, and it returns nothing at all on the swings that miss.
        { "sword_E", 1.05f, 1.25f, { 3, 4, 0, 0 }, 0.10f, 0.00f, 0.0f, 0.0f },

        // Short haul, heavy head, and the only thing here that stops an enemy
        // outright. Six tenths of a second is most of a Warrior's gap between
        // swings: it does not kill anything, it decides who swings next.
        { "hammer_", 0.95f, 1.20f, { 4, 2, 0, 0 }, 0.00f, 0.00f, 0.6f, 4.0f },

        // The middle of the table on every axis, which is what an axe is for. A
        // little of the shove and none of the rest.
        { "axe_",    1.00f, 1.10f, { 1, 2, 0, 0 }, 0.00f, 0.00f, 0.0f, 3.0f },

        // The spear: reach without the halberd's weight. Its trade is that it
        // thrusts, and a thrust catches one body where a sweep catches three.
        { "spear_",  1.10f, 1.00f, { 1, 1, 3, 0 }, 0.00f, 0.03f, 0.0f, 0.0f },

        // The rapier. Thrust, quick, and the second crit weapon - lighter on the
        // flat chance than the dagger and heavier on the stat, so it scales into a
        // skill build where the dagger opens one.
        { "sword_D", 1.00f, 0.95f, { 0, 1, 5, 0 }, 0.00f, 0.04f, 0.0f, 0.0f },

        //----------------------------------------------------------------------
        // The casters. Arcane and nothing else, and the reason is the same reason
        // the dagger has no lifesteal: their behaviour IS the magic, and a staff
        // that also stunned would be answering a question the school already
        // answers.
        //
        // The staff outweighs the wand because it is two-handed and slower to
        // bring round; the wand's compensation is that it is quick enough to hold
        // beside something else.
        //----------------------------------------------------------------------
        { "staff_",  1.00f, 1.00f, { 1, 0, 0, 6 }, 0.00f, 0.00f, 0.0f, 0.0f },
        { "wand_",   1.00f, 1.00f, { 0, 0, 1, 4 }, 0.00f, 0.00f, 0.0f, 0.0f },

        // A shield is not a weapon and has no damage worth scaling, so what it
        // carries is what standing behind it is worth: the only pure constitution
        // in the table, and the reason to give up an off hand for one.
        { "shield_", 1.00f, 1.00f, { 6, 0, 0, 0 }, 0.00f, 0.00f, 0.0f, 0.0f },
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
        constexpr const char *TwoHanded[] = { "halberd", "spear_", "sword_E", "staff_" };

        for (const char *prefix : TwoHanded)
        {
            if (name.compare(0, std::string(prefix).size(), prefix) == 0) return true;
        }

        return false;
    }
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

        stats.bonus = tweak->bonus;
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
