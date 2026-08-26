#include "world/Level.h"

#include "core/Config.h"
#include "entities/Body.h"
#include "raymath.h"
#include "render/AssetManager.h"
#include "world/PathFinder.h"

#include <cfloat>
#include <cmath>
#include <ctime>
#include <random>
#include <string>

namespace
{
    // Edge directions, and the order the vertex mask packs them in
    constexpr int DirEast  = 0;   // +X
    constexpr int DirSouth = 1;   // +Z
    constexpr int DirWest  = 2;   // -X
    constexpr int DirNorth = 3;   // -Z

    int PopCount(int mask)
    {
        int bits = 0;
        for (; mask != 0; mask &= mask - 1) bits++;

        return bits;
    }

    //------------------------------------------------------------------------------
    // Does a junction piece stand where these edges meet?
    //
    // Three or four arms always need one. Two need one only if they turn - two in
    // line are just a wall passing through, and the edge pieces run right over the
    // vertex on their own. One arm is a dead end, which is the capped half's job,
    // and none is open floor.
    //
    // Deliberately a function of the mask alone and not of which models loaded: if
    // a missing asset could change the answer, the halves either side would change
    // length with it and the level would come apart rather than lose one piece.
    //------------------------------------------------------------------------------
    bool HasJunction(int mask)
    {
        const int arms = PopCount(mask);

        if (arms >= 3) return true;
        if (arms < 2) return false;

        constexpr int AcrossX = (1 << DirEast) | (1 << DirWest);
        constexpr int AcrossZ = (1 << DirNorth) | (1 << DirSouth);

        return (mask != AcrossX) && (mask != AcrossZ);
    }

    //------------------------------------------------------------------------------
    // Turning a piece a quarter turn about +Y sends whatever pointed along
    // direction i to direction (i - 1). So to aim a model's own arm - which points
    // somewhere fixed by the artist - at direction `target`, turn it by their
    // difference.
    //
    // Every rotation in this file goes through here, which is what makes the tables
    // below readable: they only have to say which way a piece points as authored.
    //------------------------------------------------------------------------------
    float YawToAim(int modelDir, int target)
    {
        const int quarters = ((modelDir - target)%4 + 4)%4;

        return quarters*(PI*0.5f);
    }

    //------------------------------------------------------------------------------
    // A stable number for a grid position.
    //
    // Dressing has to be decided fresh every frame - none of it is stored - so the
    // answer has to depend on nothing but where the piece is. Any cheap integer
    // hash does; this is the usual xorshift-multiply, and its only requirement is
    // that neighbouring cells land nowhere near each other.
    //------------------------------------------------------------------------------
    int GridHash(int a, int b, int salt)
    {
        unsigned int h = (unsigned int)(a*73856093) ^ (unsigned int)(b*19349663) ^
                         (unsigned int)(salt*83492791);

        h ^= h >> 13;
        h *= 0x5bd1e995u;
        h ^= h >> 15;

        return (int)(h & 0x7fffffffu);
    }

    // The hash as a 0..1 fraction, for comparing against a Config chance
    float HashFraction(int hash)
    {
        return (hash%1000)/1000.0f;
    }
}

//----------------------------------------------------------------------------------
// A door blocks until it has swung far enough out of the way, whichever way it
// went.
//
// The magnitude matters and the sign does not: which way a leaf opens is decided
// by the jamb it hangs from, so half of them swing negative. Comparing the signed
// angle let those through the test forever - they stood visibly wide open and went
// on stopping everything, and being a per-door coin flip it looked like one
// particular doorway was broken rather than half of them.
//----------------------------------------------------------------------------------
bool Door::Blocks() const
{
    return fabsf(angle) < Config::DoorPassableAngle*DEG2RAD;
}

//----------------------------------------------------------------------------------
// One model out of the dungeon pack.
//
// Pack 1.1 puts every model on one atlas with a white tint, so unlike 1.0 there is
// nothing to correct at load: the texture carries the colour and the lit shader
// does the rest.
//
// Every piece is loaded against the one canonical copy of that atlas. Each folder
// still keeps its own PNG because the glTF files name it by relative URI and will
// not load without it, but those copies live only as long as the model that pulled
// them in - GetModel frees each one as it rebinds. Skipping that argument is not a
// small waste: eighty props would be eighty 1024-square atlases.
//----------------------------------------------------------------------------------
Model *Level::Piece(AssetManager &assets, const char *file)
{
    static const std::string dir = "models/dungeon/";
    static const std::string atlas = dir + "dungeon_texture.png";

    if (!FileExists(AssetManager::Resolve(dir + file).c_str())) return nullptr;

    Model &model = assets.GetModel(dir + file, atlas);

    for (int i = 0; i < model.materialCount; i++) model.materials[i].shader = *lit;

    return &model;
}

void Level::Load(AssetManager &assets)
{
    Load(assets, (Config::LevelSeed != 0u) ? Config::LevelSeed
                                           : (unsigned int)time(nullptr));
}

//----------------------------------------------------------------------------------
// Build the level from a named seed.
//
// Taking the seed as an argument rather than reading Config directly is what lets
// the debug key regenerate in place: judging what a generator produces means
// seeing twenty of its maps, and seeing twenty of them cannot mean twenty
// restarts. The seed is kept on the Map so a map worth having can be read back
// and pinned into Config::LevelSeed.
//----------------------------------------------------------------------------------
void Level::Load(AssetManager &assets, unsigned int seed)
{
    // One deeper. Deliberately BEFORE the early work below, and deliberately not
    // reset by it: every call to Load is another floor, so the first map built is
    // depth 1 and the debug regenerate key is the way down.
    ++depth;

    // A regenerate comes straight back through here, so anything that accumulates
    // has to be cleared first. The models themselves are the AssetManager's to
    // keep - it caches by path, so a second level costs no reloading - but the
    // lists holding them are ours, and so is the fallback box.
    wallVariants.clear();
    bannerProps.clear();
    props.clear();
    propCells.clear();
    floorPlan.clear();

    if (wallBoxReady) UnloadModel(wallBox);
    wallBoxReady = false;

    map.Generate(seed);

    spawn = map.SpawnPoint();
    portal = map.PortalPoint();
    floorHeight = 0.0f;

    // Shared by every piece: raylib holds a shader per material, so this has to be
    // set on each model as it loads rather than bound once around the draw
    lit = &assets.GetShader("shaders/lit.vs", "shaders/lit.fs");

    const int dirSlot = GetShaderLocation(*lit, "lightDir");
    const int ambientSlot = GetShaderLocation(*lit, "ambient");
    const float ambient = Config::SunAmbient;

    SetShaderValue(*lit, dirSlot, Config::SunDirection, SHADER_UNIFORM_VEC3);
    SetShaderValue(*lit, ambientSlot, &ambient, SHADER_UNIFORM_FLOAT);

    // Optional throughout. A missing pack costs the dressing and nothing else:
    // Draw falls back to the debug slabs, which is also what makes this safe to
    // develop against before the art is in place.
    auto piece = [&](const char *file) -> Model * { return Piece(assets, file); };

    floorTile   = piece("floor/floor_tile_large.gltf");

    wallFull    = piece("walls/wall.gltf");
    wallHalf    = piece("walls/wall_half.gltf");
    wallHalfCap = piece("walls/wall_half_endcap.gltf");
    wallCorner  = piece("walls/wall_corner.gltf");
    wallTee     = piece("walls/wall_Tsplit.gltf");
    wallCross   = piece("walls/wall_crossing.gltf");

    //------------------------------------------------------------------------------
    // Full-edge stand-ins for `wall`, every one of them SOLID.
    //
    // Which ones those are was measured, not assumed: each mesh was projected onto
    // its own face and sampled for gaps. The result contradicted the names twice -
    // `wall_arched` is solid, its arch being relief rather than an opening, while
    // `wall_broken` is 9% open. Anything that lets daylight through shows the
    // player the nothing outside the maze, so the pierced ones are all excluded:
    //
    //   broken 8.9%   window_open 10.7%   archedwindow_open 18.6%
    //   archedwindow_gated 11.8%   gated 27.4%   open_scaffold 65.7%
    //   sloped 44.4%
    //
    // wall_pillar lives with the pillars rather than the walls and is 1.500 deep
    // instead of 1.000, so its engaged column stands proud of the face - which is
    // the point of including it.
    //------------------------------------------------------------------------------
    for (const char *variant : { "walls_special/wall_cracked.gltf",
                                 "walls_special/wall_arched.gltf",
                                 "walls_special/wall_window_closed.gltf",
                                 "walls_special/wall_window_closed_scaffold.gltf",
                                 "walls_special/wall_scaffold.gltf",
                                 "walls_special/wall_shelves.gltf",
                                 "pillars/wall_pillar.gltf" })
    {
        if (Model *model = piece(variant)) wallVariants.push_back(model);
    }

    // Colour is the whole point of a banner - the stonework has none - so the
    // spread of colours matters more than the count of patterns.
    for (const char *banner : { "wallmount/banner_red.gltf",
                                "wallmount/banner_blue.gltf",
                                "wallmount/banner_green.gltf",
                                "wallmount/banner_yellow.gltf",
                                "wallmount/banner_white.gltf",
                                "wallmount/banner_brown.gltf",
                                "wallmount/banner_patternA_red.gltf",
                                "wallmount/banner_patternA_blue.gltf",
                                "wallmount/banner_patternB_green.gltf",
                                "wallmount/banner_patternB_yellow.gltf",
                                "wallmount/banner_patternC_red.gltf",
                                "wallmount/banner_patternC_white.gltf",
                                "wallmount/banner_thin_blue.gltf",
                                "wallmount/banner_triple_red.gltf",
                                "wallmount/banner_shield_blue.gltf",
                                "wallmount/banner_shield_green.gltf" })
    {
        if (Model *model = piece(banner)) bannerProps.push_back(model);
    }

    torchProp = piece("wallmount/torch_mounted.gltf");

    //------------------------------------------------------------------------------
    // The doorway, whose glTF holds the frame and the swinging leaf as two meshes
    // of one model. Told apart by height: the leaf is 2.750 tall against the
    // frame's 4.000, which is a fact about the art rather than about the file, so
    // it survives the pack being re-exported in a different order.
    //------------------------------------------------------------------------------
    doorFrame = piece("walls_special/wall_doorway.gltf");

    if (doorFrame != nullptr)
    {
        float tallest = -1.0f;

        for (int i = 0; i < doorFrame->meshCount; i++)
        {
            const BoundingBox box = GetMeshBoundingBox(doorFrame->meshes[i]);
            const float height = box.max.y - box.min.y;

            if (height > tallest) { tallest = height; doorFrameMesh = i; }
        }

        for (int i = 0; i < doorFrame->meshCount; i++)
            if (i != doorFrameMesh) doorLeafMesh = i;

        if (doorLeafMesh < 0)
        {
            TraceLog(LOG_WARNING, "LEVEL: wall_doorway has %i mesh(es), expected 2 - "
                                  "doors will not swing", doorFrame->meshCount);
        }
    }

    BuildDoors();

    // After the doors, because placement has to know where they are: a barrel in
    // a doorway is not clutter, it is a room the player cannot get into
    DressRooms(assets);

    // A wall-thickness slab standing in for a missing pack. Long enough to cover
    // the vertex square at each end, so a fallback run still closes at its corners
    // rather than showing a hole where every junction should be.
    const float size = map.CellSize();

    wallBox = LoadModelFromMesh(GenMeshCube(size + Config::WallThickness,
                                           Config::WallHeight,
                                           Config::WallThickness));
    wallBox.materials[0].shader = *lit;
    wallBox.materials[0].maps[MATERIAL_MAP_DIFFUSE].color =
        Color{ Config::WallStone[0], Config::WallStone[1], Config::WallStone[2], 255 };
    wallBoxReady = true;

    int solid = 0;
    for (const Prop &prop : props) if (prop.blocks) solid++;

    TraceLog(LOG_INFO, "LEVEL: seed %u, %ix%i cells at %.1f, %i rooms, %i doors, "
                       "%i props (%i solid), walls %s, floor %s",
             map.Seed(), map.Width(), map.Depth(), size, (int)map.Rooms().size(),
             (int)doors.size(), (int)props.size(), solid,
             (wallFull != nullptr) ? "modular" : "FALLBACK SLABS",
             (floorTile != nullptr) ? "ok" : "MISSING");

    for (size_t i = 0; i < map.Rooms().size(); i++)
    {
        const Room &room = map.Rooms()[i];

        TraceLog(LOG_INFO, "LEVEL:   %-10s %ix%i at (%2i,%2i), %i exits, %2i props, %s",
                 Rooms::Spec(room.kind).name, room.w, room.d, room.x, room.z,
                 map.RoomExits(room), (i < roomProps.size()) ? roomProps[i] : 0,
                 Rooms::StateName(room.state));
    }

    AuditReachability();
}

