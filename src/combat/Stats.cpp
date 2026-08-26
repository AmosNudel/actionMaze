#include "combat/Stats.h"

#include "raylib.h"

namespace
{
    const char *Names[(int)Stat::Count] = { "CONSTITUTION", "ARMS", "SKILL", "ARCANE" };

    // Points a stat sits above - or below - the neutral line. Every derived value
    // here is this number times a rate, which is what keeps the whole system linear.
    int Above(int value) { return value - Config::StatBase; }

    // Rounded rather than truncated. At two percent a point a truncating cast loses
    // most of the first few points of constitution entirely, and a stat that visibly
    // does nothing for its first three points is a stat nobody spends on.
    int Round(float value) { return (int)(value + (value < 0.0f ? -0.5f : 0.5f)); }
}

const char *StatName(Stat stat)
{
    const int index = (int)stat;
    if (index < 0 || index >= (int)Stat::Count) return Names[0];

    return Names[index];
}

int StatValue(const StatBlock &block, Stat stat)
{
    switch (stat)
    {
        case Stat::Arms:   return block.arms;
        case Stat::Skill:  return block.skl;
        case Stat::Arcane: return block.arc;
        default:           return block.con;
    }
}

void StatAdd(StatBlock &block, Stat stat, int amount)
{
    switch (stat)
    {
        case Stat::Arms:   block.arms += amount; break;
        case Stat::Skill:  block.skl  += amount; break;
        case Stat::Arcane: block.arc  += amount; break;
        default:           block.con  += amount; break;
    }
}

int StatBonusHealth(const StatBlock &block, int baseHealth)
{
    return Round(baseHealth*Config::StatHealthPerPoint*Above(block.con));
}

int StatBonusDamage(const StatBlock &block, int baseDamage)
{
    return Round(baseDamage*Config::StatDamagePerPoint*Above(block.arms));
}

int WeaponDamageWith(const StatBlock &block, int baseDamage)
{
    const int total = baseDamage + StatBonusDamage(block, baseDamage);

    // A blow that connects has to do something. Held here so a deeply negative
    // arms line cannot make a weapon heal what it hits.
    return (total < 1) ? 1 : total;
}

int StatBonusSpellPower(const StatBlock &block, int baseSpellPower)
{
    return Round(baseSpellPower*Config::StatSpellPerPoint*Above(block.arc));
}

float StatCritChance(const StatBlock &block)
{
    float chance = Config::StatCritBaseChance + Config::StatCritPerPoint*Above(block.skl);

    if (chance < 0.0f) chance = 0.0f;
    if (chance > Config::StatCritChanceCap) chance = Config::StatCritChanceCap;

    return chance;
}

float StatCritDamage(const StatBlock &block)
{
    const float damage = Config::StatCritBaseDamage
                       + Config::StatCritDamagePerPoint*Above(block.skl);

    return (damage < 1.0f) ? 1.0f : damage;
}

namespace
{
    // Thousandths, because GetRandomValue is integral - finer than any rate here
    // can resolve, and it keeps the roll on raylib's generator so a logged seed
    // reproduces the fight along with the level that produced it.
    bool Roll(float chance)
    {
        if (chance <= 0.0f) return false;

        return GetRandomValue(0, 999) < (int)(chance*1000.0f + 0.5f);
    }
}

bool StatRollCrit(const StatBlock &attacker)
{
    return Roll(StatCritChance(attacker));
}

bool RollWeaponCrit(const StatBlock &attacker, float bonusChance)
{
    float chance = StatCritChance(attacker) + bonusChance;

    // Re-clamped rather than trusted: StatCritChance already capped its own half,
    // and the sum has to be capped again or the flat bonus is a way round the
    // ceiling
    if (chance < 0.0f) chance = 0.0f;
    if (chance > Config::StatCritChanceCap) chance = Config::StatCritChanceCap;

    return Roll(chance);
}

int ResolveDamage(int raw, const StatBlock &attacker, bool crit)
{
    float damage = (float)raw;

    if (crit) damage *= StatCritDamage(attacker);

    const int out = (int)(damage + 0.5f);

    return (out < 1) ? 1 : out;
}
