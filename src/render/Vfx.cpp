#include "render/Vfx.h"

#include "core/Config.h"
#include "raymath.h"
#include "rlgl.h"
#include "render/AssetManager.h"

#include <algorithm>

namespace
{
    //------------------------------------------------------------------------------
    // The sheets, in VfxKind order.
    //
    // Frame counts are a property of the FILE and cannot be inferred from it: the
    // strip is one row, so its width says frames*side and nothing about where the
    // cuts are. A wrong count here shears the animation rather than failing, which
    // is why the number lives next to the path and not in a caller.
    //
    // The rates are the pace each effect was drawn at rather than one house speed:
    // a muzzle flash is a snap and smoke rolls, and playing both at 24 makes one
    // sluggish and the other a flicker.
    //------------------------------------------------------------------------------
    struct SheetDef
    {
        const char *path;
        int frames;
        float fps;
        bool additive;
    };

    constexpr SheetDef Sheets[(int)VfxKind::Count] =
    {
        { "textures/vfx/blood.png",     13, 26.0f, false },
        { "textures/vfx/muzzle.png",     8, 30.0f, true  },
        { "textures/vfx/splash.png",     9, 22.0f, true  },
        { "textures/vfx/explosion.png", 24, 30.0f, true  },
        { "textures/vfx/poison.png",    20, 18.0f, false },
        { "textures/vfx/flame.png",     28, 26.0f, true  },
        { "textures/vfx/muzzlebig.png", 16, 28.0f, true  },
        { "textures/vfx/lightning.png", 20, 30.0f, true  },
    };
}

void VfxManager::Load(AssetManager &assets)
{
    for (int i = 0; i < (int)VfxKind::Count; ++i)
    {
        const SheetDef &def = Sheets[i];

        sheets[i].frames = def.frames;
        sheets[i].fps = def.fps;
        sheets[i].additive = def.additive;
        sheets[i].texture = nullptr;

        if (!FileExists(AssetManager::Resolve(def.path).c_str()))
        {
            TraceLog(LOG_WARNING, "VFX: %s not found, that effect plays nothing", def.path);
            continue;
        }

        Texture2D &texture = assets.GetTexture(def.path);
        if (texture.id == 0) continue;

        // The frames are soft glows with no hard edges, drawn a couple of units
        // across at whatever distance the fight happens to be. Point sampling them
        // shows the 128px source as blocks the moment the player is close.
        SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);

        sheets[i].texture = &texture;
    }
}

void VfxManager::Spawn(VfxKind kind, Vector3 at, float size, Color tint)
{
    const int index = (int)kind;
    if (index < 0 || index >= (int)VfxKind::Count) return;

    // A missing sheet plays nothing at all rather than occupying a slot for the
    // length of an animation nobody can see
    if (sheets[index].texture == nullptr) return;

    Effect effect;
    effect.kind = kind;
    effect.position = at;
    effect.size = size;
    effect.tint = tint;

    if ((int)playing.size() < Config::MaxActiveVfx)
    {
        playing.push_back(effect);
        return;
    }

    // Full. Recycle whatever has been playing longest - it is the one closest to
    // finishing and the one the player has already read.
    auto oldest = std::max_element(playing.begin(), playing.end(),
                                   [](const Effect &a, const Effect &b)
                                   { return a.elapsed < b.elapsed; });

    *oldest = effect;
}

void VfxManager::Update(float delta)
{
    for (Effect &effect : playing) effect.elapsed += delta;

    // One shot: an effect is done the moment its frame index would run off the end
    // of its strip. Nothing loops here.
    playing.erase(std::remove_if(playing.begin(), playing.end(),
                                 [this](const Effect &effect)
                                 {
                                     const Sheet &sheet = sheets[(int)effect.kind];
                                     return effect.elapsed*sheet.fps >= (float)sheet.frames;
                                 }),
                  playing.end());
}

void VfxManager::DrawPass(const Camera3D &camera, bool additive) const
{
    //------------------------------------------------------------------------------
    // The quad's up axis, in world space.
    //
    // DrawBillboardPro takes its RIGHT from the view matrix already, so it only
    // needs telling which way is up - and the obvious answer, {0,1,0}, is the wrong
    // one here. That gives a billboard that only spins about Y, so an effect looked
    // down on from above foreshortens to a sliver, and the shot that hit a skeleton
    // at your feet shows nothing. Handing it the camera's own up instead keeps the
    // quad square to the screen from every angle, which is the point of drawing a
    // flat picture in a 3D world at all.
    //------------------------------------------------------------------------------
    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    const Vector3 up = Vector3CrossProduct(right, forward);

    for (const Effect &effect : playing)
    {
        const Sheet &sheet = sheets[(int)effect.kind];
        if (sheet.additive != additive) continue;

        // Clamped rather than wrapped: Update retires an effect on the frame its
        // index runs off the end, but the last draw of its life happens first, and
        // wrapping would flash frame 0 on the way out.
        int frame = (int)(effect.elapsed*sheet.fps);
        if (frame >= sheet.frames) frame = sheet.frames - 1;
        if (frame < 0) frame = 0;

        const float frameWidth = sheet.texture->width/(float)sheet.frames;

        const Rectangle source = { frame*frameWidth, 0.0f,
                                   frameWidth, (float)sheet.texture->height };

        //--------------------------------------------------------------------------
        // Square art, so one size on both axes - the effect is as tall as it is
        // wide by construction and cannot be stretched by a caller getting it wrong.
        //
        // The origin is HALF the size, and that is not a nicety. DrawBillboardPro
        // builds its quad from `position` outwards along right and up, so with a
        // zero origin the point given is the BOTTOM LEFT corner and the whole
        // effect hangs up and to the right of the impact that caused it - by half
        // its own width, which for a three-unit blast is most of a body. Naming
        // the centre as the origin is what makes `position` mean the centre, which
        // is what every caller here already assumes it means.
        //--------------------------------------------------------------------------
        const Vector2 span = { effect.size, effect.size };
        const Vector2 centre = { span.x*0.5f, span.y*0.5f };

        DrawBillboardPro(camera, *sheet.texture, source, effect.position, up,
                         span, centre, 0.0f, effect.tint);
    }
}

void VfxManager::Draw(const Camera3D &camera) const
{
    if (playing.empty()) return;

    // Neither pass writes depth: these are billboards standing in the middle of the
    // body they went off on, and a depth-writing quad through a skeleton's chest
    // carves a visible hole in it. They still TEST depth, so an effect behind a wall
    // stays behind it.
    rlDisableDepthMask();

    DrawPass(camera, false);

    BeginBlendMode(BLEND_ADDITIVE);
    DrawPass(camera, true);
    EndBlendMode();

    rlEnableDepthMask();
}

void VfxManager::Clear()
{
    playing.clear();
}