//----------------------------------------------------------------------------------
// How big a prop is, where its footprint lands, and whether it stops anybody.
//
// Measured off the meshes rather than authored per prop, which is the same reason
// the door leaf is found by its height rather than by its index: a table given
// hand-written dimensions is a table whose collision is wrong the day the pack is
// re-exported, and nothing about it looks wrong until you walk into it.
//
// The rotated footprint is the axis-aligned box AROUND the turned model, so a prop
// at a free yaw over-reports slightly and never under-reports. Over-reporting
// costs a few centimetres of clearance; under-reporting is a body inside a table.
//
// The model-space centre is not assumed to be the origin. Several props in the
// pack are authored off to one side of it, and a footprint centred on `at` would
// sit beside those rather than on them.
//----------------------------------------------------------------------------------
void Level::MeasureProp(Prop &prop, PropRole role)
{
    prop.foot = Slab{};
    prop.top = 0.0f;
    prop.lift = 0.0f;
    prop.lie = false;
    prop.blocks = false;

    if ((prop.model == nullptr) || (prop.model->meshCount <= 0)) return;

    BoundingBox box = GetMeshBoundingBox(prop.model->meshes[0]);

    for (int i = 1; i < prop.model->meshCount; i++)
    {
        const BoundingBox part = GetMeshBoundingBox(prop.model->meshes[i]);

        box.min = Vector3Min(box.min, part.min);
        box.max = Vector3Max(box.max, part.max);
    }

    //------------------------------------------------------------------------------
    // Was this authored standing on a floor?
    //
    // A model whose lowest point is its own origin was. One that hangs below its
    // origin was authored to be held, dropped or mounted - and set down as though
    // it stood on its feet it sinks through the flags by exactly that much. This
    // is a fact about the art, read off the art, rather than a list of names that
    // goes stale the moment a palette gains a prop.
    //------------------------------------------------------------------------------
    const bool standing = (box.min.y >= -0.05f);

    // ...and is it already lying down? A coin is authored centred on its own face,
    // so it hangs below its origin exactly like a torch does - and it is a flat
    // disc that is already the right way up. Tipping it over stands it on its edge.
    // The tell is which dimension is smallest: a thing already flattest through Y
    // is already lying flat.
    const float ySpan = box.max.y - box.min.y;
    const bool onItsFace = (ySpan <= (box.max.x - box.min.x)) &&
                           (ySpan <= (box.max.z - box.min.z));

    // Something dropped lies where it fell. Only for scatter: an edge prop that is
    // not authored standing is a shelf, and a shelf lying on the floor is a plank.
    prop.lie = !standing && !onItsFace && (role == PropRole::Scatter);

    Matrix rotation = MatrixRotateY(prop.yaw);

    // Tipped onto its back first, then turned - so the yaw still spins it about the
    // world's up axis rather than about whatever axis it used to stand on
    if (prop.lie) rotation = MatrixMultiply(MatrixRotateX(-PI*0.5f), rotation);

    //------------------------------------------------------------------------------
    // The turned box, from its eight corners.
    //
    // Every corner rather than the two-dimensional shortcut this used to take,
    // because laying a prop down is a rotation the shortcut cannot express - and a
    // footprint that ignores it is a footprint at right angles to the thing it is
    // supposed to be describing.
    //------------------------------------------------------------------------------
    Vector3 low = { FLT_MAX, FLT_MAX, FLT_MAX };
    Vector3 high = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

    for (int corner = 0; corner < 8; corner++)
    {
        const Vector3 point = { (corner & 1) ? box.max.x : box.min.x,
                                (corner & 2) ? box.max.y : box.min.y,
                                (corner & 4) ? box.max.z : box.min.z };

        const Vector3 turned = Vector3Transform(point, rotation);

        low = Vector3Min(low, turned);
        high = Vector3Max(high, turned);
    }

    // A wall shelf hangs at a height; everything else is dropped until its lowest
    // point rests on the floor
    const bool mounted = !standing && (role == PropRole::Edge);

    prop.lift = mounted ? Config::PropMountHeight : -low.y;

    prop.foot.minX = prop.at.x + low.x;
    prop.foot.maxX = prop.at.x + high.x;
    prop.foot.minZ = prop.at.z + low.z;
    prop.foot.maxZ = prop.at.z + high.z;

    prop.top = high.y + prop.lift;

    const float spanX = high.x - low.x;
    const float spanZ = high.z - low.z;

    prop.blocks = (fmaxf(spanX, spanZ) >= Config::PropBlockMinSize) &&
                  ((high.y - low.y) >= Config::PropBlockMinHeight);

    // Nothing hanging on a wall blocks the floor under it. The wall behind it
    // already stops anybody getting there, and a shelf that also stopped them at
    // knee height would be an obstruction with nothing visible at knee height.
    if (mounted) prop.blocks = false;

    // Nor does anything lying where it fell. A torch on its side measures a solid
    // half-unit and is still litter: furniture is what you walk around, and a
    // dropped weapon is what you walk over.
    if (prop.lie) prop.blocks = false;
}

void Level::DrawProp(const Prop &prop) const
{
    if (prop.model == nullptr) return;

    Matrix rotation = MatrixRotateY(prop.yaw);

    if (prop.lie) rotation = MatrixMultiply(MatrixRotateX(-PI*0.5f), rotation);

    prop.model->transform = rotation;
    DrawModel(*prop.model, { prop.at.x, prop.at.y + prop.lift, prop.at.z }, 1.0f, WHITE);
    prop.model->transform = MatrixIdentity();   // Leave the shared asset as we found it
}

