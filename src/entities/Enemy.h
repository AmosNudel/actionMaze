#pragma once

#include "combat/MagicKind.h"
#include "combat/StatBlock.h"
#include "core/Config.h"
#include "entities/Body.h"
#include "entities/EnemyRank.h"
#include "raylib.h"
#include "render/Ragdoll.h"

#include <vector>

// What the enemy's skeleton is doing.
//
// Nearly all of this is purely what you see: the capsule, its reach and its
// damage do not change with the pose. Block is the exception - a guard that
// played the animation and let full damage through would teach the player to
// ignore the animation - so Enemy::IsBlocking gates damage in TakeDamageFrom.
//
// Idle, Walk and Block loop and are chosen fresh every frame. Spawn, Attack,
// Shoot, Hit, BlockHit and Death play once and own the body until they finish;
// see EnemyManager::ClipOwnsBody, which is the only place that decides it.
//
// Attack and Shoot share one clip slot - an archetype names one attack animation
// either way - and differ only in what happens when it lands: Attack damages the
// player directly on the frame it starts, Shoot puts an arrow in the air partway
// through. Which one an enemy plays is Config::EnemyArchetype::ranged.
enum class EnemyAnim
{
    Idle,
    Walk,
    Spawn,
    Attack,
    Shoot,
    Block,
    Hit,
    BlockHit,
    Death
};

//----------------------------------------------------------------------------------
// A walking enemy: a Body, so it inherits gravity, acceleration and
// Level::ResolveBody for free, plus the numbers combat cares about.
//
// Its hit volume is a vertical capsule from the feet to `height`, radius
// body.radius - rotation free, which keeps every distance test cheap and means
// melee never needs to know which way an enemy is facing.
//----------------------------------------------------------------------------------
struct Enemy
{
    Body body;
    // Handed out by EnemyManager and never reused. Position in the vector is not
    // an identity - RemoveDead compacts it - and a swept blade needs to remember
    // across frames which bodies it has already cut, so it needs one that survives
    // the compaction.
    int id = 0;
    // Which row of Config::EnemyTypes this one is. Everything below that varies by
    // kind - health, height, reach, what it holds - is read through it.
    int type = 0;
    // Which Config::SpawnCamps row put it here, or -1 for a body that belongs to
    // no camp. A camp counts its own living bodies to decide whether to send
    // another, so this is what makes the garrison a number rather than a guess.
    int camp = -1;

    //------------------------------------------------------------------------------
    // Which event sent this body, or 0 for the floor's ordinary population.
    //
    // A tag rather than a pointer or an index: an event counts its own living bodies
    // to know whether it is finished, and it has to be able to do that across the
    // frames in which RemoveDead compacts the list out from under it. Tags are
    // handed out and never reused, so a body from a hunt that ended cannot be
    // counted towards a later one.
    //------------------------------------------------------------------------------
    int eventTag = 0;

    //------------------------------------------------------------------------------
    // A raider: something that walked in for the relic and is not interested in the
    // player at all.
    //
    // It is a whole separate behaviour rather than the ordinary chase pointed
    // somewhere else, and that is deliberate on both counts. Retargeting the chase
    // would mean threading a target through sight, detection, alarm and the guard -
    // all of which are about the PLAYER by nature - and what came out the other end
    // would be a raider that had to see the relic to walk to it.
    //
    // What a raider actually is: it knows where the relic is, it walks there, and it
    // swings at it. It cannot be distracted, which is the entire fight - the player
    // is not being attacked, they are being ignored, and the answer is to make
    // themselves impossible to ignore by killing the things doing the ignoring.
    //------------------------------------------------------------------------------
    bool raiding = false;
    Vector3 raidTarget = { 0.0f, 0.0f, 0.0f };

    // A raid swing landed and has not been paid to the relic yet. The same idiom
    // `expPending` uses, and for the same reason: the enemy knows it swung and has
    // no idea what a relic is.
    bool raidHitPending = false;
    float height = 2.0f;                // Overwritten from the archetype at spawn
    float yaw = 0.0f;                   // Facing, radians - drives its own attacks
    int health = 1;
    int maxHealth = 1;

