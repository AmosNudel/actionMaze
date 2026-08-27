#include "combat/Projectile.h"

#include "combat/Collider.h"
#include "core/Config.h"
#include "entities/Enemy.h"
#include "entities/Player.h"
#include "raymath.h"
#include "render/AssetManager.h"
#include "render/Glow.h"
#include "render/Vfx.h"
#include "rlgl.h"
#include "world/Level.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr char ArrowPath[] = "models/enemies/props/Skeleton_Arrow.gltf";

    // Which way Skeleton_Arrow's nose points in its own space. Its length runs
    // along Y - the box is 0.117 x 0.749 x 0.102 - but the box cannot say which
    // end is the point, and the two ends are nearly symmetric. It is -Y: authored
    // nose-down, so +Y flies it fletching first.
    //
    // Note this disagrees with every weapon the player holds, which are modelled
    // tip-up at +Y. That is why the axis rides on ProjectileLook rather than
    // living here as the one true answer.
    constexpr Vector3 ArrowAxis = { 0.0f, -1.0f, 0.0f };

}

void ProjectileManager::Load(AssetManager &assets)
{
    arrow = nullptr;

    // Before the early returns below: an arrow model that is missing is no reason
    // for the player's magic to be invisible too. Shared with the portal and owned
    // by the AssetManager, so there is nothing here to release.
    glow = &GlowTexture(assets);

    if (!FileExists(AssetManager::Resolve(ArrowPath).c_str()))
    {
        TraceLog(LOG_WARNING, "PROJECTILES: %s not found, shots draw as lines", ArrowPath);
        return;
    }

    Model &model = assets.GetModel(ArrowPath);
    if (model.meshCount <= 0)
    {
        TraceLog(LOG_WARNING, "PROJECTILES: %s has no mesh", ArrowPath);
        return;
    }

    arrow = &model;
}

void ProjectileManager::Spawn(Vector3 from, Vector3 direction, float speed, int damage,
                              ProjectileSide side, ProjectileLook look, bool crit)
{
    const float length = Vector3Length(direction);
    if (length < 1e-4f) return;

    // Resolve the default here rather than at every call site: a shot with no
    // model of its own is an arrow, and an arrow has its own nose axis.
    //
    // A mote is not short of a model, it HAS none - so it is excluded rather than
    // handed the arrow, which is what the placeholder used to be
    if (look.model == nullptr && look.magic == nullptr)
    {
        look.model = arrow;
        look.scale = Config::ProjectileDrawScale;
        look.axis = ArrowAxis;
    }

    Shot shot;
    shot.position = from;
    shot.origin = from;
    shot.velocity = Vector3Scale(direction, speed/length);
    shot.life = Config::ProjectileLife;
    shot.damage = damage;
    shot.crit = crit;
    shot.side = side;
    shot.look = look;

    shots.push_back(shot);
}

//----------------------------------------------------------------------------------
// A mote arriving.
//
// The mote and the impact are the same cast seen at two moments, so this is where
// one becomes the other: the glow stops existing and its school's sheet starts
// playing in the same cubic centimetre. There is no overlap and no fade between
// them - the swap happens inside a frame, which is what makes it read as the mote
// having BECOME the effect rather than as two things that happened near each other.
//
// Backed off along the heading before the effect is placed. Left exactly where the
// mote stopped, a burst on a wall has half its billboard buried in the stone and a
// burst on an enemy is centred inside the chest - both of which cost most of the
// picture to the depth test.
//----------------------------------------------------------------------------------
void ProjectileManager::Burst(Shot &shot, Vector3 at, VfxManager &vfx) const
{
    const MagicDef &magic = *shot.look.magic;

    const Vector3 heading = Vector3Normalize(shot.velocity);
    const Vector3 centre = Vector3Subtract(at, Vector3Scale(heading, Config::MoteBurstBackoff));

    // The school's own sheet unless the shot named another - see impactOverride. An
    // overridden effect takes the MOTE's colour rather than the school's impact
    // tint, because the tint is authored against the sheet it belongs to and means
    // nothing on a different one.
    const bool overridden = (shot.look.impactOverride != VfxKind::Count);

    const VfxKind kind = overridden ? shot.look.impactOverride : magic.impact;
    const Color tint = overridden ? magic.colour : magic.impactTint;

    vfx.Spawn(kind, centre, magic.impactSize*shot.look.impactScale, tint);

    // Spent, not stuck. Update sweeps it up at the end of the frame.
    shot.life = 0.0f;
}

