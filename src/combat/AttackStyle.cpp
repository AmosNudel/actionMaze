#include "combat/AttackStyle.h"

namespace
{
    const StyleTiming Timings[] =
    {
        { 0.13f, 0.22f, false },    // Swing:  committed, recovers slower than it lands
        { 0.09f, 0.16f, false },    // Thrust: quickest in and out
        { 0.12f, 0.18f, true  },    // Block:  stays up as long as the button is down
        { 0.18f, 0.30f, false },    // Cast:   deliberate, settles back slowly
        // Throw: sharp release, then long enough to cover the hand being empty and
        // the next one sliding in - ThrowHideTime + ThrowReturnTime. The state has
        // to stay busy for the whole visual recovery or the player can throw a
        // knife they are not yet holding.
        { 0.12f, 0.34f, false },
    };
}

const char *AttackStyleName(AttackStyle style)
{
    switch (style)
    {
        case AttackStyle::Swing:  return "swing";
        case AttackStyle::Thrust: return "thrust";
        case AttackStyle::Block:  return "block";
        case AttackStyle::Cast:   return "cast";
        case AttackStyle::Throw:  return "throw";
    }

    return "?";
}

const StyleTiming &TimingFor(AttackStyle style)
{
    return Timings[(int)style];
}

bool IsMeleeStyle(AttackStyle style)
{
    return (style == AttackStyle::Swing) || (style == AttackStyle::Thrust);
}
