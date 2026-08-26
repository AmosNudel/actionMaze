#include "render/Ragdoll.h"

#include "raymath.h"

#include <cmath>

namespace
{
    // Verlet needs a little velocity loss or the body jitters forever
    constexpr float Damping = 0.97f;
    // Enough passes to keep limbs attached without the solve showing up in a profile
    constexpr int SolverIterations = 8;
    // How much horizontal speed a bone loses while dragging on the floor
    constexpr float GroundFriction = 0.6f;
    //--------------------------------------------------------------------------
    // How far past its bind length a link has to arrive before the ragdoll
    // believes the animation meant it.
    //
    // KayKit's death clips throw `handslot` clear of the hand over their second
    // half - the skeleton drops its weapon as it dies - so that link arrives at
    // the handover 5.8x its bind length. Snapping it back is a hard one-frame
    // teleport of whatever was being held, straight back into the corpse's fist:
    // measured at 0.42 world units in 1/60s on a Warrior's blade.
    //
    // Every other link arrives at exactly 1.0x, so this is a wide margin around
    // a bimodal measurement rather than a number needing tuning. A link that
    // arrives *shorter* than its bind length is a bent joint, not a break, and
    // keeps the bind length as it always did.
    //--------------------------------------------------------------------------
    constexpr float ThrownStretch = 1.5f;

    Matrix TransformMatrix(Vector3 scale, Quaternion rotation, Vector3 translation)
    {
        // Same order raylib builds bone matrices in, so ours compose with its bind pose
        return MatrixMultiply(MatrixMultiply(MatrixScale(scale.x, scale.y, scale.z),
                                             QuaternionToMatrix(rotation)),
                              MatrixTranslate(translation.x, translation.y, translation.z));
    }
}

void Ragdoll::Begin(const ModelSkeleton &skeleton, const Matrix *bones, Vector3 impulse)
{
    Clear();

    const int count = skeleton.boneCount;
    if ((count <= 0) || (skeleton.bones == nullptr) || (skeleton.bindPose == nullptr) || (bones == nullptr)) return;

    particles.resize(count);
    parent.resize(count);
    primaryChild.assign(count, -1);
    bindPosition.resize(count);
    bindRotation.resize(count);
    bindScale.resize(count);
    invBind.resize(count);

    for (int i = 0; i < count; i++)
    {
        parent[i] = skeleton.bones[i].parent;

        const Transform &bind = skeleton.bindPose[i];
        bindPosition[i] = bind.translation;
        bindRotation[i] = bind.rotation;
        bindScale[i] = bind.scale;

        const Matrix bindMatrix = TransformMatrix(bind.scale, bind.rotation, bind.translation);
        invBind[i] = MatrixInvert(bindMatrix);

        // bones[i] is inverse(bind) combined with the posed transform, so putting
        // the bind pose back recovers where the animation actually left this bone
        Vector3 posed{};
        Quaternion ignoredRotation{};
        Vector3 ignoredScale{};
        MatrixDecompose(MatrixMultiply(bindMatrix, bones[i]), &posed, &ignoredRotation, &ignoredScale);

        particles[i].position = posed;
        particles[i].previous = Vector3Subtract(posed, impulse);
    }

    // What a link should hold two bones at. The bind pose, because that is what a
    // bone length always is - except where the clip we are taking over from has
    // already pulled it well past that, which is the animation saying the joint
    // has come apart on purpose. Preserving what arrived is what stops a dropped
    // weapon being sucked back into the hand on the first physics frame.
    auto RestFor = [&](int a, int b)
    {
        const float bind = Vector3Distance(bindPosition[a], bindPosition[b]);
        const float arrived = Vector3Distance(particles[a].position, particles[b].position);

        return (arrived > bind*ThrownStretch) ? arrived : bind;
    };

    // Bone lengths, measured in the bind pose because that is what they always are
    // - unless the clip handing over deliberately says otherwise, see RestFor
    for (int i = 0; i < count; i++)
    {
        const int p = parent[i];
        if ((p < 0) || (p >= count)) continue;

        Link bone;
        bone.a = i;
        bone.b = p;
        bone.rest = RestFor(i, p);
        links.push_back(bone);

        // Bracing a bone to its grandparent is what stops the joint between them
        // folding flat - the cheap stand-in for an angle limit
        const int g = parent[p];
        if ((g < 0) || (g >= count)) continue;

        Link brace;
        brace.a = i;
        brace.b = g;
        brace.rest = RestFor(i, g);
        links.push_back(brace);
    }

    // The bone each joint takes its direction from. Longest child, so the hips
    // follow the spine rather than a leg and the mesh twists less.
    for (int i = 0; i < count; i++)
    {
        const int p = parent[i];
        if ((p < 0) || (p >= count)) continue;

        const float reach = Vector3Distance(bindPosition[i], bindPosition[p]);
        const int held = primaryChild[p];

        if ((held < 0) || (reach > Vector3Distance(bindPosition[held], bindPosition[p])))
        {
            primaryChild[p] = i;
        }
    }

    active = true;
}

