#pragma once

#include "combat/Modifiers.h"
#include "raylib.h"

//----------------------------------------------------------------------------------
// A temporary combat bonus, and the table of them a Buff pickup rolls from - see
// world/Pickup.h for what drops one and Player::ApplyBuff for what running one
// actually does.
//
// The same shape TraitDef and MagicDef already are: a row is a name, a colour and
// a Modifiers, and there is no buff-specific code anywhere downstream of BuffAt.
//
// Every row uses the FLAT and FRACTION columns of Modifiers and none of the STAT
// ones - see the long note at the top of Modifiers.h on why "anything temporary"
// belongs there: a percentage of the character's CURRENT line survives a duty
// cycle without making the run quadratic the way a temporary stat point would.
//
// Lives in combat/ rather than in world/Pickup.h alongside the thing that drops
// one, because Player has to know the shape of a buff to run its clock and
// world/ has no business being something entities/Player.h depends on.
//----------------------------------------------------------------------------------
enum class BuffKind { Might, Surge, Bastion, Wellspring, Count };

struct BuffDef
{
    const char *name;
    const char *desc;
    Color colour;
    Modifiers mods;
};

int BuffCount();
const BuffDef &BuffAt(BuffKind kind);   // Clamps out of range to row 0
