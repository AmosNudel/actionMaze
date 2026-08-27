#pragma once

#include "combat/Buff.h"
#include "raylib.h"

#include <vector>

class AssetManager;
class Player;

//----------------------------------------------------------------------------------
// Health, mana, and a temporary buff, lying on the floor - the same idea as a
// currency drop (see world/Loot.h) but PAID INTO THE PLAYER directly rather than
// into a purse: walking over one heals, restores mana, or grants a buff on the
// spot, instead of banking a number to spend later at a counter.
//
// --- Why not LootManager -----------------------------------------------------------
// A currency drop is patient - it can sit uncollected for a floor and still be
// worth exactly as much when it is finally picked up. A pickup is not: it exists
// to be taken NOW, mid-fight, and its whole value is in the moment it is grabbed.
// Folding a third payout rule into LootManager would mean every read of that
// class had to ask "which kind of drop is this" - the same reasoning that keeps
// the events their own class rather than a mode of the loot one.
//
// --- Real props, coloured to say what they are --------------------------------------
// Health and mana both borrow the dungeon pack's own potion bottle, tinted red or
// blue the way a currency drop borrows the coin and tints it - see Loot.h. There is
// no buff prop in the pack at all, so a Buff pickup borrows a plate of food instead,
// tinted its own buff's colour and given a bigger aura on top - seeing a plate of
// food should not read as "eat me", and the light around it is what says "this one
// is not a snack".
//----------------------------------------------------------------------------------
enum class PickupKind { Health, Mana, Buff, Count };

//----------------------------------------------------------------------------------
// One pickup, on the floor. What a Buff pickup grants comes off combat/Buff.h's
// table - see the note there for why that table lives apart from this class.
//----------------------------------------------------------------------------------
struct FloorPickup
{
    PickupKind kind = PickupKind::Health;
    BuffKind buff = BuffKind::Might;    // Only meaningful when kind == Buff

    Vector3 at{};
    float age = 0.0f;      // Drives the bob, the spin and the aura's pulse
};

class PickupManager
{
public:
    void Load(AssetManager &assets);

    void Spawn(PickupKind kind, Vector3 at);

    // A buff, whichever the table rolls - so a caller wanting "one of the four,
    // I don't care which" does not have to know the table exists.
    void SpawnBuff(Vector3 at);

    void Update(float delta);

    // Inside BeginMode3D
    void Draw(const Camera3D &camera) const;

    // Takes whatever the player is standing on and pays it straight into
    // `player` - Heal, GiveMana or ApplyBuff, whichever the pickup was. Unlike
    // LootManager::Collect this hands nothing back: there is no purse column
    // for "how much health", and a caller wanting to know what just happened
    // can read Player's own health, mana and buff state directly.
    void Collect(Vector3 feet, Player &player);

    void Clear();

    int Count() const { return (int)pickups.size(); }

private:
    std::vector<FloorPickup> pickups;

    Texture2D *glow = nullptr;

    Model *bottle = nullptr;        // Health and mana both borrow it, tinted
    Model *food[2] = { nullptr, nullptr };   // Buff's stand-in - see the class note

    Model *ModelFor(const FloorPickup &pickup) const;
    Color ColourFor(const FloorPickup &pickup) const;
};