//----------------------------------------------------------------------------------
// Furnishes every room from its kind's palette.
//
// Three passes per room, because the three want placing differently and not
// because they are three sorts of object. An anchor goes in the middle and there
// is at most one. Edge props stand with their backs to a wall, facing in - a bed
// against a wall is a bedroom, a bed in the middle of the floor is a warehouse
// that lost its shelving. Scatter props go wherever there is room left.
//
// Everything is rejected against one cell mask rather than against a list of
// special cases. A cell is forbidden if it is rock, if it is the inside of an
// opening, or if it is where the player stands up at the start. That single test
// covers doorways, the wide arches that get no door, corridor mouths and the
// spawn, and it cannot fall out of step with any of them.
//----------------------------------------------------------------------------------
void Level::DressRooms(AssetManager &assets)
{
    props.clear();

    const int w = map.Width();
    const int d = map.Depth();
    const float size = map.CellSize();
    const float half = Config::WallHalfThickness;

    floorPlan.assign((size_t)(w*d), floorTile);
    propCells.assign((size_t)(w*d), std::vector<int>());

    if (map.Rooms().empty()) return;

    //------------------------------------------------------------------------------
    // Cells nothing may be placed in.
    //
    // The inside of every opening on every room perimeter. A barrel dropped in a
    // doorway is not clutter, it is a room the player cannot enter - and the same
    // is true of the two-cell arches, which have no Doorway record at all and so
    // would be missed entirely by a test that only knew about doors.
    //------------------------------------------------------------------------------
    std::vector<unsigned char> forbidden((size_t)(w*d), 0);

    auto forbid = [&](int cx, int cz)
    {
        if ((cx < 0) || (cz < 0) || (cx >= w) || (cz >= d)) return;

        forbidden[(size_t)(cz*w + cx)] = 1;
    };

    for (const Room &room : map.Rooms())
    {
        for (int cz = room.z; cz < room.z + room.d; cz++)
        {
            if (!map.WallOnX(room.x, cz)) forbid(room.x, cz);
            if (!map.WallOnX(room.x + room.w, cz)) forbid(room.x + room.w - 1, cz);
        }

        for (int cx = room.x; cx < room.x + room.w; cx++)
        {
            if (!map.WallOnZ(cx, room.z)) forbid(cx, room.z);
            if (!map.WallOnZ(cx, room.z + room.d)) forbid(cx, room.z + room.d - 1);
        }
    }

    //------------------------------------------------------------------------------
    // The spots that have to stay walkable whatever the dressing rolls.
    //
    // Where the player stands up, where they leave, and the middle of every room
    // holding an objective. A game that starts inside a crate is a bug the player
    // has no way to read as one; an exit behind a shelf is the same bug at the far
    // end of the floor, and worse, because by then they have no reason to believe
    // the room is even the right one.
    //
    // The event markers are the reason this is a loop rather than two calls. They
    // were chosen AFTER the floor was dressed once, and a marker chosen after the
    // furniture is a marker that lands inside a table - which is what happened. The
    // map picks its event rooms in AssignKinds now, precisely so this pass can see
    // them.
    //
    // The cell and its four neighbours each time. One cell is not enough: a table on
    // the cell next door still reaches into this one, and a marker you have to fight
    // the collision to stand in is a marker that does not work.
    //------------------------------------------------------------------------------
    auto keepClear = [&](Vector3 at)
    {
        int cx = 0, cz = 0;
        map.WorldToCell(at.x, at.z, cx, cz);

        forbid(cx, cz);
        forbid(cx + 1, cz);
        forbid(cx - 1, cz);
        forbid(cx, cz + 1);
        forbid(cx, cz - 1);
    };

    keepClear(spawn);
    keepClear(portal);

    for (int index : map.EventRooms())
    {
        const Room &room = map.Rooms()[index];

        keepClear(map.CellCenter(room.CenterX(), room.CenterZ()));
    }

    //------------------------------------------------------------------------------
    // Will this footprint do? Every cell it touches has to be open floor and not
    // forbidden, and it must not overlap anything already standing.
    //------------------------------------------------------------------------------
    auto fits = [&](const Slab &foot)
    {
        int minX = 0, minZ = 0, maxX = 0, maxZ = 0;
        map.WorldToCell(foot.minX, foot.minZ, minX, minZ);
        map.WorldToCell(foot.maxX, foot.maxZ, maxX, maxZ);

        for (int cz = minZ; cz <= maxZ; cz++)
        {
            for (int cx = minX; cx <= maxX; cx++)
            {
                if ((cx < 0) || (cz < 0) || (cx >= w) || (cz >= d)) return false;
                if (map.IsWall(cx, cz)) return false;
                if (forbidden[(size_t)(cz*w + cx)]) return false;
            }
        }

        for (const Prop &other : props)
        {
            const float pad = Config::PropClearance;

            if ((foot.minX - pad < other.foot.maxX) && (foot.maxX + pad > other.foot.minX) &&
                (foot.minZ - pad < other.foot.maxZ) && (foot.maxZ + pad > other.foot.minZ))
                return false;
        }

        return true;
    };

    //------------------------------------------------------------------------------
    // Is this cell filled by something solid already placed?
    //
    // Deliberately the same question, of the same numbers, that PathFinder asks:
    // a cell counts as blocked when a solid footprint covers its middle within
    // the clearance a body needs. If the dressing pass and the AI disagreed about
    // that, the dressing pass would cheerfully approve a room the AI could not
    // walk into.
    //------------------------------------------------------------------------------
    auto cellBlocked = [&](int cx, int cz, const Slab *also)
    {
        const Vector3 centre = map.CellCenter(cx, cz);
        const float clear = Config::PathClearRadius;

        if ((also != nullptr) && also->Contains(centre.x, centre.z, clear)) return true;

        for (const Prop &other : props)
        {
            if (!other.blocks) continue;
            if (other.foot.Contains(centre.x, centre.z, clear)) return true;
        }

        return false;
    };

    //------------------------------------------------------------------------------
    // Would this prop cut the room in half?
    //
    // The one failure the placement rules above cannot see. Every individual test
    // is local - this cell is floor, that cell is not a doorway, nothing overlaps -
    // and being cut in half is not a property of any single cell.
    //
    // It bites hardest on the small rooms, which is exactly where it was found: a
    // corridor that runs INTO a 2x2 room and out the other side leaves two open
    // cells, and Storage, the kind that wants to be crowded, fills both. The room
    // still has both its doorways, both doorway cells are still clear, and the map
    // is now in two pieces.
    //
    // So the invariant is checked directly: every open cell of the room must still
    // be reachable from one of its doorways, walking only through the room. In-room
    // only, which is stricter than it has to be - two doorways could always be
    // joined by going the long way round outside - but a room whose two halves are
    // joined by a hundred-unit detour is not a room anyone should have to solve.
    //------------------------------------------------------------------------------
    std::vector<int> mouths;
    std::vector<unsigned char> reached;

    auto severs = [&](const Slab &foot, const Room &room)
    {
        if (mouths.empty()) return false;

        const int rw = room.w;
        const int rh = room.d;

        reached.assign((size_t)(rw*rh), 0);

        //--------------------------------------------------------------------------
        // From ONE doorway, not from all of them.
        //
        // Seeding the flood at every doorway at once looks equivalent and is not:
        // a room cut cleanly in half, with a doorway in each half, has every open
        // cell reached and is still two rooms. Since a doorway is what the test
        // exists to protect, starting from a single one is what makes reaching the
        // others mean something.
        //--------------------------------------------------------------------------
        std::vector<int> stack;

        {
            const int cx = mouths[0]%w;
            const int cz = mouths[0]/w;
            const int local = (cz - room.z)*rw + (cx - room.x);

            // A doorway cell is never allowed to be filled, so this cannot start
            // from something already blocked
            reached[(size_t)local] = 1;
            stack.push_back(local);
        }

        while (!stack.empty())
        {
            const int at = stack.back();
            stack.pop_back();

            const int lx = at%rw;
            const int lz = at/rw;
            const int stepX[4] = { 1, -1, 0, 0 };
            const int stepZ[4] = { 0, 0, 1, -1 };

            for (int n = 0; n < 4; n++)
            {
                const int nlx = lx + stepX[n];
                const int nlz = lz + stepZ[n];

                if ((nlx < 0) || (nlz < 0) || (nlx >= rw) || (nlz >= rh)) continue;

                const int next = nlz*rw + nlx;
                if (reached[(size_t)next]) continue;
                if (cellBlocked(room.x + nlx, room.z + nlz, &foot)) continue;

                reached[(size_t)next] = 1;
                stack.push_back(next);
            }
        }

        // Anything open and unreached is a pocket the furniture has walled off
        for (int lz = 0; lz < rh; lz++)
        {
            for (int lx = 0; lx < rw; lx++)
            {
                if (reached[(size_t)(lz*rw + lx)]) continue;
                if (cellBlocked(room.x + lx, room.z + lz, &foot)) continue;

                return true;
            }
        }

        return false;
    };

    // Stable per level rather than per frame: the dressing has to be the same
    // thing the collision was built against, and it is built once
    std::mt19937 rng(map.Seed() ^ 0x9e3779b9u);

    auto listLength = [](const char *const *list)
    {
        int n = 0;
        while ((n < 8) && (list[n] != nullptr)) n++;
        return n;
    };

    auto load = [&](const char *path) -> Model *
    {
        return Piece(assets, (std::string(path) + ".gltf").c_str());
    };

    roomProps.assign(map.Rooms().size(), 0);

    for (size_t roomIndex = 0; roomIndex < map.Rooms().size(); roomIndex++)
    {
        const Room &room = map.Rooms()[roomIndex];
        const RoomKindSpec &spec = Rooms::Spec(room.kind);
        const int before = (int)props.size();

        // The cells this room is entered through, which are the cells the sever
        // test floods out from. Same rule the forbidden mask was built with.
        mouths.clear();

        for (int cz = room.z; cz < room.z + room.d; cz++)
        {
            if (!map.WallOnX(room.x, cz)) mouths.push_back(cz*w + room.x);
            if (!map.WallOnX(room.x + room.w, cz)) mouths.push_back(cz*w + room.x + room.w - 1);
        }

        for (int cx = room.x; cx < room.x + room.w; cx++)
        {
            if (!map.WallOnZ(cx, room.z)) mouths.push_back(room.z*w + cx);
            if (!map.WallOnZ(cx, room.z + room.d)) mouths.push_back((room.z + room.d - 1)*w + cx);
        }

        //--------------------------------------------------------------------------
        // The floor this room stands on. Resolved here rather than in Draw so the
        // floor pass stays a straight lookup instead of a room search per cell.
        //--------------------------------------------------------------------------
        if (const char *override = Rooms::FloorFor(room.state))
        {
            if (Model *tile = load(override))
            {
                for (int cz = room.z; cz < room.z + room.d; cz++)
                    for (int cx = room.x; cx < room.x + room.w; cx++)
                        floorPlan[(size_t)(cz*w + cx)] = tile;
            }
        }

        if (room.state == ChamberState::Stripped) continue;


        // A campsite is somebody else living in the room, so it ignores what the
        // room was for entirely. That substitution IS the state.
        const bool camped = (room.state == ChamberState::Campsite);
        const char *const *edgeList = camped ? Rooms::CampsiteEdge : spec.edge;
        const char *const *scatterList = camped ? Rooms::CampsiteScatter : spec.scatter;

        const int edgeCount = listLength(edgeList);
        const int scatterCount = listLength(scatterList);

        // How much survived. Fire takes the most, water and a fallen ceiling less.
        float density = Config::PropDensityScale;

        if (room.state == ChamberState::Ashes)   density *= 0.35f;
        if (room.state == ChamberState::Flooded) density *= 0.6f;
        if (room.state == ChamberState::Rubble)  density *= 0.7f;

        // Wrecked and Rubble throw the furniture about; everything else is where
        // somebody put it, which means square to the walls
        const bool tumbled = (room.state == ChamberState::Wrecked) ||
                             (room.state == ChamberState::Rubble);

        const float interiorMinX = room.x*size + half;
        const float interiorMaxX = (room.x + room.w)*size - half;
        const float interiorMinZ = room.z*size + half;
        const float interiorMaxZ = (room.z + room.d)*size - half;

        //--------------------------------------------------------------------------
        // The anchor: one feature in the middle of the room, and the thing that
        // most tells the player what they walked into. Skipped where the room has
        // been taken over or brought down on itself.
        //--------------------------------------------------------------------------
        if ((spec.anchor != nullptr) && !camped && (room.state != ChamberState::Rubble))
        {
            Prop prop;
            prop.model = load(spec.anchor);

            if (prop.model != nullptr)
            {
                std::uniform_int_distribution<int> quarter(0, 3);

                prop.yaw = quarter(rng)*(PI*0.5f);
                prop.at = map.CellCenter(room.CenterX(), room.CenterZ());
                prop.at.y = floorHeight;

                MeasureProp(prop, PropRole::Anchor);

                if (fits(prop.foot) && !(prop.blocks && severs(prop.foot, room)))
                    props.push_back(prop);
            }
        }

        //--------------------------------------------------------------------------
        // Edge props, backs to a wall and faces to the room. Placed by measuring
        // the model first and then solving for the position that puts its own back
        // face on the wall - so a prop authored off its origin, and several in the
        // pack are, still lands against the stone rather than beside it.
        //--------------------------------------------------------------------------
        if (edgeCount > 0)
        {
            std::uniform_int_distribution<int> whichEdge(0, edgeCount - 1);
            std::uniform_int_distribution<int> whichSide(0, 3);
            std::uniform_int_distribution<int> countRoll(spec.edgeMin, spec.edgeMax);
            std::uniform_real_distribution<float> along(0.0f, 1.0f);

            const int want = (int)(countRoll(rng)*density + 0.5f);

            for (int placed = 0, tries = 0; (placed < want) && (tries < want*12); tries++)
            {
                const char *path = edgeList[whichEdge(rng)];

                // Rubble replaces part of what should be here. What is left is what
                // was too heavy to be worth crushing.
                if ((room.state == ChamberState::Rubble) && ((tries%3) == 0))
                    path = Rooms::RubbleProps[tries%2];

                Prop prop;
                prop.model = load(path);
                if (prop.model == nullptr) continue;

                // Which wall, and which way that makes it face
                const int side = whichSide(rng);
                const int faceDir = (side == 0) ? DirEast : (side == 1) ? DirWest :
                                    (side == 2) ? DirSouth : DirNorth;

                prop.yaw = YawToAim(DirSouth, faceDir);

                if (tumbled)
                {
                    std::uniform_real_distribution<float> knock(-0.5f, 0.5f);
                    prop.yaw += knock(rng);
                }

                // Measured at the origin first, so its own extents can be solved
                // for rather than assumed
                prop.at = { 0.0f, floorHeight, 0.0f };
                MeasureProp(prop, PropRole::Edge);

                const float backMinX = prop.foot.minX;
                const float backMaxX = prop.foot.maxX;
                const float backMinZ = prop.foot.minZ;
                const float backMaxZ = prop.foot.maxZ;

                const float slideX = interiorMinX - backMinX +
                                     along(rng)*fmaxf(0.0f, (interiorMaxX - backMaxX) - (interiorMinX - backMinX));
                const float slideZ = interiorMinZ - backMinZ +
                                     along(rng)*fmaxf(0.0f, (interiorMaxZ - backMaxZ) - (interiorMinZ - backMinZ));

                // Against the wall on one axis, slid along it on the other
                if (side == 0)      prop.at = { interiorMinX - backMinX, floorHeight, slideZ };
                else if (side == 1) prop.at = { interiorMaxX - backMaxX, floorHeight, slideZ };
                else if (side == 2) prop.at = { slideX, floorHeight, interiorMinZ - backMinZ };
                else                prop.at = { slideX, floorHeight, interiorMaxZ - backMaxZ };

                MeasureProp(prop, PropRole::Edge);

                if (!fits(prop.foot)) continue;

                // ...and it must not wall off part of its own room. See `severs`.
                if (prop.blocks && severs(prop.foot, room)) continue;

                props.push_back(prop);
                placed++;
            }
        }

        //--------------------------------------------------------------------------
        // Scatter: the small things, anywhere there is floor left
        //--------------------------------------------------------------------------
        if (scatterCount > 0)
        {
            std::uniform_int_distribution<int> which(0, scatterCount - 1);
            std::uniform_int_distribution<int> countRoll(spec.scatterMin, spec.scatterMax);
            std::uniform_real_distribution<float> anywhere(0.0f, 1.0f);
            std::uniform_real_distribution<float> turn(0.0f, 2.0f*PI);

            const int want = (int)(countRoll(rng)*density + 0.5f);

            for (int placed = 0, tries = 0; (placed < want) && (tries < want*12); tries++)
            {
                Prop prop;
                prop.model = load(scatterList[which(rng)]);
                if (prop.model == nullptr) continue;

                prop.yaw = turn(rng);
                prop.at = { 0.0f, floorHeight, 0.0f };
                MeasureProp(prop, PropRole::Scatter);

                const float spanX = (interiorMaxX - prop.foot.maxX) - (interiorMinX - prop.foot.minX);
                const float spanZ = (interiorMaxZ - prop.foot.maxZ) - (interiorMinZ - prop.foot.minZ);

                if ((spanX < 0.0f) || (spanZ < 0.0f)) continue;

                prop.at = { interiorMinX - prop.foot.minX + anywhere(rng)*spanX,
                            floorHeight,
                            interiorMinZ - prop.foot.minZ + anywhere(rng)*spanZ };

                MeasureProp(prop, PropRole::Scatter);

                if (!fits(prop.foot)) continue;

                // ...and it must not wall off part of its own room. See `severs`.
                if (prop.blocks && severs(prop.foot, room)) continue;

                props.push_back(prop);
                placed++;
            }
        }

        roomProps[roomIndex] = (int)props.size() - before;
    }

    //------------------------------------------------------------------------------
    // Bucket the props by cell. ResolveBody then tests the three or four things
    // near the body rather than the two hundred in the level.
    //
    // Built here rather than anywhere else because an index built away from the
    // thing it indexes is an index that can be stale.
    //------------------------------------------------------------------------------
    for (int i = 0; i < (int)props.size(); i++)
    {
        const Prop &prop = props[i];

        if (!prop.blocks) continue;

        int minX = 0, minZ = 0, maxX = 0, maxZ = 0;
        map.WorldToCell(prop.foot.minX, prop.foot.minZ, minX, minZ);
        map.WorldToCell(prop.foot.maxX, prop.foot.maxZ, maxX, maxZ);

        for (int cz = minZ; cz <= maxZ; cz++)
        {
            for (int cx = minX; cx <= maxX; cx++)
            {
                if ((cx < 0) || (cz < 0) || (cx >= w) || (cz >= d)) continue;

                propCells[(size_t)(cz*w + cx)].push_back(i);
            }
        }
    }
}

