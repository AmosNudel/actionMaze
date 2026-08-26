#pragma once

#include "raylib.h"

class AssetManager;

//----------------------------------------------------------------------------------
// The skybox: a cubemap on the inside of a unit cube.
//
// A cubemap is the format this actually wanted all along. Six square faces, each
// covering ninety degrees, so every direction is sampled the same way - no poles
// to pinch, no edges that were never meant to meet, no seam to hide. The earlier
// attempt wrapped a flat 16:9 painting onto a sphere and spent all its effort
// mirroring the texture to disguise the join; none of that is needed here.
//
// The cube is drawn at the origin at unit scale no matter where the camera is:
// skybox.vs strips the translation out of the view matrix, so only the rotation
// survives and the box is effectively infinitely far away. That is also why it
// needs no radius - there is no distance to get wrong.
//----------------------------------------------------------------------------------
class Sky
{
public:
    // Silently does nothing if the image is missing, leaving the cleared
    // background showing - one absent PNG should not cost the level
    void Load(AssetManager &assets, const char *cubemapPath);
    void Unload();

    // First in the frame, writing no depth, so the level draws over all of it
    void Draw() const;

    bool Ready() const { return ready; }

private:
    // The cube is generated and the cubemap is built here, so both are owned here.
    // The shader is not - that comes from the AssetManager.
    mutable Model box{};
    bool ready = false;
};
