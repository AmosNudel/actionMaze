#include "entities/EnemyRank.h"

#include "core/Config.h"

#include <cmath>

namespace
{
    //------------------------------------------------------------------------------
    // The tiers, in EnemyTier order.
    //
    // Every column is a multiplier rather than an offset, which is exactly what
    // makes the spread survive to level 40: a hot body is half again an even one at
    // every point in the game.
    //
    // The colours are cold and washed out behind the player and warm then hot ahead
    // of them, because the tint answers one question and only one: is this body
    // behind me, with me, or ahead of me. It is not a rank readout - there is no
    // colour per rank, and a single rank of difference is left white, because that
    // much is noise and a tint that fires on noise stops being read.
    //------------------------------------------------------------------------------
    constexpr EnemyTierDef Tiers[(int)EnemyTier::Count] =
    {
        //------------------------------------------------------------------------
        // POISE is the one column that is not a smooth ramp, and deliberately so.
        //
        // Three tiers flinch at every blow, and the fourth does not flinch until it
        // has taken an eighth of its pool. That is a step change rather than a
        // gradient because it is a change in what the fight IS: everything up to
        // elite can be staggered into standing still, and a champion cannot. A
        // champion at 0.04 would simply be an elite that flinched slightly less
        // often, which is not worth a column.
        //------------------------------------------------------------------------
        //  name          health  damage  speed  scale  swing  poise  tint
        { "worn",       0.75f,  0.75f, 1.00f, 0.95f, 1.00f, 0.00f, { 150, 170, 185, 255 } },
        { "even",       1.00f,  1.00f, 1.00f, 1.00f, 1.00f, 0.00f, { 255, 255, 255, 255 } },
        { "elite",      1.35f,  1.25f, 1.08f, 1.06f, 1.05f, 0.00f, { 255, 200, 130, 255 } },
        { "champion",   1.50f,  1.33f, 1.15f, 1.12f, 1.10f, 0.125f, { 255, 120, 110, 255 } },
    };
}

int RankCentreForDepth(int depth)
{
    if (depth < 1) depth = 1;

    int centre = Config::EnemyDepthRankBase + (depth - 1)*Config::EnemyDepthRankStep;

    // The last two floors climb faster than the ordinary step alone would take
    // them - see the note on Config::EnemyDeepFloorStart.
    if (depth >= Config::EnemyDeepFloorStart)
    {
        centre += (depth - Config::EnemyDeepFloorStart + 1)*Config::EnemyDeepFloorRankStep;
    }

    if (centre < 1) centre = 1;
    if (centre > Config::EnemyMaxRank) centre = Config::EnemyMaxRank;

    return centre;
}

int RollRankForDepth(int depth)
{
    const int centre = RankCentreForDepth(depth);

    // Only the ranks that exist: clipped at 1 below - there is no rank 0 - and at
    // the ceiling above. The first floor's centre is 1, so its window is entirely
    // above it and a level 1 character meets a real fight rather than a walkover.
    int lo = centre - Config::EnemyRankSpanBelow;
    int hi = centre + Config::EnemyRankSpanAbove;

    if (lo < 1) lo = 1;
    if (hi > Config::EnemyMaxRank) hi = Config::EnemyMaxRank;
    if (hi < lo) return lo;

    constexpr int window = Config::EnemyRankSpanBelow + Config::EnemyRankSpanAbove + 1;

    float weight[window] = {};
    float total = 0.0f;

    for (int rank = lo; rank <= hi; ++rank)
    {
        const int away = rank - centre;

        const float w = (away <= 0)
                      ? 1.0f/(1.0f + Config::EnemyRankBelowSpread*(float)(away*away))
                      : powf(Config::EnemyRankAboveFalloff, (float)away);

        weight[rank - lo] = w;
        total += w;
    }

    // Roulette over the window. Drawn in thousandths because GetRandomValue is
    // integral - plenty of resolution against a total that is never much over 3,
    // and it keeps the roll on raylib's generator so a logged seed reproduces the
    // whole floor, ranks included.
    float pick = GetRandomValue(0, 999)/1000.0f*total;

    for (int rank = lo; rank <= hi; ++rank)
    {
        pick -= weight[rank - lo];
        if (pick <= 0.0f) return rank;
    }

    return hi;      // Only reachable on a rounding edge
}

namespace
{
    // Ranks past the first. Rank 1 is the table row unchanged, which is what makes
    // the table readable as the thing it describes rather than as a hidden baseline.
    int Steps(int rank) { return (rank > 1) ? (rank - 1) : 0; }
}

int RankedHealth(int baseHealth, int rank)
{
    const int out = (int)(baseHealth*(1.0f + Config::EnemyRankHealthGrowth*Steps(rank)) + 0.5f);

    return (out < 1) ? 1 : out;
}

int RankedDamage(int baseDamage, int rank)
{
    const int out = (int)(baseDamage*(1.0f + Config::EnemyRankDamageGrowth*Steps(rank)) + 0.5f);

    return (out < 1) ? 1 : out;
}

int RankedExp(int baseExp, int rank)
{
    const int out = (int)(baseExp*powf(Config::EnemyRankExpGrowth, (float)Steps(rank)) + 0.5f);

    return (out < 1) ? 1 : out;
}

EnemyTier TierFor(int rank, int playerLevel)
{
    const int ahead = rank - playerLevel;

    if (ahead <= -2) return EnemyTier::Worn;
    if (ahead >=  2) return EnemyTier::Champion;
    if (ahead ==  1) return EnemyTier::Elite;

    return EnemyTier::Even;     // At the player's level, or one below
}

const EnemyTierDef &TierAt(EnemyTier tier)
{
    const int index = (int)tier;
    if (index < 0 || index >= (int)EnemyTier::Count) return Tiers[(int)EnemyTier::Even];

    return Tiers[index];
}
