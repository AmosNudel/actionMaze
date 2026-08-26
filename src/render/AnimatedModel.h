#pragma once

#include "raylib.h"

#include <string>
#include <vector>

class AssetManager;

//----------------------------------------------------------------------------------
// A rigged glTF character, loaded once and drawn many times in different poses.
//
// raylib is built here with SUPPORT_GPU_SKINNING, so skinning happens in the
// vertex shader rather than by rewriting vertex buffers on the CPU. That is what
// makes one shared Model enough for a crowd: DrawModel reads the pose out of
// Model::boneMatrices at draw time, so a caller keeps its own Matrix array per
// instance and this class swaps the pointer in for the duration of the call.
//
// Without GPU skinning the pose would live in the model's vertex buffers and
// every enemy sharing the Model would move as one - hence the local libraylib.a
// in lib/, see the Makefile.
//
// Poses come from Pose(), which the owner calls while updating; Draw() only
// reads what Pose() produced, so drawing stays const.
//
// Clips can come from several files at once. KayKit splits its animation library
// by category - idle and death in one file, running in another, the swing in a
// third - so the clips one enemy needs are never in one place. They are loaded
// separately and indexed as one flat list, which is why FindClip is by name.
//----------------------------------------------------------------------------------
class AnimatedModel
{
public:
    // `animPaths` is empty when the clips ship inside the model file itself.
    // Clips whose skeleton does not match the model are loaded but never returned
    // by FindClip. Returns false, having logged why, if nothing usable loaded.
    bool Load(AssetManager &assets, const std::string &modelPath,
              const std::vector<std::string> &animPaths = {});
    void Unload();

    bool Ready() const { return ready; }
    int BoneCount() const { return ready ? model.skeleton.boneCount : 0; }

    // Bone names, parents and the bind pose, all in model space. What anything
    // generating its own poses - Ragdoll - needs to work from.
    const ModelSkeleton &Skeleton() const { return model.skeleton; }

    // Index of a bone by name, -1 if the rig has no such bone.
    //
    // Always resolve attachment points this way. Bone *order* differs between
    // characters in the same pack - Skeleton_Minion has upperleg.r at index 2
    // where Skeleton_Warrior has spine - so an index is only ever valid for the
    // model it was looked up against.
    int FindBone(const std::string &name) const;

    // Where a bone has ended up, in model space, for the pose in `bones`. Compose
    // it with the owner's scale, facing and position to hang something off it.
    Matrix BoneTransform(int bone, const Matrix *bones) const;

    // First clip whose name contains one of `candidates`, matched case
    // insensitively and tried in order, so a caller can list its fallbacks
    // from most to least specific. -1 when none of them hit.
    int FindClip(const std::vector<std::string> &candidates) const;

    // Seconds of *playable* clip - see PlayableFrames. 0 if the clip does not exist.
    float ClipDuration(int clip) const;

    // Writes the pose of `clip` at `time` seconds into `bones`, which must hold
    // BoneCount() matrices. A clip that does not loop holds its last frame.
    void Pose(int clip, float time, bool loop, Matrix *bones) const;

    // Cross-fade of two clips, each at its own time: `blend` 0 is all of
    // `fromClip`, 1 all of `toClip`. Either side missing falls back to the other,
    // so a pack without one of the clips cuts rather than fails.
    void PoseBlended(int fromClip, float fromTime, bool fromLoop,
                     int toClip, float toTime, bool toLoop,
                     float blend, Matrix *bones) const;

    void Draw(const Matrix *bones, Vector3 position, float yawRadians, float scale, Color tint) const;

    // Scale that makes the model stand `targetHeight` tall
    float FitScaleFor(float targetHeight) const;

private:
    // One LoadModelAnimations call: kept whole because UnloadModelAnimations
    // frees the array it was given, not individual clips
    struct ClipFile
    {
        ModelAnimation *anims = nullptr;
        int count = 0;
    };

    // Time in seconds to a keyframe index, wrapped or clamped to the clip
    float FrameFor(int clip, float time, bool loop) const;
    bool Playable(int clip) const;

    Model model{};                      // Owned by the AssetManager, not by us
    std::vector<ClipFile> files;        // Owned here
    std::vector<ModelAnimation *> clips; // Flat view over `files`, index = clip id
    float bindHeight = 0.0f;            // Model height in its own units, bind pose
    bool ready = false;
};