//----------------------------------------------------------------------------------
// Can every room still be walked to, now that there is furniture in the way?
//
// Map::Generate guarantees the LAYOUT is connected, by construction, and that
// guarantee says nothing whatever about the dressing pass that runs afterwards.
// A corridor one cell wide with a barrel in it is a corridor nobody can use, and
// the symptom is not a crash or a warning - it is a room the player never finds
// and a camp that sits in it for the whole level waiting for a fight that cannot
// reach it.
//
// So it is checked, against the same Passable the AI will actually use, rather
// than reasoned about. Load-time only, and about forty searches of a thousand
// cells, which is nothing next to loading the models.
//----------------------------------------------------------------------------------
void Level::AuditReachability() const
{
    PathFinder finder;
    std::vector<Vector3> route;

    int sealed = 0;

    for (size_t i = 1; i < map.Rooms().size(); i++)
    {
        const Room &room = map.Rooms()[i];

        //--------------------------------------------------------------------------
        // Any cell in the room will do, and the centre specifically will not.
        //
        // The question is whether the player can get INTO this room, and the
        // room's middle is the one cell most likely to be occupied - it is where
        // the anchor prop stands. Asking for the centre reported a seal every
        // time a guardroom had its table, which is every guardroom.
        //--------------------------------------------------------------------------
        bool anyOpen = false;
        bool anyReached = false;

        for (int cz = room.z; (cz < room.z + room.d) && !anyReached; cz++)
        {
            for (int cx = room.x; (cx < room.x + room.w) && !anyReached; cx++)
            {
                if (!PathFinder::Passable(*this, cx, cz)) continue;

                anyOpen = true;
                anyReached = finder.Find(*this, spawn, map.CellCenter(cx, cz), route);
            }
        }

        if (anyReached) continue;

        sealed++;

        TraceLog(LOG_WARNING, "LEVEL: the %s at (%i,%i) cannot be walked to - %s",
                 Rooms::Spec(room.kind).name, room.x, room.z,
                 anyOpen ? "furniture blocks the way in" : "it is furnished solid");
    }

    if (sealed > 0)
    {
        TraceLog(LOG_WARNING, "LEVEL: %i of %i rooms unreachable on seed %u",
                 sealed, (int)map.Rooms().size() - 1, map.Seed());
    }
}

