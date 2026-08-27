#include "world/Pickup.h"

#include "core/Config.h"
#include "entities/Player.h"
#include "raymath.h"
#include "render/AssetManager.h"
#include "render/Glow.h"
#include "rlgl.h"

#include <cmath>
#include <string>

namespace
{
    constexpr const char *DungeonDir = "models/dungeon/";
    constexpr const char *DungeonTexture = "models/dungeon/dungeon_texture.png";

    // See the identical helper in Loot.cpp - duplicated rather than shared
    // because each caller binds a different handful of props, and a shared
    // version would need to take the list as a parameter for no shorter a
    // function.
    Model *LoadProp(AssetManager &assets, Shader &lit, const char *name)
    {
        const std::string path = std::string(DungeonDir) + name;

        if (!FileExists(AssetManager::Resolve(path).c_str())) return nullptr;

        Model &model = assets.GetModel(path, DungeonTexture);

        for (int i = 0; i < model.materialCount; ++i) model.materials[i].shader = lit;

        return &model;
    }
}

void PickupManager::Load(AssetManager &assets)
{
    glow = &GlowTexture(assets);

    Shader &lit = assets.GetShader("shaders/lit.vs", "shaders/lit.fs");

    // One bottle for both health and mana, tinted apart - the same idiom
    // LootManager borrows one coin model for every currency and tints it. The
    // labeled variant reads as a potion rather than as an ordinary bottle on a
    // shelf, which matters more here than it does for a pile of furniture.
    bottle = LoadProp(assets, lit, "props_small/bottle_A_labeled_brown.gltf");

    // Buff has no prop of its own - see the class note on PickupKind. Two food
    // props rather than one, so two buffs found near each other do not look
    // like the exact same pickup twice.
    food[0] = LoadProp(assets, lit, "props_small/plate_food_A.gltf");
    food[1] = LoadProp(assets, lit, "props_small/plate_food_B.gltf");

    if (bottle == nullptr)
    {
        TraceLog(LOG_WARNING, "PICKUPS: no bottle prop - health and mana fall back to the glow billboard");
    }
}

Model *PickupManager::ModelFor(const FloorPickup &pickup) const
{
    if (pickup.kind == PickupKind::Buff) return food[(int)pickup.buff & 1];

    return bottle;
}

Color PickupManager::ColourFor(const FloorPickup &pickup) const
{
    switch (pickup.kind)
    {
        case PickupKind::Health: return { 220, 70, 80, 255 };     // The HUD's own health red
        case PickupKind::Mana:   return { 110, 165, 240, 255 };   // The HUD's own mana blue
        default:                 return BuffAt(pickup.buff).colour;
    }
}

void PickupManager::Clear()
{
    pickups.clear();
}

void PickupManager::Spawn(PickupKind kind, Vector3 at)
{
    FloorPickup pickup;

    pickup.kind = kind;
    pickup.at = at;

    pickups.push_back(pickup);
}

void PickupManager::SpawnBuff(Vector3 at)
{
    FloorPickup pickup;

    pickup.kind = PickupKind::Buff;
    pickup.buff = (BuffKind)GetRandomValue(0, BuffCount() - 1);
    pickup.at = at;

    pickups.push_back(pickup);
}

void PickupManager::Update(float delta)
{
    for (FloorPickup &pickup : pickups) pickup.age += delta;
}

//----------------------------------------------------------------------------------
// A bottle or a plate, turning slowly where it was seeded, with a soft aura under
// it - see render/Glow.h's DrawAura. Every pickup gets one, not only the ones
// missing a model: it is what makes a small prop on the floor read as something
// to walk INTO rather than as another piece of the dungeon's own furniture, the
// same job the vendor and event columns of light do at room scale. Buff pickups
// get the bigger aura - see Config::PickupAuraBuffCore/Halo - because a plate of
// food otherwise reads as exactly that, and the light is the one thing telling
// the player it is not.
//
// No pop-out on arrival, unlike a currency drop - a pickup is seeded when the
// floor is built rather than thrown clear of a body that just died, so there is
// nothing here to be thrown clear OF. Only the bob and the turn, forever.
//----------------------------------------------------------------------------------
void PickupManager::Draw(const Camera3D &camera) const
{
    if (pickups.empty()) return;

    for (const FloorPickup &pickup : pickups)
    {
        Model *model = ModelFor(pickup);
        if (model == nullptr) continue;

        const float bob = sinf(pickup.age*Config::PickupBobRate)*Config::PickupBobHeight;

        const Vector3 at = { pickup.at.x, pickup.at.y + Config::PickupRestHeight + bob,
                             pickup.at.z };

        const float yaw = pickup.age*Config::PickupSpinRate*RAD2DEG;
        const Color tint = ColourFor(pickup);

        DrawModelEx(*model, at, { 0.0f, 1.0f, 0.0f }, yaw,
                   { Config::PickupPropScale, Config::PickupPropScale, Config::PickupPropScale }, tint);
    }

    if (glow == nullptr) return;

    rlDisableDepthMask();
    BeginBlendMode(BLEND_ADDITIVE);

    for (const FloorPickup &pickup : pickups)
    {
        const bool missing = (ModelFor(pickup) == nullptr);
        const bool buff = (pickup.kind == PickupKind::Buff);

        const float bob = sinf(pickup.age*Config::PickupBobRate)*Config::PickupBobHeight;

        const Vector3 at = { pickup.at.x, pickup.at.y + Config::PickupRestHeight + bob,
                             pickup.at.z };

        const Color colour = ColourFor(pickup);

        // A missing model falls back to the full glow billboard standing in for
        // it entirely - the same rule Loot.h's "missing is not an error" idiom
        // follows - which is why it borrows the buff's bigger size regardless
        // of what it actually is: a pickup with nothing else to look at needs
        // all the light it can get.
        const float core = (missing || buff) ? Config::PickupAuraBuffCore : Config::PickupAuraCore;
        const float halo = (missing || buff) ? Config::PickupAuraBuffHalo : Config::PickupAuraHalo;

        DrawAura(camera, *glow, at, pickup.at.y, colour, core, halo,
                pickup.age*Config::PickupSpinRate);
    }

    EndBlendMode();
    rlEnableDepthMask();
}

//----------------------------------------------------------------------------------
// Paid straight into the player - see the class note on why this has no purse to
// hand a total back through.
//----------------------------------------------------------------------------------
void PickupManager::Collect(Vector3 feet, Player &player)
{
    for (int i = (int)pickups.size() - 1; i >= 0; --i)
    {
        const FloorPickup &pickup = pickups[(size_t)i];

        const float dx = feet.x - pickup.at.x;
        const float dz = feet.z - pickup.at.z;

        if ((dx*dx + dz*dz) > (Config::PickupTakeRadius*Config::PickupTakeRadius)) continue;

        switch (pickup.kind)
        {
            case PickupKind::Health: player.Heal(Config::PickupHealthAmount); break;
            case PickupKind::Mana:   player.GiveMana(Config::PickupManaAmount); break;
            default:                 player.ApplyBuff(pickup.buff); break;
        }

        pickups.erase(pickups.begin() + i);
    }
}
