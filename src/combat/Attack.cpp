#include "combat/Attack.h"

#include "combat/Collider.h"
#include "combat/Stats.h"
#include "entities/Enemy.h"
#include "raymath.h"
#include "world/Level.h"

#include <cmath>

namespace
{
    float EaseOut(float t) { return 1.0f - (1.0f - t)*(1.0f - t)*(1.0f - t); }
    float EaseInOut(float t) { return t*t*(3.0f - 2.0f*t); }

    Vector3 Flatten(Vector3 v)
    {
        v.y = 0.0f;

        return v;
    }
}

void AttackState::Reset()
{
    phase = Phase::Idle;
    timer = 0.0f;
    blend = 0.0f;
    startedThisFrame = false;
    hitCount = 0;
    shotPending = false;
}

//----------------------------------------------------------------------------------
// One stroke, one hit per enemy.
//
// The cap is a refusal, not a bypass. Once the list is full the swing can no
// longer record what it has cut, and a swing that cannot remember must not be
// allowed to keep cutting: letting it through would hand full damage to the very
// enemies already in the list, every frame, for the rest of the live window. A
// spent stroke is the safe direction to fail in, and eight bodies is already more
// than any real swing catches.
//----------------------------------------------------------------------------------
bool AttackState::CanHit(int enemyId) const
{
    if (hitCount >= Config::MaxHitsPerSwing) return false;

    for (int i = 0; i < hitCount; ++i)
    {
        if (hitIds[i] == enemyId) return false;
    }

    return true;
}

void AttackState::NoteHit(int enemyId)
{
    if (hitCount >= Config::MaxHitsPerSwing) return;

    hitIds[hitCount++] = enemyId;
}

void AttackState::Update(float delta, bool pressed, bool held, AttackStyle style)
{
    const StyleTiming &timing = TimingFor(style);

    startedThisFrame = false;

    switch (phase)
    {
        case Phase::Idle:
            blend = 0.0f;
            if (pressed)
            {
                phase = Phase::Out;
                timer = 0.0f;
                startedThisFrame = true;
                hitCount = 0;       // A fresh stroke owes nobody anything
                shotPending = true; // Consumed at releaseAt; melee never looks at it
            }
            break;

        case Phase::Out:
            timer += delta;
            if (timer >= timing.out)
            {
                blend = 1.0f;
                timer = 0.0f;
                phase = (timing.holdsAtFullExtension && held) ? Phase::Hold : Phase::Back;
            }
            else blend = EaseOut(timer/timing.out);
            break;

        case Phase::Hold:
            blend = 1.0f;
            if (!held)
            {
                phase = Phase::Back;
                timer = 0.0f;
            }
            break;

        case Phase::Back:
            timer += delta;
            if (timer >= timing.back) Reset();
            else blend = 1.0f - EaseInOut(timer/timing.back);
            break;
    }
}

bool InCone(Vector3 origin, Vector3 forward, Vector3 target, float reach, float arcDegrees)
{
    const Vector3 toTarget = Flatten(Vector3Subtract(target, origin));
    const float distance = Vector3Length(toTarget);

    if (distance > reach) return false;
    if (distance < 1e-4f) return true;      // Standing inside it

    const Vector3 facing = Vector3Normalize(Flatten(forward));
    const float cosAngle = Vector3DotProduct(facing, Vector3Scale(toTarget, 1.0f/distance));

    return cosAngle >= cosf(DEG2RAD*arcDegrees*0.5f);
}

bool MeleeIsLive(const WeaponStats &stats, const AttackState &state)
{
    if (!stats.melee) return false;
    if (state.phase != AttackState::Phase::Out) return false;

    return (state.blend >= stats.liveFrom) && (state.blend <= stats.liveTo);
}

MeleeResult SweepMelee(const Capsule &from, const Capsule &to, Vector3 eye,
                       const WeaponStats &stats, const StatBlock &attacker,
                       const Level &level, std::vector<Enemy> &enemies, AttackState &state)
{
    MeleeResult result;

    if (!MeleeIsLive(stats, state)) return result;

    for (Enemy &enemy : enemies)
    {
        if (!enemy.IsAlive()) continue;
        if (!state.CanHit(enemy.id)) continue;

        const Capsule body = BodyCapsule(enemy.body.position, enemy.height, enemy.body.radius);

        if (!SweptCapsuleHits(from, to, body, Config::MeleeSweepSteps)) continue;

        // No reaching through walls. Tested eye to body rather than tip to body:
        // the tip is already on the far side of whatever the blade passed through,
        // and what matters is whether the player has a clear line to swing along.
        if (!level.LineOfSight(eye, enemy.Center())) continue;

        //--------------------------------------------------------------------------
        // Rolled per BODY, not per swing.
        //
        // A stroke that catches three enemies is three chances at a critical, which
        // is what makes crit worth carrying on a weapon that sweeps. Rolling once
        // for the stroke would make a wide swing exactly as lucky as a narrow one,
        // and quietly turn the crit stat into a stat about attack speed.
        //--------------------------------------------------------------------------
        const bool crit = RollWeaponCrit(attacker, stats.critBonus);

        // The weapon's own damage raised by ARMS, then the crit on top of that.
        // Before the guard gets its say: what a shield takes off is a fraction of
        // whatever arrived, critical or not, and the other order would let a crit
        // punch through a block - which reads as the block being unreliable rather
        // than as the crit being good.
        const int raw = WeaponDamageWith(attacker, stats.damage);
        const int damage = ResolveDamage(raw, attacker, crit);

        const int before = enemy.health;

        // From the eye, so an enemy with its guard up only stops what it is facing.
        // Walking round behind one is the answer to a block.
        // A blade kill is a weapon kill. Cleared rather than assumed, because a body
        // grazed by a mote and finished with a sword must pay as the sword.
        enemy.killedBySpell = false;

        enemy.TakeDamageFrom(damage, eye);

        // What actually came off, which is not what was sent: a guard may have
        // eaten most of it, and the last blow of a fight only ever lands as much as
        // there was left. Lifesteal is paid on this figure.
        result.damage += before - enemy.health;

        //--------------------------------------------------------------------------
        // The weapon's own behaviours, on the frame the blow lands.
        //
        // Applied whether or not the blow killed. A hammer that staggers a body it
        // then kills has still done its job, and testing for survival first would
        // make the last hit of every fight silently different from the rest.
        //--------------------------------------------------------------------------
        if (stats.stun > 0.0f) enemy.Stun(stats.stun);

        if (stats.knockback > 0.0f)
        {
            // Along the line from the eye, flattened. Not along the blade: a swing
            // is an arc, so the blade's own heading at the moment of contact points
            // sideways as often as forwards, and a hammer that knocked bodies to
            // the left would read as a bug rather than as weight.
            enemy.Shove(Vector3Subtract(enemy.body.position, eye), stats.knockback);
        }

        // Same rule as an arrow: a blow that lands on an enemy facing the wrong
        // way tells it exactly where someone is standing, and it should go there
        enemy.NoticeAttackFrom(eye);
        state.NoteHit(enemy.id);
        result.hits++;
    }

    return result;
}