void Ragdoll::Update(float delta, float gravity, float floorY)
{
    if (!active || (delta <= 0.0f)) return;

    const float fall = gravity*delta*delta;

    for (Particle &particle : particles)
    {
        const Vector3 velocity = Vector3Scale(Vector3Subtract(particle.position, particle.previous), Damping);

        particle.previous = particle.position;
        particle.position = Vector3Add(particle.position, velocity);
        particle.position.y -= fall;
    }

    for (int pass = 0; pass < SolverIterations; pass++)
    {
        for (const Link &link : links)
        {
            Particle &a = particles[link.a];
            Particle &b = particles[link.b];

            const Vector3 delta3 = Vector3Subtract(b.position, a.position);
            const float distance = Vector3Length(delta3);

            if (distance < 1e-5f) continue;

            // Half the error each way: neither bone is more anchored than the other
            const float correction = (distance - link.rest)/distance*0.5f;
            const Vector3 shift = Vector3Scale(delta3, correction);

            a.position = Vector3Add(a.position, shift);
            b.position = Vector3Subtract(b.position, shift);
        }

        for (Particle &particle : particles)
        {
            if (particle.position.y >= floorY) continue;

            particle.position.y = floorY;

            // Scrape rather than slide: bleed off the horizontal step as well
            particle.previous.x += (particle.position.x - particle.previous.x)*GroundFriction;
            particle.previous.z += (particle.position.z - particle.previous.z)*GroundFriction;
            particle.previous.y = particle.position.y;
        }
    }
}

void Ragdoll::WriteBones(Matrix *bones) const
{
    if (!active || (bones == nullptr)) return;

    const int count = (int)particles.size();

    // Each bone turns by whatever rotation carries its bind direction onto the one
    // its particles now describe. Only a swing, no twist - a corpse does not need it.
    std::vector<Quaternion> swing(count, QuaternionIdentity());

    for (int i = 0; i < count; i++)
    {
        const int child = primaryChild[i];
        if (child < 0) continue;

        const Vector3 bindDirection = Vector3Normalize(Vector3Subtract(bindPosition[child], bindPosition[i]));
        const Vector3 nowDirection = Vector3Normalize(Vector3Subtract(particles[child].position, particles[i].position));

        if ((Vector3Length(bindDirection) < 1e-5f) || (Vector3Length(nowDirection) < 1e-5f)) continue;

        swing[i] = QuaternionFromVector3ToVector3(bindDirection, nowDirection);
    }

    // A leaf has no direction of its own, so it keeps its parent's - which is
    // already solved, because a parent always has this bone as a child
    for (int i = 0; i < count; i++)
    {
        if (primaryChild[i] >= 0) continue;

        const int p = parent[i];
        if ((p >= 0) && (p < count)) swing[i] = swing[p];
    }

    for (int i = 0; i < count; i++)
    {
        const Quaternion rotation = QuaternionMultiply(swing[i], bindRotation[i]);
        const Matrix posed = TransformMatrix(bindScale[i], rotation, particles[i].position);

        bones[i] = MatrixMultiply(invBind[i], posed);
    }
}

void Ragdoll::Clear()
{
    particles.clear();
    links.clear();
    parent.clear();
    primaryChild.clear();
    bindPosition.clear();
    bindRotation.clear();
    bindScale.clear();
    invBind.clear();
    active = false;
}
