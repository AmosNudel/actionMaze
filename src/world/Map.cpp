#include "world/Map.h"

#include "core/Config.h"
#include "world/Npc.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <random>

namespace
{
    // Hand authored test arena. One character per cell, '#' solid, '@' the spawn.
    // Deliberately awkward in places: a pillar block to swing around and hide
    // behind, one corridor barely wider than the player, and a dead end.
    const char *ArenaRows[] =
    {
        "###############",
        "#.............#",
        "#.....###.....#",
        "#.....###.....#",
        "#.....###.....#",
        "#.............#",
        "#####.###.#####",
        "#.......#.....#",
        "#.......#.....#",
        "#...@...#.....#",
        "#.......#.....#",
        "#.......#.....#",
        "###############",
    };

    //------------------------------------------------------------------------------
    // The sizes a chamber may be, after the DMG's Chamber table (p291).
    //
    // One cell is Config::MapCellSize = 4.0 world units, which is about ten feet -
    // the width the tables treat as a normal corridor - so the published sizes map
    // straight onto cell counts with no scaling decision to make.
    //
    // Weighted rather than uniform because the table is: the middling rooms are
    // what a dungeon is mostly made of, and the 50x80 hall is worth having
    // precisely because it is rare. Every entry is flipped on a coin when it is
    // rolled, so 2x3 and 3x2 both occur and the map develops no grain.
    //
    // The small end is deliberately held down. Rejection sampling already favours
    // small rooms without any help - they fit in more places, so more of the
    // proposals that survive are small - and a first pass weighted evenly came out
    // as a grid of closets with the occasional hall. The 2x2 is kept only because
    // a vault and a cell want to be cramped.
    //------------------------------------------------------------------------------
    struct RoomSize { int w, d, weight; };

    const RoomSize RoomSizes[] =
    {
        { 2, 2, 1 },    // 20 x 20 ft
        { 3, 3, 3 },    // 30 x 30
        { 4, 4, 3 },    // 40 x 40
        { 2, 3, 2 },    // 20 x 30
        { 3, 4, 4 },    // 30 x 40
        { 4, 5, 3 },    // 40 x 50
        { 5, 8, 1 },    // 50 x 80
        { 6, 6, 2 },    // 60 x 60
    };

    const int RoomSizeCount = (int)(sizeof(RoomSizes)/sizeof(RoomSizes[0]));

    // One weighted pick shared by every table in this file. Returns -1 only when
    // every weight was zero, which each caller answers in its own way.
    int PickWeighted(const int *weights, int count, std::mt19937 &rng)
    {
        int total = 0;
        for (int i = 0; i < count; i++) total += weights[i];

        if (total <= 0) return -1;

        std::uniform_int_distribution<int> roll(0, total - 1);
        int ticket = roll(rng);

        for (int i = 0; i < count; i++)
        {
            ticket -= weights[i];
            if (ticket < 0) return i;
        }

        return count - 1;
    }
}

float Map::CellSize() const
{
    return Config::MapCellSize;
}

void Map::LoadTestArena()
{
    depth = (int)(sizeof(ArenaRows)/sizeof(ArenaRows[0]));
    width = (int)TextLength(ArenaRows[0]);

    tiles.assign((size_t)(width*depth), Tile::Floor);
    spawn = { 0.0f, 0.0f, 0.0f };
    rooms.clear();
    doorways.clear();

    for (int cz = 0; cz < depth; cz++)
    {
        const char *row = ArenaRows[cz];

        for (int cx = 0; cx < width; cx++)
        {
            const char cell = row[cx];

            if (cell == '#') tiles[(size_t)(cz*width + cx)] = Tile::Wall;
            else if (cell == '@') spawn = CellCenter(cx, cz);
        }
    }
}

void Map::Fill(Tile tile)
{
    tiles.assign((size_t)(width*depth), tile);
}

void Map::Set(int cx, int cz, Tile tile)
{
    if ((cx < 0) || (cz < 0) || (cx >= width) || (cz >= depth)) return;

    tiles[(size_t)(cz*width + cx)] = tile;
}

void Map::CarveRoom(const Room &room)
{
    for (int cz = room.z; cz < room.z + room.d; cz++)
        for (int cx = room.x; cx < room.x + room.w; cx++) Set(cx, cz, Tile::Floor);
}

