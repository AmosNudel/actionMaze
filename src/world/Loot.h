#pragma once

#include "progress/Purse.h"
#include "raylib.h"

#include <vector>

class AssetManager;

//----------------------------------------------------------------------------------
// The rare currencies, lying on the floor waiting to be walked over.
//
// --- Why only two of the three -------------------------------------------------------
// Coins are NOT here. They credit straight to the purse the moment something dies,
// with a number floating off the body, and that is deliberate: a physical coin per
// kill would flood this pool the moment a swept blade took a whole pack, and the coins
// the player walked past would be coins lost to a pool overflow rather than to a
// decision.
//
// Gems and contracts are physical for exactly the opposite reason. They are rare, and
// a gem that appeared as a number among a dozen other numbers is a gem the player
// never noticed earning. Making them something to walk to is what turns "an elite
// dropped something" into a moment.
//
// --- The float is not physics ----------------------------------------------------------
// A drop is pinned to the point it was left on and bobs above it forever. It never
// rolls away, never falls through the floor, and never has to be chased. Its one
// concession to the world is a short pop outward as it appears, so three gems out of
// one body land beside each other instead of inside each other.
//
// --- Placeholder art ---------------------------------------------------------------------
// There is none. A drop is the shared glow billboard in its currency's colour, which
// is the same object the motes and the beams are made of - so it already reads as
// "something worth having" without a single new texture. A model later is one draw
// call.
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

    Texture2D *glow = nullptr;      // Shared, owned by the AssetManager
};

// The colour a currency's drop floats in, and what its number is printed in. Here
// rather than beside the Purse because it is presentation - the purse itself has no
// opinion about what gold looks like.
Color CurrencyColour(Currency currency);