    //------------------------------------------------------------------------------
    // What this body is, over and above its kind.
    //
    // `rank` is rolled off the level's depth at spawn and scales the kind's health,
    // damage and what the kill pays. `tier` is that rank re-bucketed against the
    // player's live level, and unlike the rank it is RE-EVALUATED as the player
    // levels: the body does not change, the distance to it does. See EnemyRank.h.
    //
    // `stats` is the kind's own line with nothing added - the rank does not touch
    // it, deliberately. `damage` is the ranked and tiered figure, resolved once at
    // spawn rather than recomputed per swing, because a body's strength must not
    // change halfway through a fight because the player levelled mid-swing.
    //------------------------------------------------------------------------------
    int rank = 1;
    EnemyTier tier = EnemyTier::Even;
    StatBlock stats;
    int damage = 1;
    // Draw scale and tint, both off the tier. Kept on the body so the draw reads
    // one number rather than looking the tier up per frame per enemy.
    float scale = 1.0f;
    Color tint = WHITE;

    // What killing this pays, already scaled to its rank. Held rather than derived
    // at the kill because the kill happens in Enemy, which has no player to pay.
    int exp = 0;
    // Died this frame and has not been paid out yet. EnemyManager clears it, once,
    // the way it already does for `unheardCry` - and for the same reason: damage
    // arrives from a swept blade, from a mote, and from a debug key, and none of
    // those has or should need the player.
    bool expPending = false;

    //------------------------------------------------------------------------------
    // Whether the blow that dropped this body was a MOTE rather than a blade.
    //
    // Read once, alongside `expPending`, and only to decide what the kill pays into
    // the mana pool: a weapon kill pays outright and a spell kill pays half, which is
    // what keeps a cast from ever funding the next one (see progress/Spellbook.h).
    //
    // Set by whatever dealt the fatal damage, which is the only thing that knows. It
    // is a bool rather than a source enum because there are exactly two answers the
    // pool cares about, and a third would be a reason to change this rather than a
    // reason to have written an enum first.
    //------------------------------------------------------------------------------
    bool killedBySpell = false;
    float attackCooldown = 0.0f;
    float hurtFlash = 0.0f;             // Counts down after taking a hit, for feedback

    //------------------------------------------------------------------------------
    // Damage taken since this body last gave ground, against the tier's poise
    // threshold - see EnemyTierDef::poise.
    //
    // It DECAYS (see Config::EnemyPoiseRecovery), which is what makes it a poise
    // meter rather than a counter. Without that, chip damage spread over a whole
    // fight would eventually add up to a flinch, so a champion could still be
    // staggered - just on a delay, and at a moment the player did not earn.
    //
    // Reset to zero on the flinch itself, not decremented by the threshold: giving
    // ground is a whole event, and carrying the overflow into the next one would let
    // a single enormous blow bank two flinches.
    //------------------------------------------------------------------------------
    float poise = 0.0f;

    // Seconds this body cannot act for. While it is above zero the enemy neither
    // thinks nor swings nor walks - it only falls, so a stunned body still lands
    // rather than hanging in the air.
    //
    // It is a hard stop rather than a slow: the whole value of a hammer is that a
    // blow which lands DECIDES WHO SWINGS NEXT, and a body at half speed still
    // swings. Which is also why it is measured against the gaps between swings
    // (a Warrior's is 1.90s) rather than against anything else.
    float stunTime = 0.0f;
    bool alive = true;

    //------------------------------------------------------------------------------
    // What magic left behind - see Stats.h for why this replaced elements and
    // resistances, and Enemy::ApplyMagicEffect for where these are actually set.
    //
    // FLAME's burn and REND's bleed share one mechanism (`dotTime` and its
    // neighbours) because they are the same idea at two different settings; TOXIN
    // is a separate counter because it stacks rather than ticking on its own clock.
    // All four tick and decay in EnemyManager::Update alongside poise and stun, on
    // the same rule those already follow: this is happening TO the body, not a
    // thing it is doing, so nothing pauses it.
    //------------------------------------------------------------------------------
    float dotTime = 0.0f;           // Counts down; at zero there is no DOT running
    float dotTickTimer = 0.0f;
    int   dotDamagePerTick = 0;
    bool  dotSpreads = false;       // FLAME only - spent on the one jump it gets

    int   poisonStacks = 0;         // TOXIN
    float poisonTickTimer = 0.0f;

    float slowTime = 0.0f;          // SPLASH - see Config::SplashSlowFactor
    float blindTime = 0.0f;         // FLASH - holds `detection` at zero while it runs
    float fleeTime = 0.0f;          // TOXIN's panic - see EnemyManager's flee branch

