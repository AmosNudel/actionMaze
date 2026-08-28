#include "world/Skyline.h"

#include "core/Config.h"
#include "raymath.h"
#include "render/AssetManager.h"
#include "world/Level.h"

#include <random>
#include <string>

namespace
{
    // Every piece sits on the one atlas the pack ships, which is what makes a
    // hundred buildings cost one texture - see AssetManager::GetModel
    const std::string Dir = "models/skyline/";
    const std::string Atlas = Dir + "hexagons_medieval.png";

    //------------------------------------------------------------------------------
    // The kit, split by how tall a piece stands relative to its footprint.
    //
    // Low pieces are wider than they are high and read as roofs from inside the
    // maze; tall ones are the opposite, and are the only things with any chance of
    // clearing the wall line from a distance. Which list a name is in was read off
    // the pack's own bounding boxes rather than guessed from the name - which is why
    // the market and the mine are down here with the houses despite being large.
    // They are large ACROSS, and nothing about that shows over a wall.
    //------------------------------------------------------------------------------
    constexpr const char *LowPieces[] =
    {
        "building_home_A_red",
        "building_home_B_red",
        "building_tavern_red",
        "building_blacksmith_red",
        "building_market_red",
        "building_mine_red",
        "building_well_red",
        "building_lumbermill_red",
        "building_windmill_red",
        "building_watermill_red",
        "building_barracks_red",
        "building_archeryrange_red",
    };

    constexpr const char *TallPieces[] =
    {
        "building_tower_A_red",
        "building_tower_B_red",
        "building_tower_base_red",
        "building_tower_catapult_red",
        "building_church_red",
    };

    constexpr const char *CastlePiece = "building_castle_red";
}

void Skyline::Load(AssetManager &assets)
{
    low.clear();
    tall.clear();
    castle = nullptr;

    Shader &lit = assets.GetShader("shaders/lit.vs", "shaders/lit.fs");

    // Bound to the level's own shader as it loads, for the reason every other model
    // in this project is: a building lit by a different rule than the wall it is
    // seen over stops looking like it is in the same world.
    auto load = [&](const char *name) -> Model *
    {
        const std::string path = Dir + name + ".gltf";

        if (!FileExists(AssetManager::Resolve(path).c_str())) return nullptr;

        Model &model = assets.GetModel(path, Atlas);

        for (int i = 0; i < model.materialCount; i++) model.materials[i].shader = lit;

        return &model;
    };

    for (const char *name : LowPieces)  { if (Model *m = load(name)) low.push_back(m); }
    for (const char *name : TallPieces) { if (Model *m = load(name)) tall.push_back(m); }

    castle = load(CastlePiece);

    if (low.empty() && tall.empty())
    {
        TraceLog(LOG_WARNING, "SKYLINE: no buildings under %s, the horizon stays empty",
                 Dir.c_str());
    }
}

void Skyline::Clear()
{
    buildings.clear();
}

