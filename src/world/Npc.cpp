#include "world/Npc.h"

#include "world/RoomKind.h"

namespace
{
    //------------------------------------------------------------------------------
    // The three, in NpcKind order.
    //
    // The colours double as each shop page's accent, so the vendor the player walked
    // up to and the page that opened are visibly the same person: gold for the
    // merchant, violet for the mystic - which is the magic palette - and a hard
    // military red for the captain.
    //
    // None of them is the portal's cold blue or an event's orange. A column of light
    // already means "walk into this" in four other places, and the colour is the only
    // thing saying which of them this one is.
    //------------------------------------------------------------------------------
    constexpr NpcDef Table[(int)NpcKind::Count] =
    {
        { "MERCHANT", "arms, and the forge",     Currency::Coins,     { 245, 215, 120, 255 } },
        { "MYSTIC",   "schools, and the deepening of them", Currency::Gems, { 190, 130, 255, 255 } },
        { "CAPTAIN",  "traits, and a free respec", Currency::Contracts, { 235, 120, 110, 255 } },
    };

    // The room table writes vendor lists as plain ints so that RoomKind.h can keep its
    // promise of having no dependencies. This is the seam where the two agree.
    static_assert(Rooms::VendorMerchant == (int)NpcKind::Merchant, "vendor ids out of step");
    static_assert(Rooms::VendorMystic   == (int)NpcKind::Mystic,   "vendor ids out of step");
    static_assert(Rooms::VendorCaptain  == (int)NpcKind::Captain,  "vendor ids out of step");
}

const NpcDef &NpcAt(NpcKind kind)
{
    const int index = (int)kind;

    if ((index < 0) || (index >= (int)NpcKind::Count)) return Table[0];

    return Table[index];
}

bool NpcSuitsRoom(NpcKind kind, int roomKind)
{
    if ((roomKind < 0) || (roomKind >= Rooms::KindCount)) return false;

    const int *list = Rooms::Kinds[roomKind].vendors;

    for (int i = 0; i < (int)(sizeof(Rooms::Kinds[0].vendors)/sizeof(int)); ++i)
    {
        if (list[i] == Rooms::NoVendor) return false;
        if (list[i] == (int)kind) return true;
    }

    return false;
}