    // Guard. `blocking` is the decision the AI made this frame, which is what the
    // feet and the state machine read; IsBlocking() is whether the pose is
    // actually there yet, which is what damage reads. They differ for the length
    // of one cross-fade, and using the decision for damage would hand out a
    // reduction before the arms had moved.
    bool blocking = false;
    // Rolled once per attack cooldown against the archetype's blockChance, so a
    // guard is a decision about this gap between swings rather than a coin
    // flipped every frame - which would flicker the arms up and down
    float blockRoll = 1.0f;

    //------------------------------------------------------------------------------
    // Casting a buff over its allies, and being under one.
    //
    // `channelTime` counts DOWN while the caster is committed: it stands still, does
    // not shoot, does not step aside, and is drawn lit up throughout. That cost is
    // the whole design - the buff is bought with the one thing a ranged enemy is
    // never otherwise short of, which is being hard to reach. A player who reads the
    // channel and closes gets a free kill; one who ignores it fights a pack that
    // hits half again as hard.
    //
    // `buffTime` is the other end of it. Above zero this body swings harder and
    // faster (see Config::EnemyBuff*), and carries an aura saying so - a buff the
    // player cannot see is a difficulty spike they will read as the game cheating.
    //------------------------------------------------------------------------------
    float channelTime = 0.0f;
    float channelCooldown = 0.0f;
    float buffTime = 0.0f;

    bool IsBuffed() const { return buffTime > 0.0f; }

    // A shot has been started but the arrow has not left yet. The crossbow comes
    // up over the first half of the clip, so firing on the frame the state begins
    // sends the arrow before the weapon is pointing anywhere.
    bool shotPending = false;

    //------------------------------------------------------------------------------
    // A melee swing has begun and its blow has not landed yet.
    //
    // Exactly the same shape as `shotPending` above, and added for the same reason:
    // a swing that resolved on the frame it started dealt its damage before the
    // weapon had moved. The blow now lands partway through the clip (see
    // Config::EnemyMeleeLand), and this is what stops every frame past that point
    // landing it again.
    //
    // The blow is COMMITTED once it starts - it is outside the awareness test, like
    // a loosed arrow - but it still has to be in reach and facing the right way when
    // it lands. Those are re-tested at the landing frame, which is what makes
    // stepping out of a swing work.
    //------------------------------------------------------------------------------
    bool meleePending = false;

    // Where the player was standing the last time this enemy actually saw them,
    // and how long it will keep acting on that. An enemy that forgets the instant
    // sight breaks is beaten by any corner - it stops dead the frame the player
    // steps behind a wall - so losing sight starts a clock rather than ending the
    // fight. Only meaningful while alertTime is above zero.
    Vector3 lastKnownPlayer = { 0.0f, 0.0f, 0.0f };
    float alertTime = 0.0f;

    // How sure it is that the thing in front of it is the player: 0 has noticed
    // nothing, 1 is certain and fighting. Fills while the player is inside the
    // cone with a clear line, drains when they are not.
    //
    // A meter rather than a flag because sight arriving as a switch gives the
    // player nothing to play against - there is no such thing as nearly being
    // seen, no reason to watch which way a body is facing, and no way to cross a
    // room a guard is looking down. What it also buys is that an enemy glimpsed
    // through a doorway at twenty paces does not turn and open fire in one frame.
    float detection = 0.0f;

    // A shout this body has made and that nobody has been told about yet, in
    // meter-fill at point blank. Set when it is hurt or killed; EnemyManager
    // broadcasts it to whoever is in earshot and clears it, once, on the next
    // frame.
    //
    // Recorded here rather than pushed out from the hit itself because damage
    // arrives from a swept blade, from an arrow, and from a debug key, and none
    // of those has - or should need - the enemy list. What every one of them does
    // have is the enemy that was hit.
    float unheardCry = 0.0f;

    // The trail came from being hit rather than from having seen anything. Only
    // the ranged types hunt a lost sighting - a swordsman that walks through
    // walls after a player it cannot see reads as cheating - but an arrow in the
    // back is different: it is a fact about where someone is, it arrived while
    // this enemy was looking the wrong way, and standing there taking the next
    // one is the single most obviously broken thing an enemy can do. So a
    // disturbed enemy of any kind goes and looks.
    bool investigating = false;

    //------------------------------------------------------------------------------
    // The route it is walking, as cell centres from the pathfinder.
    //
    // One vector serves both jobs a route can have, because they are the same job
    // with a different end condition: a chase walks to the end and stops, a patrol
    // walks to the end and turns round. `routeForward` is which way along it the
    // body is currently going.
    //
    // Held across frames rather than recomputed. A path recomputed every frame is
    // the same path every frame at ten times the cost, and worse, a body that
    // re-decides its route continuously never commits to one - it stands at a
    // junction swapping between two equally good ways round.
    //------------------------------------------------------------------------------
    std::vector<Vector3> route;
    int   routeIndex = 0;
    bool  routeForward = true;

