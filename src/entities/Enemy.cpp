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
    void Apply(Enemy &enemy, int amount, bool blocked, float poiseScale)
    {
        if ((amount <= 0) || !enemy.IsAlive()) return;

        //--------------------------------------------------------------------------
        // SPLASH's sunder, applied HERE rather than at the spell that caused it, so
        // it lifts every source of damage there is: the sword, the thrown dagger,
        // the burn already running, the next cast. That is what "lowers defences"
        // has to mean to be worth a slot in the book - a debuff that only made the
        // debuffer hit harder would be a damage multiplier wearing a costume.
        //
        // Rounded up rather than truncated, so a one-point tick under a sunder is
        // still visibly worse than the same tick without one.
        //--------------------------------------------------------------------------
        if (enemy.sunderTime > 0.0f)
        {
            amount = (int)(amount*Config::SplashSunderFactor + 0.5f);
        }

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
        // The kind's own floor plus whatever the tier adds - see the note on
        // Config::EnemyArchetype::poise. A Champion Warrior stacks both; a
        // Champion Mage still has only the tier's, because folding when
        // reached is the point of the kind and a Champion tag should not undo
        // it.
        const float threshold = (Config::EnemyTypes[enemy.type].poise
                                 + TierAt(enemy.tier).poise)*enemy.maxHealth;

        if (threshold > 0.0f)
        {
            enemy.poise += (float)amount*poiseScale;

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

void Enemy::Stagger(float seconds)
{
    if (!IsAlive()) return;

    // A parry earns the flinch outright - the poise gate that keeps a champion
    // on its feet through ordinary damage does not apply to a blow that never
    // landed at all.
    PlayAnim(EnemyAnim::Hit, PickVariant(Config::EnemyHitVariants));
    blocking = false;   // A guard is a decision too - see Stun's own note

    Stun(seconds);
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
    Apply(*this, amount, false, 1.0f);
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
void Enemy::TakeDamageFrom(int amount, Vector3 source, float poiseScale)
{
    if ((amount <= 0) || !IsAlive()) return;

    // A sundered body's guard is part of what SPLASH stripped - see the note on
    // Enemy::sunderTime. The arms stay up, and stop being worth anything.
    const bool blocked = IsBlocking() && (sunderTime <= 0.0f) &&
                         InCone(Center(), Forward(), source, 1000.0f, Config::EnemyBlockArc);

    if (blocked)
    {
        // A block is a reduction, never immunity: at least one point still lands,
        // or a guard plus enough armour would be unkillable from the front
        amount = (int)(amount*Config::EnemyBlockDamageScale);
        if (amount < 1) amount = 1;
    }

    Apply(*this, amount, blocked, poiseScale);
}

//----------------------------------------------------------------------------------
// What a school leaves behind, over and above the damage TakeDamageFrom already
// applied.
//
// Called for the body the mote actually hit AND for every body its burst caught,
// so everything below happens to a whole pack at once for seven of the eight -
// which is the frame to read the durations in. See ProjectileManager::Advance.
//
// Dead bodies feel nothing - checked once here rather than in every branch, since
// a corpse cannot burn, panic or be stripped and a case that tried would just be
// leaving state on something RemoveDead is about to take away.
//----------------------------------------------------------------------------------
void Enemy::ApplyMagicEffect(Magic magic)
{
    if (!IsAlive()) return;

    switch (magic)
    {
        //----------------------------------------------------------------------
        // The two DOTs, one mechanism at two settings - see the note on
        // Enemy::dotTime. Both REFRESH rather than stacking: a second cast lands
        // as the clock going back to full, not as a second burn queued behind
        // the first, which is the same rule Enemy::Stun already follows.
        //
        // `dotSource` is set on both so the tick knows which sheet to draw, and
        // it is what makes a poisoned body stop looking like it is on fire when
        // a TOXIN lands on top of a FLAME.
        //----------------------------------------------------------------------
        case Magic::Flame:
            if (dotTime <= 0.0f) dotTickTimer = Config::MagicDotTickInterval;

            dotTime = Config::FlameBurnDuration;
            dotDamagePerTick = Config::FlameBurnDamagePerTick;
            dotSource = Magic::Flame;
            break;

        //----------------------------------------------------------------------
        // TOXIN, and its panic. A body already under a poison that takes a
        // second dose breaks and runs - which is what is left of the old
        // stacking version, and it is the better half of it: the stacks were a
        // counter the player could not see, where "I poisoned it twice and it
        // ran" is a rule they can learn in one fight.
        //
        // The DOT keeps running THROUGH the flee rather than being spent on it.
        // Ten seconds of poison is the whole school; ending it early to pay for
        // the panic would make the second cast a downgrade.
        //----------------------------------------------------------------------
        case Magic::Toxin:
            if ((dotTime > 0.0f) && (dotSource == Magic::Toxin))
            {
                fleeTime = Config::ToxinFleeDuration;
            }
            else
            {
                dotTickTimer = Config::MagicDotTickInterval;
            }

            dotTime = Config::ToxinDotDuration;
            dotDamagePerTick = Config::ToxinDamagePerTick;
            dotSource = Magic::Toxin;
            break;

        // Defences down - see the note on Enemy::sunderTime for what that
        // actually reaches. Longest of the three timed debuffs, because it does
        // nothing on its own: it is bought to be spent on whatever the player
        // does next, and a window too short to swing in would buy nothing.
        case Magic::Splash:
            if (Config::SplashSunderDuration > sunderTime) sunderTime = Config::SplashSunderDuration;
            break;

        // Holds detection at zero rather than damaging anything - see
        // EnemyManager::Update, which reads this instead of running the ordinary
        // sight meter while it is above zero.
        case Magic::Flash:
            if (Config::FlashBlindDuration > blindTime) blindTime = Config::FlashBlindDuration;
            detection = 0.0f;
            break;

        //----------------------------------------------------------------------
        // The three that need something an enemy does not have, and so are not
        // here at all - see ProjectileManager::Advance for all three:
        //
        //   SPARK   its guaranteed critical was rolled at the cast
        //   NOVA    the shove wants the impact point to push away from
        //   REND    the lifesteal wants the player to pay it to
        //
        // BLAST is here for a different reason: it has no effect. Widest burst
        // and hardest hit IS the school, and giving it a rider as well would
        // leave the other seven with nothing to be.
        //----------------------------------------------------------------------
        case Magic::Spark:
        case Magic::Blast:
        case Magic::Nova:
        case Magic::Rend:
        default:
            break;
    }
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
