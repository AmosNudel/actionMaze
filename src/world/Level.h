#pragma once

#include "combat/Collider.h"
#include "raylib.h"
#include "world/Map.h"

#include <vector>

struct Body;
class AssetManager;

//----------------------------------------------------------------------------------
// The playable world: a tile grid, plus everything that needs to ask questions
// about it.
//
// Load() currently builds a hand authored arena; when the maze generator lands it
// fills the same Map and nothing here changes.
//
// Walls are drawn from the dungeon pack's modular set, standing on the grid lines
// between cells. Every piece is placed at native scale and native rotation: the
// set is cut so that a junction reaches exactly half an edge and the halves that
// fill the rest are exactly that long, which means nothing here ever stretches a
// model to make a seam meet.
//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
// A rectangle on the floor plan.
//
// Every solid part of a doorway is one of these: the leaf that swings, and the two
// jambs either side of it that do not. One shape rather than four loose floats per
// part, because the jambs used to exist only as arithmetic inside ResolveBody -
// which is exactly why an arrow flew straight through one and an archer could see
// you through the side of a shut door. Collision, sight and projectiles now ask
// the same question of the same numbers.
//----------------------------------------------------------------------------------
struct Slab
{
    float minX = 0.0f, maxX = 0.0f, minZ = 0.0f, maxZ = 0.0f;

    bool Contains(float x, float z, float pad = 0.0f) const
    {
        return (x >= minX - pad) && (x <= maxX + pad) &&
               (z >= minZ - pad) && (z <= maxZ + pad);
    }
};

//----------------------------------------------------------------------------------
// One hinged door, resolved to world space once at load.
//
// The leaf swings about one edge and stays where it is put - a door knocked open
// is open, because a door that drifted shut behind you would undo the only thing
// opening it was for.
//----------------------------------------------------------------------------------
struct Door
{
    Vector3 centre{};       // Middle of the opening, on the floor
    float yaw = 0.0f;       // Turns the frame onto its grid line
    float angle = 0.0f;     // Leaf swing in radians, 0 shut
    float target = 0.0f;    // Where it is swinging to
    float hinge = 0.0f;     // Leaf-space X of the pivot edge, one half width or the other
    bool alongX = false;    // Stands on an X line, so the frame runs along Z

    // The opening's own slab, solid only while the leaf still fills it
    Slab leaf;

    // The frame either side of the leaf. A 4-unit opening against a 2-unit leaf
    // leaves a whole unit of stone at each jamb: not a detail, it is half the
    // doorway. Never moves, so it is solid and opaque whatever the leaf is doing.
    Slab jambs[2];

    bool Blocks() const;
};

//----------------------------------------------------------------------------------
// One piece of furniture standing in a room.
//
// The wall dressing above it is re-derived from a hash every frame and stores
// nothing. This cannot be: a prop the player can walk into has a footprint, and a
// footprint recomputed per frame is one that can disagree with the one collision
// resolved against last frame. So the dressing pass runs once and keeps what it
// decided.
//
// `foot` is the same Slab the doors use, for the same reason - collision and
// placement ask one shape the same question rather than two shapes a similar one.
//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
// Where a prop is going, which is also what to do with one that was not authored
// standing on a floor.
//
// Ten models in the pack have their origin somewhere other than their base -
// `sword_shield` is centred on its own middle and hangs 0.82 below it, the wall
// shelves 0.30 - because they were authored to be held, dropped or mounted rather
// than set down. Placed as though they stood on their feet they sink through the
// floor, which is what they did.
//
// The role is what tells the two cases apart, and it needs no list of names: an
// EDGE prop that is not authored standing is something that hangs on the wall, and
// a SCATTER prop that is not is something lying where it fell.
//----------------------------------------------------------------------------------
enum class PropRole { Anchor, Edge, Scatter };

struct Prop
{
    Model *model = nullptr;     // Owned by the AssetManager
    Vector3 at{};               // Floor position, before `lift`
    float yaw = 0.0f;

    // Laid on its side, so a dropped sword lies on the flags rather than standing
    // to attention on its point
    bool lie = false;

    // How far off `at` the model is actually drawn. Usually just enough to put its
    // lowest point on the floor; for something mounted on a wall, the height it
    // hangs at.
    float lift = 0.0f;

    Slab foot;                  // Footprint on the floor plan
    float top = 0.0f;           // How high it stands, off its own bounding box

    // Whether a body is stopped by it. Measured at load from the model's size, so
    // a plate does not stop anybody and a table does.
    bool blocks = false;
};

class Level
{
public:
    void Load(AssetManager &assets);

    // The same, from a named seed - what the debug regenerate key calls. See the
    // note on the definition.
    void Load(AssetManager &assets, unsigned int seed);

    void Unload();
    void Draw() const;

    // What this map was built from, so one worth keeping can be pinned into
    // Config::LevelSeed
    unsigned int Seed() const { return map.Seed(); }

