#pragma once

#include "progress/Purse.h"
#include "raylib.h"

//----------------------------------------------------------------------------------
// The three vendors, and what each one trades in.
//
// There are three, and the split is the point: each sells a different KIND of power
// for a different currency, so the three currencies cannot be substituted for each
// other and a run is shaped by which of them it happened to earn.
//
//     MERCHANT   weapons   for COINS       - every kill pays a few; also forges
//     MYSTIC     schools   for GEMS        - a rare drop; also empowers
//     CAPTAIN    traits    for CONTRACTS   - events only; also a free respec
//
// --- Where they stand ----------------------------------------------------------------
// One to three per floor, each in its own room, and WHICH rooms is decided by the room
// kinds themselves: every RoomKindSpec carries a short list of the vendors that make
// sense standing in it. A mystic belongs in a library or a shrine; a captain belongs in
// a guardroom or a barracks; nobody belongs in a lair, because something already lives
// there.
//
// That is deliberately not a random scatter. A vendor found in a room that suits them
// is a room that has a reason to exist, and the alternative - a merchant standing in a
// crypt - makes both the vendor and the room mean less.
//
// --- The art -------------------------------------------------------------------------
// There is none yet. A vendor is drawn as a coloured column of light with their name
// over it, which is the same object the events and the portal already use - so the
// player learns "a column of light is something to walk into" once and it keeps being
// true. Swapping in a model later is one function.
//----------------------------------------------------------------------------------
enum class NpcKind
{
    Merchant = 0,
    Mystic,
    Captain,

    Count
};

struct NpcDef
{
    const char *name;       // Over their head and on the prompt - keep it short
    const char *title;      // The shop page's subheading
    Currency currency;      // What they trade in, for the price columns
    Color colour;           // Their column, and the shop page's accent
};

const NpcDef &NpcAt(NpcKind kind);

// The three, by their room-kind lists. A kind whose list is empty never gets a vendor,
// which is how the entrance, the portal room and the lair stay clear.
bool NpcSuitsRoom(NpcKind kind, int roomKind);
