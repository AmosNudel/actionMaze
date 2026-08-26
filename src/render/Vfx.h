#pragma once

#include "raylib.h"

#include <vector>

class AssetManager;

//----------------------------------------------------------------------------------
// Which sheet an effect plays from.
//
// One entry per strip under assets/textures/vfx/, in the order of the table in
// Vfx.cpp. These are the effect ART and nothing else - what a given magic looks
// like when it lands is the magic table's decision (see combat/Magic.h), not this
// enum's, which is why the names here are the pictures rather than the spells.
//----------------------------------------------------------------------------------
enum class VfxKind
{
    Blood = 0,
    Muzzle,         // Short, snapping flash
    Splash,         // Particle burst
    Explosion,
    Poison,         // Rolling smoke
    Flame,
    MuzzleBig,      // Slower, wider flash
    Lightning,

    Count
};

//----------------------------------------------------------------------------------
// One-shot sprite effects, played in the world and turned to face the player.
//
// The art is 2D and the world is not, which is the whole design of this: an impact
// is a flat animation drawn on a quad that is rebuilt every frame to lie square to
// the camera. From the player's eye it is a full screen-facing picture no matter
// which way they walked round it, and there is no angle it can be seen edge-on
// from - which is exactly what a hand-drawn explosion needs and what a modelled one
// would cost a great deal more to get.
//
// Every sheet is one horizontal strip of square frames, so a frame is
// texture.width/frames across and texture.height tall. Effects are sized in world
// units edge to edge and positioned by their CENTRE - not by their feet, the way a
// character is - because the art is drawn centred in its frame.
//
// Nothing holds a handle to a playing effect. Spawn it and forget it: it retires
// itself on the frame after its last one is shown. When the pool is full the oldest
// is recycled rather than the newest dropped, because a full pool means the screen
// is already busy and the effect just asked for is the one being waited on.
//----------------------------------------------------------------------------------
class VfxManager
{
public:
    // Optional per sheet: a strip that fails to load simply plays nothing rather
    // than taking the frame down with it
    void Load(AssetManager &assets);

    // `size` is how many world units across the effect is drawn, centred on `at`.
    // `tint` colours the art - WHITE leaves it as drawn.
    void Spawn(VfxKind kind, Vector3 at, float size, Color tint);

    void Update(float delta);

    // Inside BeginMode3D. `camera` is read every frame rather than cached at spawn:
    // the quad has to be square to where the player is looking NOW, not to where
    // they were standing when the effect went off.
    void Draw(const Camera3D &camera) const;

    void Clear();
    int Count() const { return (int)playing.size(); }

private:
    struct Sheet
    {
        Texture2D *texture = nullptr;   // Owned by the AssetManager; null when missing
        int frames = 1;
        float fps = 24.0f;
        // Drawn as light rather than as matter. The flashes, sparks and fire
        // brighten what is behind them instead of masking it, which is what makes
        // them read as light at all; blood and smoke are matter and would turn into
        // a pale haze if added.
        bool additive = false;
    };

    struct Effect
    {
        VfxKind kind = VfxKind::Blood;
        Vector3 position{};
        float size = 1.0f;
        Color tint = WHITE;
        float elapsed = 0.0f;   // Seconds since it started; the frame is derived
    };

    // One pass per blend mode, so the mode is switched twice a frame rather than
    // once per effect - and so every glow lands over the solid effects rather than
    // being interleaved with them.
    void DrawPass(const Camera3D &camera, bool additive) const;

    Sheet sheets[(int)VfxKind::Count];
    std::vector<Effect> playing;
};