//----------------------------------------------------------------------------------
// An L bend between two cells, one or two cells wide.
//
// `xFirst` picks which leg runs first, and it is chosen per corridor rather than
// fixed so the map does not develop a grain - every corridor turning the same way
// reads as a pattern from the first room you walk into.
//
// The widening is clamped off the outermost ring. Nothing outside the grid is
// drawable, so a corridor carved into the edge column would want a wall on a line
// with no cell on its far side to anchor it.
//----------------------------------------------------------------------------------
void Map::CarveCorridor(int fromX, int fromZ, int toX, int toZ, bool xFirst, int width2)
{
    const int cornerX = xFirst ? toX : fromX;
    const int cornerZ = xFirst ? fromZ : toZ;
    const int extra = (width2 > 1) ? 1 : 0;

    // The border ring is reserved, so a widened leg simply gives up its second
    // lane there rather than pushing the corridor off the map
    auto carve = [&](int cx, int cz)
    {
        if ((cx < 1) || (cz < 1) || (cx >= width - 1) || (cz >= depth - 1)) return;

        Set(cx, cz, Tile::Floor);
    };

    auto runX = [&](int a, int b, int cz)
    {
        for (int cx = std::min(a, b); cx <= std::max(a, b); cx++)
        {
            carve(cx, cz);
            if (extra) carve(cx, cz + 1);
        }
    };

    auto runZ = [&](int a, int b, int cx)
    {
        for (int cz = std::min(a, b); cz <= std::max(a, b); cz++)
        {
            carve(cx, cz);
            if (extra) carve(cx + 1, cz);
        }
    };

    runX(fromX, cornerX, fromZ);
    runZ(fromZ, cornerZ, cornerX);
    runX(cornerX, toX, toZ);
    runZ(cornerZ, toZ, toX);
}

//----------------------------------------------------------------------------------
// Extra corridors between rooms that already sit near each other.
//
// The chain that Generate lays down is what makes the map connected, and it makes
// it a single thread: one route, walked out and walked back. These edges are what
// turn the thread into a place. Every one of them gives the player somewhere to
// circle back through, and gives an enemy somewhere to come from that is not the
// way the player came in.
//
// Runs strictly after the chain, and only between rooms the chain has already
// joined by some path, so nothing here can ever be load-bearing for connectivity.
//----------------------------------------------------------------------------------
void Map::CarveLoops(const std::vector<std::pair<int, int>> &chain, std::mt19937 &rng)
{
    if (rooms.size() < 3) return;

    std::uniform_real_distribution<float> chance(0.0f, 1.0f);
    std::uniform_int_distribution<int> coin(0, 1);
    std::uniform_real_distribution<float> wide(0.0f, 1.0f);

    auto joined = [&](int a, int b)
    {
        for (size_t i = 0; i < chain.size(); i++)
        {
            if ((chain[i].first == a) && (chain[i].second == b)) return true;
            if ((chain[i].first == b) && (chain[i].second == a)) return true;
        }

        return false;
    };

    int made = 0;

    for (size_t a = 0; a < rooms.size() && made < Config::LoopMaxCount; a++)
    {
        for (size_t b = a + 1; b < rooms.size() && made < Config::LoopMaxCount; b++)
        {
            // A pair the chain already joined would be a second corridor beside
            // the first rather than a new way round
            if (joined((int)a, (int)b)) continue;

            const int dx = rooms[a].CenterX() - rooms[b].CenterX();
            const int dz = rooms[a].CenterZ() - rooms[b].CenterZ();

            if (std::abs(dx) + std::abs(dz) > Config::LoopMaxDistance) continue;
            if (chance(rng) > Config::LoopChance) continue;

            const int lanes = (wide(rng) < Config::CorridorWideChance) ? 2 : 1;

            CarveCorridor(rooms[a].CenterX(), rooms[a].CenterZ(),
                          rooms[b].CenterX(), rooms[b].CenterZ(), coin(rng) != 0, lanes);
            made++;
        }
    }
}

//----------------------------------------------------------------------------------
// Short passages off a corridor that stop in solid rock.
//
// Cheap content: somewhere to search, somewhere for the dressing pass to put
// wreckage, and somewhere a chase can go wrong. The DMG's Passage table rolls one
// on a 10 and offers a secret door at the end of it; there are no secret doors
// here yet, but the dead end is worth having on its own.
//
// A stub may only be carved into cells that touch nothing but rock, or it would
// quietly become a second route between two corridors - which is a loop, and
// loops are CarveLoops' job to place deliberately.
//----------------------------------------------------------------------------------
void Map::CarveDeadEnds(std::mt19937 &rng)
{
    std::vector<int> corridor;

    for (int cz = 1; cz < depth - 1; cz++)
        for (int cx = 1; cx < width - 1; cx++)
            if (!IsWall(cx, cz) && (RoomAt(cx, cz) < 0)) corridor.push_back(cz*width + cx);

    if (corridor.empty()) return;

    std::uniform_int_distribution<int> pick(0, (int)corridor.size() - 1);
    std::uniform_int_distribution<int> dirRoll(0, 3);
    std::uniform_int_distribution<int> lengthRoll(Config::DeadEndMin, Config::DeadEndMax);

    const int stepX[4] = { 1, -1, 0, 0 };
    const int stepZ[4] = { 0, 0, 1, -1 };

    for (int made = 0, tries = 0; (made < Config::DeadEndCount) && (tries < Config::DeadEndCount*8); tries++)
    {
        const int start = corridor[(size_t)pick(rng)];
        const int dir = dirRoll(rng);
        const int length = lengthRoll(rng);

        int cx = start%width;
        int cz = start/width;
        int carved = 0;

        for (int step = 0; step < length; step++)
        {
            const int nx = cx + stepX[dir];
            const int nz = cz + stepZ[dir];

            if ((nx < 1) || (nz < 1) || (nx >= width - 1) || (nz >= depth - 1)) break;
            if (!IsWall(nx, nz)) break;

            // Everything the candidate touches must be rock, except the cell the
            // stub is arriving from. Otherwise it opens onto something.
            bool clear = true;

            for (int n = 0; n < 4; n++)
            {
                const int tx = nx + stepX[n];
                const int tz = nz + stepZ[n];

                if ((tx == cx) && (tz == cz)) continue;
                if (!IsWall(tx, tz)) { clear = false; break; }
            }

            if (!clear) break;

            Set(nx, nz, Tile::Floor);
            cx = nx;
            cz = nz;
            carved++;
        }

        if (carved > 0) made++;
    }
}