bool Level::PropBlocksAt(Vector3 point, float radius) const
{
    const int w = map.Width();

    int cx = 0, cz = 0;
    map.WorldToCell(point.x, point.z, cx, cz);

    // The bucket the point is in cannot be the whole answer: a prop overlapping
    // this cell by a hair is listed here, but one in the next cell along that the
    // radius still reaches is not. So the neighbours are walked too.
    for (int nz = cz - 1; nz <= cz + 1; nz++)
    {
        for (int nx = cx - 1; nx <= cx + 1; nx++)
        {
            if ((nx < 0) || (nz < 0) || (nx >= w) || (nz >= map.Depth())) continue;

            for (int index : propCells[(size_t)(nz*w + nx)])
            {
                if (props[(size_t)index].foot.Contains(point.x, point.z, radius)) return true;
            }
        }
    }

    return false;
}

void Level::Unload()
{
    // The box is the one thing here the AssetManager does not own
    if (wallBoxReady) UnloadModel(wallBox);
    wallBoxReady = false;
}

//----------------------------------------------------------------------------------
// Circle against the wall cells it overlaps.
//
// Resolving the overlap after the fact rather than sweeping means no previous
// position is needed, and sliding falls out of it for free: only the component of
// the motion heading into the wall is cancelled, so speed along the wall
// survives - which matters, because Body::Update accelerates from whatever
// velocity it finds.
//----------------------------------------------------------------------------------
void Level::ResolveBody(Body &body) const
{
    const float size = map.CellSize();
    const float radius = body.radius;

    //------------------------------------------------------------------------------
    // What counts as the floor here.
    //
    // Usually the level's, but a low prop the body has got on top of is floor as
    // far as that body is concerned - a chest is something to stand on, and
    // without this it would be something to stand inside. Only counted while the
    // body is already at or above the thing's top, which is what makes the
    // difference between standing on a crate and walking through one: below that
    // height the prop pass below pushes the body out sideways instead.
    //------------------------------------------------------------------------------
    float ground = floorHeight;

    if (!propCells.empty())
    {
        int cx = 0, cz = 0;
        map.WorldToCell(body.position.x, body.position.z, cx, cz);

        if ((cx >= 0) && (cz >= 0) && (cx < map.Width()) && (cz < map.Depth()))
        {
            for (int index : propCells[(size_t)(cz*map.Width() + cx)])
            {
                const Prop &prop = props[(size_t)index];

                if (prop.top > Config::PropStepMaxHeight) continue;
                if (!prop.foot.Contains(body.position.x, body.position.z, radius*0.5f)) continue;
                if (body.position.y < floorHeight + prop.top - 0.05f) continue;

                ground = fmaxf(ground, floorHeight + prop.top);
            }
        }
    }

    // Fancy collision system against the floor
    if (body.position.y <= ground)
    {
        body.position.y = ground;
        body.velocity.y = 0.0f;
        body.isGrounded = true;  // Enable jumping
    }

    // Two passes so an inside corner, which needs two pushes, settles
    for (int pass = 0; pass < 2; pass++)
    {
        // Reach has to cover the slab's own half thickness as well as the body, or
        // a body already pressed against a wall would not find the line it stands on
        const float half = Config::WallHalfThickness;
        const float reach = radius + half;

        int minX = 0, minZ = 0, maxX = 0, maxZ = 0;
        map.WorldToCell(body.position.x - reach, body.position.z - reach, minX, minZ);
        map.WorldToCell(body.position.x + reach, body.position.z + reach, maxX, maxZ);

        //--------------------------------------------------------------------------
        // Push the body clear of one slab. Shared by both families of edges,
        // because a wall standing on a Z line is one on an X line transposed.
        //--------------------------------------------------------------------------
        auto pushOut = [&](float boxMinX, float boxMaxX, float boxMinZ, float boxMaxZ)
        {
            // Nearest point on the slab to the body's centre
            const float closestX = Clamp(body.position.x, boxMinX, boxMaxX);
            const float closestZ = Clamp(body.position.z, boxMinZ, boxMaxZ);

            const float dx = body.position.x - closestX;
            const float dz = body.position.z - closestZ;
            const float distanceSq = dx*dx + dz*dz;

            if (distanceSq >= radius*radius) return;

            if (distanceSq > 1e-8f)
            {
                const float distance = sqrtf(distanceSq);
                const float normalX = dx/distance;
                const float normalZ = dz/distance;

                body.position.x += normalX*(radius - distance);
                body.position.z += normalZ*(radius - distance);

                const float into = body.velocity.x*normalX + body.velocity.z*normalZ;
                if (into < 0.0f)
                {
                    body.velocity.x -= into*normalX;
                    body.velocity.z -= into*normalZ;
                }
            }
            else
            {
                // Centre exactly inside the slab: leave by the nearest face
                const float gapLeft = body.position.x - boxMinX;
                const float gapRight = boxMaxX - body.position.x;
                const float gapBack = body.position.z - boxMinZ;
                const float gapFront = boxMaxZ - body.position.z;
                const float least = fminf(fminf(gapLeft, gapRight), fminf(gapBack, gapFront));

                if (least == gapLeft)
                {
                    body.position.x = boxMinX - radius;
                    body.velocity.x = fminf(body.velocity.x, 0.0f);
                }
                else if (least == gapRight)
                {
                    body.position.x = boxMaxX + radius;
                    body.velocity.x = fmaxf(body.velocity.x, 0.0f);
                }
                else if (least == gapBack)
                {
                    body.position.z = boxMinZ - radius;
                    body.velocity.z = fminf(body.velocity.z, 0.0f);
                }
                else
                {
                    body.position.z = boxMaxZ + radius;
                    body.velocity.z = fmaxf(body.velocity.z, 0.0f);
                }
            }
        };

        // Slabs are grown by half a thickness at each end so that the square at a
        // grid vertex - which a corner or T piece fills with stone - reads solid
        // from either of the edges meeting there. Nothing else fills it.
        for (int cz = minZ; cz <= maxZ; cz++)
        {
            for (int ex = minX; ex <= maxX + 1; ex++)
            {
                if (!map.WallOnX(ex, cz)) continue;

                pushOut(ex*size - half, ex*size + half,
                        cz*size - half, (cz + 1)*size + half);
            }
        }

        for (int ez = minZ; ez <= maxZ + 1; ez++)
        {
            for (int cx = minX; cx <= maxX; cx++)
            {
                if (!map.WallOnZ(cx, ez)) continue;

                pushOut(cx*size - half, (cx + 1)*size + half,
                        ez*size - half, ez*size + half);
            }
        }

        //--------------------------------------------------------------------------
        // Doorways. The wall pass skipped these lines entirely - as far as it is
        // concerned a doorway is a hole - so everything solid about one is here.
        //
        // The jambs are frame and never move, so they are solid always. The leaf
        // between them is solid only while it still fills the gap.
        //--------------------------------------------------------------------------
        const float reachSq = (reach + size)*(reach + size);

        for (const Door &door : doors)
        {
            // Cheap rejection first: most doors are nowhere near most bodies
            const float dx = door.centre.x - body.position.x;
            const float dz = door.centre.z - body.position.z;

            if ((dx*dx + dz*dz) > reachSq) continue;

            for (const Slab &jamb : door.jambs) pushOut(jamb.minX, jamb.maxX, jamb.minZ, jamb.maxZ);

            if (door.Blocks()) pushOut(door.leaf.minX, door.leaf.maxX, door.leaf.minZ, door.leaf.maxZ);
        }

        //--------------------------------------------------------------------------
        // Furniture.
        //
        // Only the props that block - a plate underfoot stops nobody - and only
        // while the feet are below the thing's own top, so a low crate is
        // something to step onto rather than a permanent wall. Above
        // PropStepMaxHeight nothing is steppable: the player is two units tall,
        // and hauling yourself onto a shelf is a climb, not a step.
        //
        // The buckets are what make this cheap. Without them every body would test
        // every prop in the level, twice a pass, twice a frame.
        //--------------------------------------------------------------------------
        if (!propCells.empty())
        {
            const int w = map.Width();

            for (int cz = minZ; cz <= maxZ; cz++)
            {
                if ((cz < 0) || (cz >= map.Depth())) continue;

                for (int cx = minX; cx <= maxX; cx++)
                {
                    if ((cx < 0) || (cx >= w)) continue;

                    for (int index : propCells[(size_t)(cz*w + cx)])
                    {
                        const Prop &prop = props[(size_t)index];

                        // Already standing on top of it: the floor pass above has
                        // made this thing the ground, and pushing a body off the
                        // ground it is standing on is how you get thrown off a crate
                        if ((prop.top <= Config::PropStepMaxHeight) &&
                            (body.position.y >= floorHeight + prop.top - 0.05f)) continue;

                        pushOut(prop.foot.minX, prop.foot.maxX, prop.foot.minZ, prop.foot.maxZ);
                    }
                }
            }
        }
    }
}