//----------------------------------------------------------------------------------
// The town, laid out around all four sides of the map.
//
// Built side by side and row by row rather than on a ring, because the map is a
// SQUARE and a ring around a square hugs the middle of each side and pulls away from
// every corner - which reads as four separate hamlets with holes between them rather
// than as one continuous complex.
//
// Every position comes out of one generator seeded from the map's own seed, so a
// floor's skyline is a fact about that floor: the same every time it is drawn, and a
// different place on the next floor down.
//----------------------------------------------------------------------------------
void Skyline::Place(const Level &level)
{
    Clear();

    if (low.empty() && tall.empty() && (castle == nullptr)) return;

    const Map &map = level.Grid();

    const float size = map.CellSize();
    const float spanX = map.Width()*size;
    const float spanZ = map.Depth()*size;

    // The map's own footprint, which every distance below is measured out from
    const float midX = spanX*0.5f;
    const float midZ = spanZ*0.5f;

    std::mt19937 rng(map.Seed() ^ 0x5bf03635u);

    auto roll = [&](float lo, float hi)
    {
        return lo + (hi - lo)*((float)(rng() % 10000u)/10000.0f);
    };

    const float base = level.FloorHeight() - Config::SkylineSink;

    for (int row = 0; row < Config::SkylineRows; ++row)
    {
        const float out = Config::SkylineNearGap + row*Config::SkylineRowGap;
        const float scale = Config::SkylineRowScale[row];

        //--------------------------------------------------------------------------
        // The near row is roofs and the rows behind it are towers - see the note on
        // the two lists. A row whose list is empty falls back to the other one, so a
        // half-missing pack thins the town rather than leaving a gap in it.
        //--------------------------------------------------------------------------
        const std::vector<Model *> &kit = (row == 0)
                                        ? (low.empty() ? tall : low)
                                        : (tall.empty() ? low : tall);

        if (kit.empty()) continue;

        for (int side = 0; side < 4; ++side)
        {
            // How far along this side the band runs, widened by the row's own
            // distance so an outer row reaches past the corners of the one inside
            // it. Without that the town is a square with four notches cut out.
            const float along = ((side % 2) == 0) ? spanX : spanZ;
            const float reach = along*0.5f + out;

            const float slot = (reach*2.0f)/(float)Config::SkylinePerRow;

            for (int i = 0; i < Config::SkylinePerRow; ++i)
            {
                const float centre = -reach + (i + 0.5f)*slot;

                const float jitter = roll(-1.0f, 1.0f)*slot*Config::SkylineJitter;
                const float depth = out + roll(-1.0f, 1.0f)*Config::SkylineRowGap*0.18f;

                Placed placed;

                placed.model = kit[rng() % kit.size()];

                // Sides 0 and 2 run along X at a fixed Z; 1 and 3 the other way round
                switch (side)
                {
                    case 0:  placed.at = { midX + centre + jitter, base, -depth }; break;
                    case 1:  placed.at = { spanX + depth, base, midZ + centre + jitter }; break;
                    case 2:  placed.at = { midX + centre + jitter, base, spanZ + depth }; break;
                    default: placed.at = { -depth, base, midZ + centre + jitter }; break;
                }

                // Free rotation rather than snapped to the map's grid. These are not
                // part of the dungeon's architecture, and lining them up with it
                // would say that they were.
                placed.yaw = roll(0.0f, 2.0f*PI);

                placed.scale = scale*roll(1.0f - Config::SkylineScaleVary,
                                          1.0f + Config::SkylineScaleVary);

                buildings.push_back(placed);
            }
        }
    }

    //------------------------------------------------------------------------------
    // The castle, placed once on a side of its own choosing.
    //
    // Deliberately not part of the loop above. It is the one building meant to be
    // recognised from anywhere on the floor, and a landmark that might roll twice on
    // one side and not at all on another is not a landmark.
    //------------------------------------------------------------------------------
    if (castle != nullptr)
    {
        const int side = (int)(rng() % 4u);
        const float out = Config::SkylineCastleOut;
        const float drift = roll(-0.18f, 0.18f);

        Placed placed;

        placed.model = castle;
        placed.scale = Config::SkylineCastleScale;
        placed.yaw = roll(0.0f, 2.0f*PI);

        switch (side)
        {
            case 0:  placed.at = { midX + spanX*drift, base, -out }; break;
            case 1:  placed.at = { spanX + out, base, midZ + spanZ*drift }; break;
            case 2:  placed.at = { midX + spanX*drift, base, spanZ + out }; break;
            default: placed.at = { -out, base, midZ + spanZ*drift }; break;
        }

        buildings.push_back(placed);
    }

    TraceLog(LOG_INFO, "SKYLINE: %i buildings around a %.0f x %.0f map",
             (int)buildings.size(), spanX, spanZ);
}

void Skyline::Draw() const
{
    for (const Placed &placed : buildings)
    {
        if (placed.model == nullptr) continue;

        // Through the model's own transform and handed back as found - the same
        // idiom Level::DrawProp uses, and for the same reason: these Models are the
        // AssetManager's, and several placements share each one.
        placed.model->transform = MatrixRotateY(placed.yaw);
        DrawModel(*placed.model, placed.at, placed.scale, WHITE);
        placed.model->transform = MatrixIdentity();
    }
}
