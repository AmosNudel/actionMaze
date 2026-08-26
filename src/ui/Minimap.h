#pragma once

#include "raylib.h"

#include <vector>

class Level;
class EventManager;
class VendorManager;

//----------------------------------------------------------------------------------
// The map you have drawn for yourself, in the corner of the screen.
//
// Fog of war, and it only ever lifts. What has been seen stays on the map after
// you leave it, because the thing a dungeon map is FOR is the part you are not
// standing in - a minimap that showed only your surroundings would be a compass.
//
// Two rules decide what counts as seen, and they are different on purpose:
//
//   - a room is learned WHOLE, the moment you set foot in it. You do not come to
//     know a chamber one flagstone at a time; you walk in, and you have seen the
//     room.
//   - a corridor is learned as you walk it, out to what you can actually see from
//     where you stand, so a passage you have not been down stays dark.
//
// Only floor is remembered. Walls are not stored at all - they are drawn from the
// grid at the point of drawing, for any line with a remembered cell on either side
// of it, which is exactly the set of walls you would have seen from the floor you
// have walked. Storing them as well would be a second copy of the level to keep in
// step with the first.
//
// North is up and the whole map is on screen at once. It is a floor plan rather
// than a radar: a plan you can compare with the room you are standing in is worth
// more than one that spins under you.
//----------------------------------------------------------------------------------
class Minimap
{
public:
    // Forgets everything and sizes itself to this level. Must be called whenever
    // the level changes under it, or the fog is a memory of a map that is gone.
    void Reset(const Level &level);

    // Marks what can be seen from where the player is standing.
    //
    // Costs nothing on most frames: it does no work at all until the player
    // crosses into a cell they were not in last frame, which is the only time the
    // answer can have changed.
    void Update(const Level &level, Vector3 playerPos);

    //------------------------------------------------------------------------------
    // The plan, and the three things on it worth walking towards.
    //
    // The events, the vendors and the way down are drawn under exactly the same rule
    // as the floor they stand on: a marker appears when the CELL it is in has been seen,
    // and not before. That is the whole point of putting them here - a map that
    // showed every objective from the first step would turn exploring a floor into
    // reading a list, and one that showed none of them leaves the player who has
    // already walked the room re-walking it to find the way out.
    //
    // Which is also why the test is the fog and not the room: the fog already means
    // "you have been here", and a second rule for markers is a second thing that
    // can disagree with what is drawn under them.
    //------------------------------------------------------------------------------
    void Draw(const Level &level, const EventManager &events, const VendorManager &vendors,
              Vector3 playerPos, float playerYaw) const;

    //------------------------------------------------------------------------------
    // How much of the corner this takes up, in SCREEN pixels, so whatever is laid
    // out beside it knows where it may start. Both zero before the first Reset.
    //
    // Height is the panel's own height and does not include the margin above it;
    // Right is a screen X, margin included, because that is what a caller placing
    // something to the right of the map actually wants. They are asymmetric on
    // purpose - the two callers each want the form they get.
    //------------------------------------------------------------------------------
    float Height() const;
    float Right() const;

private:
    // The plan's three metrics in screen pixels: a design figure out of Config times
    // the shared UI scale. Static because they are about the WINDOW rather than
    // about any one map, and the layout above asks for them before a map exists.
    static float CellPixels();
    static float Padding();
    static float Margin();

    bool Seen(int cx, int cz) const;
    void Mark(int cx, int cz);

    // True when the cell a world point stands in has been seen. What gates every
    // marker, so the events and the portal cannot answer that question differently.
    bool SeenAt(const Level &level, Vector3 at) const;

    // One marker on the plan, at a WORLD point. `hollow` is the resolved state - a
    // ring rather than a disc, so a finished event stays on the map as somewhere
    // the player has been rather than vanishing and taking the memory with it.
    void DrawMarker(const Level &level, Vector3 at, Color colour, bool hollow) const;

    // Where a world point lands on the drawn plan, in screen pixels
    Vector2 ToScreen(const Level &level, Vector3 at) const;

    std::vector<unsigned char> seen;

    // Which cells belong to a room, worked out once at Reset. Only so rooms can be
    // shaded differently from corridors - but RoomAt is a scan of every room, and
    // asking it per cell per frame is a scan of every room a thousand times a frame.
    std::vector<unsigned char> inRoom;

    int width = 0;
    int depth = 0;

    // The cell the player was in when this last did any work. -1 forces the first
    // update to run, which is what reveals the room they start in.
    int lastX = -1;
    int lastZ = -1;
};
