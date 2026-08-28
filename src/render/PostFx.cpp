#include "render/PostFx.h"

#include "core/Config.h"
#include "render/AssetManager.h"

#include <cmath>
#include <string>

namespace
{
    // The window scaled and rounded, with a floor of one pixel so a minimised
    // window cannot ask for a zero-sized target
    int Scaled(int pixels)
    {
        const int out = (int)(pixels*Config::PostRenderScale + 0.5f);

        return (out < 1) ? 1 : out;
    }
}

int PostBufferWidth()  { return Scaled(GetScreenWidth()); }
int PostBufferHeight() { return Scaled(GetScreenHeight()); }

void PostFx::Load(AssetManager &assets)
{
    const std::string resolved = AssetManager::Resolve("shaders/post.fs");

    if (!FileExists(resolved.c_str()))
    {
        TraceLog(LOG_WARNING, "POSTFX: shaders/post.fs missing - the world goes to the screen ungraded");
        return;
    }

    // No vertex shader of its own: this is a screen-filling quad drawn by
    // DrawTexturePro, and raylib's default vertex stage already hands the
    // fragment stage the texture coordinate and the tint it needs.
    shader = assets.GetShader("", "shaders/post.fs");

    texelLoc = GetShaderLocation(shader, "texelSize");
    hurtLoc  = GetShaderLocation(shader, "hurt");
    pulseLoc = GetShaderLocation(shader, "pulse");

    ready = true;

    TraceLog(LOG_INFO, "POSTFX: ready, rendering the world at %.2fx", Config::PostRenderScale);
}

void PostFx::Unload()
{
    if (scene.id != 0)
    {
        UnloadRenderTexture(scene);
        scene = { 0 };
    }

    // The shader is the AssetManager's - see the note on the member
    ready = false;
}

//----------------------------------------------------------------------------------
// The buffer, built on demand and rebuilt when the window changes shape. Cheap to
// check every frame, and it means fullscreen being toggled mid-run does not leave
// the world stretched across a target the wrong size.
//----------------------------------------------------------------------------------
void PostFx::Resize()
{
    const int width = PostBufferWidth();
    const int height = PostBufferHeight();

    if ((scene.id != 0) && (scene.texture.width == width) && (scene.texture.height == height)) return;

    if (scene.id != 0) UnloadRenderTexture(scene);

    scene = LoadRenderTexture(width, height);

    // The whole point of rendering big. Point sampling would throw three of every
    // four rendered pixels away and the supersample with them - see the class note.
    SetTextureFilter(scene.texture, TEXTURE_FILTER_BILINEAR);
}

void PostFx::BeginPass()
{
    Resize();

    BeginTextureMode(scene);
}

void PostFx::EndPass()
{
    EndTextureMode();
}

void PostFx::Draw(float hurt, float time) const
{
    if (scene.id == 0) return;

    // Render textures come out upside down, hence the negative height - the same
    // flip ViewModel::Composite does
    const Rectangle source = { 0.0f, 0.0f,
                               (float)scene.texture.width, -(float)scene.texture.height };

    const Rectangle dest = { 0.0f, 0.0f, (float)GetScreenWidth(), (float)GetScreenHeight() };

    if (ready)
    {
        //--------------------------------------------------------------------------
        // The quad is the size of the WINDOW, so the shader runs once per finished
        // pixel rather than once per rendered one. Every tap it takes is against
        // the big buffer, which is what makes the supersample free: the centre
        // sample resolves it and the glow taps come along for the ride.
        //--------------------------------------------------------------------------
        const float texel[2] = { 1.0f/(float)scene.texture.width,
                                 1.0f/(float)scene.texture.height };

        const float pulse = 0.5f + 0.5f*sinf(time*Config::HurtPulseRate);

        SetShaderValue(shader, texelLoc, texel, SHADER_UNIFORM_VEC2);
        SetShaderValue(shader, hurtLoc, &hurt, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, pulseLoc, &pulse, SHADER_UNIFORM_FLOAT);

        BeginShaderMode(shader);
    }

    DrawTexturePro(scene.texture, source, dest, { 0.0f, 0.0f }, 0.0f, WHITE);

    if (ready) EndShaderMode();
}
