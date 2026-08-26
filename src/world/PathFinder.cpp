#include "world/PathFinder.h"

#include "core/Config.h"
#include "world/Level.h"

#include <algorithm>
#include <cstdlib>
#include <queue>
#include <utility>

namespace
{
    // Four-connected, in a fixed order so the same query gives the same route.
    // See the header for why this list has no diagonals in it.
    const int StepX[4] = { 1, -1, 0, 0 };
    const int StepZ[4] = { 0, 0, 1, -1 };

    constexpr float Unreached = 1e30f;

    //------------------------------------------------------------------------------
    // Manhattan, which is exact for a four-connected grid of unit steps and so
    // never overestimates. An A* with an admissible heuristic that matches the
    // step set this closely barely searches at all - it walks more or less
    // straight at the goal and only spreads out where a wall makes it.
    //------------------------------------------------------------------------------
    float Heuristic(int ax, int az, int bx, int bz)
    {
        return (float)(std::abs(ax - bx) + std::abs(az - bz));
    }
}

bool PathFinder::Passable(const Level &level, int cx, int cz)
{
    const Map &map = level.Grid();

    if ((cx < 0) || (cz < 0) || (cx >= map.Width()) || (cz >= map.Depth())) return false;
    if (map.IsWall(cx, cz)) return false;

    // Furniture is not a wall, but it is still something a body cannot walk
    // through, and a route that ignored it would send an enemy grinding along the
    // side of a table for as long as it stayed alert
    return !level.PropBlocksAt(map.CellCenter(cx, cz), Config::PathClearRadius);
}

bool PathFinder::Find(const Level &level, Vector3 from, Vector3 to, std::vector<Vector3> &out)
{
    out.clear();

    const Map &map = level.Grid();
    const int w = map.Width();
    const int d = map.Depth();

    if ((w <= 0) || (d <= 0)) return false;

    int startX = 0, startZ = 0, goalX = 0, goalZ = 0;
    map.WorldToCell(from.x, from.z, startX, startZ);
    map.WorldToCell(to.x, to.z, goalX, goalZ);

    if ((startX == goalX) && (startZ == goalZ)) return true;   // Already there

    //------------------------------------------------------------------------------
    // A goal that cannot be stood in is common and is not a failure. The last
    // place an enemy saw the player is a position, not a cell it checked - the
    // player may have been standing half in the next cell, or on top of a crate -
    // so a blocked goal is nudged to the nearest cell that will hold a body rather
    // than abandoned.
    //------------------------------------------------------------------------------
    if (!Passable(level, goalX, goalZ))
    {
        bool found = false;

        for (int ring = 1; (ring <= 2) && !found; ring++)
        {
            for (int oz = -ring; (oz <= ring) && !found; oz++)
            {
                for (int ox = -ring; (ox <= ring) && !found; ox++)
                {
                    if (!Passable(level, goalX + ox, goalZ + oz)) continue;

                    goalX += ox;
                    goalZ += oz;
                    found = true;
                }
            }
        }

        if (!found) return false;
    }

    // The body's own cell is taken on trust. ResolveBody keeps bodies out of
    // walls, but a body shoved half into a prop by a fight would otherwise have
    // nowhere legal to start from and would stop pathing until it worked free.

    const size_t cells = (size_t)(w*d);

    cost.assign(cells, Unreached);
    came.assign(cells, -1);
    closed.assign(cells, 0);

    typedef std::pair<float, int> Entry;      // Estimated total, cell
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry> > open;

    const int start = startZ*w + startX;
    const int goal = goalZ*w + goalX;

    cost[(size_t)start] = 0.0f;
    open.push(std::make_pair(Heuristic(startX, startZ, goalX, goalZ), start));

    int expanded = 0;
    bool reached = false;

    while (!open.empty() && (expanded < Config::PathMaxNodes))
    {
        const int at = open.top().second;
        open.pop();

        if (closed[(size_t)at]) continue;     // A stale entry left by a better route
        closed[(size_t)at] = 1;
        expanded++;

        if (at == goal) { reached = true; break; }

        const int cx = at%w;
        const int cz = at/w;

        for (int step = 0; step < 4; step++)
        {
            const int nx = cx + StepX[step];
            const int nz = cz + StepZ[step];

            if (!Passable(level, nx, nz)) continue;

            const int next = nz*w + nx;
            if (closed[(size_t)next]) continue;

            const float through = cost[(size_t)at] + 1.0f;
            if (through >= cost[(size_t)next]) continue;

            cost[(size_t)next] = through;
            came[(size_t)next] = at;

            open.push(std::make_pair(through + Heuristic(nx, nz, goalX, goalZ), next));
        }
    }

    if (!reached) return false;

    // Walk the trail back, then turn it round. The start cell is dropped: the body
    // is already standing in it, and heading for its middle first is a sidestep.
    for (int at = goal; (at != -1) && (at != start); at = came[(size_t)at])
    {
        out.push_back(map.CellCenter(at%w, at/w));
    }

    std::reverse(out.begin(), out.end());

    return !out.empty();
}