//----------------------------------------------------------------------------------
// Grid DDA: step cell to cell along the segment rather than sampling points, so
// the answer is exact and costs one iteration per cell crossed.
//----------------------------------------------------------------------------------
bool Level::LineOfSight(Vector3 from, Vector3 to) const
{
    const float size = map.CellSize();

    int cx = 0, cz = 0;
    map.WorldToCell(from.x, from.z, cx, cz);

    int endX = 0, endZ = 0;
    map.WorldToCell(to.x, to.z, endX, endZ);

    if (map.IsWall(cx, cz)) return false;       // Starting inside solid

    // A shut door is opaque, and it stands on a line the cell walk cannot see -
    // both cells either side of it are floor, so the DDA passes straight through
    if (DoorBlocksSight(from, to)) return false;

    if ((cx == endX) && (cz == endZ)) return true;

    float dirX = to.x - from.x;
    float dirZ = to.z - from.z;
    const float length = sqrtf(dirX*dirX + dirZ*dirZ);

    if (length < 1e-5f) return true;

    dirX /= length;
    dirZ /= length;

    const int stepX = (dirX > 0.0f) ? 1 : -1;
    const int stepZ = (dirZ > 0.0f) ? 1 : -1;

    // Distance along the ray to the first cell boundary on each axis
    const float toBoundaryX = (dirX > 0.0f) ? ((cx + 1)*size - from.x) : (from.x - cx*size);
    const float toBoundaryZ = (dirZ > 0.0f) ? ((cz + 1)*size - from.z) : (from.z - cz*size);

    const bool movesX = (fabsf(dirX) > 1e-6f);
    const bool movesZ = (fabsf(dirZ) > 1e-6f);

    float nextX = movesX ? (toBoundaryX/fabsf(dirX)) : FLT_MAX;
    float nextZ = movesZ ? (toBoundaryZ/fabsf(dirZ)) : FLT_MAX;
    const float strideX = movesX ? (size/fabsf(dirX)) : FLT_MAX;
    const float strideZ = movesZ ? (size/fabsf(dirZ)) : FLT_MAX;

    // Bounded so a degenerate direction can never spin here forever
    const int maxSteps = 2*(map.Width() + map.Depth()) + 4;

    for (int i = 0; i < maxSteps; i++)
    {
        if (fminf(nextX, nextZ) > length) return true;   // Reached the target first

        if (nextX < nextZ)
        {
            cx += stepX;
            nextX += strideX;
        }
        else
        {
            cz += stepZ;
            nextZ += strideZ;
        }

        if ((cx == endX) && (cz == endZ)) return true;
        if (map.IsWall(cx, cz)) return false;
    }

    return true;
}

//----------------------------------------------------------------------------------
// Walk the segment and stop short of anything solid.
//
// Sampled rather than a DDA like LineOfSight, because the two are asked different
// questions. LineOfSight wants a yes or no about a whole line and a DDA answers
// that exactly; this wants a POSITION, and the useful answer is a point a safe
// distance back from the obstruction rather than the exact place the line first
// touched it - a shot spawned precisely on a wall face is still half inside it.
//
// The segment is short by construction - a weapon's reach, so around two units
// against a three unit cell - so a handful of samples covers it and the loop can
// never run away.
//----------------------------------------------------------------------------------
Vector3 Level::ClipSpawn(Vector3 from, Vector3 to) const
{
    const float length = Vector3Length(Vector3Subtract(to, from));
    if (length < 1e-4f) return from;

    // Fine enough that nothing thinner than a quarter cell can be stepped over
    const int steps = (int)(length/(map.CellSize()*0.25f)) + 2;

    Vector3 safe = from;

    for (int i = 1; i <= steps; i++)
    {
        const Vector3 at = Vector3Lerp(from, to, (float)i/steps);

        // Walls are solid to any height, and the floor stops everything. There is
        // no ceiling: aiming at the sky is allowed to put the muzzle above the
        // walls, because nothing is up there to spawn inside of.
        if (map.SolidAtWorld(at.x, at.z)) break;
        if (at.y <= floorHeight) break;

        safe = at;
    }

    return safe;
}

//----------------------------------------------------------------------------------
// Every doorway the map found, resolved to world space once.
//
// Done here rather than in Map because a door is a thing with a position and a
// swing, and Map deals in cells. What Map knows is which grid line has a door on
// it; where that lands and which way it opens is the level's business.
//----------------------------------------------------------------------------------
void Level::BuildDoors()
{
    doors.clear();

    const float size = map.CellSize();
    const float half = Config::WallHalfThickness;
    const float leaf = Config::DoorLeafHalfWidth;

    for (const Doorway &way : map.Doorways())
    {
        Door door;
        door.alongX = way.alongX;

        // The frame fills the whole opening, which is a cell wide; the leaf only
        // fills the middle of it. What is left over at each end is a jamb.
        const float span = size*0.5f;

        if (way.alongX)
        {
            door.centre = { way.line*size, floorHeight, (way.span + 0.5f)*size };
            door.yaw = YawToAim(DirEast, DirSouth);

            door.leaf = { door.centre.x - half, door.centre.x + half,
                          door.centre.z - leaf, door.centre.z + leaf };

            door.jambs[0] = { door.leaf.minX, door.leaf.maxX,
                              door.centre.z - span, door.leaf.minZ };
            door.jambs[1] = { door.leaf.minX, door.leaf.maxX,
                              door.leaf.maxZ, door.centre.z + span };
        }
        else
        {
            door.centre = { (way.span + 0.5f)*size, floorHeight, way.line*size };
            door.yaw = 0.0f;

            door.leaf = { door.centre.x - leaf, door.centre.x + leaf,
                          door.centre.z - half, door.centre.z + half };

            door.jambs[0] = { door.centre.x - span, door.leaf.minX,
                              door.leaf.minZ, door.leaf.maxZ };
            door.jambs[1] = { door.leaf.maxX, door.centre.x + span,
                              door.leaf.minZ, door.leaf.maxZ };
        }

        // Which jamb it hangs from, decided by position so a row of them does not
        // all swing the same way, and so the same door always hangs the same way
        door.hinge = ((GridHash(way.line, way.span, 7)%2) == 0) ? -leaf : leaf;

        // Away from its own hinge, which is the only direction it can go
        door.target = 0.0f;

        doors.push_back(door);
    }

    TraceLog(LOG_INFO, "LEVEL: %i doorways", (int)doors.size());
}

