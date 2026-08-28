#pragma once

#include "raylib.h"

class AssetManager;

//----------------------------------------------------------------------------------
// The haze the world is seen through, and the one thing about it that moves.
//
// The fog itself lives in the fragment shaders - see the block in lit.fs, and the
// copy of it in skinning.fs. This is what feeds them: it pushes the settings from
// Config once at load, and the eye position once a frame.
//
// --- Why this is a class and not four SetShaderValue calls at each site ---------
// The world is drawn with two programs and there are eight places that attach one
// or the other to a model (the level, the skyline, the vendors, the loot, the
// pickups, the treasure, the events, every animated body). None of them should
// have to know that fog exists, and if any one of them DID have to remember to
// feed it, the bug would be one model in the game that never fogs - which looks
// like a rendering fault rather than a missed call.
//
// The AssetManager caches a shader per path pair, so all eight are handed the same
// two programs. Feeding those two here feeds every one of them.
//
// --- What is NOT fogged ---------------------------------------------------------
// The skybox, deliberately - see the note in Config. The held weapons and the
// weapon preview, because they draw with raylib's own shader and are inches from
// the eye either way. And the additive effects - impacts, the portal, the vendor
// columns, spell motes - which are light rather than surface: haze in front of a
// lamp does not dim the lamp, it spreads it.
//----------------------------------------------------------------------------------
class Fog
{
public:
    // Fetches both programs and pushes everything about the fog that never
    // changes. Safe to call before anything else has asked for either - the
    // AssetManager builds them on demand and hands the same two back later.
    void Load(AssetManager &assets);

    // The eye, once a frame, before the world is drawn. The fog is a distance
    // from the camera and the camera moves; nothing else here does.
    void SetView(Vector3 eye) const;

private:
    // One entry per program the world is drawn with. Fixed at two because that is
    // how many there are - a third would be a new .fs, which is a code change
    // either way, and a vector would hide that from whoever makes it.
    struct Bound
    {
        Shader *shader = nullptr;
        int viewLoc = -1;
    };

    Bound bound[2];
    int count = 0;
};
