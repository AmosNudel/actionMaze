#pragma once

#include "raylib.h"
#include "world/RoomKind.h"

#include <random>
#include <utility>
#include <vector>

enum class Tile : unsigned char { Floor, Wall };

// A carved rectangle of floor. Kept after generation because a room is the unit
// the dressing pass works in: props belong to a room, not to a cell.
struct Room
{
    int x = 0, z = 0;           // Cell of the low corner, inclusive
    int w = 0, d = 0;           // Size in cells

    // What the room was for, and how it has fared since. Both are assigned once
    // the whole layout exists, because both depend on it: where a room sits
    // relative to the entrance is half of what decides what it can be.
    RoomKind     kind  = RoomKind::Storage;
    ChamberState state = ChamberState::Pristine;

    int CenterX() const { return x + w/2; }
    int CenterZ() const { return z + d/2; }
    int Area() const { return w*d; }

    bool Contains(int cx, int cz) const
    {
        return (cx >= x) && (cz >= z) && (cx < x + w) && (cz < z + d);
    }
};

//----------------------------------------------------------------------------------
// A grid line that carries a door rather than open air.
//
// Doorways sit in OPENINGS, not in walls: an opening exactly one cell wide, with
// wall continuing on both sides of it, is a gap of exactly 4.000 flanked by the
// corner pieces that already stand at its ends. That is precisely the span
// `wall_doorway` fills, jambs and lintel included, so a doorway drops into the
// modular set without disturbing a single junction.
//
// Which is also why a door contributes nothing to WallOnX/WallOnZ. As far as the
// wall system is concerned a doorway is still a hole; the frame and its leaf are
// drawn over the top, and only collision is told the difference.
//
// A two-cell-wide corridor mouth therefore gets no door at all, and should not:
// the opening is 8.000 and there is no piece that fills it. What stands there is
// an open arch, which is a perfectly good thing for a dungeon to have.
//----------------------------------------------------------------------------------
struct Doorway
{
    int line = 0;       // Index of the grid line it stands on
    int span = 0;       // Which cell along that line
    bool alongX = false;// true: on an X line, so the doorway runs along Z
};

//----------------------------------------------------------------------------------
// The world as a tile grid, with walls on the lines BETWEEN cells.
//
// Cell (0,0) spans world x,z in [0, CellSize) - the origin sits at a corner, so
// converting between the two is a multiply and a floor with no centring maths.
// Anything outside the grid reads as Wall, which means collision, line of sight
// and the AI all treat the edge of the map as solid without a special case.
//
// Cells are still only Floor or Wall. What changed is where a wall IS: it is no
// longer the solid cell, it is the grid line where a Floor cell meets a Wall one.
// WallOnX and WallOnZ derive that from the cells themselves rather than storing
// it, which is the whole point - there is no second copy of the level to keep in
// step, so geometry, collision and the AI cannot drift apart. Two Wall cells have
// no wall between them because nobody can ever be there to see it.
//----------------------------------------------------------------------------------
class Map
{
public:
    void LoadTestArena();

    // Rooms carved into solid rock and joined by corridors. Same seed, same map.
    void Generate(unsigned int seed);

    int Width() const { return width; }
    int Depth() const { return depth; }
    float CellSize() const;

    // What this map was actually built from, so a map worth keeping can be pinned
    // into Config::LevelSeed and walked again
    unsigned int Seed() const { return seed; }

    Tile At(int cx, int cz) const;
    bool IsWall(int cx, int cz) const { return At(cx, cz) == Tile::Wall; }
    bool IsWallAtWorld(float x, float z) const;

    //------------------------------------------------------------------------------
    // Is there a wall standing on this grid line?
    //
    // WallOnX is the segment at world x = ex*CellSize spanning cell row cz - the
    // line between cells (ex-1, cz) and (ex, cz). WallOnZ is its transpose.
    //
    // A wall stands wherever exactly one side is open. Both open is a doorway or
    // open floor; both solid is buried rock nobody will ever stand next to.
    //------------------------------------------------------------------------------
    bool WallOnX(int ex, int cz) const { return IsWall(ex - 1, cz) != IsWall(ex, cz); }
    bool WallOnZ(int cx, int ez) const { return IsWall(cx, ez - 1) != IsWall(cx, ez); }

