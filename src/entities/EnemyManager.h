#pragma once

#include "core/Config.h"
#include "entities/Enemy.h"
#include "render/AnimatedModel.h"
// Level stays a forward declaration, but Room is a value in this header's
// interface, so its definition has to be here
#include "world/Map.h"
#include "world/PathFinder.h"

#include <vector>

class AssetManager;
class Level;
class Player;
class LootManager;
class ProjectileManager;

//----------------------------------------------------------------------------------
// Every enemy in the level, and the thinking they do.
//
// Sight is still what decides a fight: walk at the player when it can see them,
// swing when close enough, and every one of those decisions is a straight line
// through Body and Level::ResolveBody, which slides a body along whatever it
// meets. Nothing routes while it can see where it is going, because a body that
// walks an A* path across an open room takes the corners of the grid rather than
// the line, and it reads as a machine rather than as a swordsman.
//
// What routing is for is everything else. Losing sight used to end the thought
// entirely - the enemy simply stopped, and any corner in the level beat it - and
// walking a beat was not possible at all. Both are paths: one to where the player
// was last seen, one from a room to the room next door and back.
//
// A linear scan is the right shape at this size. Bucketing enemies by grid cell
// is worth it in the hundreds, not the handful.
//----------------------------------------------------------------------------------
class EnemyManager
{
public:
    // Optional: without a model the enemies draw as the debug capsules they
    // always did, so the game runs with an empty assets/models/enemies
    void Load(AssetManager &assets);
    void Unload();

    // Fills every camp to its garrison at once, for the start of a level. After
    // this the camps top themselves up on their own, from Update.
    // `playerLevel` sets the tier of everything it places. Passed rather than
    // stored from a previous frame because this runs before the first Update on a
    // fresh level, and a garrison tiered against a stale level is a floor whose
    // tints are all wrong on the frame the player walks into it.
    void PopulateCamps(const Level &level, int playerLevel);

    // `projectiles` is where ranged archetypes put their arrows. Passed in rather
    // than owned: a shot outlives the enemy that fired it, and often the enemy
    // itself, so it belongs to the level's lifetime and not to this.
    // `level` is not const: a body that walks into a shut door pushes it open, the
    // same way an arrow does. Everything else here still asks the level questions
    // rather than telling it things.
    // `refill` is whether the camps may still send anyone. False once the floor's
    // chaos is quelled: there is nothing left to work through, so a camp that went
    // on refilling would be an endless supply of bodies that count for nothing -
    // which is the difference between a floor that ends and a floor that stops
    // being worth fighting on.
    //
    // Standing bodies are left standing. Wiping them would be tidier and reads
    // badly from inside a fight: an enemy that vanishes mid-swing is the game
    // taking a kill away, and a garrison that is already out is finite anyway.
    void Update(float delta, Level &level, Player &player, ProjectileManager &projectiles,
                bool refill);
    //------------------------------------------------------------------------------
    // One body, placed where an event wants it rather than where a camp does.
    //
    // `rankBonus` is added on top of what the floor's depth would have rolled, which
    // is the one dial that makes an event's pack harder than the trash in the
    // corridor outside it. `tag` marks the body as belonging to that event, so the
    // event can count what is still standing without holding pointers into a vector
    // that is compacted every frame.
    //
    // Returns the id, or 0 if nothing could be placed. Events check it: a wave that
    // silently spawned nothing would be a hunt with no enemies that could never be
    // completed.
    //------------------------------------------------------------------------------
    int SpawnEventEnemy(int type, Vector3 at, int depth, int playerLevel,
                        int rankBonus, int tag);

    // How many bodies of `tag` are still alive. What every event that spawns
    // anything asks to know whether it is finished.
    int AliveWithTag(int tag) const;

    // Kills everything carrying `tag`, silently - no exp, no chaos, no corpses left
    // animating. What a failed or abandoned event calls to take its own bodies off
    // the floor rather than leaving a pack the player can farm for a reward the
    // event itself no longer pays.
    void ClearTag(int tag);

    // Pays the player for everything that died this frame, and re-tiers the living
    // if that levelled them. MUST run before RemoveDead - see the note on the
    // definition for why it is not simply part of Update.
    //
    // Returns the total paid, which is also exactly what those kills quell of the
    // floor's chaos. One number and not two: an enemy that is worth more to kill IS
    // more of the floor dealt with, and any rate that let the two drift apart would
    // be a way to level without progressing or to progress without levelling.
    int CollectExp(Player &player);

