#pragma once

#include "raylib.h"

class AssetManager;

//----------------------------------------------------------------------------------
// The framed bar the HUD is built out of.
//
// Three pieces of art rather than drawn rectangles: an ornamented frame with a hollow
// window in it (ui/bar.png, 118x13), and two 100x7 strips that drop into that window
// - one for the empty track and one for the fill. It is the mobile game's HUD art,
// brought over so the two projects read as one thing.
//
// --- Why it stretches in three slices ----------------------------------------------
// The frame's ends are ornamented and its middle is a plain rail. Scaling the whole
// thing to a bar's width would stretch the ornaments into smears at one width and
// squash them at another, so the two end caps are drawn at a fixed scale and only the
// rail between them is stretched. That is what lets one 118px frame serve a 150px
// experience bar and a 320px chaos bar with crisp ends on both.
//
// --- Why the fills are recoloured and not tinted -----------------------------------
// The pack ships a red strip. A tint MULTIPLIES, so blue over red art comes out
// nearly black. Each strip is recoloured per pixel at load instead, which keeps the
// artist's shading and highlight exactly and moves only the hue.
//
// Load once after the window exists; the textures are the AssetManager's and go with
// everything else at UnloadAll.
//----------------------------------------------------------------------------------
enum class BarHue
{
    Health = 0,     // The pack's own red, untouched
    Exp,            // Blue
    Chaos,          // Violet
    Event,          // Tinted per call - an event's bar is its kind's colour

    Count
};

class UiBars
{
public:
    void Load(AssetManager &assets);

    // True once the art is on the GPU. False when the files are missing, which every
    // caller answers by drawing a plain rectangle instead - the HUD still works, it
    // is just no longer ornamented.
    bool Ready() const { return frame != nullptr; }

    // One bar, `width` px wide, its top-left at (x, y). `fill` is clamped to 0..1.
    // `tint` multiplies the fill strip and is WHITE for the four stock hues - it is
    // there for the event bar, which has to be whatever colour that kind is.
    void Draw(BarHue hue, float fill, float x, float y, float width, float scale,
              Color tint = WHITE) const;

    // On-screen height of a bar drawn at `scale`, so a caller can stack them
    static float Height(float scale);

private:
    // All owned by the AssetManager. Null together - if the frame is missing the
    // whole set is treated as missing, because a fill with no frame around it is
    // worse than a plain rectangle rather than better.
    Texture2D *frame = nullptr;
    Texture2D *track = nullptr;
    Texture2D *fills[(int)BarHue::Count] = { nullptr };
};
