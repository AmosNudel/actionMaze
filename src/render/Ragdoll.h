#pragma once

#include "raylib.h"

#include <vector>

//----------------------------------------------------------------------------------
// A corpse settling under gravity, taking over from wherever an animation left it.
//
// raylib ships no physics, so this is a small verlet solver: one particle per bone,
// distance links along the skeleton to hold bone lengths, and a floor plane. Bones
// are simulated, not bodies - there are no collision volumes and nothing here talks
// to the level. It is scenery, and Enemy::IsAlive() is already false by the time it
// runs, so nothing it does can affect play.
//
// It works in the model's own space, which is where bone transforms live: raylib
// flattens glTF bind and keyframe poses to model space with cgltf_node_transform_world,
// so a bone's global transform is available without walking the hierarchy, and the
// feet sit at y = 0 because that is where Enemy::body.position draws from. Gravity
// must therefore be passed in model units - world gravity over the model scale.
//
// Angles are held only by the extra grandparent links Begin() builds. Without them a
// chain of distance constraints is a noodle and the body folds flat.
//----------------------------------------------------------------------------------
class Ragdoll
{
public:
    // Takes over from an animated pose. `bones` is what AnimatedModel::Pose wrote:
    // bone i's global transform is its bind pose combined with bones[i].
    // `impulse` is a per-bone nudge in model units, zero to simply let it drop.
    void Begin(const ModelSkeleton &skeleton, const Matrix *bones, Vector3 impulse);

    void Update(float delta, float gravity, float floorY);

    // Turns the settled particles back into bone matrices the skinning shader wants
    void WriteBones(Matrix *bones) const;

    void Clear();
    bool Active() const { return active; }

private:
    struct Particle
    {
        Vector3 position{};
        Vector3 previous{};     // Verlet keeps velocity as the gap to the last step
    };

    // A bone length, or a grandparent brace standing in for an angle limit
    struct Link
    {
        int a = 0;
        int b = 0;
        float rest = 0.0f;
    };

    std::vector<Particle> particles;    // One per bone, at the bone's origin
    std::vector<Link> links;
    std::vector<int> parent;
    std::vector<int> primaryChild;      // Longest child bone, -1 at a leaf
    std::vector<Vector3> bindPosition;
    std::vector<Quaternion> bindRotation;
    std::vector<Vector3> bindScale;
    std::vector<Matrix> invBind;        // Cached: WriteBones runs every frame
    bool active = false;
};
