#pragma once

//----------------------------------------------------------------------------------
// What a room IS, and what belongs in it.
//
// A generated room is otherwise an anonymous box, and a map of anonymous boxes
// reads as one room repeated however many of them there are. A kind is the small
// amount of meaning that fixes that: it says what the room was for, which decides
// what stands in it, whether anyone garrisons it, and how large it has to be to
// make sense at all.
//
// Modelled on the DMG's Dungeon: Lair and Dungeon: Maze chamber tables, cut down
// to the twelve the art pack can actually tell apart. A Salon and a Sitting Room
// are two rows of the d100 table and one identical pile of furniture, so they are
// one kind here or none.
//
// Lives in world/ rather than in Config.h because it is content, and because it
// is long. The density dials that tune it stay in Config.h with the rest.
//----------------------------------------------------------------------------------

// Order matters only in that Kinds[] is indexed by it
enum class RoomKind
{
    Entrance,       // Where the player starts. Kept clear, never garrisoned.
    // Where the player leaves. Assigned rather than rolled, to the room furthest
    // from the entrance, so the way down is always a walk across the whole map.
    Portal,
    Guardroom,
    Barracks,
    Storage,
    Kitchen,
    Shrine,
    Prison,
    Workshop,
    Crypt,
    Vault,
    Library,
    Lair,
    Count
};

//----------------------------------------------------------------------------------
// How the room has fared since it was last used for its purpose.
//
// The DMG's Current Chamber State table (p295) is written for a dungeon with a
// tumultuous history and lands on "damaged" fourteen times in twenty. Reweighted
// heavily toward Pristine here: a map where every room is wrecked has no wrecked
// rooms in it, only one texture. Damage has to be the exception to read as damage.
//----------------------------------------------------------------------------------
enum class ChamberState
{
    Pristine,       // Full furnishing, nothing broken
    Wrecked,        // Furniture still present but thrown about
    Rubble,         // Ceiling came down: rubble replaces much of the palette
    Ashes,          // Burned out: little left, floor scorched to dirt
    Campsite,       // Somebody else has been living here. Ignores the palette.
    Flooded,        // Standing water: contents damaged, floor swapped
    Stripped,       // Bare. Nothing at all.
    Count
};

//----------------------------------------------------------------------------------
// One row per kind.
//
// Props are split three ways because the three want placing differently, not
// because they are three sorts of object: an `anchor` goes in the middle and there
// is at most one, `edge` props stand against a wall facing in, and `scatter` props
// go anywhere there is floor. A bed against a wall is a bedroom; a bed in the
// middle of the floor is a warehouse that lost its shelving.
//
// Paths are relative to models/dungeon/ and every list is null-terminated.
//----------------------------------------------------------------------------------
struct RoomKindSpec
{
    const char *name;

    // Cells of floor. A kind whose furniture needs space to read is gated out of
    // rooms too small to hold it, which is why a 2x2 never becomes a barracks.
    int minArea, maxArea;

    int  weight;        // Against every other eligible kind. 0 is never rolled.
    bool unique;        // At most one on the map
    bool garrisoned;    // May a spawn camp claim it

    const char *anchor;         // One central feature, or nullptr
    const char *edge[8];        // Against a wall, facing into the room
    const char *scatter[8];     // Loose on the floor

    int edgeMin, edgeMax;
    int scatterMin, scatterMax;

    //------------------------------------------------------------------------------
    // Which vendors make sense standing in this room, as NpcKind values, terminated
    // by -1. Empty means none, ever.
    //
    // Here rather than in Npc.cpp because it is a fact about the ROOM. A library is
    // somewhere a mystic would be found and a guardroom is not, and that is the same
    // kind of statement as "a library has bookshelves in it" - it belongs next to the
    // bookshelves, in the row that already says what the room was for.
    //
    // Stored as ints rather than as NpcKind so this header keeps its promise of
    // having no dependencies at all. Npc.h owns the enum; this owns the opinion.
    //------------------------------------------------------------------------------
    int vendors[4];
};