int Map::RoomExits(const Room &room) const
{
    int exits = 0;

    // A perimeter line that carries no wall has floor on both sides of it, and
    // one of those sides is this room. That is an opening.
    for (int cz = room.z; cz < room.z + room.d; cz++)
    {
        if (!WallOnX(room.x, cz)) exits++;
        if (!WallOnX(room.x + room.w, cz)) exits++;
    }

    for (int cx = room.x; cx < room.x + room.w; cx++)
    {
        if (!WallOnZ(cx, room.z)) exits++;
        if (!WallOnZ(cx, room.z + room.d)) exits++;
    }

    return exits;
}

//----------------------------------------------------------------------------------
// What each room is for, and what state it is in.
//
// Last of all, because it reads the finished layout. Two things about a room
// decide most of what it can plausibly be, and neither is known until every
// corridor has been cut:
//
//   - how far it is from the entrance, which is what puts the vault and the crypt
//     at the far end of the dungeon rather than inside the front door
//   - how many ways there are into it, which is what makes a room worth guarding.
//     A junction everything passes through is where the guards are; a room with
//     one door is where the beds are.
//----------------------------------------------------------------------------------
void Map::AssignKinds(std::mt19937 &rng)
{
    if (rooms.empty()) return;

    //------------------------------------------------------------------------------
    // The entrance is room 0, and room 0 is whichever was placed first - which is
    // a size rolled off the chamber table like any other. A 2x2 is a legitimate
    // chamber and a poor place to begin: the spawn keeps its own cell and its four
    // neighbours clear, which in a 2x2 is the entire room, and the player stands up
    // in a closet with a door.
    //
    // So the first room large enough is moved to the front instead. Safe here
    // because every corridor has already been cut - the order of this vector is
    // bookkeeping by this point, not geometry.
    //------------------------------------------------------------------------------
    if (rooms[0].Area() < 9)
    {
        for (size_t i = 1; i < rooms.size(); i++)
        {
            if (rooms[i].Area() < 9) continue;

            std::swap(rooms[0], rooms[i]);
            break;
        }
    }

    // The player starts here, so nothing may be rolled for it
    rooms[0].kind = RoomKind::Entrance;
    rooms[0].state = ChamberState::Pristine;

    bool used[Rooms::KindCount] = { false };
    used[(int)RoomKind::Entrance] = true;

    const int ex = rooms[0].CenterX();
    const int ez = rooms[0].CenterZ();

    float furthest = 1.0f;

    for (const Room &room : rooms)
    {
        const float away = (float)(std::abs(room.CenterX() - ex) + std::abs(room.CenterZ() - ez));
        if (away > furthest) furthest = away;
    }

    //------------------------------------------------------------------------------
    // The way down, assigned rather than rolled.
    //
    // The room furthest from the entrance that is large enough to hold the thing -
    // furthest so that leaving is a walk across the whole map rather than a step
    // back into the room you started in, and large enough because the portal stands
    // in the middle of the floor and needs clearance on every side.
    //
    // Rolling it would mean a map could produce no portal at all, which is a map
    // with no exit. So this runs before the weighted pass and that pass simply
    // skips whatever it picked.
    //
    // `portalRoom` stays -1 only when there is no second room, which is a map with
    // one chamber in it. Nothing generates one; the check is here because a level
    // with no way out is the one failure the player cannot work around.
    //------------------------------------------------------------------------------
    int portalRoom = -1;
    float portalAway = -1.0f;

    for (size_t i = 1; i < rooms.size(); i++)
    {
        const Room &room = rooms[i];

        if (room.Area() < Rooms::Spec(RoomKind::Portal).minArea) continue;

        const float away = (float)(std::abs(room.CenterX() - ex) + std::abs(room.CenterZ() - ez));

        if (away <= portalAway) continue;

        portalAway = away;
        portalRoom = (int)i;
    }

    // Nothing was big enough. Take the largest room there is rather than leaving
    // the map without an exit - a cramped portal room is a flaw, and no portal is
    // a dead end.
    if (portalRoom < 0)
    {
        for (size_t i = 1; i < rooms.size(); i++)
        {
            if ((portalRoom < 0) || (rooms[i].Area() > rooms[portalRoom].Area())) portalRoom = (int)i;
        }
    }

    if (portalRoom >= 0)
    {
        rooms[portalRoom].kind = RoomKind::Portal;
        // Pristine, always. A burned-out or flooded exit reads as a room the map
        // gave up on, and this is the one room that has to look deliberate.
        rooms[portalRoom].state = ChamberState::Pristine;
        used[(int)RoomKind::Portal] = true;
    }

    for (size_t i = 1; i < rooms.size(); i++)
    {
        if ((int)i == portalRoom) continue;     // Already spoken for

        Room &room = rooms[i];

        const float away = (float)(std::abs(room.CenterX() - ex) + std::abs(room.CenterZ() - ez))/furthest;
        const int exits = RoomExits(room);
        const int area = room.Area();

        int weights[Rooms::KindCount] = { 0 };

        for (int k = 0; k < Rooms::KindCount; k++)
        {
            const RoomKindSpec &spec = Rooms::Kinds[k];

            if (spec.weight <= 0) continue;
            if (spec.unique && used[k]) continue;
            if ((area < spec.minArea) || (area > spec.maxArea)) continue;

            float weight = (float)spec.weight;

            // Treasure and the dead belong at the far end. Scaled rather than
            // gated, so a short map still gets a vault somewhere.
            if (((RoomKind)k == RoomKind::Vault) || ((RoomKind)k == RoomKind::Crypt))
                weight *= 0.4f + 2.0f*away;

            // Guards stand where the traffic is. Capped, because a room the
            // corridors have riddled would otherwise be a guardroom every time.
            if ((RoomKind)k == RoomKind::Guardroom)
                weight *= 1.0f + fminf((float)exits, 6.0f)*0.25f;

            // No floor added to the table's own weights. An earlier pass gave
            // every eligible kind a free point and it flattened the table into
            // near-uniform, which is what the weights exist to avoid.
            weights[k] = (int)(weight*8.0f);
        }

        int chosen = PickWeighted(weights, Rooms::KindCount, rng);

        // Nothing was eligible - a room whose area falls through every gate. Rare
        // enough to be a corner case and common enough to need an answer.
        if (chosen < 0) chosen = (int)((area >= 16) ? RoomKind::Lair : RoomKind::Storage);

        room.kind = (RoomKind)chosen;
        used[chosen] = true;

        const int stateChoice = PickWeighted(Rooms::StateWeights, (int)ChamberState::Count, rng);
        room.state = (stateChoice < 0) ? ChamberState::Pristine : (ChamberState)stateChoice;
    }

    //------------------------------------------------------------------------------
    // The stocked floor's Vault - see Config::StockedFirstFloor.
    //
    // Forced AFTER the weighted pass rather than woven into it, because the pass is
    // a weighted roll per room and there is no clean way to say "and one of these,
    // definitely" inside one. Reassigning a room afterwards is one loop and leaves
    // the roll itself exactly as it was for every other floor.
    //
    // The room taken is one that FITS a Vault - it is a small kind (see its row in
    // RoomKind.h) and a hall relabelled as one would be a warehouse with a chest in
    // it. If no room on the map is the right size the floor simply has no chest,
    // which is a map worth knowing about rather than one worth faking.
    //------------------------------------------------------------------------------
    if (stocked)
    {
        bool haveVault = false;

        for (size_t i = 1; i < rooms.size(); i++)
        {
            if (rooms[i].kind == RoomKind::Vault) { haveVault = true; break; }
        }

        if (!haveVault)
        {
            const RoomKindSpec &spec = Rooms::Kinds[(int)RoomKind::Vault];

            for (size_t i = 1; i < rooms.size(); i++)
            {
                if ((int)i == portalRoom) continue;

                const int area = rooms[i].Area();

                if ((area < spec.minArea) || (area > spec.maxArea)) continue;

                rooms[i].kind = RoomKind::Vault;

                // Pristine, so the dressing pass actually furnishes it - an Ashes or
                // Stripped vault is a treasure room with nothing in it, and the
                // chest is the whole reason this room was forced
                rooms[i].state = ChamberState::Pristine;
                break;
            }
        }
    }

    ChooseEventRooms(rng, portalRoom);
    ChooseVendorRooms(rng, portalRoom);
}

