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
// Each one is a KayKit dungeon-pack adventurer standing in an aura of their own
// colour: the mage sells magic, the rogue sells arms, the knight sells contracts.
// The aura is what the column of light became - the player already learned that
// coloured light means "walk into this" from the portal and the event markers, and
// that promise is kept by keeping the pool and the motes and losing only the height
// that used to stand where the character now does.
//
// The models are STATIC. The dungeon pack's adventurers have no rig and no clips at
// all - six loose body-part meshes and nothing to drive them - so the idle is
// procedural: see VendorManager::Draw. It is a breath and a sway rather than an
// animation, which is as close to alive as an unrigged model gets.
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
    Color colour;           // Their aura, and the shop page's accent

    // Who they are, as a model. Relative to the asset dir, and optional in the same
    // way every other model in this project is: a missing file costs the character
    // and nothing else, because the aura they stand in is drawn either way and the
    // aura is what the player is actually navigating by.
    const char *modelPath;
};

const NpcDef &NpcAt(NpcKind kind);

// The three, by their room-kind lists. A kind whose list is empty never gets a vendor,
// which is how the entrance, the portal room and the lair stay clear.
bool NpcSuitsRoom(NpcKind kind, int roomKind);
