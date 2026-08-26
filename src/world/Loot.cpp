#include "world/Loot.h"

#include "core/Config.h"
#include "raymath.h"
#include "render/AssetManager.h"
#include "render/Glow.h"
#include "rlgl.h"

#include <cmath>

namespace
{
    // How long the pop outward takes, and how far it goes. Short and small: it is
    // there to separate two drops out of one body, not to throw loot across a room.
    constexpr float PopTime = 0.35f;
    constexpr float PopReach = 0.55f;

    // The bob, once it has settled
    constexpr float RestHeight = 0.55f;
    constexpr float BobHeight = 0.10f;
    constexpr float BobRate = 2.4f;
    constexpr float SpinRate = 2.0f;

    // How close the player's feet have to be. Generous on purpose - a currency the
    // player has to line up with is a currency they walk past.
    constexpr float TakeRadius = 1.1f;

    // The drop's own size, and how much bigger the soft halo under it is drawn
    constexpr float CoreSize = 0.22f;
    constexpr float HaloSize = 0.70f;

    // Past this the oldest is dropped as a new one lands. A floor's whole worth of
    // uncollected gems is a player who has walked past everything, and the newest
    // drop is the one they can still see.
    constexpr int MaxDrops = 48;
}

Color CurrencyColour(Currency currency)
{
    switch (currency)
    {
        // The merchant's gold. Never actually dropped - coins credit straight to the
        // purse - but the HUD prints the number in it, so it lives with the others.
        case Currency::Coins:     return { 245, 215, 120, 255 };

        // The mystic's violet, which is also the magic palette
        case Currency::Gems:      return { 190, 130, 255, 255 };

        // The captain's red
        case Currency::Contracts: return { 235, 120, 110, 255 };

        default:                  return WHITE;
    }
}

void LootManager::Load(AssetManager &assets)
{
    glow = &GlowTexture(assets);
}

void LootManager::Clear()
{
    drops.clear();
}

void LootManager::Spawn(Currency currency, int amount, Vector3 at, int index, int total)
{
    if (amount <= 0) return;

    // Oldest first. A pool that silently refused new drops would lose the one the
    // player just earned and keep one they have already decided to ignore.
    if ((int)drops.size() >= MaxDrops) drops.erase(drops.begin());

    LootDrop drop;

    drop.currency = currency;
    drop.amount = amount;
    drop.rest = at;

    // Fanned around the body rather than randomly scattered, so a batch reads as one
    // payout coming apart instead of as several unrelated things appearing
    const float angle = (total > 1) ? ((index/(float)total)*2.0f*PI) : 0.0f;
    const float reach = (total > 1) ? PopReach : 0.0f;

    drop.pop = { cosf(angle)*reach, 0.0f, sinf(angle)*reach };

    drops.push_back(drop);
}

void LootManager::Update(float delta)
{
    for (LootDrop &drop : drops) drop.age += delta;
}

//----------------------------------------------------------------------------------
// A bright core in a soft halo, both billboarded, both additive.
//
// The same two-part object every mote and every beam in the game is made of, and
// sharing it is the point: the player has already learned that additive light means
// something to interact with, and a drop that was drawn any other way would have to
// teach that again.
//
// Depth is READ but not written, so a gem behind a table is hidden by the table and
// two gems side by side do not cut each other's halo into a hard edge.
//----------------------------------------------------------------------------------
void LootManager::Draw(const Camera3D &camera) const
{
    if ((glow == nullptr) || drops.empty()) return;

    rlDisableDepthMask();
    BeginBlendMode(BLEND_ADDITIVE);

    for (const LootDrop &drop : drops)
    {
        const Color colour = CurrencyColour(drop.currency);

        // Eased out rather than linear, so it leaves the body quickly and settles
        // slowly - which reads as being thrown rather than as being slid
        const float popT = (drop.age >= PopTime) ? 1.0f : (drop.age/PopTime);
        const float eased = 1.0f - (1.0f - popT)*(1.0f - popT);

        const float bob = sinf(drop.age*BobRate)*BobHeight;

        const Vector3 at = { drop.rest.x + drop.pop.x*eased,
                             drop.rest.y + RestHeight + bob,
                             drop.rest.z + drop.pop.z*eased };

        // A slow turn on the core's size rather than on its orientation - it is a
        // billboard and has no orientation to turn - so it pulses instead
        const float pulse = 1.0f + 0.12f*sinf(drop.age*SpinRate);

        DrawBillboard(camera, *glow, at, HaloSize, Fade(colour, 0.30f));
        DrawBillboard(camera, *glow, at, CoreSize*pulse, colour);

        // A pool on the floor under it, so the light has somewhere to land and the
        // drop reads as being IN the room rather than pasted over it
        const Vector3 floorAt = { at.x, drop.rest.y + 0.02f, at.z };

        DrawBillboard(camera, *glow, floorAt, HaloSize*0.9f, Fade(colour, 0.16f));
    }

    EndBlendMode();
    rlEnableDepthMask();
}

bool LootManager::Collect(Vector3 feet, Purse &purse, int taken[(int)Currency::Count])
{
    for (int i = 0; i < (int)Currency::Count; ++i) taken[i] = 0;

    bool any = false;

    for (int i = (int)drops.size() - 1; i >= 0; --i)
    {
        const LootDrop &drop = drops[(size_t)i];

        // Not on the frame it appeared. The player is standing on top of whatever
        // they just killed, and loot collected before it has finished popping out is
        // loot they never saw arrive.
        if (drop.age < PopTime*0.5f) continue;

        const float dx = feet.x - drop.rest.x;
        const float dz = feet.z - drop.rest.z;

        if ((dx*dx + dz*dz) > (TakeRadius*TakeRadius)) continue;

        purse.Add(drop.currency, drop.amount);
        taken[(int)drop.currency] += drop.amount;
        any = true;

        drops.erase(drops.begin() + i);
    }

    return any;
}