//----------------------------------------------------------------------------------
// Which rooms hold this floor's objectives.
//
// Here rather than in the module that runs them, because the DRESSING PASS has to
// know: an event marker stands in the middle of its room and the middle of the room
// is exactly where a table goes. Chosen after a floor was dressed, a marker lands
// inside the furniture - which is what it did.
//
// Neither the entrance nor the portal, and big enough to hold a fight. Beyond that
// there is nothing to score a room ON, so what decides between them is SPACING and
// spacing alone: the first is drawn at random and each one after it is whichever
// candidate is furthest from everything already chosen.
//
// Furthest-first rather than reject-if-too-close, because rejecting drops candidates
// as it goes and can run out having placed one objective on a floor the design says
// has two. Half a floor's objectives is a worse outcome than two that ended up a
// little close together.
//----------------------------------------------------------------------------------
void Map::ChooseEventRooms(std::mt19937 &rng, int portalRoom)
{
    eventRooms.clear();

    std::vector<int> candidates;

    for (int i = 1; i < (int)rooms.size(); i++)
    {
        if (i == portalRoom) continue;
        if (rooms[i].Area() < Config::EventRoomArea) continue;

        candidates.push_back(i);
    }

    //------------------------------------------------------------------------------
    // The stocked floor prefers to keep its Vault free - see Config::StockedFirstFloor.
    //
    // Game::SeedRoomLoot skips any room that holds an event, a vendor or a camp, and
    // the chest is seeded from inside that pass - so an objective landing in the one
    // Vault silently costs the floor its chest. Four event rooms instead of two made
    // that likely rather than rare.
    //
    // A PREFERENCE and not a rule: dropped again the moment removing it would leave
    // too few rooms to place every kind. Events are the thing being guaranteed here
    // and the chest has its own fallback in Game::SeedRoomLoot, so when the map is
    // too small for both the events win.
    //------------------------------------------------------------------------------
    if (stocked)
    {
        std::vector<int> spared;

        for (int i : candidates)
        {
            if (rooms[i].kind != RoomKind::Vault) spared.push_back(i);
        }

        if ((int)spared.size() >= Config::StockedEventCount) candidates = spared;
    }

    // One room per event KIND on the stocked floor, so every kind can be placed -
    // see Config::StockedFirstFloor and EventManager::Place, which assigns them
    const int wanted = stocked ? Config::StockedEventCount : Config::EventCount;

    while (!candidates.empty() && ((int)eventRooms.size() < wanted))
    {
        int pick = 0;

        if (eventRooms.empty())
        {
            // On the level's own generator, not raylib's, so which rooms got
            // objectives is part of what a seed reproduces
            pick = (int)(rng()%candidates.size());
        }
        else
        {
            int best = -1;

            for (int i = 0; i < (int)candidates.size(); i++)
            {
                int nearest = -1;

                for (int taken : eventRooms)
                {
                    const int away = std::abs(rooms[candidates[i]].CenterX() - rooms[taken].CenterX())
                                   + std::abs(rooms[candidates[i]].CenterZ() - rooms[taken].CenterZ());

                    if ((nearest < 0) || (away < nearest)) nearest = away;
                }

                if (nearest > best) { best = nearest; pick = i; }
            }
        }

        eventRooms.push_back(candidates[pick]);
        candidates.erase(candidates.begin() + pick);
    }

    if ((int)eventRooms.size() < Config::EventCount)
    {
        // Worth saying out loud rather than leaving to be noticed. A floor short of
        // objectives opens its portal earlier than the design intends, and the cause
        // is always the same: too few rooms cleared EventRoomArea.
        TraceLog(LOG_WARNING, "MAP: only %i of %i event rooms - too few of %i cells",
                 (int)eventRooms.size(), Config::EventCount, Config::EventRoomArea);
    }
}

