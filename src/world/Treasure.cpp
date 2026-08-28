#include "world/Treasure.h"

#include "combat/Weapon.h"
#include "core/Config.h"
#include "progress/Arsenal.h"
#include "render/AssetManager.h"
#include "render/Glow.h"
#include "rlgl.h"

#include <string>

namespace
{
    // The gold light around the chest - warmer than a pickup's or a loot drop's,
    // because this is the one thing on the floor that is entirely reward and
    // nothing else.
    constexpr Color AuraColour = { 255, 210, 90, 255 };

    // How fast the core pulses - see DrawAura's own pulsePhase. Slow: a chest is
    // found and walked to, not chased, so it does not need the urgency a pickup's
    // faster pulse reads as.
    constexpr float PulseRate = 1.0f;
}

//----------------------------------------------------------------------------------
// The same model the Vault's own decorative anchor and the Defend relic already
// use - loaded the same way EventManager::Load binds the relic: against the one
// canonical dungeon atlas, and rebound onto the lit shader. See the note there for
// why both matter.
//----------------------------------------------------------------------------------
void TreasureManager::Load(AssetManager &assets)
{
    glow = &GlowTexture(assets);

    const std::string dir = "models/dungeon/";
    const std::string path = dir + Config::DefendRelicModel;

    if (!FileExists(AssetManager::Resolve(path).c_str()))
    {
        TraceLog(LOG_WARNING, "TREASURE: no %s - a chest is light only", path.c_str());

        return;
    }

    Shader &lit = assets.GetShader("shaders/lit.vs", "shaders/lit.fs");

    model = &assets.GetModel(path, dir + "dungeon_texture.png");

    for (int i = 0; i < model->materialCount; ++i) model->materials[i].shader = lit;
}

void TreasureManager::Spawn(Vector3 at)
{
    Chest chest;

    chest.at = at;

    chests.push_back(chest);
}

void TreasureManager::Update(float delta)
{
    if (lastFoundAge < 1e8f) lastFoundAge += delta;
}

void TreasureManager::Draw(const Camera3D &camera) const
{
    if (chests.empty()) return;

    if (model != nullptr)
    {
        for (const Chest &chest : chests)
        {
            DrawModelEx(*model, chest.at, { 0.0f, 1.0f, 0.0f }, 0.0f,
                       { Config::TreasureChestScale, Config::TreasureChestScale,
                         Config::TreasureChestScale }, WHITE);
        }
    }

    if (glow == nullptr) return;

    rlDisableDepthMask();
    BeginBlendMode(BLEND_ADDITIVE);

    for (const Chest &chest : chests)
    {
        DrawAura(camera, *glow, chest.at, chest.at.y, AuraColour,
                Config::TreasureAuraCore, Config::TreasureAuraHalo,
                (float)GetTime()*PulseRate);
    }

    EndBlendMode();
    rlEnableDepthMask();
}

bool TreasureManager::At(Vector3 position) const
{
    for (const Chest &chest : chests)
    {
        const float dx = position.x - chest.at.x;
        const float dz = position.z - chest.at.z;

        if ((dx*dx + dz*dz) <= Config::TreasureTakeRadius*Config::TreasureTakeRadius) return true;
    }

    return false;
}

void TreasureManager::Open(Vector3 position, Arsenal &arsenal)
{
    for (size_t i = 0; i < chests.size(); ++i)
    {
        const Chest &chest = chests[i];

        const float dx = position.x - chest.at.x;
        const float dz = position.z - chest.at.z;

        if ((dx*dx + dz*dz) > Config::TreasureTakeRadius*Config::TreasureTakeRadius) continue;

        int unowned[MaxWeapons];
        int unownedCount = 0;

        for (int w = 0; (w < arsenal.Count()) && (unownedCount < MaxWeapons); ++w)
        {
            if (!arsenal.Owns(w)) unowned[unownedCount++] = w;
        }

        if (unownedCount > 0)
        {
            const int pick = unowned[GetRandomValue(0, unownedCount - 1)];

            arsenal.Give(pick);

            // The DISPLAY name, not the model name the arsenal keys on - see
            // WeaponDisplayName. This line is read by a player, and "axe_B" is a
            // filename. Safe to hold as a bare pointer: the table it comes back
            // from is static, and the fallback for a weapon with no row is the
            // arsenal's own string, which outlives the message either way.
            lastFoundName = WeaponDisplayName(arsenal.NameAt(pick));
        }
        else
        {
            // Every weapon in the game, already owned - the same "nothing left"
            // outcome a fully-bought merchant has, just found on the floor
            // instead of at a counter.
            lastFoundName = "nothing new";
        }

        lastFoundAge = 0.0f;

        chests.erase(chests.begin() + (long)i);

        return;
    }
}

void TreasureManager::Clear()
{
    chests.clear();
}
