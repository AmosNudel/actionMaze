#include "ui/Minimap.h"

#include "core/Config.h"
#include "ui/UiTheme.h"
#include "world/Event.h"
#include "world/Level.h"
#include "world/Vendors.h"

#include <cmath>

namespace
{
    // Presentation, so it lives here rather than in Config with the tunables.
    // Everything is drawn over the world, so everything is faded: a minimap that
    // reads as a solid panel is a hole punched in the game.
    const Color PanelFill    = { 8, 10, 14, 175 };
    const Color PanelEdge    = { 190, 190, 200, 80 };

    // Explored floor has to read as explored against a nearly black panel, so it
    // is lighter than it looks like it should be on paper. Rooms are a clear step
    // above corridors: at this size that step is most of what makes a remembered
    // plan legible, since otherwise the map is one grey mass and the rooms are
    // wherever it happens to be wider.
    const Color CorridorFill = { 84, 90, 102, 235 };
    const Color RoomFill     = { 128, 134, 148, 240 };

    // Dimmer than the floor is bright. A 1-pixel line at full white against a
    // 5-pixel cell is not a wall around a room, it is a room made of walls.
    const Color WallLine     = { 178, 180, 190, 220 };
    const Color DoorLine     = { 240, 184, 72, 255 };
    const Color PlayerMark   = { 244, 88, 74, 255 };
    const Color PlayerEdge   = { 26, 12, 10, 255 };

    // The way down. The portal's own colour off Config, so the blip and the column
    // of light at the far end of the map are visibly the same object.
    const Color PortalMark   = { Config::PortalColour[0], Config::PortalColour[1],
                                 Config::PortalColour[2], 255 };

    // Every marker is drawn on this ring of dark first. The plan underneath runs
    // from near-black rock to a light grey room floor, and a coloured disc with no
    // outline reads on one of those and disappears into the other.
    const Color MarkEdge     = { 12, 13, 18, 235 };

    // In cell-pixels. A marker has to be bigger than the cell it stands in or it is
    // a slightly different coloured flagstone, and small enough that two events in
    // neighbouring rooms are two marks rather than one blob.
    constexpr float MarkRadius = 0.9f;
}

//----------------------------------------------------------------------------------
// The plan's three sizes, in SCREEN pixels.
//
// All three are design figures times the shared UI scale - see UiScale in UiTheme.h.
// The cell size stays a float all the way to the draw: rounding it to a whole number
// would land a 34-cell grid up to 34 pixels short of the panel drawn around it.
//----------------------------------------------------------------------------------
float Minimap::CellPixels() { return Config::MinimapCellPixels*UiScale(); }
float Minimap::Padding()    { return Config::MinimapPadding*UiScale(); }
float Minimap::Margin()     { return Config::MinimapMargin*UiScale(); }

bool Minimap::Seen(int cx, int cz) const
{
    if ((cx < 0) || (cz < 0) || (cx >= width) || (cz >= depth)) return false;

    return seen[(size_t)(cz*width + cx)] != 0;
}

void Minimap::Mark(int cx, int cz)
{
    if ((cx < 0) || (cz < 0) || (cx >= width) || (cz >= depth)) return;

    seen[(size_t)(cz*width + cx)] = 1;
}

float Minimap::Height() const
{
    if (depth <= 0) return 0.0f;

    return depth*CellPixels() + Padding()*2.0f;
}

float Minimap::Right() const
{
    if (width <= 0) return 0.0f;

    return Margin() + width*CellPixels() + Padding()*2.0f;
}

void Minimap::Reset(const Level &level)
{
    const Map &map = level.Grid();

    width = map.Width();
    depth = map.Depth();

    seen.assign((size_t)(width*depth), 0);
    inRoom.assign((size_t)(width*depth), 0);

    for (const Room &room : map.Rooms())
    {
        for (int cz = room.z; cz < room.z + room.d; cz++)
            for (int cx = room.x; cx < room.x + room.w; cx++)
                if ((cx >= 0) && (cz >= 0) && (cx < width) && (cz < depth))
                    inRoom[(size_t)(cz*width + cx)] = 1;
    }

    // Forces the next Update to do its work, which is what puts the room the
    // player starts in on the map before they have taken a step
    lastX = -1;
    lastZ = -1;
}