//----------------------------------------------------------------------------------
// Which rooms hold this floor's vendors, and which vendor is in each.
//
// One to three, and never the same vendor twice - a floor with two merchants on it
// has one merchant and a wasted room.
//
// --- Which rooms are eligible ---------------------------------------------------
// Not any room. Every room KIND carries a short list of the vendors that make sense
// standing in it (see RoomKindSpec::vendors), and a vendor may only be placed in a
// room whose list names them. A mystic belongs in a library, a captain in a
// guardroom, and nobody at all in a lair - something already lives there.
//
// That is what stops this reading as a scatter. A merchant found in a storeroom is a
// storeroom with a reason to exist; a merchant found in a crypt makes both the vendor
// and the crypt mean less.
//
// --- Why never an event room -----------------------------------------------------
// A vendor standing in the middle of a hunt is a shop the player cannot use and a
// column of light that means two different things at once. Events are chosen first
// and this takes what is left, which is also why this runs second.
//
// --- Why at least one ------------------------------------------------------------
// A floor with no vendor is a floor whose loot has nowhere to go, and the player has
// no way to know that before walking all of it. So the roll is over how many ABOVE
// one, and the first is placed unconditionally - falling back to any room the kinds
// allow, then giving up loudly rather than silently.
//----------------------------------------------------------------------------------
void Map::ChooseVendorRooms(std::mt19937 &rng, int portalRoom)
{
    vendorRooms.clear();
    vendorKinds.clear();

    // How many this floor gets. One to three, flat - a weighted roll would make the
    // three-vendor floor a rarity the player learns to hope for, and this is meant to
    // be the ordinary texture of a floor rather than an event in itself.
    // All three on the stocked floor - see Config::StockedFirstFloor
    const int wanted = stocked ? (int)NpcKind::Count
                               : (1 + (int)(rng()%(unsigned int)Config::VendorsPerFloorMax));

    // Every vendor, shuffled, so which one a short floor gets is not always the
    // merchant. Small enough that a swap loop is the whole algorithm.
    int order[(int)NpcKind::Count];

    for (int i = 0; i < (int)NpcKind::Count; ++i) order[i] = i;

    for (int i = (int)NpcKind::Count - 1; i > 0; --i)
    {
        const int j = (int)(rng()%(unsigned int)(i + 1));
        const int swap = order[i];

        order[i] = order[j];
        order[j] = swap;
    }

    for (int slot = 0; (slot < (int)NpcKind::Count) && ((int)vendorRooms.size() < wanted); ++slot)
    {
        const NpcKind kind = (NpcKind)order[slot];

        std::vector<int> candidates;

        for (int i = 1; i < (int)rooms.size(); i++)
        {
            if (i == portalRoom) continue;
            if (rooms[i].Area() < Config::VendorRoomArea) continue;

            // Not on top of an objective, and not on top of another vendor
            if (std::find(eventRooms.begin(), eventRooms.end(), i) != eventRooms.end()) continue;
            if (std::find(vendorRooms.begin(), vendorRooms.end(), i) != vendorRooms.end()) continue;

            // And not in the stocked floor's reserved Vault - see ChooseEventRooms
            if (stocked && (rooms[i].kind == RoomKind::Vault)) continue;

            if (!NpcSuitsRoom(kind, (int)rooms[i].kind)) continue;

            candidates.push_back(i);
        }

        //--------------------------------------------------------------------------
        // A stocked floor takes any free room rather than going without.
        //
        // "One of everything" has to mean it, and which rooms suit which vendor is a
        // flavour rule (a mystic belongs in a library) that a map can simply fail to
        // satisfy - there may be no library on it. On an ordinary floor going without
        // is the right answer; on the floor that exists to have one of each it is the
        // one thing that must not happen.
        //--------------------------------------------------------------------------
        if (stocked && candidates.empty())
        {
            // Any free room of the right size, suitable or not
            for (int i = 1; i < (int)rooms.size(); i++)
            {
                if (i == portalRoom) continue;
                if (rooms[i].Area() < Config::VendorRoomArea) continue;
                if (stocked && (rooms[i].kind == RoomKind::Vault)) continue;

                if (std::find(eventRooms.begin(), eventRooms.end(), i) != eventRooms.end()) continue;
                if (std::find(vendorRooms.begin(), vendorRooms.end(), i) != vendorRooms.end()) continue;

                candidates.push_back(i);
            }
        }

        //--------------------------------------------------------------------------
        // Last resort: share a room with an objective.
        //
        // A four-event floor eats most of the map's usable rooms, so on a small one
        // there may be nothing left that is free at all - and "one of everything"
        // has to mean it. The two do not actually conflict: FindOpenSpotIn places
        // the vendor somewhere the furniture is not, and an event marker in the same
        // room is one more thing standing in it.
        //
        // Still ordered last, so this only happens on the maps where it has to.
        //--------------------------------------------------------------------------
        if (stocked && candidates.empty())
        {
            for (int i = 1; i < (int)rooms.size(); i++)
            {
                if (i == portalRoom) continue;
                if (rooms[i].Area() < Config::VendorRoomArea) continue;
                if (rooms[i].kind == RoomKind::Vault) continue;

                if (std::find(vendorRooms.begin(), vendorRooms.end(), i) != vendorRooms.end()) continue;

                candidates.push_back(i);
            }
        }

        if (candidates.empty()) continue;

        const int pick = candidates[(int)(rng()%candidates.size())];

        vendorRooms.push_back(pick);
        vendorKinds.push_back((int)kind);

        TraceLog(LOG_INFO, "MAP: %s in the %s at (%i, %i)", NpcAt(kind).name,
                 Rooms::Spec(rooms[pick].kind).name,
                 rooms[pick].CenterX(), rooms[pick].CenterZ());
    }

    if (vendorRooms.empty())
    {
        // A floor with nowhere to spend is worth saying out loud. The cause is always
        // the same: no room rolled a kind whose vendor list is non-empty and that
        // cleared VendorRoomArea.
        TraceLog(LOG_WARNING, "MAP: no vendor could be placed - every suitable room "
                              "is an objective or too small");
    }
}

