#include "world/Loot.h"

#include "core/Config.h"
#include "raymath.h"
#include "render/AssetManager.h"
#include "render/Glow.h"
#include "rlgl.h"

#include <cmath>
#include <string>

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
        // The merchant's gold. Ordinary kills still credit it straight to the purse
        // - see the note on Loot.h - but a treasure room can drop it physically
        // now too (Game::SeedRoomLoot), and the character page prints the total
        // regardless of which way it arrived.
        case Currency::Coins:     return { 245, 215, 120, 255 };

        // The mystic's violet, which is also the magic palette
        case Currency::Gems:      return { 190, 130, 255, 255 };

        // The captain's red
        case Currency::Contracts: return { 235, 120, 110, 255 };

        default:                  return WHITE;
    }
}

namespace
{
    constexpr const char *DungeonDir = "models/dungeon/";
    constexpr const char *DungeonTexture = "models/dungeon/dungeon_texture.png";

    //------------------------------------------------------------------------------
    // Loads one prop against the shared dungeon atlas and rebinds it onto the lit
    // shader - the same two steps EventManager::Load takes for the relic, and for
    // the same reasons: without the atlas argument glTF's relative-URI texture
    // gives this its own private copy of a PNG the level already has, and without
    // the shader it comes out flat white beside furniture that is being lit.
    //
    // Missing is not an error - see the note on Loot.h - so this returns null
    // rather than logging a warning per model; LootManager::Load logs once for
    // the whole set instead.
    //------------------------------------------------------------------------------
    Model *LoadProp(AssetManager &assets, Shader &lit, const char *name)
    {
        const std::string path = std::string(DungeonDir) + name;

        if (!FileExists(AssetManager::Resolve(path).c_str())) return nullptr;

        Model &model = assets.GetModel(path, DungeonTexture);

        for (int i = 0; i < model.materialCount; ++i) model.materials[i].shader = lit;

        return &model;
    }
}

void LootManager::Load(AssetManager &assets)
{
    glow = &GlowTexture(assets);

    Shader &lit = assets.GetShader("shaders/lit.vs", "shaders/lit.fs");

    coin = LoadProp(assets, lit, "props_small/coin.gltf");
    stackSmall = LoadProp(assets, lit, "props_small/coin_stack_small.gltf");
    stackMedium = LoadProp(assets, lit, "props_small/coin_stack_medium.gltf");
    stackLarge = LoadProp(assets, lit, "props_small/coin_stack_large.gltf");

    if (coin == nullptr)
    {
        TraceLog(LOG_WARNING, "LOOT: no coin props - drops fall back to the glow billboard");
    }
}

//----------------------------------------------------------------------------------
// Which of the four a drop this size draws as. Thresholds are a first guess at
// what reads as "a coin", "a few", "a handful" and "a pile" - there is no amount
// on the table yet that reaches the top one from a single kill, so LARGE is
// currently only ever seen from the treasure-room seeding pass.
//----------------------------------------------------------------------------------
Model *LootManager::ModelFor(int amount) const
{
    if (amount >= 16) return (stackLarge != nullptr) ? stackLarge : stackMedium;
    if (amount >= 6)  return (stackMedium != nullptr) ? stackMedium : stackSmall;
    if (amount >= 2)  return (stackSmall != nullptr) ? stackSmall : coin;

    return coin;
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
// A coin or a coin stack, spinning slowly where it fell - or, for whichever drops
// have no model at all, the glow billboard everything additive in the game is
// made of.
//
// Two passes rather than one branch per drop: the props are opaque and
// depth-tested like any other piece of furniture, and the fallback glow is
// additive with depth writes off, same as a mote's impact. Mixing the two states
// per draw call would mean toggling the blend mode and the depth mask inside the
// loop, once per drop, for what is in practice never more than a handful of
// bodies on screen.
//----------------------------------------------------------------------------------
void LootManager::Draw(const Camera3D &camera) const
{
    if (drops.empty()) return;

    for (const LootDrop &drop : drops)
    {
        Model *model = ModelFor(drop.amount);
        if (model == nullptr) continue;

        // Eased out rather than linear, so it leaves the body quickly and settles
        // slowly - which reads as being thrown rather than as being slid
        const float popT = (drop.age >= PopTime) ? 1.0f : (drop.age/PopTime);
        const float eased = 1.0f - (1.0f - popT)*(1.0f - popT);

        const float bob = sinf(drop.age*BobRate)*BobHeight;

        const Vector3 at = { drop.rest.x + drop.pop.x*eased,
                             drop.rest.y + RestHeight + bob,
                             drop.rest.z + drop.pop.z*eased };

        // A slow turn in place, standing in for the pulse the billboard version
        // used - a spinning coin reads as "something worth having" the way the
        // pulse used to, and it is a real turn rather than a fake one now that
        // there is a mesh to turn.
        const float yaw = drop.age*SpinRate*RAD2DEG;

        // Untinted for coins - the model's own gold reads as coins on its own -
        // and the currency's own hue for gems and contracts, which borrow the
        // same mesh. See the note on Loot.h for why there is no separate gem prop.
        const Color tint = (drop.currency == Currency::Coins) ? WHITE : CurrencyColour(drop.currency);

        DrawModelEx(*model, at, { 0.0f, 1.0f, 0.0f }, yaw,
                   { Config::LootPropScale, Config::LootPropScale, Config::LootPropScale }, tint);
    }

    if (glow == nullptr) return;

    //------------------------------------------------------------------------------
    // The additive pass: a full glow billboard for whichever drops have no prop
    // model at all, and a smaller aura on top of the model for whichever drops
    // are a currency worth more than an ordinary coin.
    //
    // Gems and contracts get the aura EVEN THOUGH the coin prop they borrow
    // loaded fine - see the note on Loot.h for why there is no separate gem
    // model. A coin the pack drew for gold reads as gold on its own; the same
    // mesh tinted violet or red needs the light around it to read as something
    // rarer rather than as a coloured coin.
    //------------------------------------------------------------------------------
    bool anyGlow = false;

    for (const LootDrop &drop : drops)
    {
        if ((ModelFor(drop.amount) == nullptr) || (drop.currency != Currency::Coins))
        {
            anyGlow = true;
            break;
        }
    }

    if (!anyGlow) return;

    rlDisableDepthMask();
    BeginBlendMode(BLEND_ADDITIVE);

    for (const LootDrop &drop : drops)
    {
        const bool fallback = (ModelFor(drop.amount) == nullptr);

        if (!fallback && (drop.currency == Currency::Coins)) continue;

        const Color colour = CurrencyColour(drop.currency);

        const float popT = (drop.age >= PopTime) ? 1.0f : (drop.age/PopTime);
        const float eased = 1.0f - (1.0f - popT)*(1.0f - popT);

        const float bob = sinf(drop.age*BobRate)*BobHeight;

        const Vector3 at = { drop.rest.x + drop.pop.x*eased,
                             drop.rest.y + RestHeight + bob,
                             drop.rest.z + drop.pop.z*eased };

        const float core = fallback ? CoreSize : Config::LootAuraCore;
        const float halo = fallback ? HaloSize : Config::LootAuraHalo;

        DrawAura(camera, *glow, at, drop.rest.y, colour, core, halo, drop.age*SpinRate,
                Config::LootAuraIntensity);
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
