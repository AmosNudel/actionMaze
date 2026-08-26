#include "render/Sky.h"

#include "core/Config.h"
#include "render/AssetManager.h"
#include "rlgl.h"

#include <string>

void Sky::Load(AssetManager &assets, const char *cubemapPath)
{
    const std::string resolved = AssetManager::Resolve(cubemapPath);

    if (!FileExists(resolved.c_str()))
    {
        TraceLog(LOG_WARNING, "SKY: %s missing - falling back to a flat background",
                 cubemapPath);
        return;
    }

    box = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));

    Shader &shader = assets.GetShader("shaders/skybox.vs", "shaders/skybox.fs");
    box.materials[0].shader = shader;

    // Which sampler the fragment shader reads. MATERIAL_MAP_CUBEMAP is a slot
    // index, not a texture id - it says "the cubemap this material carries".
    const int cubemapSlot = MATERIAL_MAP_CUBEMAP;

    // Both belong to the HDR path. This is a plain 8-bit PNG: it is already in
    // the space the screen wants, and it is the right way up as authored, so
    // correcting either would be undoing work nobody did.
    const int off = 0;

    SetShaderValue(shader, GetShaderLocation(shader, "environmentMap"),
                   &cubemapSlot, SHADER_UNIFORM_INT);
    SetShaderValue(shader, GetShaderLocation(shader, "doGamma"), &off, SHADER_UNIFORM_INT);
    SetShaderValue(shader, GetShaderLocation(shader, "vflipped"), &off, SHADER_UNIFORM_INT);

    // Loaded as an Image rather than a Texture: the six faces have to be cut out
    // of the cross on the CPU before any of it reaches the GPU as a cubemap
    Image image = LoadImage(resolved.c_str());

    // Stated rather than auto-detected. AUTO_DETECT infers the layout from the
    // aspect ratio, which for a 4:3 image is right - but it would also silently
    // pick something else the day the art is replaced, and a skybox assembled
    // wrongly looks like a bug in the renderer rather than a mismatched asset.
    box.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture =
        LoadTextureCubemap(image, CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE);

    UnloadImage(image);

    ready = true;

    TraceLog(LOG_INFO, "SKY: cubemap ready from %s", cubemapPath);
}

void Sky::Unload()
{
    if (!ready) return;

    // Built here rather than fetched from the AssetManager, so it is this one's to
    // release. UnloadModel deliberately leaves material textures alone.
    UnloadTexture(box.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture);
    UnloadModel(box);

    ready = false;
}

void Sky::Draw() const
{
    if (!ready) return;

    // Seen from the inside, so the faces turned toward us are the ones raylib
    // culls by default
    rlDisableBackfaceCulling();

    // Passes the depth test against the cleared buffer but writes nothing, so
    // every piece of the level drawn afterwards covers it
    rlDisableDepthMask();

    // Origin and unit scale on purpose: skybox.vs drops the translation from the
    // view matrix, which puts the box around the camera wherever the camera is
    DrawModel(box, { 0.0f, 0.0f, 0.0f }, 1.0f, WHITE);

    rlEnableDepthMask();
    rlEnableBackfaceCulling();
}
