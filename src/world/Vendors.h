#pragma once

#include "raylib.h"
#include "world/Npc.h"

#include <vector>

class AssetManager;
class Level;

//----------------------------------------------------------------------------------
// The vendors standing on this floor.
//
// Which ones and where is the map's decision - see Map::ChooseVendorRooms, and the
// note there for why it cannot be made here. This reads that back, draws them, and
// answers the one question the rest of the game has: is the player close enough to
// one to open its counter, and which.
//
// --- The art -------------------------------------------------------------------------
// A character standing in an aura of their own colour, with their name over their
// head - see the note in Npc.h for which model is which and why. The aura is the
// column of light this used to be, cut down to a pool and its motes: the player
// already learned from the portal and the event markers that coloured light is
// something to walk into, and that lesson is worth keeping even once there is
// somebody standing in it.
//
// --- Why the idle is procedural --------------------------------------------------
// The dungeon pack's adventurers are STATIC. No skin, no clips, just six loose
// body-part meshes with flat materials - there is no idle animation in the file to
// play. So the life comes from moving the whole model: a slow breath up and down and
// a slight sway, out of phase per vendor so three of them on one floor do not
// breathe in unison like a machine.
//
// They also turn to face the player, which is the one piece of animation that
// actually matters for somebody you walk up to and trade with - a vendor with his
// back to you reads as scenery however well he is lit.
//----------------------------------------------------------------------------------
class VendorManager
{
public:
    void Load(AssetManager &assets);

    // Reads the map's choices back. Called after the level is built, and after
    // EventManager::Place, since the two share the pool of rooms.
    void Place(const Level &level);

    void Update(float delta);

    // Inside BeginMode3D
    void Draw(const Camera3D &camera) const;

    // Their names, over their columns. Screen space, so it runs after EndMode3D -
    // which is why it is not part of Draw.
    void DrawLabels(const Camera3D &camera) const;

    //------------------------------------------------------------------------------
    // The vendor the player is standing at, or none.
    //
    // `NpcKind::Count` is the "none" answer rather than a bool plus an out-parameter:
    // every caller wants the kind, and a caller that only wants to know whether there
    // is one can compare against Count as readably as it could test a flag.
    //------------------------------------------------------------------------------
    NpcKind At(Vector3 position) const;

    void Clear();

    int Count() const { return (int)vendors.size(); }

    // Where each one stands and which it is, for the minimap
    Vector3 PointAt(int index) const;
    NpcKind KindAt(int index) const;

private:
    struct Vendor
    {
        NpcKind kind = NpcKind::Merchant;
        Vector3 at{};

        // Where this one is in its own breath, in radians. Rolled at placement so
        // three vendors on a floor are visibly three people rather than one person
        // drawn three times - see the note above.
        float phase = 0.0f;
    };

    //------------------------------------------------------------------------------
    // One vendor's model, by kind. Owned by the AssetManager; null when the file is
    // missing, which costs the character and nothing else.
    //
    // `scale` fits the model's own height to Config::VendorHeight, worked out once
    // at load from its bounding box rather than written down per row: the three
    // adventurers are not authored at the same size, and a hand-tuned number per
    // row would be three numbers to redo the day the pack is re-exported.
    //
    // `foot` is how far the model's lowest point sits below its origin, so a
    // character stands ON the floor instead of half through it. Same reason it is
    // measured rather than declared.
    //------------------------------------------------------------------------------
    struct Look
    {
        Model *model = nullptr;
        float scale = 1.0f;
        float foot = 0.0f;
    };

    Look looks[(int)NpcKind::Count];

    // One vendor's body - facing, breath and sway. Split out of Draw because Draw's
    // own job is the aura and the order the two go down in.
    void DrawFigure(const Vendor &vendor, const Camera3D &camera) const;

    std::vector<Vendor> vendors;

    // Shared by every aura, so the three turn together. One clock rather than one
    // per vendor: the light is the same object in three colours, and three clocks
    // that could drift apart would be three objects that look like a mistake. The
    // BODIES are the opposite case and carry a phase each - see Vendor::phase.
    float spin = 0.0f;

    // Seconds since the floor was built, for the breath. Separate from `spin`
    // because the two are different speeds and one of them wraps.
    float clock = 0.0f;

    Texture2D *glow = nullptr;      // Shared, owned by the AssetManager
};
