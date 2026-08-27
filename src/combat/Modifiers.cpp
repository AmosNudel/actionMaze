#include "combat/Modifiers.h"

#include "combat/Stats.h"
#include "raylib.h"

#include <cstdio>
#include <cstring>

Modifiers ModifiersAdd(const Modifiers &a, const Modifiers &b)
{
    Modifiers out;

    out.stat.con  = a.stat.con  + b.stat.con;
    out.stat.arms = a.stat.arms + b.stat.arms;
    out.stat.skl  = a.stat.skl  + b.stat.skl;
    out.stat.arc  = a.stat.arc  + b.stat.arc;

    out.flatHealth = a.flatHealth + b.flatHealth;
    out.flatDamage = a.flatDamage + b.flatDamage;
    out.flatSpell  = a.flatSpell  + b.flatSpell;
    out.flatMana   = a.flatMana   + b.flatMana;

    out.attackSpeed = a.attackSpeed + b.attackSpeed;
    out.moveSpeed   = a.moveSpeed   + b.moveSpeed;
    out.critChance  = a.critChance  + b.critChance;
    out.lifesteal   = a.lifesteal   + b.lifesteal;
    out.spellPower  = a.spellPower  + b.spellPower;
    out.damageDealt = a.damageDealt + b.damageDealt;
    out.damageTaken = a.damageTaken + b.damageTaken;
    out.manaCost    = a.manaCost    + b.manaCost;

    out.manaPerKill = a.manaPerKill + b.manaPerKill;

    //------------------------------------------------------------------------------
    // Conversions are CONCATENATED rather than summed.
    //
    // Two traits each granting "20% of arcane to arms" are two rules, and merging
    // them into one 40% rule would be the same answer only while both read the same
    // pair of stats. Keeping them separate means a third rule reading a different
    // pair cannot be folded into either by accident.
    //
    // Anything past the fourth is dropped. See the note on MaxStatConverts - it is a
    // limit rather than a bug, and a silent one because the alternative is a warning
    // per frame for a trait combination the table cannot actually produce.
    //------------------------------------------------------------------------------
    for (int i = 0; (i < a.convertCount) && (out.convertCount < MaxStatConverts); ++i)
    {
        out.convert[out.convertCount++] = a.convert[i];
    }

    for (int i = 0; (i < b.convertCount) && (out.convertCount < MaxStatConverts); ++i)
    {
        out.convert[out.convertCount++] = b.convert[i];
    }

    return out;
}

//----------------------------------------------------------------------------------
// The spent line, plus points, plus conversions - in that order.
//
// The order is the whole of it. Points first, so a conversion reads the stat the
// character actually ended up with; conversions last and all from the SAME snapshot,
// so two rules cannot feed each other. Reading `out` as it was built would let "arms
// from arcane" and "arcane from arms" chase each other into a spiral off one point.
//----------------------------------------------------------------------------------
StatBlock ApplyModifiers(const StatBlock &spent, const Modifiers &mods)
{
    StatBlock out;

    out.con  = spent.con  + mods.stat.con;
    out.arms = spent.arms + mods.stat.arms;
    out.skl  = spent.skl  + mods.stat.skl;
    out.arc  = spent.arc  + mods.stat.arc;

    if (mods.convertCount <= 0) return out;

    const StatBlock from = out;

    for (int i = 0; i < mods.convertCount; ++i)
    {
        const StatConvert &rule = mods.convert[i];

        if ((rule.from >= (unsigned char)Stat::Count) || (rule.to >= (unsigned char)Stat::Count))
        {
            continue;
        }

        // Of what is ABOVE the neutral line, not of the raw figure. A character who
        // has spent nothing is at 10 in everything, and a rule reading the raw value
        // would hand them free points for having a pulse.
        const int above = StatValue(from, (Stat)rule.from) - Config::StatBase;

        if (above <= 0) continue;

        StatAdd(out, (Stat)rule.to, (int)(above*rule.frac + 0.5f));
    }

    return out;
}

//----------------------------------------------------------------------------------
// What it grants, in one line.
//
// Only the non-zero columns, so a trait that does one thing prints one thing. The
// buffer is static and the caller must copy it before the next call - which is
// exactly what TextFormat does, and every caller here is already living with that.
//----------------------------------------------------------------------------------
const char *ModifiersText(const Modifiers &mods)
{
    static char line[256];

    line[0] = '\0';
    int used = 0;

    // Appends " <text>" if there is room, and does nothing if there is not. A
    // summary that ran off the end of a fixed buffer would be the one place in this
    // file that could corrupt memory, and it is not worth being clever about.
    auto add = [&](const char *text)
    {
        const int room = (int)sizeof(line) - used - 1;

        if (room <= 1) return;

        const int wrote = snprintf(line + used, (size_t)room, "%s%s", used ? "  " : "", text);

        if (wrote > 0) used += (wrote < room) ? wrote : (room - 1);
    };

    if (mods.stat.con  != 0) add(TextFormat("%+i CON", mods.stat.con));
    if (mods.stat.arms != 0) add(TextFormat("%+i ARMS", mods.stat.arms));
    if (mods.stat.skl  != 0) add(TextFormat("%+i SKILL", mods.stat.skl));
    if (mods.stat.arc  != 0) add(TextFormat("%+i ARCANE", mods.stat.arc));

    if (mods.flatHealth != 0) add(TextFormat("%+i health", mods.flatHealth));
    if (mods.flatDamage != 0) add(TextFormat("%+i damage", mods.flatDamage));
    if (mods.flatSpell  != 0) add(TextFormat("%+i spell", mods.flatSpell));
    if (mods.flatMana   != 0) add(TextFormat("%+i mana", mods.flatMana));

    if (mods.critChance  != 0.0f) add(TextFormat("%+.0f%% crit", mods.critChance*100.0f));
    if (mods.lifesteal   != 0.0f) add(TextFormat("%+.0f%% drain", mods.lifesteal*100.0f));
    if (mods.attackSpeed != 0.0f) add(TextFormat("%+.0f%% speed", mods.attackSpeed*100.0f));
    if (mods.moveSpeed   != 0.0f) add(TextFormat("%+.0f%% move", mods.moveSpeed*100.0f));
    if (mods.spellPower  != 0.0f) add(TextFormat("%+.0f%% spell", mods.spellPower*100.0f));
    if (mods.damageDealt != 0.0f) add(TextFormat("%+.0f%% damage", mods.damageDealt*100.0f));

    // Same convention as manaCost above: negative IS the good direction, so a
    // shield's row stores -0.15 and this prints "-15% damage taken" without
    // needing to know that.
    if (mods.damageTaken != 0.0f) add(TextFormat("%+.0f%% damage taken", mods.damageTaken*100.0f));

    // Printed with the sign flipped, because this is the one column stored inverted
    // and "-20% cost" is what the player needs to read
    if (mods.manaCost   != 0.0f) add(TextFormat("%+.0f%% cost", mods.manaCost*100.0f));
    if (mods.manaPerKill != 0)   add(TextFormat("%+i mana/kill", mods.manaPerKill));

    for (int i = 0; i < mods.convertCount; ++i)
    {
        const StatConvert &rule = mods.convert[i];

        if ((rule.from >= (unsigned char)Stat::Count) || (rule.to >= (unsigned char)Stat::Count))
        {
            continue;
        }

        add(TextFormat("%.0f%% %s to %s", rule.frac*100.0f,
                       StatName((Stat)rule.from), StatName((Stat)rule.to)));
    }

    return line;
}
