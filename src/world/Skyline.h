#pragma once

#include "raylib.h"

#include <vector>

class AssetManager;
class Level;

//----------------------------------------------------------------------------------
// The town outside the maze.
//
// The dungeon is roofless and always has been - see the note on Sky - so the player
// spends the whole game looking up at open air over a four-unit wall. That air was
// empty, and empty air is what makes a maze read as a maze: a corridor with nothing
// beyond it is a corridor that was built for you to walk down.
//
// This fills it. A band of medieval buildings wrapped round all four sides of the
// map, scaled far past their authored size and stacked in rows that grow taller as
// they go out, so what shows over the wall is roofs, then towers, then a castle
// standing over the lot. The player cannot walk to any of it and is never meant to:
// it is there to say that the thing they are inside is one wing of something much
// bigger.
//
// --- Why it is drawn and not built ---------------------------------------------
// Nothing here collides, nothing here is entered, nothing here is lit differently
// from the walls beside it. It is scenery in the strict sense - the level's own
// geometry stops at the outer wall, and this starts on the other side of it with a
// gap between the two that no player can stand in.
//
// --- Why the bases are allowed to float ----------------------------------------
// There is no ground out there and there is no need for one. A building is only
// ever seen from inside the maze, over a wall, which means it is only ever seen
// from the wall's top edge UPWARD - the base of anything far enough away to be
// visible is already below that line and hidden by the wall itself. Laying a floor
// out there would be a hundred metres of geometry nobody can see.
//----------------------------------------------------------------------------------
class Skyline
{
public:
    // Optional throughout, like every other pack in this project: a missing model is
    // one building fewer and nothing else. Loads nothing twice - every piece is
    // cached by the AssetManager against the one shared hexagon atlas.
    void Load(AssetManager &assets);

    //------------------------------------------------------------------------------
    // Lays the town out around `level`, from the map's own seed - so a floor looks
    // the same every time it is drawn, and two different floors look like two
    // different places. Called wherever the floor is built.
    //------------------------------------------------------------------------------
    void Place(const Level &level);

    // Inside BeginMode3D, after the sky and before or alongside the level - it is
    // ordinary solid geometry and depth-tests against everything.
    void Draw() const;

    void Clear();

    int Count() const { return (int)buildings.size(); }

private:
    struct Placed
    {
        Model *model = nullptr;
        Vector3 at{};
        float yaw = 0.0f;
        float scale = 1.0f;
    };

    //------------------------------------------------------------------------------
    // The kit, split by what a piece is FOR rather than by what it is called.
    //
    // Which row a building lands in is the only thing this distinction decides: the
    // low clutter fills the band nearest the wall where only roofs show, the tall
    // pieces go behind it where they can clear the wall line, and the castle is a
    // landmark placed once by hand rather than rolled with the rest.
    //------------------------------------------------------------------------------
    std::vector<Model *> low;
    std::vector<Model *> tall;
    Model *castle = nullptr;

    std::vector<Placed> buildings;
};
