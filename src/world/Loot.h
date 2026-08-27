#pragma once

#include "progress/Purse.h"
#include "raylib.h"

#include <vector>

class AssetManager;

//----------------------------------------------------------------------------------
// The rare currencies, lying on the floor waiting to be walked over.
//
// --- Why an ordinary kill never drops a coin ------------------------------------------
// A KILL still credits coins straight to the purse, with a number floating off the
// body, rather than leaving one here: a physical coin per kill would flood this pool
// the moment a swept blade took a whole pack, and the coins the player walked past
// would be coins lost to a pool overflow rather than to a decision.
//
// Gems and contracts are physical for exactly the opposite reason. They are rare, and
// a gem that appeared as a number among a dozen other numbers is a gem the player
// never noticed earning. Making them something to walk to is what turns "an elite
// dropped something" into a moment.
//
// Coins CAN still be dropped here, deliberately, from exactly one place:
// Game::SeedRoomLoot scatters a few in a Vault to make it read as a treasure room
// rather than a stat increment. That is a scripted, floor-bounded amount rather than
// a per-kill rate, so the flood risk above never applies to it.
//
// --- The float is not physics ----------------------------------------------------------
// A drop is pinned to the point it was left on and bobs above it forever. It never
// rolls away, never falls through the floor, and never has to be chased. Its one
// concession to the world is a short pop outward as it appears, so three gems out of
// one body land beside each other instead of inside each other.
//
// --- Real props, not a glow ----------------------------------------------------------
// A drop is the dungeon pack's own coin (or, for a bigger amount, a coin STACK),
// tinted by CurrencyColour - gold and untouched for coins, violet for gems, red for
// contracts, because the pack ships no gem model at all and a coin says "money" in
// any colour. Loaded the same way EventManager binds the relic: against the shared
// lit shader and the one dungeon texture atlas, so a drop is lit like the room it
// is lying in rather than looking pasted over it.
//
// Falls back to the glow billboard the motes and beams already use if the model is
// missing - the same "missing is not an error" rule the enemy animations follow -
// so a stripped-down asset folder still shows SOMETHING worth walking over.
//----------------------------------------------------------------------------------
struct LootDrop
{
    Currency currency = Currency::Gems;
    int amount = 1;

    Vector3 rest{};         // The floor point it floats above
    Vector3 pop{};          // Sideways offset it drifts out to as it appears

    float age = 0.0f;       // Seconds since it was dropped: drives the pop and the bob
};

class LootManager
{
public:
    void Load(AssetManager &assets);

    //------------------------------------------------------------------------------
    // Drops `amount` of `currency` at `at`, scattered.
    //
    // `index` and `total` are which of a batch this is and how many there are, and
    // they only decide which way it pops - so a bounty's three gems fan out instead
    // of stacking. Passing 0 and 1 is a single drop that pops straight up.
    //------------------------------------------------------------------------------
    void Spawn(Currency currency, int amount, Vector3 at, int index = 0, int total = 1);

    void Update(float delta);

    // Inside BeginMode3D
    void Draw(const Camera3D &camera) const;

    //------------------------------------------------------------------------------
    // Takes everything the player is standing on, paying it into `purse`.
    //
    // All of them in one call rather than one per call: the drops are already a pool
    // the caller does not own, and a loop that had to be run until it returned
    // nothing would be a rule every caller has to remember. Returns how much of each
    // was taken, or nothing at all when the player was not standing on any.
    //------------------------------------------------------------------------------
    bool Collect(Vector3 feet, Purse &purse, int taken[(int)Currency::Count]);

    void Clear();

    int Count() const { return (int)drops.size(); }

private:
    std::vector<LootDrop> drops;

    Texture2D *glow = nullptr;      // Fallback only, if a coin model is missing

    // The coin, and three stacks for bigger amounts - see ModelFor. Null for
    // whichever the asset folder does not have; DrawDrop falls back to the glow
    // billboard for that one drop rather than skipping it.
    Model *coin = nullptr;
    Model *stackSmall = nullptr;
    Model *stackMedium = nullptr;
    Model *stackLarge = nullptr;

    // Which of the four models a drop this size draws as.
    Model *ModelFor(int amount) const;
};

// The colour a currency's drop floats in, and what its number is printed in. Here
// rather than beside the Purse because it is presentation - the purse itself has no
// opinion about what gold looks like.
Color CurrencyColour(Currency currency);