    //------------------------------------------------------------------------------
    // Pays for the dead in the three currencies, and refills the mana pool.
    //
    // Split from CollectExp rather than folded into it, because the two answer to
    // different owners: experience and chaos are the floor's arithmetic and belong to
    // the manager, while coins, gems and the pool belong to the run. Handing this a
    // LootManager as well as a Player is what that split costs, and it is cheaper
    // than an EnemyManager that knows what a gem is worth.
    //
    // Reads the same `expPending` flag CollectExp does, so it MUST run in the same
    // window - after everything that can deal damage and before RemoveDead - and it
    // must run FIRST, since CollectExp is what clears the flag.
    //------------------------------------------------------------------------------
    void PayLoot(Player &player, LootManager &loot);

    void RemoveDead();
    // `camera` is for the buff auras, which are billboards - see DrawAuras. The
    // bodies themselves are models and do not need it.
    void Draw(const Camera3D &camera) const;

    std::vector<Enemy> &All() { return enemies; }
    const std::vector<Enemy> &All() const { return enemies; }
    int AliveCount() const;

    // Which rooms this floor's camps actually claimed, -1s included. For anything
    // that has to know a room is already spoken for without knowing the camp table
    // itself - see Game's "no room left with nothing in it" pass.
    const std::vector<int> &CampRooms() const { return campRooms; }

private:
    //------------------------------------------------------------------------------
    // One of these per row of Config::EnemyTypes, not per enemy. The shared
    // Model plus per-enemy bone arrays is the whole reason GPU skinning is on, so
    // twenty warriors still cost one model and one set of clips.
    //------------------------------------------------------------------------------
    struct LoadedType
    {
        AnimatedModel model;
        float scale = 1.0f;
        bool ready = false;

        // What it carries. Both slots are the same thing - a model and the bone it
        // hangs off - so a shield on handslot.l costs nothing the blade did not.
        struct HeldProp
        {
            Model *model = nullptr; // Owned by the AssetManager
            int bone = -1;          // Resolved by name, -1 for an empty slot
            // This prop's own grip, composed once from Config::EnemyPropGrips.
            // Per prop because the pack does not author them all the same way
            // round - the crossbow points down +Z where everything else is +Y.
            Matrix grip{};
        };

        HeldProp props[Config::EnemyPropSlots];

        // Resolved once at load, -1 when the pack has no clip by that name.
        // Everything that reads these goes through ClipFor, which keeps the -1 so
        // ClipOwnsBody can tell "played out" from "was never there"; the idle
        // fallback happens at the one place that draws, not here.
        int spawnClip = -1;
        int blockClip = -1;         // Loops, held while the guard is up
        int blockHitClip = -1;      // One-shot, a hit that the guard caught

        // Standing still. Two alternates so a camp of idlers is not one pose
        // repeated - the variant is rolled per body at spawn and kept, because an
        // enemy that reshuffled its idle every time it stopped would twitch.
        int idleClips[Config::EnemyIdleVariants] = { -1, -1 };

        // Moving. Two speeds, not two alternates: `runClip` is for closing on the
        // player and `walkClip` for everything slower - giving ground, stepping
        // out of a firing line, walking a cold trail. Running_A under a body
        // moving at half speed is the skating the walk clip exists to stop.
        int walkClip = -1;
        int runClip = -1;

        // Swings. [0] is the one every type is expected to have; the rest are
        // whatever else its row named, and a row naming one clip leaves the others
        // at -1 and always plays [0].
        int attackClips[Config::EnemyAttackVariants] = { -1, -1, -1 };
        int attackCount = 0;        // How many of the above actually resolved

        // Alternates, picked per hit and per death. [0] is the one every type is
        // expected to have; a missing [1] falls back to it.
        int hitClips[Config::EnemyHitVariants] = { -1, -1 };
        int deathClips[Config::EnemyDeathVariants] = { -1, -1 };
    };

    LoadedType &TypeOf(const Enemy &enemy) { return types[enemy.type]; }
    const LoadedType &TypeOf(const Enemy &enemy) const { return types[enemy.type]; }
    static const Config::EnemyArchetype &SpecOf(const Enemy &enemy) { return Config::EnemyTypes[enemy.type]; }

    void Separate();
    void PushOffPlayer(Player &player);
    // Final, enemy-only separation - the one that actually guarantees clearance
    void ClearOfPlayer(const Player &player);

    //------------------------------------------------------------------------------
    // Population. Each camp counts what it still has standing and sends one more
    // when it is short, on a timer of its own.
    //------------------------------------------------------------------------------
    void UpdateSpawning(float delta, const Level &level, const Player &player);