namespace Rooms
{
    // Terminates a vendor list. A row that wants no vendor writes { None, ... } and a
    // row that wants two writes both and then this.
    constexpr int NoVendor = -1;

    // The three, by value, so the table below reads as names rather than as numbers.
    // They must stay in step with NpcKind - which is what the static_assert in
    // Npc.cpp is for.
    constexpr int VendorMerchant = 0;
    constexpr int VendorMystic   = 1;
    constexpr int VendorCaptain  = 2;
}

namespace Rooms
{
    // Indexed by RoomKind. Every path here was checked against the 1.1 pack.
    constexpr RoomKindSpec Kinds[] =
    {
        // Entrance -----------------------------------------------------------
        // Deliberately almost empty. The player spawns in the middle of this room,
        // and a crate landing on that spot is a game that starts inside a crate.
        { "Entrance", 4, 999, 0, true, false,
          nullptr,
          { "pillars/wall_pillar", "pillars/pillar", nullptr },
          { nullptr },
          0, 2, 0, 0,
          { Rooms::NoVendor } },

        // Portal -------------------------------------------------------------
        // Assigned, never rolled - weight 0, like the entrance. The portal stands
        // in the middle of it, so the anchor slot has to stay empty and the
        // scatter list has to stay short: a crate on the exit is an exit the
        // player has to fight the collision to reach.
        //
        // Dressed as a place that was built for this rather than a room something
        // happens to be standing in. Columns and lit candles, and nothing else -
        // it is the one room whose furniture is there to say "this is the way
        // out" rather than to say what the room was for.
        // The minimum is 16 - a 4x4 - and it is doing real work. The portal keeps
        // its own cell and its four neighbours clear, which in a 3x3 is five of the
        // nine cells: what is left is four corners, and a room that is a portal
        // with four corners round it is a cupboard. 16 leaves room to walk round
        // the thing, which matters because the player has to be able to step OUT
        // of it as easily as in.
        { "Portal", 16, 999, 0, true, false,
          nullptr,
          { "pillars/pillar_decorated", "pillars/column", "pillars/wall_pillar",
            "props_medium/shelf_small_candles", nullptr },
          { "props_small/candle_lit", "props_small/candle_triple", nullptr },
          3, 6, 2, 4,
          { Rooms::NoVendor } },

        // Guardroom ----------------------------------------------------------
        // The DMG's most common room by a distance, in every dungeon type. A table
        // to sit at, something to put your back to, and the dice game still out.
        { "Guardroom", 6, 30, 4, false, true,
          "props_large/table_medium",
          { "props_large/barrel_large", "props_large/box_large", "props_large/shelf_large",
            "props_large/barrier", "props_large/barrier_half", nullptr },
          { "props_medium/chair", "props_medium/stool", "props_small/sword_shield",
            "props_small/plate", "props_small/bottle_A_brown", nullptr },
          3, 6, 2, 5,
          { Rooms::VendorCaptain, Rooms::NoVendor } },

        // Barracks -----------------------------------------------------------
        // Beds against the walls, a chest at the foot of each. Needs floor: a
        // barracks that fits two beds is a bedroom.
        { "Barracks", 12, 48, 2, false, true,
          nullptr,
          { "props_large/bed_frame", "props_large/bed_decorated", "props_large/bed_floor",
            "props_medium/trunk_medium_A", "props_medium/trunk_medium_B",
            "props_medium/trunk_medium_C", "props_medium/shelf_small", nullptr },
          { "props_medium/stool", "props_small/bottle_A_brown", "props_small/plate",
            "props_small/candle", nullptr },
          4, 8, 2, 4,
          { Rooms::VendorCaptain, Rooms::VendorMerchant, Rooms::NoVendor } },

        // Storage ------------------------------------------------------------
        // The one kind that wants to be crowded. Stacked to the walls and piled in
        // the middle, because that is what storage looks like.
        { "Storage", 4, 30, 4, false, false,
          "props_large/box_stacked",
          { "props_large/barrel_large", "props_large/crates_stacked", "props_large/shelves",
            "props_large/shelf_large", "props_large/box_large",
            "props_medium/barrel_small_stack", nullptr },
          { "props_medium/barrel_small", "props_medium/box_small",
            "props_medium/trunk_small_A", "props_medium/trunk_small_B", nullptr },
          4, 8, 3, 6,
          { Rooms::VendorMerchant, Rooms::NoVendor } },

        // Kitchen ------------------------------------------------------------
        { "Kitchen", 9, 30, 2, true, false,
          "props_large/table_long",
          { "props_medium/shelf_small", "props_large/barrel_large", "props_medium/keg",
            "props_medium/keg_decorated", "props_large/shelves", nullptr },
          { "props_small/plate_stack", "props_small/plate_food_A", "props_small/plate_food_B",
            "props_small/bottle_B_green", "props_small/plate_small", nullptr },
          3, 6, 3, 6,
          { Rooms::VendorMerchant, Rooms::NoVendor } },

        // Shrine -------------------------------------------------------------
        // The pack ships no altar, so the decorated table stands in for one. Dressed
        // with candles it reads as an altar and not as furniture.
        { "Shrine", 9, 36, 2, true, true,
          "props_large/table_medium_decorated_A",
          { "pillars/pillar_decorated", "pillars/column", "props_medium/shelf_small_candles",
            nullptr },
          { "props_small/candle_lit", "props_small/candle_triple", "props_small/candle_melted",
            "props_small/coin_stack_small", nullptr },
          2, 4, 3, 6,
          { Rooms::VendorMystic, Rooms::NoVendor } },

        // Prison -------------------------------------------------------------
        // Barriers are the pack's only bars. Nothing else in here is comfortable.
        { "Prison", 6, 24, 2, false, true,
          nullptr,
          { "props_large/barrier", "props_large/barrier_half", "props_large/barrier_column",
            "props_medium/rubble_large", nullptr },
          { "props_medium/rubble_half", "props_small/plate",
            "props_small/bottle_A_labeled_brown", "props_small/candle", nullptr },
          3, 6, 2, 4,
          { Rooms::VendorCaptain, Rooms::NoVendor } },

        // Workshop -----------------------------------------------------------
        { "Workshop", 9, 36, 2, false, true,
          "props_large/table_long_broken",
          { "props_large/shelf_large", "props_large/box_large",
            "props_medium/barrel_small_stack", "props_large/shelves",
            "props_medium/trunk_large_A", nullptr },
          { "props_medium/stool", "props_small/torch", "props_small/sword_shield_broken",
            "props_small/key", nullptr },
          3, 6, 3, 5,
          { Rooms::VendorMerchant, Rooms::NoVendor } },

        // Crypt --------------------------------------------------------------
        // Nothing here is furniture. Rubble, a guttered candle, and whatever the
        // last visitor dropped.
        { "Crypt", 4, 24, 3, false, true,
          nullptr,
          { "props_medium/rubble_large", "props_medium/rubble_half", "props_large/box_large",
            "props_medium/trunk_large_B", nullptr },
          { "props_small/candle", "props_small/candle_melted", "props_small/coin",
            "props_small/sword_shield_broken", nullptr },
          2, 5, 2, 5,
          { Rooms::VendorMystic, Rooms::NoVendor } },

        // Vault --------------------------------------------------------------
        // Small on purpose. A vault the size of a hall is a warehouse; the point of
        // the room is that it is cramped, sealed, and holds the good chest.
        //
        // Weight 2 rather than 1, and this row is now the ONLY thing deciding how
        // often a treasure chest exists - see Config::TreasureChestChance, which used
        // to be a second coin flip on top of it. Two gates meant a whole run could go
        // by with one chest in it, which made the treasure room a room that usually
        // held no treasure.
        { "Vault", 4, 12, 2, true, false,
          "props_medium/chest_gold",
          { "props_medium/chest", "props_medium/trunk_large_A", "props_medium/trunk_large_B",
            "props_medium/trunk_large_C", "props_medium/shelf_small", nullptr },
          { "props_small/coin_stack_small", "props_small/coin_stack_medium",
            "props_small/coin_stack_large", "props_small/keyring", nullptr },
          3, 5, 3, 6,
          { Rooms::VendorMerchant, Rooms::VendorMystic, Rooms::NoVendor } },

        // Library ------------------------------------------------------------
        { "Library", 9, 36, 1, true, false,
          "props_large/table_medium_decorated_A",
          { "props_large/shelf_large", "props_large/shelves",
            "props_medium/shelf_small_candles", "props_medium/shelf_small", nullptr },
          { "props_small/candle_thin_lit", "props_medium/stool", "props_small/plate",
            "props_small/key", nullptr },
          4, 7, 2, 4,
          { Rooms::VendorMystic, Rooms::NoVendor } },

        // Lair ----------------------------------------------------------------
        // Not built, occupied. Whatever lives here dragged in what it wanted and
        // broke the rest, so this is the one kind whose furniture is wreckage.
        { "Lair", 16, 999, 2, false, true,
          nullptr,
          { "props_medium/rubble_large", "props_large/box_stacked", "props_large/barrel_large",
            "props_large/crates_stacked", nullptr },
          { "props_medium/rubble_half", "props_small/torch",
            "props_small/sword_shield_broken", "props_small/bottle_C_brown", nullptr },
          3, 6, 3, 6,
          { Rooms::NoVendor } },
    };