    // Where the current route was asked for. A goal that has moved more than a
    // cell is a different question and wants asking again.
    Vector3 routeGoal = { 0.0f, 0.0f, 0.0f };

    // Counts down to the next time this body may ask for a route. Staggered at
    // spawn so a camp of five does not do all its thinking on the same frame.
    float repathTimer = 0.0f;

    // How long it has been trying to reach the same waypoint. A body wedged on a
    // prop, or shoved off its route by a fight, throws the route away and asks
    // again rather than pushing at the thing in its way for ever.
    float stuckTime = 0.0f;

    // Walks a beat rather than standing its post. Decided once when the camp
    // sends it out, and kept for life: a body that took up patrolling halfway
    // through its shift would abandon whatever it was doing to do it.
    bool patrols = false;

    // Fighting the player right now, as decided last frame. Read by SpreadAlarm,
    // which runs before anyone thinks - so it has to be what this body concluded,
    // not what it is about to.
    bool inCombat = false;

    // How long it has been trying to close on the player without getting any
    // closer. Walking straight at somebody is the right way to fight and the wrong
    // way to get round a table, and this is what tells the two apart.
    float pushTime = 0.0f;
    float lastGap = 0.0f;

    // Sidestepping to clear its own line of fire: +1 to its right, -1 to its
    // left, 0 for not stepping. Held across frames rather than recomputed,
    // because a side picked fresh every frame is not a decision - the moment the
    // step starts to work the reason for it changes, and the enemy jitters in
    // place. `strafeTimer` is how long this side has been tried, so a body that
    // has walked into a wall eventually gives up and tries the other way.
    float strafeDir = 0.0f;
    float strafeTimer = 0.0f;

    // Animation. `bones` is this enemy's own skinning pose, which is what lets
    // every enemy share one Model and still move independently - see AnimatedModel.
    EnemyAnim anim = EnemyAnim::Idle;
    float animTime = 0.0f;              // Seconds into the current clip
    // Which alternate of the current state is playing - Hit_A or Hit_B, Death_A or
    // Death_B. States with only one clip ignore it, and a type missing the
    // alternate falls back to the first, so this is always safe to set.
    int animVariant = 0;
    // Which idle this body stands in, rolled once when it spawns and kept for
    // life. Per body rather than per entry into the state: an enemy that reshuffled
    // its idle every time it stopped walking would twitch between poses.
    int idleVariant = 0;
    // What the body is fading out of, so a state change is a cross-fade and not a
    // cut. The outgoing clip keeps running while it fades, or the pose it fades
    // from would be frozen mid-stride. The variant travels with it: fading out of
    // Death_B has to keep reading Death_B, not Death_A.
    EnemyAnim previousAnim = EnemyAnim::Idle;
    float previousAnimTime = 0.0f;
    int previousAnimVariant = 0;
    // Whether the walk cycle is running backwards, because the enemy is giving
    // ground rather than closing. The pack ships no backpedal clip, so a retreat
    // is the walk played in reverse; anything else is a moonwalk, with the feet
    // striding one way while the body slides the other.
    //
    // Travels into the cross-fade the same way the variant does, and for the same
    // reason: fading out of a reversed walk has to keep reading it reversed, or
    // the feet flip direction for the length of the fade.
    bool animReversed = false;
    bool previousAnimReversed = false;
    float animBlend = 1.0f;             // 1 once the fade is done and `anim` owns the body
    float deathTime = 0.0f;             // Counts up once killed, so the corpse can finish dying
    std::vector<Matrix> bones;

    // Takes the body over once the death clip has played out, so it settles where
    // it fell instead of holding the clip's last frame
    Ragdoll ragdoll;

    bool IsAlive() const { return alive && (health > 0); }

    // Restarts the clip even when it is already the one playing, so a second hit
    // during a hit reaction reads as a second hit - and so a swing that follows a
    // swing fades out of the end of the first rather than snapping to its start.
    void PlayAnim(EnemyAnim next, int variant = 0)
    {
        previousAnim = anim;
        previousAnimTime = animTime;
        previousAnimVariant = animVariant;
        previousAnimReversed = animReversed;
        animBlend = 0.0f;

        anim = next;
        animTime = 0.0f;
        animVariant = variant;
    }

