#include "render/AnimatedModel.h"

#include "raymath.h"
#include "render/AssetManager.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

namespace
{
    // raylib resamples every glTF clip to a fixed rate when it loads it, so a
    // time in seconds converts to a keyframe index by multiplying by this
    constexpr float GltfFrameRate = 60.0f;

    // Matches MAX_BONE_NUM in assets/shaders/skinning.vs
    constexpr int MaxShaderBones = 128;

    std::string Lowered(const std::string &text)
    {
        std::string out = text;
        std::transform(out.begin(), out.end(), out.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });

        return out;
    }

    //------------------------------------------------------------------------------
    // Drops a clip's final keyframe, which is not animation.
    //
    // raylib resamples every glTF clip at a fixed rate and takes its last sample at
    // exactly the clip's duration. That lookup lands a hair past the last key, the
    // sampler reports failure, and those bones keep their default transform - so
    // the final frame of a fall is the body standing back up. A looping clip hides
    // it as a one frame hitch, but a clip that holds its end, like a death, holds
    // the wrong pose for as long as the corpse is there.
    //
    // Trimming it here rather than skipping it at playback keeps raylib's own frame
    // wrapping honest: it blends toward keyframeCount, so leaving the bad frame in
    // place would still bleed it into the end of every loop. The dropped pose is
    // freed exactly as UnloadModelAnimations would have freed it.
    //------------------------------------------------------------------------------
    void TrimFailedLastFrame(ModelAnimation &anim)
    {
        if (anim.keyframeCount < 2) return;

        Transform *dropped = anim.keyframePoses[anim.keyframeCount - 1];
        if (dropped != nullptr) MemFree(dropped);

        anim.keyframePoses[anim.keyframeCount - 1] = nullptr;
        anim.keyframeCount--;
    }

    //------------------------------------------------------------------------------
    // Reorders a clip file's poses into the model's own bone order, in place.
    //
    // UpdateModelAnimation reads anim.keyframePoses[frame][i] for bone i of the
    // *model*, so the two orders have to agree. They often do not: KayKit lists
    // the same 23 bones in one order in a character and another in the animation
    // library, and raylib 6.0 cannot catch it - ModelAnimation no longer carries
    // bone names, so IsModelAnimationValid compares bone counts and nothing else.
    // 23 == 23 passes, then every limb is skinned with another limb's motion.
    //
    // Matching is by name, which is why the clip file has to be loaded as a Model
    // too: its skeleton is the only place those names survive.
    //------------------------------------------------------------------------------
    bool RemapPosesToModel(const Model &model, const Model &clipRig, ModelAnimation *anims, int count)
    {
        const int bones = model.skeleton.boneCount;

        if ((clipRig.skeleton.boneCount != bones) || (clipRig.skeleton.bones == nullptr)) return false;

        // remap[i] is where the model's bone i sits in the clip file's order
        std::vector<int> remap(bones, -1);

        for (int i = 0; i < bones; i++)
        {
            for (int j = 0; j < bones; j++)
            {
                if (strncmp(model.skeleton.bones[i].name, clipRig.skeleton.bones[j].name, 32) == 0)
                {
                    remap[i] = j;
                    break;
                }
            }

            // A bone the clips have no motion for: better to reject the file than
            // to skin one joint from whatever happened to share its slot
            if (remap[i] < 0) return false;
        }

        bool identity = true;
        for (int i = 0; i < bones; i++)
        {
            if (remap[i] != i) { identity = false; break; }
        }

        if (identity) return true;

        std::vector<Transform> scratch(bones);

        for (int a = 0; a < count; a++)
        {
            if (anims[a].boneCount != bones) continue;

            for (int k = 0; k < anims[a].keyframeCount; k++)
            {
                Transform *pose = anims[a].keyframePoses[k];
                if (pose == nullptr) continue;

                for (int i = 0; i < bones; i++) scratch[i] = pose[remap[i]];
                for (int i = 0; i < bones; i++) pose[i] = scratch[i];
            }
        }

        return true;
    }
}

