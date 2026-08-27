#include "render/WeaponPreview.h"

#include "raymath.h"
#include "render/AssetManager.h"

#include <cmath>

namespace
{
    // The render target's own size. An icon, not a viewport - it is drawn at
    // whatever the row happens to be, and a texture much bigger than the largest
    // row it is ever asked to fill would be resolution spent on nothing.
    constexpr int Resolution = 220;

    constexpr float SpinRate = 0.6f;       // Radians a second
    constexpr float Tilt = 18.0f*DEG2RAD;  // Looking down at it slightly, not level
    constexpr float Fovy = 30.0f;

    // Air around the model inside the frame, so a blade tip does not touch the
    // edge of its own icon.
    constexpr float FrameMargin = 1.3f;
}

void WeaponPreview::Load(AssetManager &assetsIn)
{
    assets = &assetsIn;
}

void WeaponPreview::Unload()
{
    if (target.id != 0)
    {
        UnloadRenderTexture(target);
        target = { 0 };
    }
}

//----------------------------------------------------------------------------------
// How far back the camera has to sit, and what it looks at, so the model's own
// bounding box fills the frame at `Fovy` regardless of how big the model is.
//----------------------------------------------------------------------------------
WeaponPreview::Framing WeaponPreview::FramingFor(Model &model)
{
    auto it = framing.find(&model);
    if (it != framing.end()) return it->second;

    const BoundingBox box = GetModelBoundingBox(model);

    Framing out;
    out.center = Vector3Scale(Vector3Add(box.min, box.max), 0.5f);

    const Vector3 size = Vector3Subtract(box.max, box.min);
    const float span = fmaxf(size.x, fmaxf(size.y, size.z));

    out.distance = (span*0.5f)/tanf(Fovy*0.5f*DEG2RAD)*FrameMargin;
    if (out.distance < 0.1f) out.distance = 0.1f;

    framing[&model] = out;

    return out;
}

void WeaponPreview::Draw(const std::string &name, Rectangle dest)
{
    if ((assets == nullptr) || name.empty()) return;

    // The exact path ViewModel::Load fetched this model from - see the note on
    // AssetManager::GetModel. Every weapon the game knows about was already
    // loaded there at startup, so this is always a cache hit and never touches
    // disk.
    Model &model = assets->GetModel("models/weapons/" + name + ".gltf");

    if (model.meshCount <= 0) return;

    if ((target.id == 0) || (target.texture.width != Resolution))
    {
        if (target.id != 0) UnloadRenderTexture(target);
        target = LoadRenderTexture(Resolution, Resolution);
    }

    const Framing frame = FramingFor(model);
    const float angle = fmodf((float)GetTime()*SpinRate, 2.0f*PI);

    Camera3D camera{};
    camera.target = frame.center;
    camera.position = {
        frame.center.x + sinf(angle)*frame.distance*cosf(Tilt),
        frame.center.y + frame.distance*sinf(Tilt),
        frame.center.z + cosf(angle)*frame.distance*cosf(Tilt),
    };
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fovy = Fovy;
    camera.projection = CAMERA_PERSPECTIVE;

    BeginTextureMode(target);
    ClearBackground(BLANK);
    BeginMode3D(camera);

    // No lit shader, on purpose - see the note on ViewModel::DrawWeapon, which
    // draws every one of these models the same unlit way in the player's own
    // hands. A preview shaded differently from the weapon it is a preview OF
    // would read as a second, slightly wrong copy of it.
    DrawModel(model, Vector3Zero(), 1.0f, WHITE);

    EndMode3D();
    EndTextureMode();

    // Render textures come out upside down - see the identical note on
    // ViewModel::Composite.
    const Rectangle source = { 0.0f, 0.0f, (float)target.texture.width,
                               -(float)target.texture.height };

    DrawTexturePro(target.texture, source, dest, { 0.0f, 0.0f }, 0.0f, WHITE);
}
