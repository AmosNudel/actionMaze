#include "render/AssetManager.h"

#include "core/Config.h"
#include "rlgl.h"       // rlGetTextureIdDefault, to tell raylib's white 1x1 apart

std::string AssetManager::Resolve(const std::string &path)
{
    return std::string(Config::AssetDir) + path;
}

Texture2D &AssetManager::GetTexture(const std::string &path)
{
    auto found = textures.find(path);
    if (found != textures.end()) return found->second;

    return textures.emplace(path, LoadTexture(Resolve(path).c_str())).first->second;
}

Texture2D *AssetManager::FindTexture(const std::string &name)
{
    auto found = textures.find(name);

    return (found != textures.end()) ? &found->second : nullptr;
}

Texture2D &AssetManager::AdoptTexture(const std::string &name, Texture2D texture)
{
    auto found = textures.find(name);

    if (found != textures.end())
    {
        // Replacing rather than adding. Leaving the old one on the GPU would be a
        // leak with no way to reach it - the map is the only handle there is.
        UnloadTexture(found->second);
        found->second = texture;

        return found->second;
    }

    return textures.emplace(name, texture).first->second;
}

Model &AssetManager::GetModel(const std::string &path, const std::string &sharedTexturePath)
{
    auto found = models.find(path);
    if (found != models.end()) return found->second;

    Model &model = models.emplace(path, LoadModel(Resolve(path).c_str())).first->second;

    if (sharedTexturePath.empty()) return model;

    // Fetched before the swap loop so the map is not rehashed mid-iteration, and
    // by reference because the cached texture is what every model must end up
    // pointing at - a copy would be the same GPU id but a different owner to reason
    // about later
    const Texture2D &shared = GetTexture(sharedTexturePath);

    for (int i = 0; i < model.materialCount; i++)
    {
        Texture2D &slot = model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture;

        // The default 1x1 white belongs to raylib and outlives every model, so it
        // is the one id that must never be handed to UnloadTexture
        if ((slot.id != shared.id) && (slot.id != rlGetTextureIdDefault())) UnloadTexture(slot);

        slot = shared;
    }

    return model;
}

Sound &AssetManager::GetSound(const std::string &path)
{
    auto found = sounds.find(path);
    if (found != sounds.end()) return found->second;

    return sounds.emplace(path, LoadSound(Resolve(path).c_str())).first->second;
}

Shader &AssetManager::GetShader(const std::string &vsPath, const std::string &fsPath)
{
    const std::string key = vsPath + "|" + fsPath;

    auto found = shaders.find(key);
    if (found != shaders.end()) return found->second;

    const std::string vs = Resolve(vsPath);
    const std::string fs = Resolve(fsPath);
    Shader shader = LoadShader(vsPath.empty() ? 0 : vs.c_str(), fsPath.empty() ? 0 : fs.c_str());

    return shaders.emplace(key, shader).first->second;
}

//----------------------------------------------------------------------------------
// A clip file's animations, parsed once - see the note on the declaration.
//
// Cached even when the file turns out to hold nothing, so a missing or animation
// free file is parsed once and answered from the map every time after. A null that
// had to be re-derived would put the slowest call in the project on the path that
// gets nothing out of it.
//----------------------------------------------------------------------------------
const ModelAnimation *AssetManager::GetAnimations(const std::string &path, int *count)
{
    auto found = animations.find(path);

    if (found == animations.end())
    {
        Animations loaded;
        loaded.anims = LoadModelAnimations(Resolve(path).c_str(), &loaded.count);

        if (loaded.anims == nullptr) loaded.count = 0;

        found = animations.emplace(path, loaded).first;
    }

    if (count != nullptr) *count = found->second.count;

    return found->second.anims;
}

void AssetManager::UnloadAll()
{
    for (auto &entry : textures) UnloadTexture(entry.second);
    for (auto &entry : models) UnloadModel(entry.second);
    for (auto &entry : sounds) UnloadSound(entry.second);
    for (auto &entry : shaders) UnloadShader(entry.second);

    // The cached originals. Every rig that played these holds its own permuted
    // copy and frees that itself - see AnimatedModel::Unload.
    for (auto &entry : animations)
    {
        if (entry.second.anims != nullptr) UnloadModelAnimations(entry.second.anims, entry.second.count);
    }

    textures.clear();
    models.clear();
    sounds.clear();
    shaders.clear();
    animations.clear();
}