    constexpr int KindCount = (int)(sizeof(Kinds)/sizeof(Kinds[0]));

    inline const RoomKindSpec &Spec(RoomKind kind)
    {
        return Kinds[(int)kind];
    }

    // For the load-time report, which is the only way to see at a glance what a
    // seed actually produced
    inline const char *StateName(ChamberState state)
    {
        switch (state)
        {
            case ChamberState::Pristine: return "pristine";
            case ChamberState::Wrecked:  return "wrecked";
            case ChamberState::Rubble:   return "rubble";
            case ChamberState::Ashes:    return "burned out";
            case ChamberState::Campsite: return "campsite";
            case ChamberState::Flooded:  return "flooded";
            case ChamberState::Stripped: return "stripped bare";
            default:                     return "?";
        }
    }

    // Rolled per room. See the note on ChamberState for why this is not the DMG's
    // own distribution.
    constexpr int StateWeights[(int)ChamberState::Count] =
    {
        12, // Pristine
        4,  // Wrecked
        3,  // Rubble
        2,  // Ashes
        2,  // Campsite
        1,  // Flooded
        1,  // Stripped
    };

    //------------------------------------------------------------------------------
    // A campsite ignores the room's own palette entirely - that is what makes it
    // read as somebody else's camp rather than as this room with fewer things in
    // it. Held here so the dressing pass has one place to look.
    //------------------------------------------------------------------------------
    constexpr const char *CampsiteEdge[] =
    {
        "props_large/crates_stacked", "props_medium/barrel_small_stack",
        "props_large/bed_floor", "props_large/box_large", nullptr
    };

    constexpr const char *CampsiteScatter[] =
    {
        "props_small/torch_lit", "props_small/plate_food_A", "props_small/bottle_A_brown",
        "props_medium/stool", "props_small/candle_lit", nullptr
    };

    // Swapped in for a share of the palette when the ceiling has come down
    constexpr const char *RubbleProps[] =
    {
        "props_medium/rubble_large", "props_medium/rubble_half", nullptr
    };

    //------------------------------------------------------------------------------
    // Floor overrides. The floor pass draws one tile per open cell, so a room in a
    // given state can simply name a different tile and every cell it owns changes.
    // nullptr leaves the default floor/floor_tile_large alone.
    //------------------------------------------------------------------------------
    inline const char *FloorFor(ChamberState state)
    {
        switch (state)
        {
            case ChamberState::Ashes:   return "floor/floor_dirt_large";
            case ChamberState::Rubble:  return "floor/floor_tile_large_rocks";
            case ChamberState::Flooded: return "floor/floor_dirt_large_rocky";
            default:                    return nullptr;
        }
    }
}