//----------------------------------------------------------------------------------
// Plant a shot where it stopped.
//
// It keeps its velocity, which is not used to move it any more: the direction is
// still what the draw and the debug line are built from, and zeroing it would
// leave both with nothing to orient against.
//
// The pose is frozen into `look` rather than recomputed, because from here on the
// shot has to keep the angle it arrived at whatever else changes - including the
// door swinging out from under it.
//----------------------------------------------------------------------------------
void ProjectileManager::Stick(Shot &shot, Vector3 at, const Level &level, int door) const
{
    const Vector3 heading = Vector3Normalize(shot.velocity);

    shot.stuck = true;
    shot.life = Config::ProjectileStickLife;

    // A little past where it stopped being clear, so the head is under the surface
    // rather than resting on it
    shot.position = Vector3Add(at, Vector3Scale(heading, Config::ProjectileStickBite));

    if (!shot.look.fixed)
    {
        shot.look.orientation = QuaternionToMatrix(
            QuaternionFromVector3ToVector3(shot.look.axis, heading));
        shot.look.fixed = true;
    }

    shot.stuckDoor = door;
    if (door < 0) return;

    // Its world pose, expressed in the leaf's frame. The leaf is shut at the
    // moment of the hit and about to swing, and this is what carries the arrow
    // round with it instead of leaving it hanging in the empty doorway.
    const Matrix world = MatrixMultiply(shot.look.orientation,
                                        MatrixTranslate(shot.position.x, shot.position.y,
                                                        shot.position.z));

    shot.stuckLocal = MatrixMultiply(world, MatrixInvert(level.DoorLeafTransform(door)));
}

