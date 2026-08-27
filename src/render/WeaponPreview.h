#pragma once

#include "raylib.h"

#include <string>
#include <unordered_map>

class AssetManager;

//----------------------------------------------------------------------------------
// A weapon, drawn small and turning, wherever a shop row or an inventory row used
// to print the weapon's name instead - see ShopScreen's merchant list and
// CharacterSheet::DrawInventoryTab.
//
// One shared render target reused row by row within a frame, rather than one per
// weapon: a row is rebuilt from scratch every frame already (see the note on
// ShopScreen::Row), and a texture per weapon would be a few dozen live GPU
// targets for something never seen at more than a handful at once.
//
// Draws the exact same models ViewModel does, fetched back off the AssetManager
// by the path ViewModel loaded them from - see Draw. Nothing here owns a model or
// loads one on its own account.
//
// The turn is read off the wall clock rather than accumulated, so every weapon on
// screen turns in step and this needs no delta and no per-row state.
//----------------------------------------------------------------------------------
class WeaponPreview
{
public:
    void Load(AssetManager &assets);
    void Unload();

    // Draws `name`'s model turning inside `dest`, in the current 2D pass - call it
    // between BeginDrawing and EndDrawing, anywhere after the row it belongs to
    // has had its background drawn. Does nothing for a name with no matching
    // model: an empty box is a smaller failure than a crash over a typo.
    void Draw(const std::string &name, Rectangle dest);

private:
    // Where the camera looks and how far back it sits, so a dagger and a
    // greatsword both fill the frame without either one being hand-measured.
    struct Framing
    {
        Vector3 center{};
        float distance = 1.0f;
    };

    Framing FramingFor(Model &model);

    AssetManager *assets = nullptr;
    RenderTexture2D target = { 0 };

    // Cached per model rather than recomputed every row every frame -
    // GetModelBoundingBox walks every vertex, and a shop row asks for the same
    // half dozen weapons' framing sixty times a second.
    std::unordered_map<Model *, Framing> framing;
};