bool AnimatedModel::Load(AssetManager &assets, const std::string &modelPath,
                         const std::vector<std::string> &animPaths)
{
    Unload();

    if (!FileExists(AssetManager::Resolve(modelPath).c_str()))
    {
        TraceLog(LOG_WARNING, "ANIMMODEL: %s not found, falling back", modelPath.c_str());
        return false;
    }

    model = assets.GetModel(modelPath);

    if ((model.meshCount <= 0) || (model.skeleton.boneCount <= 0))
    {
        TraceLog(LOG_WARNING, "ANIMMODEL: %s has no rigged mesh (%i meshes, %i bones)",
                 modelPath.c_str(), model.meshCount, model.skeleton.boneCount);
        return false;
    }

    if (model.skeleton.boneCount > MaxShaderBones)
    {
        TraceLog(LOG_WARNING, "ANIMMODEL: %s has %i bones, shader holds %i - raise MAX_BONE_NUM",
                 modelPath.c_str(), model.skeleton.boneCount, MaxShaderBones);
    }

    // Every material, not just one: how many a KayKit character splits into is
    // its own business, and any that keeps the default shader draws in bind pose
    Shader &skinning = assets.GetShader("shaders/skinning.vs", "shaders/skinning.fs");

    if (skinning.locs[SHADER_LOC_MATRIX_BONETRANSFORMS] == -1)
    {
        TraceLog(LOG_WARNING, "ANIMMODEL: skinning shader has no boneMatrices uniform");
    }

    for (int i = 0; i < model.materialCount; i++) model.materials[i].shader = skinning;

    // Clips either ship inside the character file or come from the animation
    // library, which is built on the same rig and split across several files
    std::vector<std::string> sources = animPaths;
    if (sources.empty()) sources.push_back(modelPath);

    // Clip names are the one thing that cannot be guessed from here, so say what
    // arrived - a mismatch between these and the names the caller looks for is
    // the likeliest reason an enemy stands still
    TraceLog(LOG_INFO, "ANIMMODEL: %s - %i bones, %i meshes", modelPath.c_str(),
             model.skeleton.boneCount, model.meshCount);

    int valid = 0;

    for (const std::string &source : sources)
    {
        if (!FileExists(AssetManager::Resolve(source).c_str()))
        {
            TraceLog(LOG_WARNING, "ANIMMODEL: clip file %s not found", source.c_str());
            continue;
        }

        ClipFile file;
        file.anims = LoadModelAnimations(AssetManager::Resolve(source).c_str(), &file.count);

        if ((file.anims == nullptr) || (file.count <= 0))
        {
            TraceLog(LOG_WARNING, "ANIMMODEL: no animations in %s", source.c_str());
            continue;
        }

        // Loaded only for its skeleton, to line the clips up with ours by name
        Model clipRig = LoadModel(AssetManager::Resolve(source).c_str());
        const bool usable = RemapPosesToModel(model, clipRig, file.anims, file.count);
        UnloadModel(clipRig);

        if (!usable)
        {
            TraceLog(LOG_WARNING, "ANIMMODEL:   %s - bones do not line up with %s, skipped",
                     GetFileName(source.c_str()), GetFileName(modelPath.c_str()));

            UnloadModelAnimations(file.anims, file.count);
            continue;
        }

        files.push_back(file);

        for (int i = 0; i < file.count; i++)
        {
            TrimFailedLastFrame(file.anims[i]);
            clips.push_back(&file.anims[i]);
        }

        valid += file.count;

        TraceLog(LOG_INFO, "ANIMMODEL:   %s - %i clips", GetFileName(source.c_str()), file.count);
    }

    if (valid == 0)
    {
        TraceLog(LOG_WARNING, "ANIMMODEL: no clip matches the skeleton of %s", modelPath.c_str());
        Unload();
        return false;
    }

    const BoundingBox box = GetModelBoundingBox(model);
    bindHeight = box.max.y - box.min.y;

    ready = true;

    return true;
}

void AnimatedModel::Unload()
{
    // The Model and its shader belong to the AssetManager; only the clips are ours
    for (ClipFile &file : files)
    {
        if (file.anims != nullptr) UnloadModelAnimations(file.anims, file.count);
    }

    files.clear();
    clips.clear();
    bindHeight = 0.0f;
    ready = false;
    model = Model{};
}