//----------------------------------------------------------------------------------
// One substep of one shot. Returns false when it has stopped travelling - either
// spent in a body, or stuck in whatever it ran into.
//
// The order matters: bodies are tested before the wall behind them, or a shot
// taken with your back to a wall passes through you and dies in the brickwork.
//----------------------------------------------------------------------------------
bool ProjectileManager::Advance(Shot &shot, float step, Level &level, Player &player,
                                std::vector<Enemy> &enemies, VfxManager &vfx,
                                bool &enemyCrit) const
{
    // A mote has nothing to leave behind, so every way this function can stop it
    // ends in the same place. Named once here rather than branched on at each of
    // the six exits below.
    const bool mote = (shot.look.magic != nullptr);

    const Vector3 was = shot.position;
    shot.position = Vector3Add(shot.position, Vector3Scale(shot.velocity, step));

    // The shot is not a point at the end of the step, it is the whole line it
    // travelled along, fattened by its own radius - so it cannot skip past a body
    // however fast it is going or however long the frame ran. Bodies are the same
    // vertical capsule melee tests against, from the one helper, so the debug
    // overlay and the hit agree by construction.
    const Capsule path = { was, shot.position, Config::ProjectileRadius };

    if (shot.side == ProjectileSide::AtPlayer)
    {
        const Capsule body = BodyCapsule(player.body.position, Config::PlayerEyeHeight, player.body.radius);

        if (CapsulesOverlap(path, body))
        {
            const Vector3 contact = CapsuleContactPoint(path, body);

            // From where the shot met the body, so a raised shield only stops what
            // it is pointed at - exactly as a melee swing is judged. Never a
            // parry: that is a timed read of a swing, not something a shield
            // does to an arrow.
            player.TakeDamageFrom(shot.damage, contact, false);

            if (mote) Burst(shot, contact, vfx);

            return false;
        }
    }
    else
    {
        for (Enemy &enemy : enemies)
        {
            if (!enemy.IsAlive()) continue;     // A corpse does not stop a shot

            const Capsule body = BodyCapsule(enemy.body.position, enemy.height, enemy.body.radius);

            if (!CapsulesOverlap(path, body)) continue;

            const Vector3 contact = CapsuleContactPoint(path, body);

            // Before the damage, so a body that dies to this blow is already marked
            // when EnemyManager comes to pay for it. `mote` is the school's own
            // flag - a thrown dagger is a weapon kill, however far it flew.
            enemy.killedBySpell = mote;

            enemy.TakeDamageFrom(shot.damage, contact);

            if (shot.crit) enemyCrit = true;

            //----------------------------------------------------------------------
            // What the school does, over and above the damage just applied - to
            // the body the mote actually hit, and then to the burst around it.
            //
            // BLAST's shove wants the bolt's own line of travel for the direct
            // hit and is handled here rather than inside Enemy::ApplyMagicEffect,
            // which does not have it; every other school's own effect (burn,
            // stacks, slow, blind, bleed, SPARK's crit already rolled) is applied
            // through that function alone.
            //----------------------------------------------------------------------
            if (mote)
            {
                const Magic school = shot.look.magic->school;
                const MagicDef &def = *shot.look.magic;

                enemy.ApplyMagicEffect(school);

                if (school == Magic::Blast)
                {
                    // Along the bolt's own line of travel, so the shove reads as
                    // the impact carrying the target rather than as a push from
                    // nowhere. Interrupts whatever the blow caught it doing, the
                    // same way a hammer's stun already does to the player's own
                    // fights - a shove that left a swing or a channel running
                    // through it would not read as a knockback at all.
                    enemy.Shove(Vector3Normalize(shot.velocity), Config::BlastKnockbackSpeed);

                    enemy.meleePending = false;
                    enemy.shotPending = false;
                    enemy.channelTime = 0.0f;
                }

                //------------------------------------------------------------------
                // The burst: every OTHER living body within this school's own
                // aoeRadius (see the note on MagicDef::aoeRadius) takes the same
                // blow AND the same effect `enemy` above just did. This is what
                // turns every school into a real area of effect rather than a
                // single target with a wide picture drawn round it - NOVA's own
                // trick, now every school's.
                //------------------------------------------------------------------
                for (Enemy &other : enemies)
                {
                    if (&other == &enemy) continue;
                    if (!other.IsAlive()) continue;

                    const float dx = other.body.position.x - contact.x;
                    const float dz = other.body.position.z - contact.z;

                    if ((dx*dx + dz*dz) > def.aoeRadius*def.aoeRadius) continue;

                    other.killedBySpell = true;
                    other.TakeDamageFrom(shot.damage, contact);
                    other.ApplyMagicEffect(school);

                    if (school == Magic::Blast)
                    {
                        // Radially outward from the impact rather than along the
                        // bolt's own line - these bodies were never on that line,
                        // and an explosion pushes everyone away from where it
                        // went off rather than all in one direction.
                        other.Shove(Vector3Subtract(other.body.position, contact),
                                   Config::BlastKnockbackSpeed);

                        other.meleePending = false;
                        other.shotPending = false;
                        other.channelTime = 0.0f;
                    }
                }
            }

            // The effect goes off ON the enemy, at the point the sweep says the two
            // actually met - not at the centre of the body and not at the end of
            // the substep. A blast that plays a foot to the side of the skeleton it
            // just killed reads as a near miss that worked anyway.
            if (mote) Burst(shot, contact, vfx);

            // Where the shot was loosed from, not where it landed. An enemy shot
            // from cover it never saw now goes and looks there, which is the
            // difference between an archer duel and shooting a statue.
            enemy.NoticeAttackFrom(shot.origin);

            return false;   // One body per shot: nothing here pierces
        }
    }

    //------------------------------------------------------------------------------
    // Geometry, but only where there is any.
    //
    // The grid has no vertical extent: a wall cell reads solid all the way to
    // infinity, and everything off the edge of the map reads solid too. Below the
    // wall tops that is right - it is why a shot cannot be lobbed over one, worth
    // revisiting when the half-height wallSingle tiles go in. Above them it is a
    // fiction, and the moment shots started sticking rather than quietly dying it
    // became a visible one: anything fired at the sky pinned itself to the first
    // wall cell or map edge it flew over and hung there in the skybox.
    //------------------------------------------------------------------------------
    const float tops = level.FloorHeight() + Config::WallHeight;

    if (shot.position.y >= tops)
    {
        // Out of the level altogether. Nothing up here to stick in, so it is gone
        // rather than stopped.
        if (shot.position.y >= level.FloorHeight() + Config::ProjectileSkyHeight) return false;
    }
    else
    {
        if (level.Grid().SolidAtWorld(shot.position.x, shot.position.z))
        {
            if (mote) Burst(shot, shot.position, vfx);
            else Stick(shot, shot.position, level, -1);

            return false;
        }

        // A shut door stops a shot and is knocked open by it. Without this a bow
        // or a staff could never open one, which on a level whose rooms are behind
        // doors means a ranged loadout can get itself permanently stuck. The arrow
        // stays in the leaf and swings with it, which is also the clearest possible
        // signal that shooting the door is what opened it.
        const int struck = level.StrikeDoorAt(shot.position, Config::ProjectileRadius);

        if (struck >= 0)
        {
            if (mote) Burst(shot, shot.position, vfx);
            else Stick(shot, shot.position, level, struck);

            return false;
        }

        // The jambs. Stone, not door: nothing opens them, and they are a full unit
        // wide each - a shot through one was a shot through what is plainly a wall.
        if (level.DoorFrameAt(shot.position, Config::ProjectileRadius))
        {
            if (mote) Burst(shot, shot.position, vfx);
            else Stick(shot, shot.position, level, -1);

            return false;
        }
    }

    // The floor stops a shot. Nothing above does: these levels are open topped,
    // and killing shots at WallHeight was an invisible lid a metre and a half over
    // the player's eyes - a staff whose muzzle sits 1.98 units out spawned its
    // bolt at 3.47 when aimed up, already past a ceiling at 3.00, so the shot died
    // before its first step. Anything fired at the sky now simply flies until
    // ProjectileLife runs out, which is what it looks like it should do.
    if (shot.position.y <= level.FloorHeight())
    {
        // Where it crossed the plane, not where the substep landed under it. A
        // shallow shot covers a lot of ground per unit of drop, so the sample
        // point can be most of a step past the floor - far enough to bury the
        // whole shaft in it.
        const float drop = was.y - shot.position.y;
        const float t = (drop > 1e-5f) ? ((was.y - level.FloorHeight())/drop) : 0.0f;
        const Vector3 landed = Vector3Lerp(was, shot.position, t);

        if (mote) Burst(shot, landed, vfx);
        else Stick(shot, landed, level, -1);

        return false;
    }

    return true;
}