//----------------------------------------------------------------------------------
// Rooms by rejection, then a corridor from each new room to the nearest room
// already standing.
//
// Joining to an ALREADY PLACED room is what guarantees the map is connected: room
// n reaches into a set that is connected by induction, so every room is reachable
// from every other by construction and no flood fill is needed to prove it.
// Everything added afterwards - loops, dead ends - is added on top of a map that
// is already known to be whole.
//
// Nearest rather than previous, which is what this used to be. The guarantee is
// identical either way, since both join into the connected set; the difference is
// the corridors. Rooms are placed at random over the whole grid, so consecutive
// placements are usually nowhere near each other, and joining them produced
// twenty-five-cell straight runs with nothing whatever in them. Nearest gives the
// same connectivity with corridors that are corridors.
//
// A one cell border is left uncarved all the way round. Nothing outside the grid
// is drawable, so a room touching the edge would want a wall on a line that has no
// cell on its far side to anchor it.
//----------------------------------------------------------------------------------
void Map::Generate(unsigned int levelSeed, int floorDepth)
{
    // One of everything on the first floor, for playtesting only - see
    // Config::StockedFirstFloor and the passes that read Stocked() back
    stocked = Config::StockedFirstFloor && (floorDepth <= 1);

    width = Config::MapWidth;
    depth = Config::MapDepth;
    seed = levelSeed;

    Fill(Tile::Wall);
    rooms.clear();
    doorways.clear();

    std::mt19937 rng(levelSeed);
    std::uniform_int_distribution<int> coin(0, 1);

    // Which pairs the chain actually joined, so CarveLoops does not lay a second
    // corridor alongside one that is already there
    std::vector<std::pair<int, int>> chain;
    std::uniform_real_distribution<float> wide(0.0f, 1.0f);

    int sizeWeights[RoomSizeCount];
    for (int i = 0; i < RoomSizeCount; i++) sizeWeights[i] = RoomSizes[i].weight;

    for (int attempt = 0; attempt < Config::RoomAttempts; attempt++)
    {
        const int shape = PickWeighted(sizeWeights, RoomSizeCount, rng);
        if (shape < 0) break;

        Room room;

        // Flipped on a coin so an entry like 2x3 supplies both orientations and
        // the table does not have to carry each one twice
        if (coin(rng) != 0) { room.w = RoomSizes[shape].w; room.d = RoomSizes[shape].d; }
        else                { room.w = RoomSizes[shape].d; room.d = RoomSizes[shape].w; }

        // The +1 border keeps rooms off the grid edge; the -1 keeps the far side in
        std::uniform_int_distribution<int> placeX(1, width - room.w - 1);
        std::uniform_int_distribution<int> placeZ(1, depth - room.d - 1);

        if (placeX.max() < placeX.min()) continue;
        if (placeZ.max() < placeZ.min()) continue;

        room.x = placeX(rng);
        room.z = placeZ(rng);

        // Grown by the spacing on every side for the overlap test only, so two
        // rooms end up with at least that much rock between them
        const int pad = Config::RoomSpacing;
        bool clear = true;

        for (const Room &other : rooms)
        {
            const bool apartX = (room.x + room.w + pad <= other.x) ||
                                (other.x + other.w + pad <= room.x);
            const bool apartZ = (room.z + room.d + pad <= other.z) ||
                                (other.z + other.d + pad <= room.z);

            if (!apartX && !apartZ) { clear = false; break; }
        }

        if (!clear) continue;

        CarveRoom(room);

        if (!rooms.empty())
        {
            int nearest = 0;
            int nearestDistance = width + depth;

            for (size_t i = 0; i < rooms.size(); i++)
            {
                const int gap = std::abs(rooms[i].CenterX() - room.CenterX()) +
                                std::abs(rooms[i].CenterZ() - room.CenterZ());

                if (gap < nearestDistance) { nearestDistance = gap; nearest = (int)i; }
            }

            const int lanes = (wide(rng) < Config::CorridorWideChance) ? 2 : 1;

            CarveCorridor(rooms[nearest].CenterX(), rooms[nearest].CenterZ(),
                          room.CenterX(), room.CenterZ(), coin(rng) != 0, lanes);

            chain.push_back(std::make_pair(nearest, (int)rooms.size()));
        }

        rooms.push_back(room);
    }

    // Nothing placed at all - a degenerate config rather than bad luck. Fall back
    // to the arena so the game still has somewhere to stand.
    if (rooms.empty()) { LoadTestArena(); return; }

    CarveLoops(chain, rng);
    CarveDeadEnds(rng);

    // After all carving, never during: an opening is only one cell wide once the
    // last corridor that might have widened it has been cut
    FindDoorways();

    AssignKinds(rng);

    spawn = CellCenter(rooms.front().CenterX(), rooms.front().CenterZ());

    // The way down. Falls back to the spawn when a map somehow has no portal room
    // at all - which is a map with one chamber in it, and a portal standing on the
    // player's head is still better than a floor with no exit.
    portal = spawn;

    for (const Room &room : rooms)
    {
        if (room.kind != RoomKind::Portal) continue;

        portal = CellCenter(room.CenterX(), room.CenterZ());
        break;
    }
}

