#pragma once

#include "raylib.h"

#include <vector>

class Level;

//----------------------------------------------------------------------------------
// A route through the level, as A* over the cell grid.
//
// FOUR-connected, never eight. Walls stand on the lines BETWEEN cells, so two
// cells that are diagonal neighbours can both be open floor with solid stone on
// the corner they share - and a diagonal step would walk a body straight through
// it. This is the one mistake the grid representation makes easy, and it does not
// show up as a body in a wall so much as a body that occasionally teleports past
// one.
//
// Cheap enough not to need a budget: a full search of the whole grid is about a
// thousand cells. The node cap exists so a pathological request cannot stall a
// frame, not because the ordinary case is expensive.
//
// One of these is owned by whoever does the asking, so the working arrays are
// allocated once rather than per query.
//----------------------------------------------------------------------------------
class PathFinder
{
public:
    //------------------------------------------------------------------------------
    // Fills `out` with the cell centres to walk, from the cell after `from` up to
    // and including the goal. False when there is no route at all, in which case
    // `out` is emptied - a caller that gets false should fall back to whatever it
    // did before there was a pathfinder, not stand still.
    //
    // The start cell is left out on purpose: it is the cell the body is already
    // standing in, and walking to the middle of it first is a visible sidestep
    // before setting off.
    //------------------------------------------------------------------------------
    bool Find(const Level &level, Vector3 from, Vector3 to, std::vector<Vector3> &out);

    // Whether a body could stand in this cell at all: open floor, and no solid
    // prop filling it. Public because callers want to test a destination before
    // committing to a search for it.
    static bool Passable(const Level &level, int cx, int cz);

private:
    // Reallocated only when the grid changes size, which in practice is once
    std::vector<float> cost;
    std::vector<int> came;
    std::vector<unsigned char> closed;
};