bool ProjectileManager::Update(float delta, Level &level, Player &player,
                               std::vector<Enemy> &enemies, VfxManager &vfx)
{
    bool enemyCrit = false;

    for (Shot &shot : shots)
    {
        shot.life -= delta;
        if (shot.life <= 0.0f) continue;        // Swept up below

        if (shot.stuck)
        {
            // Nothing moves it any more except the thing it is stuck in. Stone
            // does not move; a door does, and an arrow left behind in the empty
            // doorway would undo the whole point of it having stuck at all.
            if (shot.stuckDoor >= 0)
            {
                const Matrix world = MatrixMultiply(shot.stuckLocal,
                                                    level.DoorLeafTransform(shot.stuckDoor));

                shot.position = { world.m12, world.m13, world.m14 };

                // Draw composes scale, orientation and translation itself, so the
                // translation has to come back out of the rotation or it lands twice
                shot.look.orientation = world;
                shot.look.orientation.m12 = 0.0f;
                shot.look.orientation.m13 = 0.0f;
                shot.look.orientation.m14 = 0.0f;
            }

            continue;
        }

        float remaining = delta;
        const float speed = Vector3Length(shot.velocity);

        // Short enough that nothing can be stepped over. A whole frame at 24 u/s
        // is 0.4 units, which is a whole body radius.
        const float stepTime = (speed > 1e-4f) ? Config::ProjectileStep/speed : remaining;

        while (remaining > 0.0f)
        {
            const float step = fminf(stepTime, remaining);

            // Stopping is not the same as being spent: a shot that stuck has just
            // been given a fresh lifetime to sit there for, and zeroing it here
            // would delete it on the frame it landed
            if (!Advance(shot, step, level, player, enemies, vfx, enemyCrit))
            {
                if (!shot.stuck) shot.life = 0.0f;

                break;
            }

            remaining -= step;
        }
    }

    shots.erase(std::remove_if(shots.begin(), shots.end(),
                               [](const Shot &shot) { return shot.life <= 0.0f; }),
                shots.end());

    return enemyCrit;
}