int Map::RoomAt(int cx, int cz) const
{
    for (int i = 0; i < (int)rooms.size(); i++)
        if (rooms[i].Contains(cx, cz)) return i;

    return -1;
}

//----------------------------------------------------------------------------------
// Every opening that deserves a door.
//
// Three things have to be true, and together they are the definition of a doorway
// rather than a gap:
//
//   - the line is OPEN here, so there is somewhere for a door to hang
//   - it is walled on both sides along the same line, which makes the opening
//     exactly one cell - a 4.000 span, the exact width of `wall_doorway`, already
//     flanked by the corner pieces those two walls put at its ends
//   - exactly one side is inside a room, which makes it a room's entrance rather
//     than a bend in a corridor. Doors on corridor kinks are doors in the middle
//     of nowhere.
//
// A two-cell corridor mouth fails the second test and is left as an open arch,
// which is the right answer: there is no piece in the set that fills 8.000.
//----------------------------------------------------------------------------------
void Map::FindDoorways()
{
    doorways.clear();

    // Exactly one side in a room: a threshold has an inside and an outside
    auto threshold = [&](int ax, int az, int bx, int bz)
    {
        return (RoomAt(ax, az) >= 0) != (RoomAt(bx, bz) >= 0);
    };

    for (int cz = 0; cz < depth; cz++)
    {
        for (int ex = 0; ex <= width; ex++)
        {
            if (WallOnX(ex, cz)) continue;
            if (!WallOnX(ex, cz - 1) || !WallOnX(ex, cz + 1)) continue;
            if (!threshold(ex - 1, cz, ex, cz)) continue;

            doorways.push_back({ ex, cz, true });
        }
    }

    for (int ez = 0; ez <= depth; ez++)
    {
        for (int cx = 0; cx < width; cx++)
        {
            if (WallOnZ(cx, ez)) continue;
            if (!WallOnZ(cx - 1, ez) || !WallOnZ(cx + 1, ez)) continue;
            if (!threshold(cx, ez - 1, cx, ez)) continue;

            doorways.push_back({ ez, cx, false });
        }
    }
}