void Minimap::Update(const Level &level, Vector3 playerPos)
{
    const Map &map = level.Grid();

    // The level was rebuilt without anybody telling us. Cheap to check, and the
    // alternative is remembering a map that no longer exists.
    if ((map.Width() != width) || (map.Depth() != depth)) Reset(level);
    if (seen.empty()) return;

    int cx = 0, cz = 0;
    map.WorldToCell(playerPos.x, playerPos.z, cx, cz);

    if ((cx == lastX) && (cz == lastZ)) return;

    lastX = cx;
    lastZ = cz;

    // A room, whole, the moment you are in it
    const int room = map.RoomAt(cx, cz);

    if (room >= 0)
    {
        const Room &here = map.Rooms()[(size_t)room];

        for (int rz = here.z; rz < here.z + here.d; rz++)
            for (int rx = here.x; rx < here.x + here.w; rx++) Mark(rx, rz);
    }

    //------------------------------------------------------------------------------
    // ...and whatever else is genuinely in sight.
    //
    // Line of sight rather than a plain radius, so standing beside a wall does not
    // reveal the corridor on the other side of it. Only floor is tested: a wall
    // cell would block the very line drawn to it, and walls are not remembered
    // anyway - they are drawn from any remembered cell beside them.
    //------------------------------------------------------------------------------
    const int reach = Config::MinimapRevealCells;

    for (int nz = cz - reach; nz <= cz + reach; nz++)
    {
        for (int nx = cx - reach; nx <= cx + reach; nx++)
        {
            const int dx = nx - cx;
            const int dz = nz - cz;

            if ((dx*dx + dz*dz) > reach*reach) continue;
            if ((nx < 0) || (nz < 0) || (nx >= width) || (nz >= depth)) continue;
            if (map.IsWall(nx, nz)) continue;
            if (!level.LineOfSight(playerPos, map.CellCenter(nx, nz))) continue;

            Mark(nx, nz);
        }
    }
}

Vector2 Minimap::ToScreen(const Level &level, Vector3 at) const
{
    const float size = level.Grid().CellSize();
    const float cell = CellPixels();
    const float origin = Margin() + Padding();

    return { origin + (at.x/size)*cell, origin + (at.z/size)*cell };
}

bool Minimap::SeenAt(const Level &level, Vector3 at) const
{
    int cx = 0, cz = 0;
    level.Grid().WorldToCell(at.x, at.z, cx, cz);

    return Seen(cx, cz);
}

//----------------------------------------------------------------------------------
// One objective, or the way down.
//
// A disc while there is something to do about it and a ring once there is not. The
// difference has to be readable at four pixels across, which rules out the obvious
// answer of fading a finished one out: a dimmed dot on a grey floor is a dot the
// player cannot tell from one they have not been to yet, which is exactly backwards.
//----------------------------------------------------------------------------------
void Minimap::DrawMarker(const Level &level, Vector3 at, Color colour, bool hollow) const
{
    const Vector2 p = ToScreen(level, at);
    const float r = MarkRadius*CellPixels();

    const float edge = UiScale();

    if (hollow)
    {
        DrawCircleLinesV(p, r + edge, MarkEdge);
        DrawCircleLinesV(p, r, Fade(colour, 0.85f));

        return;
    }

    DrawCircleV(p, r + edge, MarkEdge);
    DrawCircleV(p, r, colour);
}