//----------------------------------------------------------------------------------
// Motes first, then everything with geometry.
//
// Split into two passes because the motes are additive and depth-mask-off, and
// arrows are neither. One pass would mean setting and unsetting both states per
// shot, in whatever order the vector happens to hold them, and a volley of six
// arrows and two motes is fourteen state changes to draw eight things.
//----------------------------------------------------------------------------------
void ProjectileManager::Draw(const Camera3D &camera) const
{
    DrawMotes(camera);

    for (const Shot &shot : shots)
    {
        if (shot.look.magic != nullptr) continue;   // Drawn above

        if (shot.look.model == nullptr)
        {
            // No model is not a reason to be invisible: a shot you cannot see is a
            // shot you cannot learn to dodge
            const Vector3 tail = Vector3Subtract(shot.position, Vector3Scale(Vector3Normalize(shot.velocity), 0.4f));
            DrawLine3D(tail, shot.position, (Color){ 240, 220, 150, 255 });
            continue;
        }

        // A thrown weapon keeps the pose it was thrown in; everything else points
        // the way it is going, by rotating its own nose axis onto the velocity.
        //
        // One rotation from one measured fact beats a yaw and a pitch, which need
        // the authored axis baked into both of them and go degenerate on a shot
        // travelling straight up.
        const Matrix turn = shot.look.fixed
                          ? shot.look.orientation
                          : QuaternionToMatrix(QuaternionFromVector3ToVector3(
                                shot.look.axis, Vector3Normalize(shot.velocity)));

        Model drawn = *shot.look.model;
        drawn.transform =
            MatrixMultiply(MatrixMultiply(MatrixScale(shot.look.scale, shot.look.scale, shot.look.scale),
                                          turn),
                           MatrixTranslate(shot.position.x, shot.position.y, shot.position.z));

        DrawModel(drawn, Vector3Zero(), 1.0f, WHITE);
    }
}