    // Begins a state with no cross-fade, for the one case where there is nothing
    // to fade from: an enemy that has just been created. Spawn_Ground starts with
    // the body on the floor, so fading into it from the default idle pose shows
    // the skeleton standing upright for a moment before it drops back down.
    void StartAnim(EnemyAnim next)
    {
        anim = previousAnim = next;
        animTime = previousAnimTime = 0.0f;
        animVariant = previousAnimVariant = 0;
        animReversed = previousAnimReversed = false;
        animBlend = 1.0f;
    }

    // Whether the guard is up far enough to actually stop anything. The decision
    // in `blocking` leads the pose by one cross-fade, and a block reaction counts
    // as guarding because being hit is not a reason for the arms to drop.
    bool IsBlocking() const
    {
        if (!blocking) return false;
        if (anim == EnemyAnim::BlockHit) return true;

        return (anim == EnemyAnim::Block) && (animBlend > 0.75f);
    }

    // Facing as a direction. Matches Body::Update's convention, where forward at
    // yaw 0 runs along -Z; Config::EnemyModelYaw is what turns the model around to
    // meet it, and is deliberately not part of this.
    Vector3 Forward() const;

    // Its own right hand side, matching Body::Update's strafe axis exactly: a
    // move.x of +1 travels along this. Being the same vector the feet use means a
    // decision expressed as "step right" cannot come out as a step left.
    Vector3 Right() const;

    // Middle of the capsule: what a cone test aims at
    Vector3 Center() const
    {
        return { body.position.x, body.position.y + height*0.5f, body.position.z };
    }

    Vector3 Head() const
    {
        return { body.position.x, body.position.y + height, body.position.z };
    }

    // Frozen for `seconds`, or for longer if it already was. Never shortened: two
    // hammer blows in quick succession must not leave a body LESS stunned than one
    // of them did, which is what assigning rather than maxing would do.
    void Stun(float seconds);

    // What a PARRY costs it: a flinch it does not get to sidestep on poise the
    // way a champion soaks ordinary chip damage, held well past an ordinary Hit
    // clip - see Config::ParryStunTime. Player::TakeDamageFrom decides whether a
    // blocked blow counts as one; this is what the enemy that swung it pays.
    void Stagger(float seconds);

    // Shoved along `direction` at `speed` world units a second. The direction need
    // not be normalised or flattened; a zero one is ignored.
    //
    // Written into the velocity rather than the position, so the shove is subject
    // to the same drag, gravity and wall resolution everything else is - a body
    // knocked at a wall stops at it instead of being pushed through.
    void Shove(Vector3 direction, float speed);

    // Damage with nowhere to come from - a fall, a trap, a debug key. A guard
    // cannot be pointed at any of those, so none of it is blocked.
    void TakeDamage(int amount);
    // Damage from somewhere, which a raised guard can be pointed at. This is what
    // combat calls; the arc test is the same InCone the player's shield uses.
    void TakeDamageFrom(int amount, Vector3 source);

    //------------------------------------------------------------------------------
    // What a school of magic does to whatever it just hit, over and above its
    // damage - see the table in combat/Magic.cpp.
    //
    // Not every school is here: SPARK's guaranteed crit is decided at the cast, and
    // BLAST's shove and NOVA's area both need the caster's own line of travel or the
    // rest of the enemy list, neither of which an enemy has - see
    // ProjectileManager::Advance for those two.
    //------------------------------------------------------------------------------
    void ApplyMagicEffect(Magic magic);

    //------------------------------------------------------------------------------
    // Something came in from `origin`. Start looking there.
    //
    // Deliberately separate from TakeDamageFrom, whose `source` is the contact
    // point on this enemy's own body: that answers which way to hold the guard and
    // says nothing whatever about where the attacker is standing. An arrow's origin
    // is where it was loosed from, which is the only useful thing to walk towards.
    //
    // Safe to call whether or not the enemy can already see the player: an enemy
    // that can overwrites this from its own eyes on the very next frame.
    //------------------------------------------------------------------------------
    void NoticeAttackFrom(Vector3 origin);

    //------------------------------------------------------------------------------
    // Something loud happened at `from`, worth `amount` of the detection meter.
    //
    // Noise is not sight, so this fills the meter rather than declaring the enemy
    // aware: a body that has heard a scuffle reacts the instant it does look the
    // right way, which is most of what being on edge means. Only once the meter is
    // full does it stop waiting and go and find out - and it walks to the NOISE,
    // not to the player, because the noise is the only thing it actually knows.
    //------------------------------------------------------------------------------
    void Hear(Vector3 from, float amount);
};