int AnimatedModel::FindClip(const std::vector<std::string> &candidates) const
{
    if (!ready) return -1;

    for (const std::string &candidate : candidates)
    {
        const std::string wanted = Lowered(candidate);

        for (size_t i = 0; i < clips.size(); i++)
        {
            if (!IsModelAnimationValid(model, *clips[i])) continue;

            if (Lowered(clips[i]->name).find(wanted) != std::string::npos) return (int)i;
        }
    }

    return -1;
}

float AnimatedModel::ClipDuration(int clip) const
{
    if (!ready || (clip < 0) || (clip >= (int)clips.size())) return 0.0f;

    return (float)clips[clip]->keyframeCount/GltfFrameRate;
}

int AnimatedModel::FindBone(const std::string &name) const
{
    if (!ready || (model.skeleton.bones == nullptr)) return -1;

    for (int i = 0; i < model.skeleton.boneCount; i++)
    {
        if (name == model.skeleton.bones[i].name) return i;
    }

    return -1;
}

Matrix AnimatedModel::BoneTransform(int bone, const Matrix *bones) const
{
    if (!ready || (bones == nullptr) || (bone < 0) || (bone >= model.skeleton.boneCount))
    {
        return MatrixIdentity();
    }

    // bones[i] is inverse(bind) combined with the posed transform, so putting the
    // bind pose back leaves the bone's own transform - the same recovery
    // Ragdoll::Begin does to find where a clip left the skeleton
    const Transform &bind = model.skeleton.bindPose[bone];
    const Matrix bindMatrix = MatrixMultiply(MatrixMultiply(MatrixScale(bind.scale.x, bind.scale.y, bind.scale.z),
                                                            QuaternionToMatrix(bind.rotation)),
                                             MatrixTranslate(bind.translation.x, bind.translation.y, bind.translation.z));

    return MatrixMultiply(bindMatrix, bones[bone]);
}

bool AnimatedModel::Playable(int clip) const
{
    return ready && (clip >= 0) && (clip < (int)clips.size()) && (clips[clip]->keyframeCount > 0);
}

float AnimatedModel::FrameFor(int clip, float time, bool loop) const
{
    const ModelAnimation &anim = *clips[clip];

    const float last = (float)(anim.keyframeCount - 1);
    float frame = time*GltfFrameRate;

    if (loop) frame = fmodf(fmaxf(frame, 0.0f), (float)anim.keyframeCount);
    else frame = fminf(fmaxf(frame, 0.0f), last);

    return frame;
}

void AnimatedModel::Pose(int clip, float time, bool loop, Matrix *bones) const
{
    if (!Playable(clip) || (bones == nullptr)) return;

    // UpdateModelAnimation takes the Model by value and writes through
    // boneMatrices, so pointing a copy at the caller's array poses that array
    // and leaves the shared Model alone
    Model posed = model;
    posed.boneMatrices = bones;

    UpdateModelAnimation(posed, *clips[clip], FrameFor(clip, time, loop));
}

void AnimatedModel::PoseBlended(int fromClip, float fromTime, bool fromLoop,
                                int toClip, float toTime, bool toLoop,
                                float blend, Matrix *bones) const
{
    if (!ready || (bones == nullptr)) return;

    // Nothing to fade between: show whichever side exists
    if (!Playable(fromClip)) { Pose(toClip, toTime, toLoop, bones); return; }
    if (!Playable(toClip)) { Pose(fromClip, fromTime, fromLoop, bones); return; }

    Model posed = model;
    posed.boneMatrices = bones;

    UpdateModelAnimationEx(posed,
                           *clips[fromClip], FrameFor(fromClip, fromTime, fromLoop),
                           *clips[toClip], FrameFor(toClip, toTime, toLoop),
                           Clamp(blend, 0.0f, 1.0f));
}

void AnimatedModel::Draw(const Matrix *bones, Vector3 position, float yawRadians, float scale, Color tint) const
{
    if (!ready || (bones == nullptr)) return;

    Model posed = model;
    posed.boneMatrices = const_cast<Matrix *>(bones);

    DrawModelEx(posed, position, { 0.0f, 1.0f, 0.0f }, yawRadians*RAD2DEG,
                { scale, scale, scale }, tint);
}

float AnimatedModel::FitScaleFor(float targetHeight) const
{
    if (!ready || (bindHeight <= 1e-4f)) return 1.0f;

    return targetHeight/bindHeight;
}