//----------------------------------------------------------------------------------
// The motes: a tail, a halo and a bright core, all additive and none of them
// writing depth - two overlapping motes should add, rather than the nearer one
// punching a transparent hole in the further.
//----------------------------------------------------------------------------------
void ProjectileManager::DrawMotes(const Camera3D &camera) const
{
    if (glow == nullptr) return;

    //------------------------------------------------------------------------------
    // The quad's up axis. The same reasoning as VfxManager::DrawPass: raylib takes
    // the right vector from the view matrix and needs telling which way is up, and
    // the default of {0,1,0} collapses the quad to a sliver for anything directly
    // above or below the eye - which for a mote fired at the floor is exactly when
    // it is being looked at.
    //------------------------------------------------------------------------------
    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    const Vector3 up = Vector3CrossProduct(right, forward);

    const Rectangle whole = { 0.0f, 0.0f, (float)glow->width, (float)glow->height };

    bool any = false;

    for (const Shot &shot : shots)
    {
        if (shot.look.magic == nullptr) continue;

        if (!any)
        {
            // Deferred until there is actually something to draw, so a frame with
            // no magic in the air costs nothing
            rlDisableDepthMask();
            BeginBlendMode(BLEND_ADDITIVE);
            any = true;
        }

        const MagicDef &magic = *shot.look.magic;

        // Towards white rather than brighter. Pushing the school's own colour up
        // past 255 clips one channel at a time, which turns an orange mote yellow
        // in the middle and a green one lime; mixing towards white keeps the hue
        // and only takes the saturation out, which is what hot things do.
        auto whiten = [](unsigned char channel)
        {
            return (unsigned char)(channel + (255.0f - channel)*Config::MoteCoreWhiten);
        };

        const Color halo = { magic.colour.r, magic.colour.g, magic.colour.b,
                             (unsigned char)Config::MoteHaloAlpha };
        const Color core = { whiten(magic.colour.r), whiten(magic.colour.g),
                             whiten(magic.colour.b), 255 };

        const float diameter = magic.moteRadius*2.0f;

        //--------------------------------------------------------------------------
        // The tail, then the mote.
        //
        // A few halos strung back along the heading, each smaller and fainter than
        // the last. It is not motion blur and is not trying to be: it is what makes
        // a mote read as travelling at all. A single billboard crossing a room at
        // thirty units a second is a dot that teleports between frames - there is
        // nothing in the picture that says which way it is going, so the player
        // cannot tell an incoming bolt from one going past them until it lands.
        //
        // Drawn back to front, faintest first, so the brightest sample is added
        // last and the head of the mote stays the brightest thing in it.
        //--------------------------------------------------------------------------
        const Vector3 back = Vector3Scale(Vector3Normalize(shot.velocity),
                                          -magic.moteRadius*Config::MoteTrailSpacing);

        for (int i = Config::MoteTrailCount; i >= 1; --i)
        {
            // 1 is the sample nearest the mote and the count is the furthest back,
            // so fade and shrink run the other way from the loop
            const float away = i/(float)(Config::MoteTrailCount + 1);
            const float fade = (1.0f - away)*(1.0f - away);

            const Vector3 at = Vector3Add(shot.position, Vector3Scale(back, (float)i));
            const Color tint = { halo.r, halo.g, halo.b, (unsigned char)(halo.a*fade) };

            const float span = diameter*Config::MoteHaloScale*(1.0f - away);

            DrawBillboardPro(camera, *glow, whole, at, up, { span, span },
                             { span*0.5f, span*0.5f }, 0.0f, tint);
        }

        // The halo and the bright core inside it. Additive, so where they overlap
        // the middle saturates to white and the rim stays coloured - which is the
        // whole difference between a glowing ball and a circle someone drew.
        //
        // Both concentric, which they only are because the origin is half the size:
        // DrawBillboardPro grows its quad from `position` along right and up, so a
        // zero origin makes that point the bottom left corner and two billboards of
        // different sizes about the same point share a CORNER rather than a centre.
        // See the same note in VfxManager::DrawPass.
        const float haloSpan = diameter*Config::MoteHaloScale;

        DrawBillboardPro(camera, *glow, whole, shot.position, up, { haloSpan, haloSpan },
                         { haloSpan*0.5f, haloSpan*0.5f }, 0.0f, halo);

        DrawBillboardPro(camera, *glow, whole, shot.position, up, { diameter, diameter },
                         { diameter*0.5f, diameter*0.5f }, 0.0f, core);
    }

    if (any)
    {
        EndBlendMode();
        rlEnableDepthMask();
    }
}

void ProjectileManager::Clear()
{
    shots.clear();
}