    //------------------------------------------------------------------------------
    // How far down this is. The first level built is depth 1 and every regenerate
    // is one deeper.
    //
    // The number the difficulty curve is anchored on: enemy ranks roll about it, so
    // going down is a step whether or not the player just levelled (see
    // entities/EnemyRank.h). It is a property of the RUN rather than of the map,
    // which is why it survives a Load - a new map at the same depth would be the
    // same floor drawn twice, and F6 is meant to be the way down.
    //------------------------------------------------------------------------------
    int Depth() const { return depth; }

    // Doors swing on their own once struck, so the level has a little state to
    // carry forward each frame
    void Update(float delta);

    // A blade sweeping from `from` to `to`, or anything else that lands. Any door
    // it passes through swings open. Returns how many it hit, for the feedback.
    int StrikeDoors(const Capsule &from, const Capsule &to);

    // A single point landing on a shut door - an arrow, a bolt. Opens it and
    // reports that something was there to stop whatever asked. Returns the index
    // of the leaf that was struck, or -1: a shot that sticks in a door has to be
    // able to name the door, so it can ride it open rather than hang in the air
    // the leaf used to fill.
    int StrikeDoorAt(Vector3 point, float radius);

    // The stone either side of a leaf. Frame rather than door - nothing opens it -
    // so this only ever reports that something is in the way.
    bool DoorFrameAt(Vector3 point, float radius) const;

    // Where a leaf has swung to, as leaf space into world space. What a stuck
    // arrow rides on; also what DrawDoor places the mesh with, so the arrow and
    // the door it is in cannot disagree about where the door is.
    Matrix DoorLeafTransform(int index) const;

    // Whether a shut door stands between these two points, for line of sight
    bool DoorBlocksSight(Vector3 from, Vector3 to) const;

    // Collision response for anything that walks through the level. Called after
    // the body has integrated its own motion.
    void ResolveBody(Body &body) const;

    // Grid traversal, for melee, aggro and anything else that must not see or
    // reach through a wall. Only x and z matter - the grid has no vertical extent.
    bool LineOfSight(Vector3 from, Vector3 to) const;

    //------------------------------------------------------------------------------
    // Where something leaving `from` toward `to` can legally begin.
    //
    // Returns `to` when the whole segment is inside the level, and otherwise the
    // furthest point along it that still is. `from` is assumed legal - it is the
    // player's eye or an enemy's chest, and ResolveBody keeps bodies out of walls.
    //
    // This exists because a muzzle is not a point on the shooter: it is the tip of
    // the held weapon, up to two units away, and a weapon can be pointed at a wall
    // you are standing against or at the floor under your feet. Spawning a shot
    // out there puts it inside geometry, where it dies on its first step and looks
    // for all the world like the weapon simply failed to fire.
    //------------------------------------------------------------------------------
    Vector3 ClipSpawn(Vector3 from, Vector3 to) const;

    Vector3 SpawnPoint() const { return spawn; }

    // Where the way down stands, on the floor in the middle of the Portal room.
    // The dressing pass keeps this cell and its neighbours clear, so it is always
    // somewhere a body can actually stand.
    Vector3 PortalPoint() const { return portal; }

    //------------------------------------------------------------------------------
    // Is a solid prop standing here?
    //
    // Asked by anything that needs somewhere free to put a body or a path: enemy
    // spawn placement, which used to test only for floor and other enemies and so
    // would happily stand a skeleton inside a table, and the pathfinder, which
    // must not route through the furniture it can see.
    //
    // Only props that block. A candle underfoot stops nothing and hides nothing.
    //------------------------------------------------------------------------------
    bool PropBlocksAt(Vector3 point, float radius) const;

    const std::vector<Prop> &Props() const { return props; }

    // Warns about any room the dressing pass has sealed off. Load-time check, and
    // the one failure mode placing solid furniture actually introduces - see the
    // note on the definition.
    void AuditReachability() const;

    const Map &Grid() const { return map; }
    float FloorHeight() const { return floorHeight; }

private:
    //------------------------------------------------------------------------------
    // Which of the four edges meeting at grid vertex (vx, vz) carry wall.
    //
    //   bit 0  +X    bit 1  +Z    bit 2  -X    bit 3  -Z
    //
    // The whole piece-selection scheme is a function of this one number, at the
    // vertex itself and at the two ends of every edge, which is what keeps the
    // rules short enough to read.
    //------------------------------------------------------------------------------
    int VertexMask(int vx, int vz) const;

    // The junction standing at a vertex - corner, T-split or crossing - and how far
    // it is turned. Nothing when the walls there run straight through or stop dead:
    // both of those the edge pieces already cover, and a piece placed anyway would
    // sit inside one of them.
    void DrawJunction(int vx, int vz) const;

    //------------------------------------------------------------------------------
    // The wall standing on one grid line, from vertex (ax, az) toward `dirAToB`.
    //
    // A junction at either end has already eaten the half of this edge nearest it,
    // so what is left is a full piece, one half, two halves, or nothing at all.
    // Ends that stop dead take the capped half instead, so a wall never terminates
    // on a raw cross-section.
    //------------------------------------------------------------------------------
    void DrawWallRun(int ax, int az, int dirAToB) const;