    // Whether `camp` may send a reinforcement this instant - the fairness rule,
    // as opposed to the bookkeeping in UpdateSpawning that decides it is short.
    bool CanReinforce(const Config::SpawnCamp &camp, int campIndex,
                      const Level &level, const Player &player) const;

    // Has this camp been given a room to hold, and where is its middle
    bool HasRoom(int campIndex) const;
    Vector3 CampCentre(int campIndex, const Level &level) const;

    // One body into the given camp. False when there was nowhere to put it, which
    // is a "try again shortly" rather than an error.
    bool SpawnAtCamp(int campIndex, const Level &level, int playerLevel);

    // Somewhere inside the camp to stand: on the floor, and not inside anyone who
    // is already there. False when CampPlacementTries attempts all failed.
    bool FindCampSlot(const Config::SpawnCamp &camp, Vector3 centre,
                      const Level &level, Vector3 &out) const;

    //------------------------------------------------------------------------------
    // Which generated room a camp should hold, or -1 to leave it unplaced.
    //
    // The camp table still says what a camp is made of - its garrison, its spread,
    // which archetypes it can send - because that is design. What it can no longer
    // say is WHERE, because a generated map has no fixed cell to author against.
    //
    // `avoid` is the vendor rooms for this floor - a shop is meant to be somewhere
    // safe to stand, and a camp claiming the same room as one puts a fight at the
    // counter. Threaded in as a plain list rather than read off a Level, because
    // this is called once per camp before there is anything else it needs the
    // level for.
    //------------------------------------------------------------------------------
    int ChooseCampRoom(int campIndex, const std::vector<Room> &rooms,
                       const std::vector<int> &avoid) const;

    // Living bodies belonging to a camp. A corpse does not hold a slot: the
    // replacement should be on its way while the body is still falling over.
    int GarrisonOf(int campIndex) const;

    // Builds a body of `type` at `position`, fully initialised and ready to be
    // pushed into the list. Shared by the opening fill and every reinforcement,
    // so the two cannot drift apart.
    // Rolls the rank off `depth`, tiers it against `playerLevel`, and resolves
    // both into the health, damage, scale and tint this body will carry
    Enemy MakeEnemy(int type, int campIndex, Vector3 position, int depth,
                    int playerLevel) const;

    // Re-buckets every living body's tier against the player's current level, and
    // rebuilds what the tier decides. The RANK does not move - that body is the
    // body it was rolled as - but the distance from the player to it does, every
    // time they level, and the tint has to say so.
    //
    // Health is scaled by the ratio rather than reset, so a body caught halfway
    // through a fight keeps the fraction of its pool it had left. Resetting it
    // would heal anything the player was in the middle of killing at the exact
    // moment they were rewarded for killing things.
    void RetierAll(int playerLevel);

    // The melee blow, landed partway through its own clip rather than on the frame
    // the swing began - see the note on the definition for why that was a bug.
    void LandMelee(Enemy &enemy, Player &player);

    // One raider, one frame - see the note on the definition. Nothing in the
    // ordinary think applies to it, so it does not run any of it.
    void UpdateRaider(Enemy &enemy, float delta, Level &level);

    // FLAME's one jump: hands `from`'s burn to the nearest other living enemy
    // within Config::FlameSpreadRadius that is not already burning, or does
    // nothing if there is none. Called once, the moment `from`'s own burn expires
    // - see the note on the call site.
    void SpreadBurnFrom(Enemy &from);

    //------------------------------------------------------------------------------
    // Would channelling be worth it right now?
    //
    // Counts the living allies inside the buff radius that are not already under
    // one. Already-buffed bodies do not count towards the threshold, so a supporter
    // does not re-cast over a pack it has just finished buffing - which without this
    // is exactly what it would do, forever, every time its cooldown came up.
    //------------------------------------------------------------------------------
    int AlliesWorthBuffing(const Enemy &caster) const;

    // The channel finished. Buffs every living ally in range - never the caster
    // itself, see the note on EnemyArchetype::support.
    void ApplyBuff(const Enemy &caster);

    // The auras: a pool of light under every buffed body, and a brighter one under a
    // caster mid-channel. Billboards, hence the camera.
    void DrawAuras(const Camera3D &camera) const;

    // What this body hits for right now - its resolved damage, raised if it is under
    // a buff. Every path that deals enemy damage goes through it, so a buff cannot
    // apply to the swing and not to the shot.
    static int BuffedDamage(const Enemy &enemy);

    // The level everything alive is currently tiered against. Compared rather than
    // trusted: re-tiering walks every body, which is not something to do sixty
    // times a second for an answer that changes a few times a run.
    int lastPlayerLevel = 1;