Tile Map::At(int cx, int cz) const
{
    // Outside the grid is solid, so the edge of the map needs no special case
    if ((cx < 0) || (cz < 0) || (cx >= width) || (cz >= depth)) return Tile::Wall;

    return tiles[(size_t)(cz*width + cx)];
}

void Map::WorldToCell(float x, float z, int &cx, int &cz) const
{
    cx = (int)floorf(x/CellSize());
    cz = (int)floorf(z/CellSize());
}

bool Map::IsWallAtWorld(float x, float z) const
{
    int cx = 0, cz = 0;
    WorldToCell(x, z, cx, cz);

    return IsWall(cx, cz);
}

Vector3 Map::CellCenter(int cx, int cz) const
{
    const float size = CellSize();

    return { (cx + 0.5f)*size, 0.0f, (cz + 0.5f)*size };
}

bool Map::SolidAtWorld(float x, float z) const
{
    int cx = 0, cz = 0;
    WorldToCell(x, z, cx, cz);

    // Uncarved rock. Its own boundary slabs stand half inside it, so anything this
    // far in is solid whichever way you measure.
    if (IsWall(cx, cz)) return true;

    const float size = CellSize();
    const float half = Config::WallHalfThickness;

    // Open cell: solid only where one of its four boundary slabs overhangs it
    const float localX = x - cx*size;
    const float localZ = z - cz*size;

    if ((localX < half) && WallOnX(cx, cz)) return true;
    if ((localX > size - half) && WallOnX(cx + 1, cz)) return true;
    if ((localZ < half) && WallOnZ(cx, cz)) return true;
    if ((localZ > size - half) && WallOnZ(cx, cz + 1)) return true;

    return false;
}

Vector3 Map::CellCorner(int cx, int cz) const
{
    const float size = CellSize();

    return { cx*size, 0.0f, cz*size };
}
