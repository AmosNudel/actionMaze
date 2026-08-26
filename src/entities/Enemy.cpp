#include "entities/Enemy.h"

#include "combat/Attack.h"
#include "raymath.h"

#include <cmath>

namespace
{
    // Which of Hit_A/Hit_B, Death_A/Death_B this one plays. Uniform: the clips are
    // alternates rather than a common case and a rare one, and biasing them would
    // just make the rare one read as a glitch when it did come up.
    int PickVariant(int count)
    {
        return (count > 1) ? GetRandomValue(0, count - 1) : 0;
    }

    //------------------------------------------------------------------------------
    // Health, flash and the one reaction the hit produces.
    //
    // One PlayAnim per hit, always. Calling it twice - once for the flinch and then
    // again to correct it to a block reaction - would leave the cross-fade reading
    // the flinch it never played as the pose to fade out of.
    //------------------------------------------------------------------------------
    void Apply(Enemy &enemy, int amount, bool blocked)
    {
        if ((amount <= 0) || !enemy.IsAlive()) return;

        enemy.health -= amount;
        enemy.hurtFlash = 0.15f;

        // Being hit is loud. Recorded, not acted on - what the rest of the room
        // makes of it is EnemyManager's business, and the loudest thing to happen
        // to this body since it was last heard is the one that carries.
        enemy.unheardCry = fmaxf(enemy.unheardCry, Config::EnemyHurtNoise);

        if (enemy.health <= 0)
        {
            enemy.unheardCry = Config::EnemyDeathNoise;
            enemy.health = 0;
            enemy.alive = false;
            enemy.blocking = false;     // Nothing left to guard with

            // Owed, not paid. This is reached from a swept blade, from a mote and
            // from a debug key, and none of those has a player to hand it to -
            // EnemyManager collects it on the next frame, once.
            enemy.expPending = true;

            // A killing blow through a guard is still a death, not a flinch
            enemy.PlayAnim(EnemyAnim::Death, PickVariant(Config::EnemyDeathVariants));

            return;
        }

        if (blocked)
        {
            enemy.PlayAnim(EnemyAnim::BlockHit);

            return;
        }

        //--------------------------------------------------------------------------
        // Flinching, and the tier that does not.
        //
        // The Hit clip is a stun the PLAYER controls: a body playing it is not
        // swinging, so against anything that takes a dozen blows to kill the winning
        // move is to stand still and attack as fast as possible. That is not a fight,
        // it is a race the enemy cannot enter.
        //
        // So a champion soaks a share of its pool first. Every blow fills the meter;
        // crossing the threshold spends the whole meter and gives ground once. Below
        // it the body takes the damage and keeps swinging, which is what turns the
        // exchange back into something the player has to time.
        //
        // Every other tier has a poise of zero and falls straight through, so this
        // costs them nothing and reads as the same code path it always was.
        //--------------------------------------------------------------------------
        const float threshold = TierAt(enemy.tier).poise*enemy.maxHealth;

        if (threshold > 0.0f)
        {
            enemy.poise += (float)amount;

            if (enemy.poise < threshold) return;

            enemy.poise = 0.0f;
        }

        enemy.PlayAnim(EnemyAnim::Hit, PickVariant(Config::EnemyHitVariants));
    }
}

Vector3 Enemy::Forward() const
{
    return { -sinf(yaw), 0.0f, -cosf(yaw) };
}

Vector3 Enemy::Right() const
{
    return { cosf(yaw), 0.0f, -sinf(yaw) };
}

void Enemy::Stun(float seconds)
{
    if (!IsAlive() || (seconds <= 0.0f)) return;

    if (seconds > stunTime) stunTime = seconds;
}

void Enemy::Shove(Vector3 direction, float speed)
{
    if (!IsAlive() || (speed <= 0.0f)) return;

    // Flattened here rather than trusted from the caller. A shove with any vertical
    // component in it launches bodies, and every caller of this wants a push across
    // the floor - so the one that would have to remember is this one.
    direction.y = 0.0f;

    const float length = Vector3Length(direction);
    if (length < 1e-4f) return;

    const Vector3 push = Vector3Scale(direction, speed/length);

    // Added, not assigned: a body already moving keeps its own momentum, so two
    // blows land as two shoves rather than the second cancelling the first.
    body.velocity.x += push.x;
    body.velocity.z += push.z;
}

void Enemy::TakeDamage(int amount)
{
    Apply(*this, amount, false);
}

//----------------------------------------------------------------------------------
// A hit that came from a direction, so the guard gets a say.
//
// The reduction happens before any of it is applied, because what is left decides
// between a flinch and a death: a blow that would have killed and is blocked down
// to a scratch has to not kill.
//
// Reach is irrelevant to the arc test - whatever hit this enemy has already been
// found to reach it - so InCone is being used purely for the angle, which is why
// the reach argument is absurd. Same shape as Player::TakeDamageFrom.
//----------------------------------------------------------------------------------
void Enemy::TakeDamageFrom(int amount, Vector3 source)
{
    if ((amount <= 0) || !IsAlive()) return;

    const bool blocked = IsBlocking() &&
                         InCone(Center(), Forward(), source, 1000.0f, Config::EnemyBlockArc);

    if (blocked)
    {
        // A block is a reduction, never immunity: at least one point still lands,
        // or a guard plus enough armour would be unkillable from the front
        amount = (int)(amount*Config::EnemyBlockDamageScale);
        if (amount < 1) amount = 1;
    }

    Apply(*this, amount, blocked);
}

void Enemy::NoticeAttackFrom(Vector3 origin)
{
    if (!IsAlive()) return;

    lastKnownPlayer = origin;
    alertTime = Config::EnemyAlertMemory;
    investigating = true;
}

void Enemy::Hear(Vector3 from, float amount)
{
    if (!IsAlive() || (amount <= 0.0f)) return;

    detection = fminf(detection + amount, 1.0f);

    if (detection < 1.0f) return;

    // Certain that something is happening, and with nothing in sight the only
    // place worth being is where the noise came from. Deliberately the same trail
    // an arrow in the back sets, investigating flag and all, so a melee type comes
    // running rather than standing in its camp listening to its friends die.
    //
    // Refreshed even when already alert: the fight moves, and the newest noise is
    // the better guess. An enemy that can actually see the player overwrites this
    // from its own eyes on the next frame regardless.
    lastKnownPlayer = from;
    alertTime = Config::EnemyAlertMemory;
    investigating = true;
}