    // The soft round light the auras are drawn out of. Shared with the motes and the
    // portal, owned by the AssetManager - see render/Glow.h.
    Texture2D *glow = nullptr;

    //------------------------------------------------------------------------------
    // Whether `shooter` can actually put a shot into the player right now.
    //
    // Tested along the line the projectile will really travel - the muzzle to the
    // player's eye - and against everything that line can meet: the wall grid, and
    // every other living enemy's capsule. Awareness still uses centre-to-eye, and
    // the two lines are not the same one: the muzzle is held out to the side and
    // low, so an enemy at a corner can see the player perfectly well and still
    // have nothing but doorframe in front of its crossbow.
    //
    // That gap is the point. Without it a ranged enemy fires into cover, or into
    // the back of the warrior standing in front of it, and looks like it cannot
    // see what the player can plainly see.
    //------------------------------------------------------------------------------
    bool HasClearShot(const Enemy &shooter, const Player &player, const Level &level) const;

    // The living enemy nearest the muzzle that is standing in the shot, or null
    // when nothing is - which includes the case where a wall is the problem, so a
    // null here does not mean the line is clear. HasClearShot answers that.
    const Enemy *FirstBlocker(const Enemy &shooter, const Player &player) const;

    //------------------------------------------------------------------------------
    // Whether the player is in front of this enemy's eyes right now.
    //
    // Three things, in the order they get cheaper to be wrong about: close enough,
    // inside the cone it is facing, and nothing in the way. Being IN SIGHT is not
    // the same as being NOTICED - that is the detection meter, which this feeds.
    //
    // Not the same line HasClearShot tests either: this runs from the chest, and
    // that one from the muzzle held out to the side. An enemy can see the player
    // perfectly well and have nothing but doorframe in front of its crossbow.
    //------------------------------------------------------------------------------
    bool CanSee(const Enemy &enemy, const Player &player, const Level &level) const;

    //------------------------------------------------------------------------------
    // Everyone who was hurt or killed since the last frame shouts, and everyone in
    // earshot hears it. Called once per frame, before anyone thinks, so a body can
    // act on a noise on the same frame the blow that made it landed.
    //
    // This is what makes a fight a fight rather than a series of private duels: a
    // camp whose garrison is being cut down one at a time in the next room used to
    // stand there facing the wrong way until the player walked into view.
    //
    // Quadratic in the enemy count, which is the right shape at this size for the
    // same reason every other scan in this file is.
    //------------------------------------------------------------------------------
    void SpreadCries();

    //------------------------------------------------------------------------------
    // Everyone already fighting draws in whoever is standing next to them.
    //
    // SpreadCries only carries as far as somebody being HURT - a body that has not
    // been hit yet makes no noise at all - so a guard could have a swordfight in
    // the middle of a room while the two beside it watched the wall. This is the
    // other half: a fight is loud, and being in the room with one is enough.
    //
    // Fills the detection meter rather than declaring anyone aware, so it reads as
    // heads turning rather than as a switch, and so an enemy that then sees
    // nothing settles back down.
    //------------------------------------------------------------------------------
    void SpreadAlarm(float delta, const Level &level);

    // Fills or drains Enemy::detection for this frame. `distance` is flat, and is
    // only read when `visible` - the rate depends on how far off the player is,
    // and the drain deliberately does not.
    static void UpdateDetection(Enemy &enemy, float delta, bool visible, float distance);

    //------------------------------------------------------------------------------
    // What a hunter does once it reaches the last place it saw the player and
    // finds nothing there.
    //
    // Called every frame it is standing on a cold trail. Enemy::alertTime is still
    // counting down throughout, so whatever this does is already time-boxed and
    // does not need its own way of giving up. Set `enemy.yaw` and `move` - the
    // caller has zeroed `move` and will run the feet and the animation from it.
    //------------------------------------------------------------------------------
    void SearchAtColdTrail(Enemy &enemy, float delta, Vector2 &move) const;

    //------------------------------------------------------------------------------
    // Routing. Three functions, and the split between them is what a body knows
    // versus what it wants.
    //------------------------------------------------------------------------------

    // Make sure `enemy.route` leads to `goal`, asking the pathfinder only when the
    // stored route has run out, the goal has moved, or the interval is up. False
    // when there is no route at all - which callers answer by walking the straight
    // line they always used to, not by standing still.
    bool Repath(Enemy &enemy, const Level &level, Vector3 goal, float delta);

    // Turn the body toward its next waypoint and report the drive to walk at, or
    // zero when the route is spent. `loop` turns round at the ends instead of
    // stopping, which is the only difference between a chase and a beat.
    float FollowRoute(Enemy &enemy, float delta, Level &level, float drive, bool loop);

