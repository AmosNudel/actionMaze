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
// --- Placeholder art -----------------------------------------------------------------
// There is none yet. A vendor is drawn as a column of light in their own colour with
// their name floating over it, which is the same object the events and the portal
// already use. That is deliberate rather than lazy: the player has already learned
// that a column of light is something to walk up to, and a vendor is exactly that.
// Swapping in a model later is one function and one table column.
//
// It also spins and bobs the way the defend relic does, so it reads as a thing that is
// there for you rather than as scenery someone left switched on.
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
    };

    std::vector<Vendor> vendors;

    // Shared by every column, so the three turn together. One clock rather than one
    // per vendor: they are the same object in three colours, and three clocks that
    // could drift apart would be three objects that look like a mistake.
    float spin = 0.0f;

    Texture2D *glow = nullptr;      // Shared, owned by the AssetManager
};
