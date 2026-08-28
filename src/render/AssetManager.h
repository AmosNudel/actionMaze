#pragma once

#include "raylib.h"

#include <string>
#include <unordered_map>

//----------------------------------------------------------------------------------
// Loads each asset once and hands out references to it.
//
// Paths are relative to Config::AssetDir, e.g. GetTexture("textures/wall.png").
// References stay valid for the lifetime of the manager (unordered_map never
// moves its values), so systems can cache what they get back.
//
// UnloadAll() must run BEFORE CloseWindow() - GPU resources need a live context.
//----------------------------------------------------------------------------------
class AssetManager
{
public:
    Texture2D &GetTexture(const std::string &path);

    //------------------------------------------------------------------------------
    // A texture this manager did not load from a file.
    //
    // Some art is cheaper to generate than to ship - a soft round light is one
    // falloff curve, and a PNG of it is a file that can silently disagree with the
    // constant that shapes it. Generated or not, it is still a GPU resource that
    // has to be released before the context goes, and this is where that already
    // happens for everything else.
    //
    // Keyed by a NAME rather than a path. Nothing on disk answers to it, and
    // GetTexture must not be used for one - it would try to load the name as a
    // file, fail, and cache the failure.
    //
    // Find returns null when nothing has been adopted under `name` yet. Adopt takes
    // ownership; adopting twice under one name releases the first.
    //------------------------------------------------------------------------------
    Texture2D *FindTexture(const std::string &name);
    Texture2D &AdoptTexture(const std::string &name, Texture2D texture);

    //------------------------------------------------------------------------------
    // A model, optionally rebound onto a texture shared with every other model that
    // names the same one.
    //
    // glTF names its texture by relative URI, so raylib loads a private copy per
    // model - and a pack whose 200 pieces all sit on one 1024x1024 atlas would load
    // that atlas 200 times, 800MB of the same pixels. Naming `sharedTexturePath`
    // swaps each material onto a single cached copy and frees the private one.
    //
    // Safe because UnloadModel frees a material's maps array but deliberately not
    // the textures in it - raylib leaves those to the caller precisely so they can
    // be shared. The shared copy is owned by `textures` and released by UnloadAll.
    //------------------------------------------------------------------------------
    Model &GetModel(const std::string &path, const std::string &sharedTexturePath = "");

    //------------------------------------------------------------------------------
    // The animation clips in a file, parsed once however many rigs want them.
    //
    // KayKit ships its clip library split across a handful of files that every
    // character shares, and parsing a glTF is by far the most expensive thing this
    // project does at load: six enemy archetypes times four clip files was
    // twenty-four parses of four files, and about two thirds of the whole run
    // load. Cached here it is four.
    //
    // What comes back is READ ONLY and shared. Clips have to be reordered into
    // each rig's own bone order before they can be played (see RemapPosesToModel),
    // and that reorder differs per character - so a caller takes a private copy and
    // permutes that. Copying pose arrays is a memcpy; parsing the file again is
    // not, and that is the whole saving.
    //
    // Returns null and leaves `count` at zero when the file holds no animations.
    //------------------------------------------------------------------------------
    const ModelAnimation *GetAnimations(const std::string &path, int *count);
    Sound &GetSound(const std::string &path);
    // Either path may be empty to use raylib's default vertex/fragment stage
    Shader &GetShader(const std::string &vsPath, const std::string &fsPath);

    void UnloadAll();

    // Turns an asset relative path into a path usable from the working directory
    static std::string Resolve(const std::string &path);

private:
    // One file's clips, as parsed. Owned here and freed by UnloadAll.
    struct Animations
    {
        ModelAnimation *anims = nullptr;
        int count = 0;
    };

    std::unordered_map<std::string, Texture2D> textures;
    std::unordered_map<std::string, Model> models;
    std::unordered_map<std::string, Animations> animations;
    std::unordered_map<std::string, Sound> sounds;
    std::unordered_map<std::string, Shader> shaders;
};