    // What a body does with nobody to chase: walk its beat, or go back to the post
    // it left during one. Returns the forward drive.
    float Patrol(Enemy &enemy, float delta, Level &level);

    // The beat itself - this room to the nearest other one - worked out once and
    // then walked end to end for as long as the body lives
    void BuildPatrolRoute(Enemy &enemy, const Level &level);

    // Which of a type's attack clips to swing, given how far off the player is.
    // `loaded.attackCount` is how many it actually has, always at least 1.
    static int AttackVariantFor(const LoadedType &loaded, float distance);

    // Which way to step out of a blocked line, +1 right / -1 left. Called only
    // when the block first appears; the answer is then held in Enemy::strafeDir.
    // No Level: what a wall is doing cannot change the answer, because the only
    // thing there is to step away from is a body.
    float ChooseStrafeDirection(const Enemy &shooter, const Player &player) const;

    // True while a one-shot clip is running the body. Asked by both the animation
    // and the feet, so the two cannot disagree about whether it may walk.
    bool ClipOwnsBody(const Enemy &enemy) const;
    // Everything that stops the feet: a one-shot clip, or a raised guard. Defined
    // in terms of ClipOwnsBody rather than beside it, so the two cannot drift -
    // §2.9 of docs/enemy-animation-plan.md is what happens when they do.
    bool FeetPinned(const Enemy &enemy) const;
    // Picks the clip for what the enemy is doing and advances it.
    //
    // `drive` is the forward input the feet were given this frame, signed: it has
    // to be the signed value and not a "moving" flag, because a ranged enemy
    // giving ground is moving as surely as one closing, and a bool cannot say
    // which way the cycle should run.
    void UpdateAnimation(Enemy &enemy, float delta, float drive);
    // Where in a clip to sample, which is not simply the time elapsed: a reversed
    // walk reads the cycle from the far end.
    static float SampleTime(const LoadedType &loaded, EnemyAnim state, int variant,
                            float time, bool reversed);
    // True once the corpse is settling under physics rather than playing a clip
    bool UpdateRagdoll(Enemy &enemy, float delta);
    // The clip a state actually names, or -1 if this type has none. No fallback:
    // "the pack has no hit reaction" and "the hit reaction has finished" have to
    // stay distinguishable, or a missing clip pins the body forever.
    static int ClipFor(const LoadedType &loaded, EnemyAnim state, int variant);
    // ...and the same with the idle fallback applied, for the two places that need
    // something to draw rather than an answer about ownership
    static int ClipOrIdle(const LoadedType &loaded, EnemyAnim state, int variant);
    // The death clip this particular corpse is playing, which is not a property of
    // the type: Death_A and Death_B differ by 1.8 seconds, and both the ragdoll
    // handover and how long the body lingers are measured off it
    int DeathClipOf(const Enemy &enemy) const;

    void LoadType(AssetManager &assets, int type);
    void LoadProp(AssetManager &assets, int type, int slot);
    // What it carries, drawn from bones of the pose the enemy already has
    void DrawProps(const Enemy &enemy) const;
    // Where a prop slot has ended up in the world, for anything that has to come
    // out of it. Falls back to the enemy's centre when the slot is empty, so a
    // shot always has somewhere to start.
    Vector3 PropMuzzle(const Enemy &enemy, int slot) const;
    // Puts an arrow in the air, partway through the shoot clip
    void ReleaseShot(Enemy &enemy, const Player &player, ProjectileManager &projectiles) const;

    std::vector<Enemy> enemies;
    // Never reused, never reset by RemoveDead: an id that came back round would let
    // a swing that already cut a dead enemy refuse to cut the one that replaced it
    int nextId = 1;
    // Counts down to the next reinforcement, one per Config::SpawnCamps row. Held
    // at the full delay while a camp is at strength, so the first loss from a
    // quiet camp waits the same time as any other rather than being replaced
    // instantly by a timer that has been running down unused.
    std::vector<float> campTimers;

    // The room each camp holds, one entry per camp, -1 when the map had none to
    // give it. Resolved once at PopulateCamps and then fixed, so a camp
    // reinforces into the room it has been fighting for rather than wandering.
    std::vector<int> campRooms;

    // Sized once in Load and never resized again: AnimatedModel owns raw clip
    // arrays, so a reallocation would leave the copies pointing at freed memory
    std::vector<LoadedType> types;

    // One searcher shared by every enemy, so its working arrays are allocated once
    // rather than per query. Nothing is kept between calls, so sharing it costs
    // nothing but the allocation it saves.
    PathFinder paths;
};