    // Every piece in the pack is authored upright at the origin, so placement is
    // only ever a yaw and a translation - never a scale
    void DrawPiece(Model *model, Vector3 at, float yaw) const;

    Map map;
    Vector3 spawn = { 0.0f, 0.0f, 0.0f };
    // Counts up across Loads rather than being reset by them - see Depth()
    int depth = 0;

    Vector3 portal = { 0.0f, 0.0f, 0.0f };

    float floorHeight = 0.0f;

    // A wall-thickness slab, generated here and owned here, standing in for the
    // pack's pieces when the pack is missing. Sized to a whole edge plus the two
    // vertex squares at its ends, so a run of them still closes at the corners.
    //
    // Mutable because raylib carries a model's rotation in the model itself:
    // drawing one means writing its transform and putting it back. That is true of
    // the pack's pieces too, which only escape it by being reached through
    // pointers - the const here was never protecting anything.
    mutable Model wallBox{};
    bool wallBoxReady = false;

    // All owned by the AssetManager, all null when the pack is missing - in which
    // case Draw falls back to slabs and planes, so the game still runs against an
    // empty assets/models/dungeon
    Model *floorTile = nullptr;

    // The modular wall set. `wallHalfCap` is the one with a finished end on it;
    // the plain half butts against whatever comes next.
    Model *wallFull = nullptr;
    Model *wallHalf = nullptr;
    Model *wallHalfCap = nullptr;
    Model *wallCorner = nullptr;
    Model *wallTee = nullptr;
    Model *wallCross = nullptr;

    //------------------------------------------------------------------------------
    // Full-edge stand-ins for `wallFull`, all exactly 4.000 x 4.000 x 1.000, so a
    // variant is a different model in the same slot and costs nothing at all.
    //------------------------------------------------------------------------------
    std::vector<Model *> wallVariants;

    // Hung on room-facing wall faces. Banners mount at the wall's centreline;
    // torches mount on its face and want lifting - see the offsets in Config.
    std::vector<Model *> bannerProps;
    Model *torchProp = nullptr;

    //------------------------------------------------------------------------------
    // The doorway frame and the leaf inside it, which glTF ships as two meshes of
    // one model. Found by measurement rather than by index: the leaf is the
    // shorter of the two, 2.750 against the frame's 4.000, and an index would be
    // a silent lie the day the pack reorders them.
    //------------------------------------------------------------------------------
    Model *doorFrame = nullptr;
    int doorLeafMesh = -1;
    int doorFrameMesh = -1;

    std::vector<Door> doors;

    //------------------------------------------------------------------------------
    // Everything the dressing pass placed, plus the two indexes over it.
    //
    // `propCells` buckets prop indices by grid cell so ResolveBody tests the three
    // or four things near the body rather than the two hundred in the level. Built
    // once beside the props themselves, because an index built anywhere else is an
    // index that can be stale.
    //
    // `floorPlan` is one model pointer per cell - what to draw the floor with
    // there. A room in a given state names a different tile and every cell it owns
    // changes, and resolving that here rather than in Draw keeps the floor pass a
    // straight lookup instead of a room search per cell per frame.
    //------------------------------------------------------------------------------
    std::vector<Prop> props;
    std::vector<std::vector<int>> propCells;
    std::vector<Model *> floorPlan;

    // How many props each room ended up with, for the load-time report. A room
    // that asked for eight and got two is a placement problem, and without this
    // the only symptom is a room that looks a bit empty.
    std::vector<int> roomProps;

    void BuildDoors();
    void DrawDoor(int index) const;

    //------------------------------------------------------------------------------
    // One dungeon-pack model, against the shared atlas and bound to the lit
    // shader.
    //
    // The atlas argument is not optional in practice: glTF names its texture by
    // relative URI, so every model loaded without it pulls in its own private copy
    // of a 1024-square PNG. Eighty props is eighty atlases.
    //
    // Null when the pack does not have the file, which every caller treats as "do
    // without" rather than as an error.
    //------------------------------------------------------------------------------
    Model *Piece(AssetManager &assets, const char *file);

    // Furnishes every room from its kind's palette, and works out the floor plan.
    // Once, at load - see the note on Prop for why this one stores its result.
    void DressRooms(AssetManager &assets);

    // Measures a model and fills in `lie`, `lift`, `foot`, `top` and `blocks` for a
    // prop at `at` turned to `yaw`. Split out because placement has to know the
    // size before it can decide whether the thing fits.
    static void MeasureProp(Prop &prop, PropRole role);

    // Furniture is not always drawn where it stands - see Prop::lift
    void DrawProp(const Prop &prop) const;

    // Hangs a banner or a torch on the face of a wall piece. `openDir` is the side
    // with floor on it - a banner on the rock side would be a banner in a cave.
    void DrawWallDressing(Vector3 centre, int openDir, int hash) const;

    // A wall piece for this edge: plain, or one of the variants when its hash says
    // so. Never random - the same edge must answer the same way every frame.
    Model *WallPieceFor(int hash) const;

    // One directional light over the whole level, so modelled relief reads as
    // relief instead of returning one flat colour whichever way it faces
    Shader *lit = nullptr;
};