void Minimap::Draw(const Level &level, const EventManager &events,
                   const VendorManager &vendors, Vector3 playerPos, float playerYaw) const
{
    if (seen.empty()) return;

    const Map &map = level.Grid();

    // Floats all the way down. The cell size lands between whole pixels at most
    // scales, and rounding it per cell is what would leave the last column of the
    // grid short of the panel drawn around it.
    const float cell = CellPixels();
    const float pad = Padding();
    const float margin = Margin();

    const float originX = margin + pad;
    const float originY = margin + pad;

    const Rectangle panel = { margin, margin, width*cell + pad*2.0f, depth*cell + pad*2.0f };

    DrawRectangleRec(panel, PanelFill);
    DrawRectangleLinesEx(panel, UiScale(), PanelEdge);

    // Floor. Rooms a shade lighter than corridors, which is most of what makes a
    // remembered plan readable at this size - without it the map is one grey mass
    // and the rooms are wherever it happens to be wider.
    for (int cz = 0; cz < depth; cz++)
    {
        for (int cx = 0; cx < width; cx++)
        {
            if (!Seen(cx, cz)) continue;

            const bool room = inRoom[(size_t)(cz*width + cx)] != 0;

            // A hair over one cell, so neighbouring cells overlap rather than
            // leaving a seam of panel between them at fractional scales
            DrawRectangleRec({ originX + cx*cell, originY + cz*cell, cell + 1.0f, cell + 1.0f },
                             room ? RoomFill : CorridorFill);
        }
    }

    //------------------------------------------------------------------------------
    // Walls, straight off the grid rather than out of the fog.
    //
    // A pixel per unit of UI scale, so the plan's lines thicken with the plan. A
    // hairline on a grid drawn at two and a half times the size is a plan with no
    // walls on it.
    //
    // A line is drawn wherever one stands AND the player has seen the floor on
    // either side of it - which is exactly the set of walls they would have been
    // able to see from the floor they have walked. Nothing about walls is stored.
    //------------------------------------------------------------------------------
    const float wallWeight = UiScale();

    for (int cz = 0; cz < depth; cz++)
    {
        for (int ex = 0; ex <= width; ex++)
        {
            if (!map.WallOnX(ex, cz)) continue;
            if (!Seen(ex - 1, cz) && !Seen(ex, cz)) continue;

            DrawLineEx({ originX + ex*cell, originY + cz*cell },
                       { originX + ex*cell, originY + (cz + 1)*cell },
                       wallWeight, WallLine);
        }
    }

    for (int ez = 0; ez <= depth; ez++)
    {
        for (int cx = 0; cx < width; cx++)
        {
            if (!map.WallOnZ(cx, ez)) continue;
            if (!Seen(cx, ez - 1) && !Seen(cx, ez)) continue;

            DrawLineEx({ originX + cx*cell, originY + ez*cell },
                       { originX + (cx + 1)*cell, originY + ez*cell },
                       wallWeight, WallLine);
        }
    }

    //------------------------------------------------------------------------------
    // Doorways, drawn across the gap the wall pass deliberately left.
    //
    // A doorway carries no wall - to the grid it is a hole - so without this a
    // door reads as an opening exactly like the wide arches do, and the one thing
    // a player wants off a dungeon map is where the doors are.
    //------------------------------------------------------------------------------
    // Thicker than the walls, on purpose. A door is the one thing on this map the
    // player is looking for, and at one pixel among a hundred pixels of wall it is
    // not on the map so much as technically present on it.
    const float doorWeight = 2.0f*UiScale();

    for (const Doorway &door : map.Doorways())
    {
        if (door.alongX)
        {
            if (!Seen(door.line - 1, door.span) && !Seen(door.line, door.span)) continue;

            DrawLineEx({ originX + door.line*cell, originY + door.span*cell },
                       { originX + door.line*cell, originY + (door.span + 1)*cell },
                       doorWeight, DoorLine);
        }
        else
        {
            if (!Seen(door.span, door.line - 1) && !Seen(door.span, door.line)) continue;

            DrawLineEx({ originX + door.span*cell, originY + door.line*cell },
                       { originX + (door.span + 1)*cell, originY + door.line*cell },
                       doorWeight, DoorLine);
        }
    }

    //------------------------------------------------------------------------------
    // The objectives and the way down, once their room has been walked into.
    //
    // Under the player arrow and over everything else, which is the right order for
    // both: a marker hidden behind a wall line is a marker the player has to hunt
    // for, and the arrow is the one thing on this map that must never be covered.
    //
    // The portal goes on LAST of the two so that an event standing in the portal's
    // own room - which the generator does not forbid - cannot hide the way out.
    //------------------------------------------------------------------------------
    for (int i = 0; i < events.BlipCount(); ++i)
    {
        const EventManager::Blip blip = events.BlipAt(i);

        if (!SeenAt(level, blip.at)) continue;

        DrawMarker(level, blip.at, blip.colour, blip.resolved);
    }

    //------------------------------------------------------------------------------
    // The vendors, as a square rather than a disc.
    //
    // Shape and not colour, because the merchant's gold and an event's orange are
    // close enough at six pixels to be confused, and the difference between "somewhere
    // to spend" and "something to fight" is the one distinction on this map that must
    // never be ambiguous. A square is a building; a circle is an objective.
    //------------------------------------------------------------------------------
    for (int i = 0; i < vendors.Count(); ++i)
    {
        const Vector3 at = vendors.PointAt(i);

        if (!SeenAt(level, at)) continue;

        const Vector2 p = ToScreen(level, at);
        const float r = MarkRadius*CellPixels();
        const float edge = UiScale();

        const Color colour = NpcAt(vendors.KindAt(i)).colour;

        DrawRectangleRec({ p.x - r - edge, p.y - r - edge,
                           (r + edge)*2.0f, (r + edge)*2.0f }, MarkEdge);

        DrawRectangleRec({ p.x - r, p.y - r, r*2.0f, r*2.0f }, colour);
    }

    if (SeenAt(level, level.PortalPoint()))
    {
        const Vector2 p = ToScreen(level, level.PortalPoint());
        const float r = MarkRadius*CellPixels();
        const float ring = UiScale();

        // A ringed disc rather than a plain one. The portal's blue and the defend
        // event's are a few units apart by design - both are cold and both mean
        // "walk into this" - so what separates them here is the shape, and a mark
        // that is visibly built differently survives being four pixels across.
        DrawCircleV(p, r + 2.0f*ring, MarkEdge);
        DrawCircleLinesV(p, r + 1.5f*ring, PortalMark);
        DrawCircleV(p, r*0.55f, PortalMark);
    }

    //------------------------------------------------------------------------------
    // Where you are, and which way you are facing.
    //
    // Matching Body::Update's convention, where forward at yaw 0 runs along -Z -
    // which is up the screen, since the plan is drawn north up.
    //
    // Wound deliberately rather than hopefully. raylib culls back faces on filled
    // triangles, so an arrowhead built from a facing direction is wound one way for
    // half the compass and the other way for the rest - and the half that comes out
    // backwards is not drawn wrong, it is not drawn at all. Forcing the sign of the
    // signed area makes the marker independent of which way the player is looking.
    //------------------------------------------------------------------------------
    const float size = map.CellSize();
    const float px = originX + (playerPos.x/size)*cell;
    const float py = originY + (playerPos.z/size)*cell;

    const float facingX = -sinf(playerYaw);
    const float facingZ = -cosf(playerYaw);

    const float nose = cell*2.1f;
    const float tail = cell*0.9f;
    const float half = cell*0.95f;

    // Perpendicular to the facing, in screen space
    const float sideX = -facingZ;
    const float sideY = facingX;

    const float baseX = px - facingX*tail;
    const float baseY = py - facingZ*tail;

    Vector2 tip  = { px + facingX*nose, py + facingZ*nose };
    Vector2 left = { baseX + sideX*half, baseY + sideY*half };
    Vector2 right = { baseX - sideX*half, baseY - sideY*half };

    const float wind = (left.x - tip.x)*(right.y - tip.y) - (right.x - tip.x)*(left.y - tip.y);

    if (wind > 0.0f) { const Vector2 swap = left; left = right; right = swap; }

    DrawTriangle(tip, left, right, PlayerMark);

    // An outline, because the arrow crosses both the light grey of a room floor and
    // the near-black of unexplored rock, and a single fill cannot read on both
    DrawTriangleLines(tip, left, right, PlayerEdge);
}