//----------------------------------------------------------------------------------
// Doors finish the swing they were given.
//
// Nothing ever closes one. A door knocked open stays open, because a door that
// drifted shut behind the player would quietly undo the only thing hitting it
// achieved.
//----------------------------------------------------------------------------------
void Level::Update(float delta)
{
    const float step = Config::DoorSwingSpeed*DEG2RAD*delta;

    for (Door &door : doors)
    {
        const float remaining = door.target - door.angle;

        if (fabsf(remaining) <= step) door.angle = door.target;
        else                          door.angle += copysignf(step, remaining);
    }
}

namespace
{
    //------------------------------------------------------------------------------
    // A capsule against an upright box, near enough.
    //
    // Sampled rather than solved: a blade is short, a door is two metres across,
    // and the difference between this and the exact test is smaller than the
    // difference between one frame's blade position and the next.
    //------------------------------------------------------------------------------
    bool CapsuleHitsSlab(const Capsule &capsule, const Slab &slab, float minY, float maxY)
    {
        constexpr int Samples = 8;

        for (int i = 0; i <= Samples; i++)
        {
            const Vector3 point = Vector3Lerp(capsule.a, capsule.b, i/(float)Samples);
            const float pad = capsule.radius;

            if (!slab.Contains(point.x, point.z, pad)) continue;
            if ((point.y < minY - pad) || (point.y > maxY + pad)) continue;

            return true;
        }

        return false;
    }

    //------------------------------------------------------------------------------
    // Does the segment a->b cross this footprint? The slab test, in x and z only.
    //
    // Exact and constant time, unlike sampling, which matters here: line of sight
    // runs per enemy per frame over segments long enough that sampling one finely
    // enough to be trustworthy would cost more than every door in the level.
    //------------------------------------------------------------------------------
    bool SegmentHitsSlab(Vector3 from, Vector3 to, const Slab &slab)
    {
        float enter = 0.0f;
        float leave = 1.0f;

        const float delta[2] = { to.x - from.x, to.z - from.z };
        const float start[2] = { from.x, from.z };
        const float low[2]   = { slab.minX, slab.minZ };
        const float high[2]  = { slab.maxX, slab.maxZ };

        for (int axis = 0; axis < 2; axis++)
        {
            // Parallel to this pair of edges: either always between them or never
            if (fabsf(delta[axis]) < 1e-6f)
            {
                if ((start[axis] < low[axis]) || (start[axis] > high[axis])) return false;
                continue;
            }

            float near = (low[axis] - start[axis])/delta[axis];
            float far  = (high[axis] - start[axis])/delta[axis];

            if (near > far) { const float swap = near; near = far; far = swap; }

            enter = fmaxf(enter, near);
            leave = fminf(leave, far);

            if (enter > leave) return false;
        }

        return true;
    }
}

//----------------------------------------------------------------------------------
// Anything landing on a shut door swings it.
//
// The sweep is tested at both ends of its travel rather than only where the blade
// finished, for the same reason SweepMelee does it: a fast swing can begin on one
// side of a door and end on the other having never been drawn touching it.
//----------------------------------------------------------------------------------
int Level::StrikeDoors(const Capsule &from, const Capsule &to)
{
    const float top = floorHeight + Config::DoorLeafHeight;
    int struck = 0;

    for (Door &door : doors)
    {
        if (!door.Blocks()) continue;

        if (!CapsuleHitsSlab(from, door.leaf, floorHeight, top) &&
            !CapsuleHitsSlab(to,   door.leaf, floorHeight, top))
        {
            continue;
        }

        // Away from the hinge - the only way it can travel
        door.target = (door.hinge < 0.0f) ? Config::DoorOpenAngle*DEG2RAD
                                          : -Config::DoorOpenAngle*DEG2RAD;
        struck++;
    }

    return struck;
}

int Level::StrikeDoorAt(Vector3 point, float radius)
{
    const float top = floorHeight + Config::DoorLeafHeight;

    for (int i = 0; i < (int)doors.size(); i++)
    {
        Door &door = doors[i];

        if (!door.Blocks()) continue;

        if (!door.leaf.Contains(point.x, point.z, radius)) continue;
        if ((point.y < floorHeight - radius) || (point.y > top + radius)) continue;

        door.target = (door.hinge < 0.0f) ? Config::DoorOpenAngle*DEG2RAD
                                          : -Config::DoorOpenAngle*DEG2RAD;
        return i;
    }

    return -1;
}

//----------------------------------------------------------------------------------
// The jambs. Stone, at full wall height, whatever the leaf is doing.
//
// Height is the frame's, not the leaf's: the lintel closes the top of the opening,
// so there is no gap over a jamb to shoot through the way there is over the leaf.
//----------------------------------------------------------------------------------
bool Level::DoorFrameAt(Vector3 point, float radius) const
{
    const float top = floorHeight + Config::WallHeight;

    if ((point.y < floorHeight - radius) || (point.y > top + radius)) return false;

    for (const Door &door : doors)
    {
        for (const Slab &jamb : door.jambs)
        {
            if (jamb.Contains(point.x, point.z, radius)) return true;
        }
    }

    return false;
}

//----------------------------------------------------------------------------------
// To the hinge, turn, and back again, then onto the grid line - the same chain
// DrawDoor places the leaf mesh with, and the only copy of it.
//----------------------------------------------------------------------------------
Matrix Level::DoorLeafTransform(int index) const
{
    if ((index < 0) || (index >= (int)doors.size())) return MatrixIdentity();

    const Door &door = doors[index];

    const Matrix place = MatrixMultiply(MatrixRotateY(door.yaw),
                                        MatrixTranslate(door.centre.x, door.centre.y,
                                                        door.centre.z));

    const Matrix swing =
        MatrixMultiply(MatrixMultiply(MatrixTranslate(-door.hinge, 0.0f, 0.0f),
                                      MatrixRotateY(door.angle)),
                       MatrixTranslate(door.hinge, 0.0f, 0.0f));

    return MatrixMultiply(swing, place);
}

//----------------------------------------------------------------------------------
// Anything opaque standing on a door's grid line.
//
// Both parts, not just the leaf. The jambs are a whole unit of stone each on a
// four unit opening, and leaving them out of this was worth an enemy that could
// watch the player through the side of a shut door - the wall DDA cannot see them
// either, because both cells flanking a doorway are floor.
//----------------------------------------------------------------------------------
bool Level::DoorBlocksSight(Vector3 from, Vector3 to) const
{
    for (const Door &door : doors)
    {
        for (const Slab &jamb : door.jambs)
        {
            if (SegmentHitsSlab(from, to, jamb)) return true;
        }

        if (!door.Blocks()) continue;

        if (SegmentHitsSlab(from, to, door.leaf)) return true;
    }

    return false;
}

//----------------------------------------------------------------------------------
// The frame, then the leaf turned about the edge it hangs from.
//
// glTF ships both as meshes of one model, so this is two DrawMesh calls rather
// than a DrawModel: the whole point is that the two halves move independently.
//----------------------------------------------------------------------------------
void Level::DrawDoor(int index) const
{
    if (doorFrame == nullptr) return;

    const Door &door = doors[index];

    const Matrix place = MatrixMultiply(MatrixRotateY(door.yaw),
                                        MatrixTranslate(door.centre.x, door.centre.y,
                                                        door.centre.z));

    if (doorFrameMesh >= 0)
    {
        DrawMesh(doorFrame->meshes[doorFrameMesh],
                 doorFrame->materials[doorFrame->meshMaterial[doorFrameMesh]], place);
    }

    if (doorLeafMesh < 0) return;

    // To the hinge, turn, and back again - a leaf rotated about its own middle
    // would swing half of itself into the jamb. Built by DoorLeafTransform rather
    // than here, because a stuck arrow rides the same chain and the two drifting
    // apart would float it off the door.
    DrawMesh(doorFrame->meshes[doorLeafMesh],
             doorFrame->materials[doorFrame->meshMaterial[doorLeafMesh]],
             DoorLeafTransform(index));
}

Model *Level::WallPieceFor(int hash) const
{
    if (wallVariants.empty()) return wallFull;
    if (HashFraction(hash) >= Config::WallVariantChance) return wallFull;

    return wallVariants[(hash/1000)%(int)wallVariants.size()];
}

//----------------------------------------------------------------------------------
// What hangs on this wall, if anything.
//
// Both props are authored facing +Z, so aiming them is the same YawToAim the wall
// pieces use. What differs is how they meet the wall: a banner's back sits at
// 0.38 against a face at 0.50, so it mounts at the centreline and leans out of the
// stone; a torch's back is at 0.00, so it mounts ON the face, and it is modelled
// around the origin rather than standing on the floor.
//----------------------------------------------------------------------------------
void Level::DrawWallDressing(Vector3 centre, int openDir, int hash) const
{
    if (HashFraction(hash) >= Config::WallPropChance) return;

    const float yaw = YawToAim(DirSouth, openDir);
    const int pick = hash/1000;

    if ((torchProp != nullptr) && ((pick%3) == 0))
    {
        const float outX = (openDir == DirEast) ? 1.0f : (openDir == DirWest) ? -1.0f : 0.0f;
        const float outZ = (openDir == DirSouth) ? 1.0f : (openDir == DirNorth) ? -1.0f : 0.0f;

        DrawPiece(torchProp,
                  { centre.x + outX*Config::WallPropTorchOut,
                    floorHeight + Config::WallPropTorchHigh,
                    centre.z + outZ*Config::WallPropTorchOut },
                  yaw);
        return;
    }

    if (bannerProps.empty()) return;

    DrawPiece(bannerProps[pick%(int)bannerProps.size()],
              { centre.x, floorHeight, centre.z }, yaw);
}

