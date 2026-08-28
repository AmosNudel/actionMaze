#pragma once

#include "raylib.h"

class AssetManager;

//----------------------------------------------------------------------------------
// The picture behind the front end - the main menu, the options page and the credits.
//
// One full-screen image in SCREEN space, drawn before anything else those pages put
// down. Brought over from the mobile game's menu_background.cpp along with the image
// itself, for the reason the HUD bar art and the font came over: the two projects are
// meant to be recognisably one thing.
//
// --- Why it is tinted ------------------------------------------------------------
// The source is a BLUE night sky, which is right for that game and wrong for this
// one - this dungeon sits under a red cubemap (see Config::SkyCubemap), and a cold
// blue front end followed by a red world reads as two games bolted together. The
// tint multiplies it into the same register the sky is in, so the menu is the same
// place the run happens in rather than a screen in front of it.
//
// Tinted rather than a second painting of the same scene because there is only one
// scene: a recolour is a constant, and a second image is 2MB and a thing to keep in
// step by hand.
//
// --- Missing is not an error -----------------------------------------------------
// A file that fails to load leaves the pages exactly as they were before this
// existed: UiPageBackdrop's flat panel, and nothing lost but the picture. Same rule
// every other optional asset in this project follows.
//----------------------------------------------------------------------------------
class MenuBackdrop
{
public:
    // Owned by the AssetManager, so there is nothing here to release.
    void Load(AssetManager &assets);

    //------------------------------------------------------------------------------
    // Fills the screen, scaled to COVER it - never letterboxed, never stretched out
    // of its own aspect. Cropped instead, which for a sky is free: what falls off
    // the edge is more sky.
    //
    // Draws nothing at all when the image is missing. The caller's own backdrop is
    // what shows through, which is why this goes down FIRST and is not responsible
    // for the page's readability on its own - see Draw's use of UiFadeOverlay.
    //------------------------------------------------------------------------------
    void Draw() const;

    bool Ready() const { return sky != nullptr; }

private:
    Texture2D *sky = nullptr;   // Owned by the AssetManager; null when missing
};
