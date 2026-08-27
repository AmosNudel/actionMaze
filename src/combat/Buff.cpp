#include "combat/Buff.h"

namespace
{
    //------------------------------------------------------------------------------
    // The table. Four rows, each written to matter without being a build in its
    // own right: enough to feel like a real spike for Config::BuffDuration
    // seconds, not enough that a player would ever plan a floor around finding
    // one - see the class note on BuffKind for why every row lives in the flat
    // and fraction columns of Modifiers and never in stat points.
    //------------------------------------------------------------------------------
    constexpr BuffDef Table[] =
    {
        // Every blow lands harder outright - the arms-flavoured spike, in the
        // same red the character page already colours ARMS.
        { "MIGHT", "weapon blows hit 6 harder", { 235, 110, 90, 255 },
          { { 0, 0, 0, 0 }, 0, 6, 0, 0 } },

        // On top of what arcane already bought - the arcane-flavoured spike,
        // in the character page's own blue for it.
        { "SURGE", "spells hit 30% harder", { 150, 160, 255, 255 },
          { { 0, 0, 0, 0 }, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.30f } },

        // A bigger pool, felt immediately - RefreshHealth carries the current
        // health up with the pool the moment this lands, the same rule a point
        // of constitution follows. Con's own green.
        { "BASTION", "+25 maximum health, right now", { 120, 210, 130, 255 },
          { { 0, 0, 0, 0 }, 25, 0, 0, 0 } },

        // The mana counterpart, in the one stat colour the other three rows
        // leave spare.
        { "WELLSPRING", "+15 maximum mana, right now", { 250, 220, 120, 255 },
          { { 0, 0, 0, 0 }, 0, 0, 0, 15 } },
    };

    constexpr int TableCount = (int)(sizeof(Table)/sizeof(Table[0]));

    static_assert(TableCount == (int)BuffKind::Count, "the buff table and BuffKind have drifted apart");
}

int BuffCount()
{
    return TableCount;
}

const BuffDef &BuffAt(BuffKind kind)
{
    const int index = (int)kind;
    if ((index < 0) || (index >= TableCount)) return Table[0];

    return Table[index];
}