int Level::VertexMask(int vx, int vz) const
{
    int mask = 0;

    // The two X lines meeting here run along Z, north and south of the vertex; the
    // two Z lines run along X, east and west of it
    if (map.WallOnZ(vx, vz))     mask |= 1 << DirEast;
    if (map.WallOnX(vx, vz))     mask |= 1 << DirSouth;
    if (map.WallOnZ(vx - 1, vz)) mask |= 1 << DirWest;
    if (map.WallOnX(vx, vz - 1)) mask |= 1 << DirNorth;

    return mask;
}

void Level::DrawPiece(Model *model, Vector3 at, float yaw) const
{
    if (model == nullptr) return;

    model->transform = MatrixRotateY(yaw);
    DrawModel(*model, at, 1.0f, WHITE);
    model->transform = MatrixIdentity();    // Leave the shared asset as we found it
}

//----------------------------------------------------------------------------------
// The junction standing where walls meet, if any stands there at all.
//
// Each piece is named by the arms it has as authored, and YawToAim turns it until
// one nominated arm lands where the mask wants it. Which arm is nominated is
// arbitrary - any arm would do - so long as the rest of the piece follows, which
// is why the match below is on the whole rotated mask rather than on one bit.
//----------------------------------------------------------------------------------
void Level::DrawJunction(int vx, int vz) const
{
    const int mask = VertexMask(vx, vz);

    if (!HasJunction(mask)) return;

    // As modelled: the corner turns from -X to +Z, the T has its stem on +Z, and
    // the crossing needs no saying
    constexpr int CornerArms = (1 << DirWest) | (1 << DirSouth);
    constexpr int TeeArms    = (1 << DirWest) | (1 << DirSouth) | (1 << DirEast);

    Model *model = nullptr;
    int authored = 0;

    switch (PopCount(mask))
    {
        case 2:  model = wallCorner; authored = CornerArms; break;
        case 3:  model = wallTee;    authored = TeeArms;    break;
        default: model = wallCross;  authored = 0;          break;
    }

    // Turn the authored arms until they sit on the arms the mask asks for. Four
    // quarters is the whole search space, and the crossing matches on the first.
    float yaw = 0.0f;

    for (int quarter = 0; quarter < 4; quarter++)
    {
        int turned = 0;

        for (int dir = 0; dir < 4; dir++)
            if (authored & (1 << dir)) turned |= 1 << ((dir - quarter + 4)%4);

        if ((authored == 0) || (turned == mask))
        {
            yaw = quarter*(PI*0.5f);
            break;
        }
    }

    DrawPiece(model, map.CellCorner(vx, vz), yaw);
}

//----------------------------------------------------------------------------------
// Everything standing on one grid line between two vertices.
//
// Whatever a junction has already taken is not drawn again, which is the whole
// reason this is decided per half rather than per edge: two junctions leave
// nothing, one leaves a half, none leaves a full piece - and an end that stops
// dead takes the capped half instead of the plain one.
//----------------------------------------------------------------------------------
void Level::DrawWallRun(int ax, int az, int dirAToB) const
{
    const bool alongX = (dirAToB == DirEast);

    const int bx = ax + (alongX ? 1 : 0);
    const int bz = az + (alongX ? 0 : 1);

    const int maskA = VertexMask(ax, az);
    const int maskB = VertexMask(bx, bz);

    const bool takenA = HasJunction(maskA);
    const bool takenB = HasJunction(maskB);

    if (takenA && takenB) return;

    // One arm at a vertex means the wall stops there and wants a finished end
    const bool capA = !takenA && (PopCount(maskA) == 1);
    const bool capB = !takenB && (PopCount(maskB) == 1);

    const Vector3 cornerA = map.CellCorner(ax, az);
    const Vector3 cornerB = map.CellCorner(bx, bz);
    const int dirBToA = (dirAToB + 2)%4;

    // Nothing claimed at either end: one piece spans the lot, which is both a draw
    // call saved and a seam that never existed
    if (!takenA && !takenB && !capA && !capB)
    {
        const Vector3 middle = { (cornerA.x + cornerB.x)*0.5f, floorHeight,
                                 (cornerA.z + cornerB.z)*0.5f };

        // `wall` is modelled along its own X and standing on its own base, so an
        // edge running along Z is the one that needs turning
        const float yaw = alongX ? 0.0f : YawToAim(DirEast, DirSouth);

        // Only full pieces are dressed. A half sits hard against a junction, and a
        // banner hung there overlaps the corner it is standing next to.
        const int hash = GridHash(ax, az, alongX ? 1 : 2);

        // The side with floor on it. One side of a wall is always rock, or there
        // would be no wall - so this is a question with exactly one answer.
        const int openDir = alongX ? (map.IsWall(ax, az) ? DirNorth : DirSouth)
                                   : (map.IsWall(ax, az) ? DirWest  : DirEast);

        if (wallFull != nullptr)
        {
            DrawPiece(WallPieceFor(hash), middle, yaw);
            DrawWallDressing(middle, openDir, hash);
        }
        else if (wallBoxReady)
        {
            // The generated slab is a cube about its own centre, not a piece
            // standing on the floor, so it has to be lifted half its height
            DrawPiece(&wallBox,
                      { middle.x, floorHeight + Config::WallHeight*0.5f, middle.z },
                      yaw);
        }

        return;
    }

    // `wall_half` runs from its origin along +X; `wall_half_endcap` runs the other
    // way, from its capped tip back along -X. Both are anchored at the vertex.
    if (!takenA)
    {
        DrawPiece(capA ? wallHalfCap : wallHalf,
                  { cornerA.x, floorHeight, cornerA.z },
                  capA ? YawToAim(DirWest, dirAToB) : YawToAim(DirEast, dirAToB));
    }

    if (!takenB)
    {
        DrawPiece(capB ? wallHalfCap : wallHalf,
                  { cornerB.x, floorHeight, cornerB.z },
                  capB ? YawToAim(DirWest, dirBToA) : YawToAim(DirEast, dirBToA));
    }
}

//----------------------------------------------------------------------------------
// Floors per cell, then junctions per grid vertex, then the wall left over on each
// grid line.
//
// The order matters only for reading: junctions are resolved first because the
// edges ask what they took, and an edge that asked before the answer existed would
// be guessing.
//----------------------------------------------------------------------------------
void Level::Draw() const
{
    const float size = map.CellSize();
    const Color floorLight = { 150, 200, 200, 255 };

    for (int cz = 0; cz < map.Depth(); cz++)
    {
        for (int cx = 0; cx < map.Width(); cx++)
        {
            if (map.IsWall(cx, cz)) continue;

            const Vector3 center = map.CellCenter(cx, cz);

            // What this cell is floored with, worked out once at load: a room in a
            // given state names a different tile and every cell it owns changes
            Model *tile = floorPlan.empty() ? floorTile
                                            : floorPlan[(size_t)(cz*map.Width() + cx)];

            if (tile != nullptr)
            {
                // Dropped by the height of the slab's own top surface, so that
                // surface lands on floorHeight - which is where ResolveBody puts
                // feet - and then a hair further, so the two are not coplanar
                DrawModel(*tile,
                          { center.x,
                            floorHeight - Config::FloorTileTop - Config::SurfaceEpsilon,
                            center.z },
                          1.0f, WHITE);
            }
            else
            {
                // Chequered, so there is something to read speed against
                const bool light = ((cx + cz)%2) == 0;

                DrawPlane({ center.x, floorHeight, center.z }, { size, size },
                          light ? floorLight : LIGHTGRAY);
            }
        }
    }

    // One past the last cell on each axis: a grid of W by D cells has W+1 by D+1
    // vertices, and the far ones carry the outside wall of the map
    for (int vz = 0; vz <= map.Depth(); vz++)
        for (int vx = 0; vx <= map.Width(); vx++) DrawJunction(vx, vz);

    // Walls on the X lines, running along Z
    for (int cz = 0; cz < map.Depth(); cz++)
        for (int ex = 0; ex <= map.Width(); ex++)
            if (map.WallOnX(ex, cz)) DrawWallRun(ex, cz, DirSouth);

    // ...and on the Z lines, running along X
    for (int ez = 0; ez <= map.Depth(); ez++)
        for (int cx = 0; cx < map.Width(); cx++)
            if (map.WallOnZ(cx, ez)) DrawWallRun(cx, ez, DirEast);

    // Furniture, standing on the floor the first pass laid down and inside the
    // walls the second and third built
    for (const Prop &prop : props) DrawProp(prop);

    // Last, because a doorway stands in a gap the wall pass has already left for
    // it - nothing here overlaps anything above
    for (int i = 0; i < (int)doors.size(); i++) DrawDoor(i);
}