    //------------------------------------------------------------------------------
    // Is this point inside stone?
    //
    // IsWallAtWorld answers at cell resolution, which is the right question for
    // anything that thinks in cells - where to spawn, which cell to patrol. This
    // is the question for anything that occupies a point: a wall no longer fills
    // its cell, it is a slab standing on a line, and half of that slab overhangs
    // the open cell next door. A shot tested the other way flies half a thickness
    // into visible stone before it notices.
    //------------------------------------------------------------------------------
    bool SolidAtWorld(float x, float z) const;

    void WorldToCell(float x, float z, int &cx, int &cz) const;
    Vector3 CellCenter(int cx, int cz) const;

    // Corner of cell (cx, cz) nearest the origin - which is also grid vertex
    // (cx, cz), where the junction pieces stand
    Vector3 CellCorner(int cx, int cz) const;

    const std::vector<Room> &Rooms() const { return rooms; }
    const std::vector<Doorway> &Doorways() const { return doorways; }

    // Which room contains this cell, or -1 for a corridor or solid rock
    int RoomAt(int cx, int cz) const;

    // How many openings this room has on its perimeter. What makes a room worth
    // guarding is how much passes through it, and this is that number.
    int RoomExits(const Room &room) const;

    // Where the player starts, in world space, standing on the floor
    Vector3 SpawnPoint() const { return spawn; }

    // ...and where they leave: the middle of the Portal room, the same way. One
    // point rather than a room index because nothing outside here wants the room -
    // what the portal, the dressing pass and the HUD all want is the spot.
    Vector3 PortalPoint() const { return portal; }

    //------------------------------------------------------------------------------
    // Which rooms hold this floor's objectives.
    //
    // Picked HERE rather than by whatever runs the objectives, and that is the whole
    // point of it being here: the dressing pass has to keep the middle of each one
    // clear, and the dressing pass runs long before anything knows what an event is.
    // A marker chosen afterwards is a marker that can land inside a table, which is
    // exactly what it did.
    //
    // Room indices rather than points, because an objective wants the ROOM as well
    // as the spot - runes are scattered over it and waves come up inside it.
    //------------------------------------------------------------------------------
    const std::vector<int> &EventRooms() const { return eventRooms; }

    //------------------------------------------------------------------------------
    // Which rooms hold this floor's vendors, and which vendor is in each.
    //
    // Chosen here for the same reason the event rooms are: the dressing pass keeps
    // the middle of a vendor's room clear, and it runs long before anything knows
    // what a vendor is.
    //
    // Two parallel vectors rather than a vector of pairs, because the room list is
    // what Level's dressing pass wants and the kinds are what the vendor manager
    // wants, and neither has any use for the other's half.
    //------------------------------------------------------------------------------
    const std::vector<int> &VendorRooms() const { return vendorRooms; }
    const std::vector<int> &VendorKinds() const { return vendorKinds; }

private:
    void FindDoorways();
    void Fill(Tile tile);
    void CarveRoom(const Room &room);

    // `width` is 1 or 2 cells. Two is an arch rather than a door at either end -
    // see the note on Doorway.
    void CarveCorridor(int fromX, int fromZ, int toX, int toZ, bool xFirst, int width);

    // Extra edges between rooms already near each other, turning the chain into a
    // network. Runs after the chain, so it can never be what connects the map.
    void CarveLoops(const std::vector<std::pair<int, int>> &chain, std::mt19937 &rng);

    // Short stubs off a corridor that stop in rock
    void CarveDeadEnds(std::mt19937 &rng);

    // What each room is for, and what state it is in. Last, because it reads the
    // finished layout: distance from the entrance and how many ways in there are.
    void AssignKinds(std::mt19937 &rng);

    // Which rooms hold objectives. Part of AssignKinds' job rather than a caller's -
    // see the note on the definition for why it cannot wait.
    void ChooseEventRooms(std::mt19937 &rng, int portalRoom);

    // ...and which hold vendors. After the events, and never in one of their rooms -
    // see the note on the definition.
    void ChooseVendorRooms(std::mt19937 &rng, int portalRoom);

    void Set(int cx, int cz, Tile tile);

    std::vector<Tile> tiles;
    std::vector<Room> rooms;
    std::vector<Doorway> doorways;
    int width = 0;
    int depth = 0;
    unsigned int seed = 0;
    Vector3 spawn = { 0.0f, 0.0f, 0.0f };
    Vector3 portal = { 0.0f, 0.0f, 0.0f };
    std::vector<int> eventRooms;
    std::vector<int> vendorRooms;
    std::vector<int> vendorKinds;
};
