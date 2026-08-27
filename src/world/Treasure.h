#pragma once

#include "raylib.h"

#include <vector>

class AssetManager;
class Arsenal;

//----------------------------------------------------------------------------------
// A chest that hands over one weapon the player does not already own, for free.
//
// Seeded by Game::SeedRoomLoot alongside the Vault's own coin piles - see the note
// there for why that is the one place that already knows which Vault this floor
// has and that it is not sitting on top of a vendor, an event or a camp. This
// class only carries the chest once it exists: where it is, whether it has been
// opened, and what the player got.
//
// --- Rarer than a merchant, on purpose --------------------------------------------
// A merchant is guaranteed on every floor and is the ordinary way an arsenal
// grows - a shop with nothing to sell would be a broken shop. A chest is meant to
// be a find, not a second shop, so it is a coin flip (Config::TreasureChestChance)
// on top of a Vault even existing, and Vault is already the rarest room kind on
// RoomKind.h's own weight table.
//
// --- Placeholder art -----------------------------------------------------------------
// The same gold chest model the Vault's own decorative anchor and the Defend
// event's relic already use - a chest reads as a chest everywhere it appears,
// and there is exactly one in the pack worth the name. A soft gold light around
// it is what tells the player it is not the anchor's decoration.
//----------------------------------------------------------------------------------
class TreasureManager
{
public:
    void Load(AssetManager &assets);

    // One chest at `at`. Called from Game::SeedRoomLoot, which has already
    // decided this floor earns one and where it goes.
    void Spawn(Vector3 at);

    void Update(float delta);

    // Inside BeginMode3D
    void Draw(const Camera3D &camera) const;

    // True while the player is standing at an unopened chest - what the HUD's
    // prompt and Game's interact key both read, the same shape as
    // VendorManager::At.
    bool At(Vector3 position) const;

    //------------------------------------------------------------------------------
    // Opens whichever chest `position` is standing at, if any, and grants one
    // random weapon not already in `arsenal`. A no-op if nothing is in range.
    //
    // An arsenal with nothing left to give still spends the chest - see
    // LastFoundName, which reads "nothing new" for that case - the same way a
    // fully-bought merchant still has a counter, just nothing on it.
    //------------------------------------------------------------------------------
    void Open(Vector3 position, Arsenal &arsenal);

    // What Open last granted, and how long ago - for the HUD's own fading
    // message, the same idiom Player::lastHitAge already uses.
    const char *LastFoundName() const { return lastFoundName; }
    float LastFoundAge() const { return lastFoundAge; }

    void Clear();

    int Count() const { return (int)chests.size(); }

private:
    struct Chest
    {
        Vector3 at{};
    };

    std::vector<Chest> chests;

    float lastFoundAge = 1e9f;
    const char *lastFoundName = "";

    Texture2D *glow = nullptr;
    Model *model = nullptr;
};
