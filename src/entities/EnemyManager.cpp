#include "entities/EnemyManager.h"

#include "combat/Attack.h"      // InCone, for the vision arc
#include "combat/Collider.h"
#include "combat/Projectile.h"
#include "core/Config.h"
#include "entities/Player.h"
#include "raymath.h"
#include "render/AssetManager.h"
#include "render/Beam.h"
#include "render/Glow.h"
#include "world/Level.h"
#include "world/Loot.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>

namespace
{
    //------------------------------------------------------------------------------
    // Whether a melee land-fraction has to read Config::EnemySliceMeleeLand
    // instead of the shared Config::EnemyMeleeLand - see the note there. A
    // slice's arm sweeps through its arc early and follows through for the
    // rest of the clip doing nothing, where a chop's weapon is still closing
    // on the target for most of it, so the two need different fractions and
    // the clip's own name is what tells them apart.
    //------------------------------------------------------------------------------
    bool IsSliceClip(const char *name)
    {
        return (name != nullptr) && (strstr(name, "slice") != nullptr);
    }

    //------------------------------------------------------------------------------
    // The grip for one prop, by path.
    //
    // Identity for anything the table does not name, which is nearly everything:
    // KayKit authors this pack's props along +Y and handslot's local +Y is the
    // out-of-the-fist direction, so they need no correction. See the measurements
    // above Config::EnemyPropGrips for the one that does.
    //------------------------------------------------------------------------------
    Matrix GripFor(const std::string &path)
    {
        const Config::PropGrip *found = &Config::EnemyPropGripDefault;

        for (int i = 0; i < Config::EnemyPropGripCount; i++)
        {
            if (path.find(Config::EnemyPropGrips[i].match) == std::string::npos) continue;

            found = &Config::EnemyPropGrips[i];
            break;
        }

        return MatrixMultiply(MatrixMultiply(MatrixScale(found->scale, found->scale, found->scale),
                                             MatrixRotateXYZ({ found->pitch*DEG2RAD,
                                                               found->yaw*DEG2RAD,
                                                               found->roll*DEG2RAD })),
                              MatrixTranslate(found->offsetX, found->offsetY, found->offsetZ));
    }

    // Idling, moving and holding a guard cycle; emerging, a swing, a flinch and a
    // death play once and hold
    bool Loops(EnemyAnim state)
    {
        return (state == EnemyAnim::Idle) || (state == EnemyAnim::Walk) ||
               (state == EnemyAnim::Block);
    }
}

//----------------------------------------------------------------------------------
// Clip names differ between character packs, so each state is a list of names
// tried in order rather than one exact string. Lowercase: FindClip folds case.
//
// Chasing uses a run before a walk - an enemy closes at Config::EnemySpeed, which
// is a run, and a walk cycle under it looks like skating.
//----------------------------------------------------------------------------------
void EnemyManager::Load(AssetManager &assets)
{
    // Sized once, and never again - see the note on `types`
    types.resize(Config::EnemyTypeCount);

    // For the buff auras. Shared with the motes and the portal, so there is nothing
    // here to release.
    glow = &GlowTexture(assets);

    for (int i = 0; i < Config::EnemyTypeCount; i++) LoadType(assets, i);
}

//----------------------------------------------------------------------------------
// One archetype: its character, its clips and whatever it holds.
//
// A type that fails to load is left not ready rather than aborting the level - it
// falls back to the debug capsules, so a missing file costs you the art for that
// one kind of enemy and nothing else.
//----------------------------------------------------------------------------------
void EnemyManager::LoadType(AssetManager &assets, int type)
{
    const Config::EnemyArchetype &spec = Config::EnemyTypes[type];
    LoadedType &loaded = types[type];

    const std::vector<std::string> animPaths(std::begin(Config::EnemyAnimPaths),
                                             std::end(Config::EnemyAnimPaths));

    if (!loaded.model.Load(assets, spec.modelPath, animPaths))
    {
        TraceLog(LOG_INFO, "ENEMIES: %s has no rigged model, drawing debug capsules", spec.name);
        return;
    }

    loaded.scale = loaded.model.FitScaleFor(spec.height)*Config::EnemyModelScale;
    loaded.ready = true;

    // FindClip matches substrings, and KayKit ships Death_A_Pose next to Death_A
    // and a T-Pose in every file - single frames that a substring match will
    // happily return if the pack is ever reordered. Nothing that short is an
    // animation, so reject it loudly rather than hold it as a death pose.
    auto resolve = [&](const std::vector<std::string> &candidates) -> int
    {
        const int clip = loaded.model.FindClip(candidates);

        if ((clip >= 0) && (loaded.model.ClipDuration(clip) < Config::EnemyMinClipDuration))
        {
            TraceLog(LOG_WARNING, "ENEMIES: %s matched '%s' to a %.3fs clip - too short to be one, rejected",
                     spec.name, candidates[0].c_str(), loaded.model.ClipDuration(clip));

            return -1;
        }

        return clip;
    };

    // The archetype's own choice first, then the shared fallbacks, so a name that
    // does not resolve costs the right swing rather than the whole enemy
    loaded.idleClips[0] = resolve({ spec.idleClip, "idle_a", "idle", "stand" });
    loaded.idleClips[1] = resolve({ "idle_b" });
    loaded.spawnClip    = resolve({ "spawn_ground", "spawn", "emerge" });

    // Two gaits. The run is what closing looks like; the walk is everything
    // slower, and until now it was never loaded at all - every enemy ran, at
    // whatever speed its feet happened to be moving.
    loaded.runClip    = resolve({ "running_a", "running", "walking_a", "walk", "move" });
    loaded.walkClip   = resolve({ "walking_a", "walking", "walk", "running_a", "move" });

    // Every swing the row named, compacted: a "" or an unresolvable name simply
    // does not take a slot, so attackCount is how many there really are and the
    // roll never has to skip a hole. The fallback chain applies to the first slot
    // only - a type with no attack at all is a bigger problem than a missing
    // alternate, and the alternates are allowed to be absent by design.
    loaded.attackCount = 0;

    for (int i = 0; i < Config::EnemyAttackVariants; i++)
    {
        const char *name = spec.attackClips[i];
        if ((name == nullptr) || (name[0] == '\0')) continue;

        const int clip = (i == 0)
                       ? resolve({ name, "melee_1h_attack_chop", "melee_unarmed_attack_punch",
                                   "melee_attack", "attack" })
                       : resolve({ name });

        if (clip >= 0) loaded.attackClips[loaded.attackCount++] = clip;
        else TraceLog(LOG_WARNING, "ENEMIES: %s has no clip '%s', variant dropped", spec.name, name);
    }

    // Only worth looking for on something that will ever raise a guard. A type
    // with blockChance 0 leaves these at -1, which is also what makes Block
    // unreachable for it - see the guard decision in Update.
    if (spec.blockChance > 0.0f)
    {
        loaded.blockClip    = resolve({ "blocking", "block_idle", "guard" });
        loaded.blockHitClip = resolve({ "block_hit", "block_impact" });
    }

    loaded.hitClips[0]   = resolve({ "hit_a", "hit_react", "hit", "damage" });
    loaded.hitClips[1]   = resolve({ "hit_b" });
    loaded.deathClips[0] = resolve({ "death_a", "death", "dying", "die" });
    loaded.deathClips[1] = resolve({ "death_b" });

    // Loud, because a clip that silently missed is the likeliest reason an enemy
    // stands still or swings the wrong way
    if (loaded.model.FindClip({ spec.attackClips[0] }) < 0)
    {
        TraceLog(LOG_WARNING, "ENEMIES: %s wanted attack clip '%s', fell back to %i",
                 spec.name, spec.attackClips[0], loaded.attackClips[0]);
    }

    // A cooldown shorter than the swing it has to contain means the next attack is
    // already due when the clip ends, and the state never gets a gap to do
    // anything else in - which is how three rows ended up unable to block
    for (int i = 0; i < loaded.attackCount; i++)
    {
        const float length = loaded.model.ClipDuration(loaded.attackClips[i]);

        if (length > spec.cooldown)
        {
            TraceLog(LOG_WARNING, "ENEMIES: %s variant %i is %.2fs against a %.2fs cooldown",
                     spec.name, i, length, spec.cooldown);
        }
    }

    if ((spec.blockChance > 0.0f) && (loaded.blockClip < 0))
    {
        TraceLog(LOG_WARNING, "ENEMIES: %s blocks %.0f%% of the time but has no guard clip - it will never block",
                 spec.name, spec.blockChance*100.0f);
    }

    TraceLog(LOG_INFO, "ENEMIES: %-8s scale %.3f  clips idle=%i/%i walk=%i run=%i spawn=%i attacks=%i",
             spec.name, loaded.scale, loaded.idleClips[0], loaded.idleClips[1],
             loaded.walkClip, loaded.runClip, loaded.spawnClip, loaded.attackCount);
    TraceLog(LOG_INFO, "ENEMIES: %-8s          hit=%i/%i death=%i/%i block=%i blockHit=%i",
             spec.name, loaded.hitClips[0], loaded.hitClips[1],
             loaded.deathClips[0], loaded.deathClips[1], loaded.blockClip, loaded.blockHitClip);

    for (int slot = 0; slot < Config::EnemyPropSlots; slot++) LoadProp(assets, type, slot);
}

// A slot stays empty unless both the prop and the bone to hang it from resolve
void EnemyManager::LoadProp(AssetManager &assets, int type, int slot)
{
    const Config::EnemyArchetype &spec = Config::EnemyTypes[type];
    LoadedType &loaded = types[type];

    const char *wantPath = (slot == 0) ? spec.propPath : spec.offhandPath;
    const char *wantBone = (slot == 0) ? spec.propBone : spec.offhandBone;

    LoadedType::HeldProp &held = loaded.props[slot];
    held.model = nullptr;
    held.bone = -1;

    const std::string path = (wantPath != nullptr) ? wantPath : "";
    if (path.empty()) return;

    if (!FileExists(AssetManager::Resolve(path).c_str()))
    {
        TraceLog(LOG_WARNING, "ENEMIES: %s prop %s not found, slot %i stays empty",
                 spec.name, path.c_str(), slot);
        return;
    }

    Model &propModel = assets.GetModel(path);

    if (propModel.meshCount <= 0)
    {
        TraceLog(LOG_WARNING, "ENEMIES: %s prop %s has no mesh", spec.name, path.c_str());
        return;
    }

    // By name, every time: bone order differs between characters in this very pack
    const int bone = loaded.model.FindBone(wantBone);

    if (bone < 0)
    {
        TraceLog(LOG_WARNING, "ENEMIES: %s rig has no bone '%s', slot %i stays empty",
                 spec.name, wantBone, slot);
        return;
    }

    held.model = &propModel;
    held.bone = bone;
    held.grip = GripFor(path);

    TraceLog(LOG_INFO, "ENEMIES: %-8s carries %-28s on '%s' (bone %i)",
             spec.name, GetFileName(path.c_str()), wantBone, bone);
}

void EnemyManager::Unload()
{
    for (LoadedType &loaded : types) loaded.model.Unload();

    types.clear();
}

Enemy EnemyManager::MakeEnemy(int type, int campIndex, Vector3 position, int depth,
                              int playerLevel) const
{
    const Config::EnemyArchetype &spec = Config::EnemyTypes[type];

    Enemy enemy;
    enemy.type = type;
    enemy.camp = campIndex;
    enemy.body.position = position;

    //------------------------------------------------------------------------------
    // What this one is, on top of what its kind is.
    //
    // Three things multiply together and the order matters for readability rather
    // than for the arithmetic: the KIND's table row, scaled by the RANK the floor
    // rolled, scaled again by the TIER that rank sits at relative to the player,
    // and finally raised by the kind's own stat line.
    //
    // The stat line comes last and is NOT touched by rank, deliberately - see the
    // note on EnemyArchetype::stats. A Minion is fragile because it is a Minion,
    // and rolling it deep makes it a tougher Minion rather than a different kind.
    //------------------------------------------------------------------------------
    enemy.rank = RollRankForDepth(depth);
    enemy.tier = TierFor(enemy.rank, playerLevel);
    enemy.stats = spec.stats;

    const EnemyTierDef &tier = TierAt(enemy.tier);

    const int ranked = RankedHealth(spec.maxHealth, enemy.rank);
    const int health = (int)(ranked*tier.health + 0.5f)
                     + StatBonusHealth(spec.stats, ranked);

    enemy.maxHealth = (health < 1) ? 1 : health;
    enemy.health = enemy.maxHealth;

    // Resolved once, here, rather than per swing. A body's strength must not change
    // halfway through a fight because the player levelled mid-swing - the tier
    // moves, and everything the tier decides has to move with it deliberately
    // (see RetierAll) rather than by being recomputed wherever it is read.
    const int rankedDamage = RankedDamage(spec.damage, enemy.rank);
    const int damage = (int)(rankedDamage*tier.damage + 0.5f)
                     + StatBonusDamage(spec.stats, rankedDamage);

    enemy.damage = (damage < 1) ? 1 : damage;

    // The kind's own worth, scaled by the rank. NOT by the tier: a tier is the
    // distance from the player, and paying more for a body that is only worth more
    // because the player is behind it would pay them for being behind.
    enemy.exp = RankedExp(spec.exp, enemy.rank);

    // The tier's own look. Scale carries the body with it - a champion that was
    // drawn larger and hit the same size would be a lie the player could walk into.
    enemy.scale = tier.scale;
    enemy.tint = tier.tint;
    enemy.height = spec.height*tier.scale;
    enemy.body.radius *= tier.scale;

    enemy.body.maxSpeed = spec.speed*tier.speed;

    // Facing is rolled rather than shared, so a camp reads as a group of bodies
    // standing about and not as a firing squad waiting for you
    enemy.yaw = (GetRandomValue(0, 3600)/3600.0f)*2.0f*PI;

    // Climbs out of the floor rather than appearing mid-stride. StartAnim and
    // not PlayAnim: there is nothing to fade from, and fading in from the
    // default idle pose would stand it up before dropping it back down.
    enemy.StartAnim(EnemyAnim::Spawn);

    // So the first gap between swings is not the same decision for every
    // enemy of the same kind
    enemy.blockRoll = GetRandomValue(0, 1000)/1000.0f;

    // Which idle it stands in, for life. Rolled here rather than on entering the
    // state so a body keeps one resting pose instead of flicking between two.
    enemy.idleVariant = GetRandomValue(0, Config::EnemyIdleVariants - 1);

    //------------------------------------------------------------------------------
    // Does this one walk a beat?
    //
    // Counted against what the camp already has out rather than rolled, so a camp
    // asked for one patroller has exactly one however many times it is refilled -
    // and so a camp whose patroller was killed sends another rather than quietly
    // becoming a camp of sentries.
    //------------------------------------------------------------------------------
    if (campIndex >= 0)
    {
        int walking = 0;

        for (const Enemy &other : enemies)
            if (other.IsAlive() && (other.camp == campIndex) && other.patrols) walking++;

        enemy.patrols = (walking < Config::SpawnCamps[campIndex].patrollers);
    }

    // Staggered, so a garrison of three does not do all its route thinking on the
    // same frame for the rest of the level
    enemy.repathTimer = (GetRandomValue(0, 1000)/1000.0f)*Config::PathRepathInterval;

    return enemy;
}

int EnemyManager::GarrisonOf(int campIndex) const
{
    int count = 0;

    for (const Enemy &enemy : enemies)
    {
        if (enemy.IsAlive() && (enemy.camp == campIndex)) count++;
    }

    return count;
}

//----------------------------------------------------------------------------------
// Somewhere inside a camp to stand.
//
// Rejection sampling rather than a ring or a grid: a camp centred near a wall has
// an awkward shape, and throwing darts at it and discarding the bad ones handles
// every shape there is without knowing what any of them are. The try count is what
// stops that being unbounded.
//
// Bodies are checked against each other because two spawning in the same place
// would be shoved apart by Separate on the next frame, which looks like they were
// fired out of a cannon.
//----------------------------------------------------------------------------------
bool EnemyManager::FindCampSlot(const Config::SpawnCamp &camp, Vector3 centre,
                                const Level &level, Vector3 &out) const
{
    for (int attempt = 0; attempt < Config::CampPlacementTries; attempt++)
    {
        // Uniform over the disc needs the square root - without it every body
        // bunches toward the middle, which is the one place they must not be
        const float angle = (GetRandomValue(0, 3600)/3600.0f)*2.0f*PI;
        const float radius = camp.spread*sqrtf(GetRandomValue(0, 1000)/1000.0f);

        Vector3 spot = { centre.x + cosf(angle)*radius,
                         level.FloorHeight(),
                         centre.z + sinf(angle)*radius };

        if (level.Grid().SolidAtWorld(spot.x, spot.z)) continue;

        // ...and not inside the furniture. This test used to be floor-or-not and
        // nothing else, which was correct right up until the rooms had tables in
        // them: a camp room is exactly the sort of room the dressing pass fills,
        // so without this a good share of every garrison stands up inside a crate.
        if (level.PropBlocksAt(spot, Config::PathClearRadius)) continue;

        bool crowded = false;

        for (const Enemy &enemy : enemies)
        {
            if (!enemy.IsAlive()) continue;

            const float dx = enemy.body.position.x - spot.x;
            const float dz = enemy.body.position.z - spot.z;
            const float clearance = enemy.body.radius*2.0f;

            if ((dx*dx + dz*dz) < clearance*clearance) { crowded = true; break; }
        }

        if (crowded) continue;

        out = spot;

        return true;
    }

    return false;
}

//----------------------------------------------------------------------------------
// Which swing an enemy throws this time.
//
// Called once per attack, on the frame it starts. The result indexes
// loaded.attackClips and is what PlayAnim carries as its variant, so it picks both
// the animation and - through ClipDuration - how long the body is committed.
//----------------------------------------------------------------------------------
int EnemyManager::AttackVariantFor(const LoadedType &loaded, float distance)
{
    if (loaded.attackCount <= 1) return 0;

    // Close in. Clips are ordered as the row named them, so for the Reaver that is
    // { chop, slice, spin } - and only a type with all three can be asked for one.
    if (distance < Config::EnemyCloseRange)
    {
        if (loaded.attackCount > 2)
        {
            // The spin is the only one that can reach a target right on top of it,
            // so give it a chance when the player is that close. The other two are
            // both too short to reach, and the Reaver's chop is too slow to be
            // useful at that range.
            const int roll = GetRandomValue(0, 1000);

            if (roll < 500) return 2;           // spin
            else if (roll < 750) return 1;      // slice

            return 0;                           // chop
        }

        return GetRandomValue(0, loaded.attackCount - 1);
    }

    // At range, where the long committed swings are a liability, everything falls
    // back to the row's first clip. Every melee type now carries at least two, so
    // this is the obvious place to spread them if the fight starts to look
    // repetitive from a distance.
    return 0;
}

void EnemyManager::SpreadCries()
{
    const float earshot = Config::EnemyHearingRange;

    for (size_t i = 0; i < enemies.size(); i++)
    {
        // Read and cleared before anything else, corpse included: a body that has
        // just been killed shouts exactly once, on its way down
        const float loudness = enemies[i].unheardCry;
        enemies[i].unheardCry = 0.0f;

        if (loudness <= 0.0f) continue;

        const Vector3 at = enemies[i].body.position;

        for (size_t j = 0; j < enemies.size(); j++)
        {
            if (j == i) continue;
            if (!enemies[j].IsAlive()) continue;

            const float dx = at.x - enemies[j].body.position.x;
            const float dz = at.z - enemies[j].body.position.z;
            const float distance = sqrtf(dx*dx + dz*dz);

            if (distance >= earshot) continue;

            // Straight falloff to nothing at the edge. No line of sight test:
            // that is the point of hearing - a wall stops you seeing a fight and
            // does very little to stop you hearing one.
            enemies[j].Hear(at, loudness*(1.0f - distance/earshot));
        }
    }
}

bool EnemyManager::CanSee(const Enemy &enemy, const Player &player, const Level &level) const
{
    const Vector3 from = enemy.Center();
    const Vector3 eye = player.EyePosition();

    const float dx = eye.x - from.x;
    const float dz = eye.z - from.z;
    const float distance = sqrtf(dx*dx + dz*dz);

    if (distance > Config::EnemyAggroRange) return false;

    // The arc, except at arm's length. InCone is given the full aggro range as its
    // reach because the distance has already been decided above - what is wanted
    // from it here is purely the angle, the same way Enemy::TakeDamageFrom uses it.
    if ((distance > Config::EnemyViewNear) &&
        !InCone(from, enemy.Forward(), eye, Config::EnemyAggroRange, Config::EnemyViewCone))
    {
        return false;
    }

    return level.LineOfSight(from, eye);
}

//----------------------------------------------------------------------------------
// The meter. Up while the player is in sight, down while they are not.
//----------------------------------------------------------------------------------
void EnemyManager::UpdateDetection(Enemy &enemy, float delta, bool visible, float distance)
{
    if (!visible)
    {
        enemy.detection = fmaxf(enemy.detection - Config::EnemyDetectDecay*delta, 0.0f);

        return;
    }

    // Already looking for the player: seeing them again confirms what it went
    // there for rather than discovering it. Without this an enemy that loses the
    // player round a pillar has to re-earn its own alert every time they lean out,
    // which leaves it permanently one beat behind a player who keeps moving.
    if (enemy.alertTime > 0.0f)
    {
        enemy.detection = 1.0f;

        return;
    }

    // Straight line between the two rates across aggro range
    const float reach = fmaxf(Config::EnemyAggroRange, 1e-3f);
    const float closeness = 1.0f - fminf(distance/reach, 1.0f);
    const float rate = Config::EnemyDetectFar +
                       (Config::EnemyDetectNear - Config::EnemyDetectFar)*closeness;

    enemy.detection = fminf(enemy.detection + rate*delta, 1.0f);
}

void EnemyManager::SearchAtColdTrail(Enemy &enemy, float delta, Vector2 &move) const
{
    // Turn on the spot and look. Cheap, and it does real work rather than just
    // filling the time: the awareness raycast runs every frame from wherever this
    // leaves the enemy facing, so a sweep genuinely reacquires a player who is
    // standing somewhere it could not see from the angle it arrived at.
    //
    // The feet stay still deliberately. `move` is left at zero, so the caller
    // animates this as an idle, which is what someone stopping to look around is.
    enemy.yaw += Config::EnemySearchTurnRate*DEG2RAD*delta;

    (void)move;
}

//----------------------------------------------------------------------------------
// Where a camp stands: the middle of the room it holds.
//
// Rooms are rectangles, so the middle is always inside one - unlike a centroid of
// anything more interesting, which is exactly the sort of thing that starts
// putting camps inside walls again.
//----------------------------------------------------------------------------------
bool EnemyManager::HasRoom(int campIndex) const
{
    return (campIndex >= 0) && (campIndex < (int)campRooms.size()) && (campRooms[campIndex] >= 0);
}

Vector3 EnemyManager::CampCentre(int campIndex, const Level &level) const
{
    if (!HasRoom(campIndex)) return level.SpawnPoint();

    const Room &room = level.Grid().Rooms()[campRooms[campIndex]];

    return level.Grid().CellCenter(room.CenterX(), room.CenterZ());
}

//----------------------------------------------------------------------------------
// Whether a camp that is short may send someone right now.
//
// Purely about fairness, not about bookkeeping: UpdateSpawning has already decided
// the camp is short and the clock is up. This is the rule that stops a body
// climbing out of the floor somewhere the player would rather it did not.
//----------------------------------------------------------------------------------
bool EnemyManager::CanReinforce(const Config::SpawnCamp &camp, int campIndex,
                                const Level &level, const Player &player) const
{
    const Vector3 centre = CampCentre(campIndex, level);

    // Measured from the near edge of the camp rather than its middle. FindCampSlot
    // scatters a body up to `spread` from the centre, and it may scatter it
    // straight at the player - so the centre passing by a hair means the actual
    // arrival can be a whole spread inside the distance this is protecting.
    const float distance = Vector3Distance(centre, player.Position()) - camp.spread;

    // Far enough away that whatever happens there is not the player's business
    if (distance > Config::CampSafeDistance) return true;

    // Close, but out of sight. Cover is what makes a near spawn acceptable: the
    // body is up and walking by the time anyone lays eyes on it, which is the
    // entire thing this rule protects. Without this clause a camp one wall away
    // stays empty forever while the player fights next door, which reads as the
    // level running out of enemies rather than as the level being fair.
    return !level.LineOfSight(centre, player.EyePosition());
}

int EnemyManager::SpawnEventEnemy(int type, Vector3 at, int depth, int playerLevel,
                                  int rankBonus, int tag)
{
    if ((type < 0) || (type >= Config::EnemyTypeCount)) return 0;

    Enemy enemy = MakeEnemy(type, -1, at, depth, playerLevel);

    //------------------------------------------------------------------------------
    // Re-rolled at a deeper centre rather than nudged.
    //
    // MakeEnemy has already resolved health, damage and tier off the rank it rolled,
    // so raising `rank` here would leave every one of those describing the rank it
    // used to be. Building it again at a depth `rankBonus` steps further down is the
    // one call that keeps all four in step - and it costs nothing, because MakeEnemy
    // is arithmetic and a couple of random draws.
    //------------------------------------------------------------------------------
    if (rankBonus > 0)
    {
        enemy = MakeEnemy(type, -1, at, depth + rankBonus, playerLevel);
    }

    enemy.id = nextId++;
    enemy.eventTag = tag;

    enemies.push_back(enemy);

    return enemy.id;
}

int EnemyManager::AliveWithTag(int tag) const
{
    int count = 0;

    for (const Enemy &enemy : enemies)
    {
        if (enemy.IsAlive() && (enemy.eventTag == tag)) count++;
    }

    return count;
}

void EnemyManager::ClearTag(int tag)
{
    // Erased rather than killed. Killing them would set expPending and leave the
    // player paid for a fight the event has just cancelled, and would leave a row of
    // corpses playing death clips in a room that is finished with.
    enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
                                 [tag](const Enemy &enemy) { return enemy.eventTag == tag; }),
                  enemies.end());
}

//----------------------------------------------------------------------------------
// The player levelled. Everything alive is now a different distance away.
//
// The RANK does not move - a body is the body the floor rolled, for its whole life.
// What moves is where the player stands relative to it, and that is the whole of
// what a tier is. So this recomputes only what the tier decides, and it recomputes
// it from the KIND's row each time rather than from the values already on the body,
// because scaling a scaled figure compounds: level three times and a champion would
// be 1.5 x 1.5 x 1.5 of what it should be.
//
// Health is carried across as a FRACTION rather than reset. A body caught halfway
// through a fight keeps the half it has left; refilling it would heal whatever the
// player was in the middle of killing at the exact moment they were rewarded for
// killing things, which is the worst possible time for a fight to get longer.
//----------------------------------------------------------------------------------
void EnemyManager::RetierAll(int playerLevel)
{
    lastPlayerLevel = playerLevel;

    for (Enemy &enemy : enemies)
    {
        // A corpse has a death clip to finish and nothing left to be a tier of
        if (!enemy.IsAlive()) continue;

        const Config::EnemyArchetype &spec = Config::EnemyTypes[enemy.type];

        enemy.tier = TierFor(enemy.rank, playerLevel);

        const EnemyTierDef &tier = TierAt(enemy.tier);

        const float left = (enemy.maxHealth > 0)
                         ? (enemy.health/(float)enemy.maxHealth) : 0.0f;

        const int ranked = RankedHealth(spec.maxHealth, enemy.rank);
        const int health = (int)(ranked*tier.health + 0.5f)
                         + StatBonusHealth(spec.stats, ranked);

        enemy.maxHealth = (health < 1) ? 1 : health;

        enemy.health = (int)(enemy.maxHealth*left + 0.5f);
        if (enemy.health < 1) enemy.health = 1;      // Retiering must never kill

        const int rankedDamage = RankedDamage(spec.damage, enemy.rank);
        const int damage = (int)(rankedDamage*tier.damage + 0.5f)
                         + StatBonusDamage(spec.stats, rankedDamage);

        enemy.damage = (damage < 1) ? 1 : damage;

        // The body's size, from the kind's own height rather than from whatever it
        // is currently drawn at. `body.radius` is the one field with no untouched
        // original to rebuild from, so it is scaled by the RATIO of the two tiers.
        if (enemy.scale > 1e-4f) enemy.body.radius *= tier.scale/enemy.scale;

        enemy.scale = tier.scale;
        enemy.tint = tier.tint;
        enemy.height = spec.height*tier.scale;
        enemy.body.maxSpeed = spec.speed*tier.speed;
    }
}

//----------------------------------------------------------------------------------
// One raider, one frame.
//
// Deliberately short, and deliberately not sharing anything with the ordinary think
// above. Everything up there is about noticing a player; none of it applies to a
// body that already knows exactly where it is going and cannot be talked out of it.
//
// It still routes rather than walking at a wall - the relic is across a room it may
// have entered from a corridor - and it still swings on its own cooldown, so a
// raider hits the relic at the rate its archetype hits anything.
//----------------------------------------------------------------------------------
void EnemyManager::UpdateRaider(Enemy &enemy, float delta, Level &level)
{
    const Config::EnemyArchetype &spec = SpecOf(enemy);

    const Vector3 toRelic = Vector3Subtract(enemy.raidTarget, enemy.body.position);
    const float distance = sqrtf(toRelic.x*toRelic.x + toRelic.z*toRelic.z);

    // Facing it, always. There is nothing else it might be looking at.
    //
    // Negated, and that is not a cosmetic detail: Body::Update runs forward along
    // -Z at yaw 0, so the un-negated form is the yaw of a body pointed AWAY from
    // the relic - which is both how it was drawn and the direction `move.y` below
    // then carried it. The whole wave walked backwards out of the room.
    if (distance > 1e-3f) enemy.yaw = atan2f(-toRelic.x, -toRelic.z);

    Vector2 move = { 0.0f, 0.0f };

    if (distance > spec.attackRange)
    {
        // Straight at it. A raider that pathfound round the furniture would be
        // better behaved and would also take long enough to arrive that the event
        // resolved itself - the rooms are open, and walking into a table for a
        // moment is a fair price for a fight that keeps its shape.
        move.y = 1.0f;
    }
    else if (enemy.attackCooldown <= 0.0f)
    {
        enemy.attackCooldown = spec.cooldown/TierAt(enemy.tier).swing;

        // A ranged raider still swings. It is standing on the thing - a bolt fired
        // at point blank at a stone plinth is the wrong picture, and every archetype
        // has an attack clip whether or not its usual answer is a shot.
        enemy.PlayAnim(EnemyAnim::Attack, AttackVariantFor(TypeOf(enemy), distance));

        // Nothing lands yet. Same rule as a blow against the player - see LandMelee.
        enemy.meleePending = true;
    }

    //------------------------------------------------------------------------------
    // The blow against the relic, partway through the clip.
    //
    // The relic cannot step out of the way, so there is nothing to re-test - which
    // is why this is four lines rather than a call to LandMelee. What it is here for
    // is the TIMING: the splash used to go off on the frame the swing started, so
    // the relic flashed before the weapon had moved.
    //
    // Owed, not paid. The raider knows it swung; it has no idea what a relic is, and
    // the event collects the flag next frame.
    //------------------------------------------------------------------------------
    if (enemy.meleePending && (enemy.anim == EnemyAnim::Attack))
    {
        const LoadedType &loaded = TypeOf(enemy);
        const int clip = ClipFor(loaded, EnemyAnim::Attack, enemy.animVariant);

        // A slice reads differently to a chop - see the note on IsSliceClip
        // and on Config::EnemySliceMeleeLand.
        const bool slice = IsSliceClip(SpecOf(enemy).attackClips[enemy.animVariant]);
        const float fraction = slice ? Config::EnemySliceMeleeLand : Config::EnemyMeleeLand;

        const float land = (clip >= 0) ? loaded.model.ClipDuration(clip)*fraction : 0.0f;

        if (enemy.animTime >= land)
        {
            enemy.meleePending = false;
            enemy.raidHitPending = true;
        }
    }

    // The same rule the ordinary think uses: every attack in the pack is animated in
    // place, so walking through one drags the model across the floor
    if (FeetPinned(enemy))
    {
        move = { 0.0f, 0.0f };
        enemy.body.Halt();
    }

    enemy.body.Update(delta, enemy.yaw, move, false, false);
    level.ResolveBody(enemy.body);

    UpdateAnimation(enemy, delta, move.y);
}

//----------------------------------------------------------------------------------
// FLAME's one jump, spent on the way out of the burn rather than at every tick -
// see the call site in Update. Nearest rather than random, so it reads as the fire
// actually catching on whatever was closest rather than picking a body across the
// room for no visible reason.
//----------------------------------------------------------------------------------
void EnemyManager::SpreadBurnFrom(Enemy &from)
{
    int target = -1;
    float nearest = Config::FlameSpreadRadius;

    for (int i = 0; i < (int)enemies.size(); ++i)
    {
        Enemy &other = enemies[(size_t)i];

        if (&other == &from) continue;
        if (!other.IsAlive()) continue;
        if (other.dotTime > 0.0f) continue;     // Already alight - nothing to catch

        const float dx = other.body.position.x - from.body.position.x;
        const float dz = other.body.position.z - from.body.position.z;
        const float away = sqrtf(dx*dx + dz*dz);

        if (away > nearest) continue;

        nearest = away;
        target = i;
    }

    if (target < 0) return;

    Enemy &spread = enemies[(size_t)target];

    spread.dotTime = Config::FlameBurnDuration;
    spread.dotTickTimer = Config::MagicDotTickInterval;
    spread.dotDamagePerTick = Config::FlameBurnDamagePerTick;
    spread.dotSpreads = true;       // It can jump again from its new host
}

int EnemyManager::BuffedDamage(const Enemy &enemy)
{
    if (!enemy.IsBuffed()) return enemy.damage;

    const int raised = (int)(enemy.damage*Config::EnemyBuffDamage + 0.5f);

    return (raised < 1) ? 1 : raised;
}

int EnemyManager::AlliesWorthBuffing(const Enemy &caster) const
{
    int count = 0;

    for (const Enemy &other : enemies)
    {
        if (&other == &caster) continue;
        if (!other.IsAlive()) continue;

        // Already under one does not count. Without this a supporter re-casts over
        // the pack it has just finished buffing, forever, every time its cooldown
        // comes up - which is not more dangerous, only more animation.
        if (other.IsBuffed()) continue;

        const float dx = other.body.position.x - caster.body.position.x;
        const float dz = other.body.position.z - caster.body.position.z;

        if ((dx*dx + dz*dz) > (Config::EnemyBuffRadius*Config::EnemyBuffRadius)) continue;

        count++;
    }

    return count;
}

//----------------------------------------------------------------------------------
// The channel finished.
//
// Everything living in range, and NOT the caster. A support that also sharpened its
// own teeth would be the biggest threat in the room as well as the one that has to
// die first, and those are two different jobs - the point of the archetype is that
// it is dangerous through others, so killing it has to be about them.
//
// It reaches through walls. The buff is not aimed at anything, so a line of sight
// test would only mean a supporter standing in a doorway buffed half the pack for
// reasons the player could not see.
//----------------------------------------------------------------------------------
void EnemyManager::ApplyBuff(const Enemy &caster)
{
    int touched = 0;

    for (Enemy &other : enemies)
    {
        if (&other == &caster) continue;
        if (!other.IsAlive()) continue;

        const float dx = other.body.position.x - caster.body.position.x;
        const float dz = other.body.position.z - caster.body.position.z;

        if ((dx*dx + dz*dz) > (Config::EnemyBuffRadius*Config::EnemyBuffRadius)) continue;

        // Refreshed rather than stacked. Two supporters in one pack should mean the
        // buff is kept up rather than that it is twice as strong - stacking would
        // make a room with a pair of Mages in it a different game.
        other.buffTime = Config::EnemyBuffTime;
        touched++;
    }

    TraceLog(LOG_INFO, "ENEMIES: %s empowered %i allies",
             Config::EnemyTypes[caster.type].name, touched);
}

//----------------------------------------------------------------------------------
// The auras.
//
// A pool of light under every buffed body, and a brighter, taller one under a caster
// mid-channel. Both are the same beam the portal and the event markers are made of,
// which is the point: light on the floor already means "something is happening
// here", and this is the same sentence about a body instead of a place.
//
// Flat and wide rather than tall. A column under a skeleton would read as a portal
// standing where the skeleton is; a pool reads as the body being lit from below,
// which is what a buff should look like on something that is still walking around.
//
// The channel is the exception and is deliberately loud - it is the one thing in the
// game the player is being asked to react to within a second and a half.
//----------------------------------------------------------------------------------
void EnemyManager::DrawAuras(const Camera3D &camera) const
{
    if (glow == nullptr) return;

    const Color colour = { Config::EnemyBuffColour[0], Config::EnemyBuffColour[1],
                           Config::EnemyBuffColour[2], 255 };

    const float spin = (float)GetTime()*Config::PortalSpinRate;

    for (const Enemy &enemy : enemies)
    {
        if (!enemy.IsAlive()) continue;

        const bool channelling = (enemy.channelTime > 0.0f);

        if (!channelling && !enemy.IsBuffed()) continue;

        BeamLook look;
        look.colour = colour;
        look.spin = spin;

        if (channelling)
        {
            look.radius = 1.1f;
            look.height = 2.4f;
            look.motes = 6;
            look.moteSize = 0.22f;
            look.columnAlpha = 90;

            // Rising as the cast completes, so the channel has a visible end rather
            // than simply stopping
            const float done = 1.0f - (enemy.channelTime/Config::EnemyChannelTime);
            look.lit = 0.35f + 0.65f*done;
        }
        else
        {
            // Wide, flat and quiet: a body walking around with a light under it,
            // not a beacon. It also FADES as the buff runs out, so the player can
            // see it expiring rather than being surprised by it stopping.
            look.radius = 0.8f;
            look.height = 0.35f;
            look.motes = 0;
            look.columnAlpha = 60;
            look.poolAlpha = 170;

            const float left = enemy.buffTime/Config::EnemyBuffTime;
            look.lit = (left < 0.35f) ? (left/0.35f) : 1.0f;
        }

        DrawBeam(camera, *glow, enemy.body.position, look);
    }
}

bool EnemyManager::SpawnAtCamp(int campIndex, const Level &level, int playerLevel)
{
    const Config::SpawnCamp &camp = Config::SpawnCamps[campIndex];

    // The camp's roster, however long it actually is
    int choices = 0;
    while ((choices < 3) && (camp.types[choices] >= 0)) choices++;

    if (choices == 0) return false;

    // A camp with no room to hold has nowhere to send anyone from
    if (!HasRoom(campIndex)) return false;

    const Vector3 centre = CampCentre(campIndex, level);

    Vector3 spot = { 0.0f, 0.0f, 0.0f };
    if (!FindCampSlot(camp, centre, level, spot)) return false;

    const int type = camp.types[GetRandomValue(0, choices - 1)];

    Enemy enemy = MakeEnemy(type, campIndex, spot, level.Depth(), playerLevel);
    enemy.id = nextId++;

    enemies.push_back(enemy);

    TraceLog(LOG_INFO, "ENEMIES: %s sent a %s rank %i (%s) (%i/%i)", camp.name,
             Config::EnemyTypes[type].name, enemy.rank, TierAt(enemy.tier).name,
             GarrisonOf(campIndex), camp.garrison);

    return true;
}

//----------------------------------------------------------------------------------
// Which room a camp holds.
//
// Camps are assigned in table order and each takes a room no earlier camp claimed,
// so the order of the table is a priority: the first camp gets the pick of the map.
//----------------------------------------------------------------------------------
int EnemyManager::ChooseCampRoom(int campIndex, const std::vector<Room> &rooms,
                                 const std::vector<int> &avoid) const
{
    if (rooms.empty()) return -1;

    // Map::Generate spawns the player in the first room it carved
    constexpr int SpawnRoom = 0;

    const Room &spawnRoom = rooms[SpawnRoom];
    const Config::SpawnCamp &camp = Config::SpawnCamps[campIndex];

    // Cells between two room middles, straight line - not walking distance, which
    // the corridors make considerably longer
    auto cellsFromSpawn = [&](const Room &room)
    {
        const float dx = (float)(room.CenterX() - spawnRoom.CenterX());
        const float dz = (float)(room.CenterZ() - spawnRoom.CenterZ());

        return sqrtf(dx*dx + dz*dz);
    };

    // Camps are spaced against the distance this map actually offers rather than
    // against a constant, which on a smaller map would sit past its own edge and
    // pile every camp into the far corner
    float furthest = 1.0f;
    for (const Room &room : rooms) furthest = fmaxf(furthest, cellsFromSpawn(room));

    // Floor the camp wants: the disc FindCampSlot scatters bodies over, plus
    // somewhere for each of them to stand and fight without touching. For the
    // table's own camps this lands near 16 cells, which is a 4x4 room.
    const float discCells = camp.spread/Config::MapCellSize;
    const float wanted = camp.garrison*4.0f + PI*discCells*discCells;

    // Table order is the running order: camp 0 sits closest to where the player
    // starts and the last one furthest, with the rest spaced evenly along the way
    const float target = furthest*(campIndex + 1)/(float)Config::SpawnCampCount;

    int best = -1;
    float bestScore = 0.0f;

    for (int i = 0; i < (int)rooms.size(); i++)
    {
        if (i == SpawnRoom) continue;

        // A vendor's room is meant to be somewhere safe to stand - see the note on
        // the parameter. Checked before the "taken" test below since it has nothing
        // to do with what earlier camps chose.
        bool isVendorRoom = false;
        for (int v : avoid) { if (v == i) { isVendorRoom = true; break; } }
        if (isVendorRoom) continue;

        // Two camps in one room is one crowded room and two camps that cannot be
        // fought separately, which is the whole point of having camps
        bool taken = false;
        for (int c = 0; c < campIndex; c++) if (campRooms[c] == i) taken = true;
        if (taken) continue;

        const Room &room = rooms[i];
        const float area = (float)(room.w*room.d);

        // Peaks at 1 where the room is exactly the size wanted and falls away on
        // BOTH sides. Too small starves FindCampSlot of places to put a body and
        // the camp comes up short; too large leaves three of them rattling around
        // a hall, never close enough to form the line the shot-line check needs.
        const float fit = fminf(area, wanted)/fmaxf(area, wanted);

        // Falls off a little with every cell of error rather than cutting off, so
        // a well sized room can still win from somewhat further out than asked
        const float placing = 1.0f/(1.0f + fabsf(cellsFromSpawn(room) - target));

        //--------------------------------------------------------------------------
        // What the room is FOR.
        //
        // A garrison belongs in a barracks or a guardroom, and the camp table now
        // says which kinds each camp would rather hold. Weighted rather than
        // required, in both directions: a map that offers no barracks still gets
        // its barrack watch, somewhere less fitting, because a camp that failed to
        // place is a camp that spawns nothing for the whole level.
        //--------------------------------------------------------------------------
        float suits = Rooms::Spec(room.kind).garrisoned ? 1.0f : 0.15f;

        for (int p = 0; p < 3; p++)
        {
            if (camp.prefer[p] == RoomKind::Count) break;
            if (camp.prefer[p] != room.kind) continue;

            // Earlier in the list is a stronger preference
            suits *= 4.0f - p;
            break;
        }

        const float score = fit*placing*suits;

        if ((best < 0) || (score > bestScore)) { bestScore = score; best = i; }
    }

    return best;
}

void EnemyManager::PopulateCamps(const Level &level, int playerLevel)
{
    // Remembered so the first Update does not immediately re-tier everything it
    // has just placed at exactly the tier it placed it at
    lastPlayerLevel = playerLevel;

    enemies.clear();
    campTimers.assign(Config::SpawnCampCount, Config::CampRespawnDelay);

    const std::vector<Room> &rooms = level.Grid().Rooms();

    // Resolved in order, and each choice is visible to the next: ChooseCampRoom
    // reads campRooms to see what earlier camps have already claimed
    campRooms.assign(Config::SpawnCampCount, -1);

    const std::vector<int> &vendorRooms = level.Grid().VendorRooms();

    for (int i = 0; i < Config::SpawnCampCount; i++)
    {
        campRooms[i] = ChooseCampRoom(i, rooms, vendorRooms);
    }

    for (int i = 0; i < Config::SpawnCampCount; i++)
    {
        const Config::SpawnCamp &camp = Config::SpawnCamps[i];

        if (campRooms[i] < 0)
        {
            TraceLog(LOG_WARNING, "ENEMIES: camp '%s' found no room to hold - skipped",
                     camp.name);
            continue;
        }

        // Straight to full strength: the opening population is not a reinforcement
        // and has no fairness rule to satisfy, because the player has not moved yet
        for (int n = 0; n < camp.garrison; n++) SpawnAtCamp(i, level, playerLevel);

        const Room &room = rooms[(size_t)campRooms[i]];

        TraceLog(LOG_INFO, "ENEMIES: camp '%s' holds the %s at (%i,%i), %i of %i on patrol",
                 camp.name, Rooms::Spec(room.kind).name, room.x, room.z,
                 camp.patrollers, camp.garrison);
    }

    TraceLog(LOG_INFO, "ENEMIES: %i camps holding %i", Config::SpawnCampCount, (int)enemies.size());
}

//----------------------------------------------------------------------------------
// Camps top themselves up.
//
// Nothing here decides how many enemies the level should have - each camp answers
// only for its own garrison, and the total falls out of the table. That is what
// keeps the population bounded no matter how long the player takes.
//----------------------------------------------------------------------------------
void EnemyManager::UpdateSpawning(float delta, const Level &level, const Player &player)
{
    if ((int)campTimers.size() != Config::SpawnCampCount)
    {
        campTimers.assign(Config::SpawnCampCount, Config::CampRespawnDelay);
    }

    for (int i = 0; i < Config::SpawnCampCount; i++)
    {
        const Config::SpawnCamp &camp = Config::SpawnCamps[i];

        // At strength: hold the timer full rather than let it run down unused, so
        // the next body lost waits a full delay instead of being replaced at once
        if (GarrisonOf(i) >= camp.garrison)
        {
            campTimers[i] = Config::CampRespawnDelay;
            continue;
        }

        campTimers[i] -= delta;
        if (campTimers[i] > 0.0f) continue;

        // The clock is up but the moment may not be right. Deliberately checked
        // after the timer, so a camp the player is standing in does not bank a
        // pile of reinforcements to release the instant they walk away.
        if (!CanReinforce(camp, i, level, player)) continue;

        if (SpawnAtCamp(i, level, player.level)) campTimers[i] = Config::CampRespawnDelay;
    }
}

//----------------------------------------------------------------------------------
// Make sure the stored route still answers the question being asked.
//
// Three things make a route stale, and only three: it ran out, the goal moved
// somewhere else, or enough time has passed that the world may have changed under
// it. Anything short of that and the body keeps walking what it has, which is the
// point - a body that re-decides its route every frame never commits to one, and
// stands at a junction swapping between two equally good ways round.
//
// False means there is no route to be had. Callers answer that by walking the
// straight line they always used to, not by standing still: a body that gives up
// because the pathfinder did is worse than one that never had a pathfinder.
//----------------------------------------------------------------------------------
bool EnemyManager::Repath(Enemy &enemy, const Level &level, Vector3 goal, float delta)
{
    if (enemy.repathTimer > 0.0f) enemy.repathTimer -= delta;

    const bool spent = enemy.route.empty() || (enemy.routeIndex >= (int)enemy.route.size());

    // The interval is a hard floor, not a hint. An earlier version also repathed
    // the instant the goal moved a cell, which is fine for a trail that sits still
    // and is not fine for a goal that is the player: a body routing round a table
    // in a fight would ask again every time the player took two steps, and every
    // body in the fight would be doing it.
    if (!spent && (enemy.repathTimer > 0.0f)) return true;

    enemy.repathTimer = Config::PathRepathInterval;
    enemy.routeGoal = goal;
    enemy.routeIndex = 0;
    enemy.routeForward = true;
    enemy.stuckTime = 0.0f;

    return paths.Find(level, enemy.body.position, goal, enemy.route);
}

//----------------------------------------------------------------------------------
// Walk the route, and say how fast.
//
// The stuck clock is what makes this survive a fight. A body that has been shoved
// off its route, or has wedged itself on the corner of a table, will otherwise
// push at the thing in its way for as long as it stays interested. Failing to
// reach the next waypoint within PathStuckTime throws the route away, and the next
// call asks for a new one from wherever the body actually ended up.
//----------------------------------------------------------------------------------
float EnemyManager::FollowRoute(Enemy &enemy, float delta, Level &level, float drive, bool loop)
{
    if (enemy.route.empty()) return 0.0f;
    if ((enemy.routeIndex < 0) || (enemy.routeIndex >= (int)enemy.route.size())) return 0.0f;

    const Vector3 target = enemy.route[(size_t)enemy.routeIndex];
    const float dx = target.x - enemy.body.position.x;
    const float dz = target.z - enemy.body.position.z;

    if ((dx*dx + dz*dz) <= Config::PathWaypointReached*Config::PathWaypointReached)
    {
        enemy.stuckTime = 0.0f;
        enemy.routeIndex += enemy.routeForward ? 1 : -1;

        if (loop)
        {
            // A beat is walked end to end and back, for ever. Stepping one past
            // the end and then turning is what stops the body pausing on the last
            // waypoint every time it gets there.
            if (enemy.routeIndex >= (int)enemy.route.size())
            {
                enemy.routeForward = false;
                enemy.routeIndex = (int)enemy.route.size() - 1;
            }
            else if (enemy.routeIndex < 0)
            {
                enemy.routeForward = true;
                enemy.routeIndex = 0;
            }
        }
        else if ((enemy.routeIndex >= (int)enemy.route.size()) || (enemy.routeIndex < 0))
        {
            return 0.0f;    // Arrived
        }
    }
    else
    {
        enemy.stuckTime += delta;

        //--------------------------------------------------------------------------
        // Pressed against something for a moment. Before giving up on the route,
        // try the one obstacle that is meant to be pushed through.
        //
        // The pathfinder routes THROUGH doorways, and it has to: a shut door
        // treated as a wall means a patroller can never leave a room that has one,
        // which is most rooms. The other half of that bargain is this - a body
        // that walks into a shut door leans on it until it swings, the same way an
        // arrow opens one, and then carries on.
        //--------------------------------------------------------------------------
        if (enemy.stuckTime > Config::EnemyDoorShoveTime)
        {
            const Vector3 forward = enemy.Forward();
            const float reach = enemy.body.radius + Config::EnemyDoorShoveReach;

            const Vector3 probe = { enemy.body.position.x + forward.x*reach,
                                    enemy.body.position.y + Config::EnemyDoorShoveHigh,
                                    enemy.body.position.z + forward.z*reach };

            if (level.StrikeDoorAt(probe, enemy.body.radius) >= 0)
            {
                enemy.stuckTime = 0.0f;     // It gave; keep walking
                return drive;
            }
        }

        if (enemy.stuckTime > Config::PathStuckTime)
        {
            enemy.route.clear();
            enemy.stuckTime = 0.0f;
            enemy.repathTimer = 0.0f;   // Ask again immediately, from where it is
            return 0.0f;
        }
    }

    const Vector3 aim = enemy.route[(size_t)enemy.routeIndex];

    // Matching Body::Update's convention, where forward at yaw 0 runs along -Z
    enemy.yaw = atan2f(-(aim.x - enemy.body.position.x), -(aim.z - enemy.body.position.z));

    return drive;
}

//----------------------------------------------------------------------------------
// The beat: this body's room to the nearest other one.
//
// Nearest rather than a tour of the whole map, because a patrol is meant to be
// somewhere the player might run into it, not a body permanently in transit. Two
// rooms and the corridor between them is a stretch a player has to cross and can
// be caught on.
//
// Worked out once and then walked for life. Recomputing it would let a patroller
// pick a different beat every time it turned round, which is not a patrol.
//----------------------------------------------------------------------------------
void EnemyManager::BuildPatrolRoute(Enemy &enemy, const Level &level)
{
    const Map &map = level.Grid();
    const std::vector<Room> &rooms = map.Rooms();

    if (rooms.size() < 2) return;

    const int home = HasRoom(enemy.camp) ? campRooms[(size_t)enemy.camp] : -1;

    int best = -1;
    float bestGap = FLT_MAX;

    for (int i = 0; i < (int)rooms.size(); i++)
    {
        if (i == home) continue;

        const Vector3 centre = map.CellCenter(rooms[(size_t)i].CenterX(), rooms[(size_t)i].CenterZ());
        const float dx = centre.x - enemy.body.position.x;
        const float dz = centre.z - enemy.body.position.z;
        const float gap = dx*dx + dz*dz;

        if (gap < bestGap) { bestGap = gap; best = i; }
    }

    if (best < 0) return;

    const Vector3 target = map.CellCenter(rooms[(size_t)best].CenterX(), rooms[(size_t)best].CenterZ());

    paths.Find(level, enemy.body.position, target, enemy.route);

    enemy.routeIndex = 0;
    enemy.routeForward = true;
    enemy.stuckTime = 0.0f;
}

//----------------------------------------------------------------------------------
// What a body does with nobody to chase.
//
// Two answers, and which one it is was decided when the camp sent this body out.
// A patroller walks its beat. Everybody else stands its post - but "stands" means
// stands THERE, and a guard that chased the player two rooms away and lost them is
// not there any more, so the first thing it does is walk back.
//----------------------------------------------------------------------------------
float EnemyManager::Patrol(Enemy &enemy, float delta, Level &level)
{
    if (enemy.patrols)
    {
        if (enemy.route.empty()) BuildPatrolRoute(enemy, level);

        return FollowRoute(enemy, delta, level, Config::EnemyPatrolDrive, true);
    }

    if (!HasRoom(enemy.camp)) { enemy.route.clear(); return 0.0f; }

    const Vector3 post = CampCentre(enemy.camp, level);
    const float dx = post.x - enemy.body.position.x;
    const float dz = post.z - enemy.body.position.z;

    // Close enough to be at its post. Not the exact centre - a garrison of three
    // all walking to one point would spend the rest of the level shoving.
    if ((dx*dx + dz*dz) < Config::EnemyPostRadius*Config::EnemyPostRadius)
    {
        enemy.route.clear();
        return 0.0f;
    }

    if (!Repath(enemy, level, post, delta)) return 0.0f;

    return FollowRoute(enemy, delta, level, Config::EnemyPatrolDrive, false);
}

//----------------------------------------------------------------------------------
// A fight draws in whoever is standing next to it.
//
// SpreadCries carries only as far as somebody being HURT: a body sets `unheardCry`
// when it takes damage, and a body that has not been hit yet is silent. Which left
// the case anyone would notice first - a guard duelling the player in the middle of
// a room while the two beside it went on facing the wall.
//
// Same room OR close by, because neither alone is right. Room alone misses the two
// guards in the corridor outside the open arch; distance alone reaches through the
// wall into a room nobody in the fight can see.
//
// It fills the detection meter rather than declaring anyone aware. That is what
// makes it read as heads turning rather than as a switch being thrown, and it means
// a body that looks round and finds nothing settles back down on its own.
//----------------------------------------------------------------------------------
void EnemyManager::SpreadAlarm(float delta, const Level &level)
{
    const Map &map = level.Grid();

    for (size_t i = 0; i < enemies.size(); i++)
    {
        if (!enemies[i].IsAlive() || !enemies[i].inCombat) continue;

        const Vector3 at = enemies[i].body.position;
        const Vector3 target = enemies[i].lastKnownPlayer;

        int fx = 0, fz = 0;
        map.WorldToCell(at.x, at.z, fx, fz);
        const int room = map.RoomAt(fx, fz);

        for (size_t j = 0; j < enemies.size(); j++)
        {
            if (j == i) continue;
            if (!enemies[j].IsAlive()) continue;

            // Already fighting, or already on its way somewhere. Nothing to add.
            if (enemies[j].inCombat) continue;

            const float dx = at.x - enemies[j].body.position.x;
            const float dz = at.z - enemies[j].body.position.z;
            const float gap = sqrtf(dx*dx + dz*dz);

            bool worth = (gap < Config::EnemyCombatAlarmRange);

            if (!worth && (room >= 0))
            {
                int ox = 0, oz = 0;
                map.WorldToCell(enemies[j].body.position.x, enemies[j].body.position.z, ox, oz);

                worth = (map.RoomAt(ox, oz) == room);
            }

            if (!worth) continue;

            enemies[j].Hear(target, Config::EnemyCombatAlarmRate*delta);
        }
    }
}

void EnemyManager::Update(float delta, Level &level, Player &player,
                          ProjectileManager &projectiles, bool refill)
{
    // Before the AI loop, so a body that arrives this frame gets its first think
    // now. Safe to grow the list here and only here: the loop below takes
    // references into it, and a push_back partway through would invalidate them.
    if (refill) UpdateSpawning(delta, level, player);

    // Before anyone thinks, so a body reacts to a shout on the same frame as the
    // blow that caused it rather than one behind it
    SpreadCries();

    // ...and the same for a fight that has not drawn blood yet. Reads `inCombat`
    // as it stood at the end of last frame, which is why it runs before the loop
    // that sets it rather than after.
    SpreadAlarm(delta, level);

    const Vector3 playerPos = player.Position();

    for (Enemy &enemy : enemies)
    {
        // The dead still animate: a corpse has a death clip to finish
        if (!enemy.IsAlive())
        {
            UpdateAnimation(enemy, delta, 0.0f);
            continue;
        }

        if (enemy.attackCooldown > 0.0f) enemy.attackCooldown -= delta;

        // The poise meter drains, which is what makes it a meter rather than a
        // counter - see the note on Enemy::poise
        if (enemy.poise > 0.0f)
        {
            enemy.poise -= Config::EnemyPoiseRecovery*enemy.maxHealth*delta;

            if (enemy.poise < 0.0f) enemy.poise = 0.0f;
        }
        if (enemy.hurtFlash > 0.0f) enemy.hurtFlash -= delta;
        if (enemy.buffTime > 0.0f) enemy.buffTime -= delta;
        if (enemy.channelCooldown > 0.0f) enemy.channelCooldown -= delta;

        //--------------------------------------------------------------------------
        // What magic left behind, ticking and decaying on the same rule poise
        // just did above: this is happening TO the body, not a thing it is doing,
        // so a stun or a channel below must not be able to pause it.
        //--------------------------------------------------------------------------
        if (enemy.dotTime > 0.0f)
        {
            enemy.dotTime -= delta;
            enemy.dotTickTimer -= delta;

            if (enemy.dotTickTimer <= 0.0f)
            {
                enemy.dotTickTimer += Config::MagicDotTickInterval;
                enemy.TakeDamage(enemy.dotDamagePerTick);
            }

            if (enemy.IsAlive() && (enemy.dotTime <= 0.0f))
            {
                enemy.dotTime = 0.0f;

                // Spent on the way out, not at every tick - a burn that kept
                // re-spreading itself every half second would turn one cast into
                // a whole room on fire rather than one jump.
                if (enemy.dotSpreads)
                {
                    enemy.dotSpreads = false;
                    SpreadBurnFrom(enemy);
                }
            }
        }

        if (enemy.IsAlive() && (enemy.poisonStacks > 0))
        {
            enemy.poisonTickTimer -= delta;

            if (enemy.poisonTickTimer <= 0.0f)
            {
                enemy.poisonTickTimer += Config::ToxinTickInterval;
                enemy.TakeDamage(Config::ToxinDamagePerStack*enemy.poisonStacks);
            }
        }

        if (enemy.slowTime > 0.0f) enemy.slowTime -= delta;
        if (enemy.blindTime > 0.0f) enemy.blindTime -= delta;

        // A DOT or a stack of poison can still kill on its own tick, same as any
        // other source of damage - and a dead body skips everything below exactly
        // the way it already does at the top of this loop.
        if (!enemy.IsAlive())
        {
            UpdateAnimation(enemy, delta, 0.0f);
            continue;
        }

        //--------------------------------------------------------------------------
        // Stunned bodies do not think.
        //
        // Everything below this is a decision - what it can see, whether to close,
        // whether to swing - and a stunned body makes none of them. What it still
        // does is FALL and be pushed: the body update and the wall resolution run,
        // so a hammer blow that stuns also shoves, and the shove lands properly
        // instead of being frozen in the air until the stun wears off.
        //
        // The attack cooldown above ticks down through it, on purpose. A stun that
        // also paused the cooldown would mean the first thing a body does on coming
        // round is swing, which turns a hammer's best moment into a free hit for
        // the enemy.
        //--------------------------------------------------------------------------
        if (enemy.stunTime > 0.0f)
        {
            enemy.stunTime -= delta;

            enemy.blocking = false;     // A guard is a decision too
            enemy.body.Update(delta, enemy.yaw, { 0.0f, 0.0f }, false, false);
            level.ResolveBody(enemy.body);

            UpdateAnimation(enemy, delta, 0.0f);
            continue;
        }

        //--------------------------------------------------------------------------
        // A caster mid-channel is committed and does nothing else.
        //
        // It stands still, does not shoot, does not step aside, and cannot be
        // interrupted by being hit. That last part is deliberate: a channel the
        // player could break with one arrow would be a channel that never finished,
        // and the fight this is meant to create - kill the supporter FIRST or fight
        // a stronger pack - only exists if the threat is real.
        //
        // What it costs the caster is the whole of its defence. A ranged enemy is
        // hard to reach because it keeps moving and keeps its distance; one that has
        // stopped doing both, and is lit up while it does, is a free kill for a
        // player who reads it.
        //--------------------------------------------------------------------------
        if (enemy.channelTime > 0.0f)
        {
            enemy.channelTime -= delta;

            if (enemy.channelTime <= 0.0f)
            {
                enemy.channelTime = 0.0f;
                ApplyBuff(enemy);
            }

            enemy.blocking = false;
            enemy.body.Halt();
            enemy.body.Update(delta, enemy.yaw, { 0.0f, 0.0f }, false, false);
            level.ResolveBody(enemy.body);

            UpdateAnimation(enemy, delta, 0.0f);
            continue;
        }

        //--------------------------------------------------------------------------
        // Raiders think about one thing and it is not the player.
        //
        // Handled here and returned from, ahead of every line of the ordinary AI,
        // because a raider does not use any of it: no sight cone, no detection
        // meter, no alarm, no guard. It knows where the relic is - it was told - it
        // walks there, and it swings.
        //
        // That is the fight. The player is not being attacked; they are being
        // IGNORED, and the only answer is to make themselves impossible to ignore.
        // A raider that could be pulled off the relic by being hit would turn the
        // event straight back into an ordinary fight in a room with an ornament.
        //--------------------------------------------------------------------------
        if (enemy.raiding)
        {
            UpdateRaider(enemy, delta, level);

            continue;
        }

        const Vector3 toPlayer = Vector3Subtract(playerPos, enemy.body.position);
        const float distance = sqrtf(toPlayer.x*toPlayer.x + toPlayer.z*toPlayer.z);

        Vector2 move = { 0.0f, 0.0f };

        //--------------------------------------------------------------------------
        // TOXIN's panic: a whole separate behaviour, the same shape as raiding
        // above and for the same reason. Nothing here shares anything with the
        // ordinary think - no sight cone, no detection, no guard, no swing - it
        // knows one thing, which is which way the player is, and it wants the
        // opposite of that.
        //--------------------------------------------------------------------------
        if (enemy.fleeTime > 0.0f)
        {
            enemy.fleeTime -= delta;

            // Facing AWAY: the negation Body::Update's own convention wants for
            // facing TOWARD something is exactly what running from it drops -
            // compare UpdateRaider's atan2f(-toRelic.x, -toRelic.z).
            if (distance > 1e-3f) enemy.yaw = atan2f(toPlayer.x, toPlayer.z);

            move.y = 1.0f;

            if (enemy.slowTime > 0.0f)
            {
                move.x *= Config::SplashSlowFactor;
                move.y *= Config::SplashSlowFactor;
            }

            enemy.blocking = false;
            enemy.body.Update(delta, enemy.yaw, move, false, false);
            level.ResolveBody(enemy.body);

            UpdateAnimation(enemy, delta, move.y);

            continue;
        }

        // An enemy still climbing out of the floor decides nothing - it does not
        // aggro, turn, close or swing on the way up. One flag covers all four
        // because they all hang off `aware`.
        const bool emerging = (enemy.anim == EnemyAnim::Spawn) && ClipOwnsBody(enemy);

        // What is in front of its eyes this instant: in range, inside its cone,
        // and nothing in the way.
        const bool visible = !emerging && CanSee(enemy, player, level);

        // ...and how much of that has registered. An enemy still filling its meter
        // is looking straight at the player and has not reacted yet, which is the
        // window the player gets to be somewhere else.
        //
        // FLASH holds this at zero instead of running the meter at all - blindTime
        // decayed alongside the other magic effects above. A blinded body cannot
        // even be FILLING the meter, or it would pick up exactly where it left off
        // the instant the blind wore off, which reads as it never having lost the
        // player at all.
        if (enemy.blindTime > 0.0f) enemy.detection = 0.0f;
        else UpdateDetection(enemy, delta, visible, distance);

        // What it can see this instant, which is not the same as what it is still
        // acting on. Splitting the two is what a chase is: `aware` decides whether
        // to fight, `alertTime` decides whether to keep looking.
        const bool aware = visible && (enemy.detection >= 1.0f);

        // Recorded for SpreadAlarm, which runs at the top of next frame. A fight is
        // loud whether or not anyone in it has been hit yet.
        enemy.inCombat = aware;

        const Config::EnemyArchetype &spec = SpecOf(enemy);

        if (aware)
        {
            enemy.lastKnownPlayer = playerPos;
            enemy.alertTime = Config::EnemyAlertMemory;
            // Its own eyes have taken over. Whatever hit it is no longer the
            // reason it is coming, so the exception that flag buys is spent.
            enemy.investigating = false;
        }
        else if (enemy.alertTime > 0.0f)
        {
            enemy.alertTime -= delta;

            if (enemy.alertTime <= 0.0f)
            {
                enemy.alertTime = 0.0f;
                enemy.investigating = false;    // Trail gone cold, and it gave up
            }
        }

        // Every type hunts a lost sighting now, which it did not used to.
        //
        // The old rule was that only the ranged types did, because a melee enemy
        // walking at a player it could not see had to walk THROUGH whatever it
        // could not see round, and that reads as cheating rather than as
        // persistence. That was a limitation of having no route to follow, not a
        // decision about swordsmen: given a corridor it can actually walk, a
        // guard that goes and looks where you went is the least a guard can do.
        const bool hunting = !aware && !emerging && (enemy.alertTime > 0.0f);

        // A lost trail is not a fight: nothing to strafe clear of, nothing to aim
        if (!aware) { enemy.strafeDir = 0.0f; enemy.strafeTimer = 0.0f; }

        if (aware && (distance > 1e-3f))
        {
            //------------------------------------------------------------------
            // Face the player - except through the committed half of a swing.
            //
            // Body::Update takes movement relative to a facing, so facing the
            // player and walking forward is the whole of a chase. The aim used to
            // keep tracking through an attack as well, on the grounds that only
            // the feet are pinned.
            //
            // That has to stop once a blow is in the air. LandMelee re-tests the
            // arc at the moment the weapon comes down, and an aim that had been
            // following the player the whole way would pass that test from any
            // angle - which would leave stepping AROUND a swing impossible while
            // stepping BACK from one worked, for no reason the player could see.
            //
            // Only while the blow is pending. Once it has landed or missed, the
            // recovery is not committed and the body turns again - so this costs
            // an enemy its aim for the wind-up and nothing else.
            //------------------------------------------------------------------
            if (!enemy.meleePending) enemy.yaw = atan2f(-toPlayer.x, -toPlayer.z);

            // A ranged enemy will not fire into a wall or into a friend's back.
            // Only asked of the ones it can constrain: a melee enemy resolves a
            // blocked line by walking into whatever is blocking it.
            const bool clearShot = !spec.ranged || HasClearShot(enemy, player, level);

            if (clearShot)
            {
                enemy.strafeDir = 0.0f;         // Nothing to solve
                enemy.strafeTimer = 0.0f;
            }
            else
            {
                // Commit to a side, and keep committing until it works or until
                // this side has clearly failed. Re-picking every frame is what
                // turns a sidestep into a shuffle.
                enemy.strafeTimer += delta;

                if ((enemy.strafeDir == 0.0f) || (enemy.strafeTimer > Config::EnemyStrafeFlipTime))
                {
                    enemy.strafeDir = (enemy.strafeDir == 0.0f)
                                    ? ChooseStrafeDirection(enemy, player)
                                    : -enemy.strafeDir;     // That side did not work
                    enemy.strafeTimer = 0.0f;
                }

                move.x = enemy.strafeDir*Config::EnemyStrafeDrive;
            }

            //------------------------------------------------------------------
            // Buff the pack, or shoot at the player.
            //
            // Tested BEFORE the attack, so a supporter with a full pack behind it
            // channels rather than firing - the whole reason it is on the table is
            // that it makes a pack more than the sum of its bodies, and one that
            // preferred its own shot would be an ordinary Mage with a spare ability.
            //
            // It does not need a clear shot to channel. The buff reaches through
            // walls because it is not aimed at anything; what it needs is allies
            // near enough to be worth it, which AlliesWorthBuffing answers.
            //------------------------------------------------------------------
            if (spec.support && (enemy.channelCooldown <= 0.0f) &&
                (AlliesWorthBuffing(enemy) >= Config::EnemyBuffMinAllies))
            {
                enemy.channelTime = Config::EnemyChannelTime;
                enemy.channelCooldown = Config::EnemyChannelCooldown;

                //--------------------------------------------------------------
                // The casting clip rather than the shooting one.
                //
                // Variant 1 is ranged_magic_spellcasting - a cast rather than a
                // shot, and the reason the Mage was given two clips in the first
                // place. Guarded rather than assumed: a clip that failed to
                // resolve leaves attackCount at 1, and PlayAnim on a variant that
                // does not exist is a body that freezes mid-channel.
                //--------------------------------------------------------------
                const int cast = (TypeOf(enemy).attackCount > 1) ? 1 : 0;

                enemy.PlayAnim(EnemyAnim::Attack, cast);

                enemy.shotPending = false;      // Not a shot: nothing leaves
                enemy.meleePending = false;     // ...and not a swing either
                continue;
            }

            // Decided before the feet are, so the swing that starts this frame
            // stops the feet on the same frame rather than one later
            if (clearShot && (enemy.attackCooldown <= 0.0f) && (distance < spec.attackRange))
            {
                // Divided, not multiplied: `swing` is how much FASTER this tier
                // swings, and the cooldown is the gap between swings. A champion
                // at 1.10 waits 1/1.10 of the table's gap.
                //
                // Careful with this one. Config's blockChance note explains that a
                // type whose cooldown does not comfortably clear its own attack
                // clip has no gap left to raise a guard in - and shortening the
                // cooldown is exactly what eats that gap. At 1.10 the Warrior's
                // 1.90 becomes 1.73 against a 1.07s chop, which still leaves the
                // guard somewhere to live. Much past that and the highest tier of
                // the one archetype that blocks would quietly stop blocking.
                // Divided by both, for the same reason: each is how much FASTER
                // this body swings, and the cooldown is the gap between swings
                enemy.attackCooldown = spec.cooldown/TierAt(enemy.tier).swing;

                if (enemy.IsBuffed()) enemy.attackCooldown /= Config::EnemyBuffHaste;

                // Which of this type's swings. Rolled per attack, so a Reaver
                // cycles chop, sweep and spin instead of tracing one arc forever.
                const int swing = AttackVariantFor(TypeOf(enemy), distance);

                if (spec.ranged)
                {
                    // Nothing happens yet. The arrow leaves partway through the
                    // clip, once the crossbow is actually pointing somewhere.
                    enemy.PlayAnim(EnemyAnim::Shoot, swing);
                    enemy.shotPending = true;
                }
                else
                {
                    // Nothing happens yet. The blow lands partway through the
                    // clip, once the weapon is actually coming down - see
                    // LandMelee and Config::EnemyMeleeLand. It used to resolve
                    // on the frame the swing STARTED, which damaged the player
                    // before the axe had begun to move.
                    enemy.PlayAnim(EnemyAnim::Attack, swing);
                    enemy.meleePending = true;
                }

                // One roll per gap between swings. A chance evaluated every frame
                // would flicker the arms up and down; this makes the guard a
                // decision about this particular gap, held for its whole length.
                enemy.blockRoll = GetRandomValue(0, 1000)/1000.0f;
            }

            // A ranged enemy holds its distance in both directions: it closes when
            // it is out of range and gives ground when the player is on top of it,
            // where a melee enemy only ever walks forward.
            // Full throttle to close, deliberately less to give ground: a body
            // backing away at a dead sprint reads as fleeing rather than as
            // keeping its distance, and the drive sets the clip as well as the speed
            if (distance > spec.stopDistance) move.y = 1.0f;
            else if (spec.ranged && (distance < spec.stopDistance*Config::EnemyRangedRetreat))
            {
                move.y = -Config::EnemyRetreatDrive;
            }

            //----------------------------------------------------------------------
            // Walking straight at somebody is the right way to fight and the wrong
            // way to get round a table.
            //
            // Closing on the player has always been a straight line, and it should
            // stay one - a body that routes across an open room takes the corners
            // of the grid rather than the line, and stops reading as a swordsman.
            // But the rooms have furniture in them now, and ResolveBody slides a
            // body along whatever it meets: against a wall that carries it round,
            // against a table in open floor it just holds it there, grinding, for
            // as long as it stays interested.
            //
            // So: keep the straight line, and notice when it has stopped working.
            // Making no ground for EnemyShoveTime is the signal, and the answer is
            // to route for a moment - long enough to get round the thing - and then
            // fall back to walking at them.
            //----------------------------------------------------------------------
            if (move.y > 0.0f)
            {
                if (distance < enemy.lastGap - Config::EnemyShoveProgress) enemy.pushTime = 0.0f;
                else enemy.pushTime += delta;

                enemy.lastGap = distance;

                if (enemy.pushTime > Config::EnemyShoveTime)
                {
                    if (Repath(enemy, level, playerPos, delta))
                    {
                        const float routed = FollowRoute(enemy, delta, level,
                                                         1.0f, false);

                        // Only take the routed answer if it produced one. A spent
                        // route means it has arrived, and arriving is the straight
                        // line's job again.
                        if (routed != 0.0f) move.y = routed;
                        else enemy.pushTime = 0.0f;
                    }
                    else
                    {
                        // Nowhere to route to either. Stop asking every frame.
                        enemy.pushTime = 0.0f;
                    }
                }
            }
            else
            {
                enemy.pushTime = 0.0f;
                enemy.lastGap = distance;
            }
        }
        else if (hunting)
        {
            // The chase. It walks to where the player WAS, not to where they are -
            // it cannot see them, and a hunter that beelines at the real position
            // is not hunting, it is cheating with extra steps. The route is to the
            // remembered spot and nowhere else, however far the player has since
            // moved from it.
            const Vector3 toTrail = Vector3Subtract(enemy.lastKnownPlayer, enemy.body.position);
            const float trailDistance = sqrtf(toTrail.x*toTrail.x + toTrail.z*toTrail.z);

            if (trailDistance > Config::EnemyTrailReached)
            {
                // Routed rather than walked at. This is what the pathfinder is
                // for: the trail almost always leads round a corner, since a
                // corner is usually how the player broke sight in the first place.
                if (Repath(enemy, level, enemy.lastKnownPlayer, delta))
                {
                    move.y = FollowRoute(enemy, delta, level, Config::EnemyHuntDrive, false);
                }

                // No route, or the route ran out short of the trail. Fall back to
                // the straight line, which is what this always did - a body that
                // stops because the search failed is worse than one that never
                // searched.
                if (move.y == 0.0f)
                {
                    enemy.yaw = atan2f(-toTrail.x, -toTrail.z);
                    move.y = Config::EnemyHuntDrive;    // Purposeful, not a sprint
                }
            }
            else
            {
                // Arrived, and nothing here. Whatever it does now is bounded by
                // alertTime, which is still running down the whole while.
                enemy.route.clear();
                SearchAtColdTrail(enemy, delta, move);
            }
        }
        else if (!emerging)
        {
            // Nothing to chase and nothing to look for. A patroller walks its
            // beat; a guard that left its post during a chase walks back to it.
            move.y = Patrol(enemy, delta, level);
        }

        // The arrow leaves partway through the clip, and it leaves whatever happens
        // next: outside the awareness test on purpose, so a shot already started is
        // committed even if the player has broken line of sight since. That is what
        // makes stepping behind a wall a way to make an archer waste a shot rather
        // than a way to cancel one.
        if (enemy.shotPending && (enemy.anim == EnemyAnim::Shoot))
        {
            const LoadedType &loaded = TypeOf(enemy);
            const int clip = ClipFor(loaded, EnemyAnim::Shoot, enemy.animVariant);
            const float release = (clip >= 0)
                                ? loaded.model.ClipDuration(clip)*Config::EnemyShootRelease
                                : 0.0f;

            if (enemy.animTime >= release) ReleaseShot(enemy, player, projectiles);
        }

        //--------------------------------------------------------------------------
        // ...and the melee blow lands the same way.
        //
        // Outside the awareness test for the same reason the arrow is: a swing that
        // has started is committed, so breaking line of sight makes an enemy WASTE
        // a swing rather than cancel one. What it does not survive is the player
        // stepping out of it - LandMelee re-tests reach and facing, and a swing that
        // no longer reaches simply misses.
        //--------------------------------------------------------------------------
        if (enemy.meleePending && (enemy.anim == EnemyAnim::Attack))
        {
            const LoadedType &loaded = TypeOf(enemy);
            const int clip = ClipFor(loaded, EnemyAnim::Attack, enemy.animVariant);

            // A slice reads differently to a chop - see the note on
            // IsSliceClip and on Config::EnemySliceMeleeLand.
            const bool slice = IsSliceClip(SpecOf(enemy).attackClips[enemy.animVariant]);
            const float fraction = slice ? Config::EnemySliceMeleeLand : Config::EnemyMeleeLand;

            const float land = (clip >= 0) ? loaded.model.ClipDuration(clip)*fraction : 0.0f;

            if (enemy.animTime >= land) LandMelee(enemy, player);
        }

        // Guard up between swings: close enough to be worth guarding against,
        // reloading, and this gap's roll came in under the archetype's appetite
        // for it. It drops shortly before the next swing so raising the arms and
        // chopping read as one motion rather than a guard that snaps into an
        // attack - and a type with no guard clip never gets one, so the decision
        // and the pose cannot disagree.
        enemy.blocking = aware && (spec.blockChance > 0.0f) &&
                         (TypeOf(enemy).blockClip >= 0) &&
                         (enemy.blockRoll < spec.blockChance) &&
                         (distance < spec.attackRange) &&
                         (enemy.attackCooldown > Config::EnemyBlockDropTime);

        // The feet only carry the body while a movement clip is playing. Every
        // attack in the pack is animated in place, so walking through one drags
        // the model across the floor - and stopping the input alone is not enough
        // to stand still, hence Halt rather than just clearing `move`. A raised
        // guard is animated in place too, hence FeetPinned and not ClipOwnsBody.
        if (FeetPinned(enemy))
        {
            move = { 0.0f, 0.0f };
            enemy.body.Halt();
        }

        // SPLASH's chill: a plain scale on the movement input itself rather than
        // on body.maxSpeed, so it fades back to the tier's own speed the instant
        // slowTime runs out without this having to remember what that speed was.
        if (enemy.slowTime > 0.0f)
        {
            move.x *= Config::SplashSlowFactor;
            move.y *= Config::SplashSlowFactor;
        }

        enemy.body.Update(delta, enemy.yaw, move, false, false);
        level.ResolveBody(enemy.body);

        // The signed input, not "is it walking forward": a ranged enemy giving
        // ground is moving too, and the sign is what runs its cycle backwards.
        //
        // A pure sidestep has no forward sign to give, and the pack ships no
        // strafe clip, so it borrows the forward cycle - which reads acceptably
        // at the distance a ranged enemy keeps. What it must not do is fall
        // through to zero, because that is the glide all over again.
        // A pure sidestep borrows the forward gait at its own magnitude, so it
        // walks rather than sprinting sideways
        const float drive = (move.y != 0.0f) ? move.y : fabsf(move.x);

        UpdateAnimation(enemy, delta, drive);
    }

    Separate();
    PushOffPlayer(player);

    // Shoving can put a body inside a wall, so the level gets its say
    for (Enemy &enemy : enemies)
    {
        if (enemy.IsAlive()) level.ResolveBody(enemy.body);
    }

    // ...and then camera comfort gets the last word, because a body a few
    // centimetres inside a wall for one frame is invisible and a skull in the lens
    // is not. Wall resolution used to run last, which is half of why the standoff
    // collapsed: an enemy pushed out of a wall goes straight back toward the player
    // with nothing after it to separate them again.
    ClearOfPlayer(player);
}

//----------------------------------------------------------------------------------
// Keep enemies out of the player's personal space.
//
// Stopping at attack range is not enough on its own: the enemy carries momentum
// past it, and nothing stops the player walking straight into an enemy. Without
// this you end up inside the capsule, looking at the back of its faces.
//
// The player takes a small share of the push rather than none, so an enemy caught
// between the player and a wall shoves its way free instead of jittering there.
//----------------------------------------------------------------------------------
void EnemyManager::PushOffPlayer(Player &player)
{
    for (Enemy &enemy : enemies)
    {
        if (!enemy.IsAlive()) continue;

        // Each kind keeps its own distance: it is sized off that model's reach
        const float minDistance = SpecOf(enemy).personalSpace;

        const float dx = enemy.body.position.x - player.body.position.x;
        const float dz = enemy.body.position.z - player.body.position.z;
        const float distanceSq = dx*dx + dz*dz;

        if (distanceSq >= minDistance*minDistance) continue;

        float normalX = 1.0f;
        float normalZ = 0.0f;
        float overlap = minDistance;

        if (distanceSq > 1e-8f)
        {
            const float distance = sqrtf(distanceSq);
            normalX = dx/distance;
            normalZ = dz/distance;
            overlap = minDistance - distance;
        }

        enemy.body.position.x += normalX*overlap*Config::EnemyPushShare;
        enemy.body.position.z += normalZ*overlap*Config::EnemyPushShare;

        player.body.position.x -= normalX*overlap*(1.0f - Config::EnemyPushShare);
        player.body.position.z -= normalZ*overlap*(1.0f - Config::EnemyPushShare);
    }
}

//----------------------------------------------------------------------------------
// Hangs what the enemy carries off its bones, from the pose it is already drawn in.
//
// Three transforms per prop, applied in that order: the grip nudge, then where the
// bone ended up in model space, then the enemy's own placement in the world. The
// bone half is the same recovery Ragdoll does - AnimatedModel::BoneTransform puts
// the bind pose back on top of the skinning matrix to get the bone's own transform.
//
// The placement half is shared by every slot, so it is built once outside the loop.
//
// It follows the corpse too. The ragdoll writes handslot.r and handslot.l like any
// other bone, so a dropped skeleton keeps hold of its blade and its shield rather
// than leaving them hanging in the air where it died.
//----------------------------------------------------------------------------------
void EnemyManager::DrawProps(const Enemy &enemy) const
{
    const LoadedType &loaded = TypeOf(enemy);

    if (enemy.bones.empty()) return;

    const float facing = enemy.yaw + Config::EnemyModelYaw*DEG2RAD;
    const Matrix placement =
        MatrixMultiply(MatrixMultiply(MatrixScale(loaded.scale, loaded.scale, loaded.scale),
                                      MatrixRotateY(facing)),
                       MatrixTranslate(enemy.body.position.x, enemy.body.position.y, enemy.body.position.z));

    for (const LoadedType::HeldProp &prop : loaded.props)
    {
        if ((prop.model == nullptr) || (prop.bone < 0)) continue;

        const Matrix bone = loaded.model.BoneTransform(prop.bone, enemy.bones.data());

        // DrawModel with no offset and unit scale leaves model.transform doing the work
        Model held = *prop.model;
        held.transform = MatrixMultiply(MatrixMultiply(prop.grip, bone), placement);

        DrawModel(held, Vector3Zero(), 1.0f, WHITE);
    }
}

//----------------------------------------------------------------------------------
// Where a prop slot has ended up in the world.
//
// Same three transforms DrawProps composes, for the same reason - an arrow has to
// leave from the crossbow the player can see, not from an abstract point at chest
// height. It is worked out here rather than shared with DrawProps because that one
// runs per frame per enemy and this one runs once per shot.
//
// An empty slot, a corpse with no pose, or a type with no model all fall back to
// the enemy's centre: a shot from slightly the wrong place beats no shot at all.
//----------------------------------------------------------------------------------
Vector3 EnemyManager::PropMuzzle(const Enemy &enemy, int slot) const
{
    const LoadedType &loaded = TypeOf(enemy);

    if ((slot < 0) || (slot >= Config::EnemyPropSlots)) return enemy.Center();

    const LoadedType::HeldProp &prop = loaded.props[slot];

    if (!loaded.ready || (prop.bone < 0) || enemy.bones.empty()) return enemy.Center();

    const Matrix bone = loaded.model.BoneTransform(prop.bone, enemy.bones.data());

    const float facing = enemy.yaw + Config::EnemyModelYaw*DEG2RAD;
    const Matrix placement =
        MatrixMultiply(MatrixMultiply(MatrixScale(loaded.scale, loaded.scale, loaded.scale),
                                      MatrixRotateY(facing)),
                       MatrixTranslate(enemy.body.position.x, enemy.body.position.y, enemy.body.position.z));

    // Through the grip as well, so a prop that is offset in the hand fires from
    // where it is drawn rather than from where the bone is
    const Matrix at = MatrixMultiply(MatrixMultiply(prop.grip, bone), placement);

    return { at.m12, at.m13, at.m14 };
}

//----------------------------------------------------------------------------------
// The arrow leaves.
//
// Aimed at the player's eye from wherever the crossbow currently is, which means
// the shot is only as accurate as the pose - and that is the point. It travels in a
// straight line, so aiming at the eye of a player who is moving is a miss, and
// standing still is what gets you hit.
//
//----------------------------------------------------------------------------------
// One blow per Attack state, guarded by meleePending.
//
// The threshold is a point in the CLIP rather than a moment in time, so a slow chop
// and a quick jab each connect at the right point in their own arc - and the flag is
// what keeps every frame past that point from landing the blow again. The same shape
// as ReleaseShot below, deliberately: they are the same event seen through two
// different weapons.
//
// Reach and facing are re-tested HERE and not trusted from the frame the swing
// started. That is the entire fix: a wind-up the player cannot step out of is not a
// wind-up, it is a delay before an unavoidable hit.
//
// The blow is cleared either way. A swing that missed is a swing spent - it does not
// get to keep trying for the rest of its clip, which would turn a miss into a hit the
// moment the player walked back into range.
//----------------------------------------------------------------------------------
void EnemyManager::LandMelee(Enemy &enemy, Player &player)
{
    enemy.meleePending = false;

    const Config::EnemyArchetype &spec = SpecOf(enemy);

    const Vector3 target = player.EyePosition();

    const Vector3 away = Vector3Subtract(target, enemy.Center());
    const float distance = sqrtf(away.x*away.x + away.z*away.z);

    // Past its reach by the time the weapon came down. The slack is what a weapon's
    // length past the body's stop distance is worth - see the note on the constant.
    if (distance > (spec.attackRange + Config::EnemyMeleeLandSlack)) return;

    // ...or no longer in front of it. The body is pinned for the whole clip, so a
    // player who has walked round it is behind the arc rather than in it.
    if (!InCone(enemy.Center(), enemy.Forward(), target,
                spec.attackRange + Config::EnemyMeleeLandSlack, Config::EnemyMeleeLandArc))
    {
        return;
    }

    //------------------------------------------------------------------------------
    // Through the same funnel the player's own blows go through, so an enemy's crit
    // and the player's are the same rule rather than two that can be tuned apart.
    //
    // `enemy.damage` and not `spec.damage`: the table row is the kind at rank 1, and
    // this body is whatever the floor rolled it as.
    //------------------------------------------------------------------------------
    const bool crit = StatRollCrit(enemy.stats);

    const bool parried = player.TakeDamageFrom(ResolveDamage(BuffedDamage(enemy), enemy.stats, crit),
                                               enemy.Center(), true);

    // Caught at exactly the wrong moment: the shield wins outright, and this
    // body pays for the swing instead of landing it.
    if (parried) enemy.Stagger(Config::ParryStunTime);
}

// One shot per Shoot state, guarded by shotPending: the release frame is a
// threshold, not an event, so without the flag every frame after it would fire.
//----------------------------------------------------------------------------------
void EnemyManager::ReleaseShot(Enemy &enemy, const Player &player, ProjectileManager &projectiles) const
{
    enemy.shotPending = false;

    const Vector3 from = PropMuzzle(enemy, 0);
    const Vector3 to = player.EyePosition();

    // Rolled and resolved as it leaves rather than on impact. The shot carries a
    // number, not a stat block, and rolling here is what keeps one shot to one roll
    // - an arrow that re-rolled every substep would crit eventually, always.
    const bool crit = StatRollCrit(enemy.stats);

    //------------------------------------------------------------------------------
    // A caster sends a mote; everything else sends an object.
    //
    // The school decides the COLOUR and the size of the thing in the air, and
    // nothing else: the speed and the damage stay the archetype's, because those are
    // what the kind is balanced on. Borrowing a school's numbers would make a Mage
    // as strong as whatever the player last pressed a number key for.
    //
    // And it bursts into a small flash rather than the school's own sheet. That art
    // is the PLAYER's magic - the loudest thing on the screen, and how a cast reads
    // as having landed - and an enemy repainting it every couple of seconds would
    // drown out the player's own casts. See ProjectileLook::impactOverride.
    //------------------------------------------------------------------------------
    ProjectileLook look;

    const Config::EnemyArchetype &spec = SpecOf(enemy);

    if (spec.magic >= 0)
    {
        look.magic = &MagicAt((Magic)spec.magic);
        look.impactOverride = VfxKind::Muzzle;
        look.impactScale = Config::EnemyMoteImpactScale;
    }

    projectiles.Spawn(from, Vector3Subtract(to, from), Config::ProjectileSpeed,
                      ResolveDamage(BuffedDamage(enemy), enemy.stats, crit),
                      ProjectileSide::AtPlayer, look);
}

const Enemy *EnemyManager::FirstBlocker(const Enemy &shooter, const Player &player) const
{
    const Vector3 muzzle = PropMuzzle(shooter, 0);
    const Vector3 target = player.EyePosition();

    const Enemy *nearest = nullptr;
    float nearestDistance = 0.0f;

    for (const Enemy &other : enemies)
    {
        if (&other == &shooter) continue;
        if (!other.IsAlive()) continue;     // A corpse is not cover

        const Capsule body = BodyCapsule(other.body.position, other.height, other.body.radius);

        // The shot is a segment, not a ray, so a body past the player is not in
        // the way - which SegmentDistance gets right for free
        const float gap = SegmentDistance(muzzle, target, body.a, body.b);

        if (gap > (body.radius + Config::ProjectileRadius + Config::EnemyShotClearance)) continue;

        // Nearest to the muzzle, because that is the one to step around: solving
        // for a body behind another body solves nothing
        const float distance = Vector3Distance(muzzle, other.body.position);

        if ((nearest == nullptr) || (distance < nearestDistance))
        {
            nearest = &other;
            nearestDistance = distance;
        }
    }

    return nearest;
}

bool EnemyManager::HasClearShot(const Enemy &shooter, const Player &player, const Level &level) const
{
    // Walls first: cheaper than walking the crowd, and it rejects the common case
    if (!level.LineOfSight(PropMuzzle(shooter, 0), player.EyePosition())) return false;

    return FirstBlocker(shooter, player) == nullptr;
}

//----------------------------------------------------------------------------------
// Which way to step out of a blocked line.
//
// Called once when the block appears, not every frame: the answer is held in
// Enemy::strafeDir until it works or until EnemyStrafeFlipTime says this side has
// failed, at which point the caller simply negates it and tries the other way.
// So this only has to be right often enough to beat a coin, and being wrong costs
// a second and a bit rather than a stuck enemy.
//----------------------------------------------------------------------------------
float EnemyManager::ChooseStrafeDirection(const Enemy &shooter, const Player &player) const
{
    // Either side is as good as the other, so spread the enemies across both.
    // A fixed answer here would have every enemy stuck on the same corner pick
    // the same way and walk into one another's backs.
    auto either = []() { return (GetRandomValue(0, 1) == 0) ? -1.0f : 1.0f; };

    const Enemy *blocker = FirstBlocker(shooter, player);

    // A wall, not a body. There is no landmark to step away from - FirstBlocker
    // only knows about bodies - so there is no better side to pick.
    if (blocker == nullptr) return either();

    // Which side the blocker sits on. A position is a place and Right() is a
    // direction, so the two are only comparable once the subtraction turns the
    // blocker into an arrow pointing at it from here. Neither vector needs
    // normalising: only the sign of the dot is read, and scaling cannot change it.
    const Vector3 toBlocker = Vector3Subtract(blocker->body.position, shooter.body.position);
    const float side = Vector3DotProduct(toBlocker, shooter.Right());

    // Squarely in the middle of the line, where stepping either way opens the
    // angle equally. Left to a coin so two enemies in this spot do not both
    // resolve it identically and stay in each other's way.
    if (fabsf(side) < 1e-3f) return either();

    // Away from it: a positive dot puts the blocker on the right, and the way
    // past something on your right is to your left.
    return (side > 0.0f) ? -1.0f : 1.0f;
}

//----------------------------------------------------------------------------------
// The guarantee PushOffPlayer cannot make: every living enemy ends at least
// EnemyPersonalSpace from the player, full stop.
//
// PushOffPlayer moves the player too, a share per enemy, one enemy at a time. That
// is right for feel - it stops a body wedged between the player and a wall locking
// up - but it cannot promise clearance in a crowd, because the last enemy processed
// pushes the player back toward one already dealt with, and nothing rechecks it.
// With four enemies closing that is how the camera ends up inside a skull.
//
// Here nothing moves the player, so each enemy is independent and one pass is
// exact. An enemy with a wall behind it gets pushed into the wall instead; that is
// the trade, and a body briefly inside a wall is invisible from where the player is
// standing while the inside of a skull very much is not.
//----------------------------------------------------------------------------------
void EnemyManager::ClearOfPlayer(const Player &player)
{
    for (Enemy &enemy : enemies)
    {
        if (!enemy.IsAlive()) continue;

        const float minDistance = SpecOf(enemy).personalSpace;

        const float dx = enemy.body.position.x - player.body.position.x;
        const float dz = enemy.body.position.z - player.body.position.z;
        const float distanceSq = dx*dx + dz*dz;

        if (distanceSq >= minDistance*minDistance) continue;

        // Straight out along the line to the player, or an arbitrary direction if
        // it is standing exactly on them
        float normalX = 1.0f;
        float normalZ = 0.0f;
        float overlap = minDistance;

        if (distanceSq > 1e-8f)
        {
            const float distance = sqrtf(distanceSq);
            normalX = dx/distance;
            normalZ = dz/distance;
            overlap = minDistance - distance;
        }

        enemy.body.position.x += normalX*overlap;
        enemy.body.position.z += normalZ*overlap;
    }
}

// Push overlapping enemies apart so a crowd does not collapse into one column
void EnemyManager::Separate()
{
    for (size_t i = 0; i < enemies.size(); i++)
    {
        if (!enemies[i].IsAlive()) continue;

        for (size_t j = i + 1; j < enemies.size(); j++)
        {
            if (!enemies[j].IsAlive()) continue;

            Body &a = enemies[i].body;
            Body &b = enemies[j].body;

            const float dx = b.position.x - a.position.x;
            const float dz = b.position.z - a.position.z;
            const float distanceSq = dx*dx + dz*dz;
            const float minDistance = a.radius + b.radius;

            if ((distanceSq >= minDistance*minDistance) || (distanceSq < 1e-8f)) continue;

            const float distance = sqrtf(distanceSq);
            const float push = (minDistance - distance)*0.5f*Config::EnemySeparation;
            const float normalX = dx/distance;
            const float normalZ = dz/distance;

            a.position.x -= normalX*push;
            a.position.z -= normalZ*push;
            b.position.x += normalX*push;
            b.position.z += normalZ*push;
        }
    }
}

//----------------------------------------------------------------------------------
// Picks the clip that matches what the enemy is doing and advances it.
//
// A hit reaction or a swing owns the body until its clip runs out; anything else
// is decided fresh each frame by whether the enemy is moving. Only a state change
// restarts the clock, so a run cycle keeps running across frames.
//
// Death is not decided here - TakeDamage sets it, and nothing overrides it after.
// The corpse plays the authored fall and then hands off to the ragdoll, which is
// what it settles under until RemoveDead takes it away.
//----------------------------------------------------------------------------------
float EnemyManager::SampleTime(const LoadedType &loaded, EnemyAnim state, int variant,
                               float time, bool reversed)
{
    if (!reversed) return time;

    const float duration = loaded.model.ClipDuration(ClipOrIdle(loaded, state, variant));
    if (duration <= 0.0f) return time;

    // Read the cycle from the far end. The mirror has to happen here, in positive
    // time: AnimatedModel::FrameFor clamps a negative frame to the first one
    // rather than wrapping it, so simply running the clock backwards would freeze
    // the enemy on frame zero instead of reversing it.
    return duration - fmodf(time, duration);
}

void EnemyManager::UpdateAnimation(Enemy &enemy, float delta, float drive)
{
    enemy.animTime += delta;
    enemy.previousAnimTime += delta;

    if (enemy.animBlend < 1.0f)
    {
        enemy.animBlend += delta/Config::EnemyAnimBlendTime;
        if (enemy.animBlend > 1.0f) enemy.animBlend = 1.0f;
    }

    if (!enemy.IsAlive()) enemy.deathTime += delta;
    else
    {
        if (!ClipOwnsBody(enemy))
        {
            // The three looping states, decided fresh every frame. Nothing else
            // is chosen here: a swing comes from the AI, a flinch and a death
            // from TakeDamage, and a spawn from being created.
            //
            // Any drive at all means walking. Testing it for "forward" is what let
            // a retreating archer play its idle while the body slid backwards.
            const bool moving = (drive != 0.0f);

            const EnemyAnim next = enemy.blocking ? EnemyAnim::Block
                                                  : (moving ? EnemyAnim::Walk : EnemyAnim::Idle);

            // The gait comes out of how hard the feet are being driven, which is
            // the same number Body::Update turns into speed - so the clip and the
            // distance covered cannot disagree. Idle carries this body's own
            // resting pose instead.
            const int variant = (next == EnemyAnim::Walk)
                              ? ((fabsf(drive) > Config::EnemyRunThreshold) ? 1 : 0)
                              : ((next == EnemyAnim::Idle) ? enemy.idleVariant : 0);

            // A change of gait is a change of clip, so it has to restart the state
            // the same way a change of state does - otherwise a body that drops
            // from a run to a walk keeps running
            if ((enemy.anim != next) || (enemy.animVariant != variant)) enemy.PlayAnim(next, variant);

            // After PlayAnim, so the snapshot it took is of the direction the
            // outgoing clip was running. Decided every frame rather than at the
            // state change: an enemy can turn from closing to giving ground
            // without ever leaving the walk cycle.
            enemy.animReversed = (next == EnemyAnim::Walk) && (drive < 0.0f);
        }
    }

    const LoadedType &loaded = TypeOf(enemy);
    if (!loaded.ready) return;

    if (enemy.bones.size() != (size_t)loaded.model.BoneCount())
    {
        enemy.bones.assign(loaded.model.BoneCount(), MatrixIdentity());
    }

    if (UpdateRagdoll(enemy, delta)) return;

    // Here, and only here, a state with no clip of its own falls back to idle -
    // this is the point that has to draw something no matter what the pack ships
    const int clip = ClipOrIdle(loaded, enemy.anim, enemy.animVariant);
    if (clip < 0) return;

    const float now = SampleTime(loaded, enemy.anim, enemy.animVariant,
                                 enemy.animTime, enemy.animReversed);

    if (enemy.animBlend >= 1.0f)
    {
        loaded.model.Pose(clip, now, Loops(enemy.anim), enemy.bones.data());
        return;
    }

    loaded.model.PoseBlended(ClipOrIdle(loaded, enemy.previousAnim, enemy.previousAnimVariant),
                             SampleTime(loaded, enemy.previousAnim, enemy.previousAnimVariant,
                                        enemy.previousAnimTime, enemy.previousAnimReversed),
                             Loops(enemy.previousAnim),
                             clip, now, Loops(enemy.anim),
                             enemy.animBlend, enemy.bones.data());
}

//----------------------------------------------------------------------------------
// Runs the corpse once the death clip is done. Returns true when the ragdoll owns
// the pose, so the caller leaves the clips alone.
//
// The handover happens on the frame the clip ends, seeded from the pose it ended
// in, which is why the authored fall still reads: the physics only has to settle a
// body that is already most of the way down.
//
// Everything is in model space. Gravity is divided by the model scale because the
// character is drawn scaled, so a model unit is smaller than a world unit, and the
// floor is y = 0 because that is where Enemy::body.position sits.
//----------------------------------------------------------------------------------
bool EnemyManager::UpdateRagdoll(Enemy &enemy, float delta)
{
    const LoadedType &loaded = TypeOf(enemy);

    // The corpse's own death clip, not the type's: Death_A falls in 0.80s and
    // Death_B in 2.63s, so handing over on the wrong one either cuts the fall
    // short or leaves the body holding a pose for nearly two seconds
    const int deathClip = DeathClipOf(enemy);

    if (enemy.IsAlive() || (deathClip < 0)) return false;

    if (enemy.deathTime < loaded.model.ClipDuration(deathClip)) return false;

    if (!enemy.ragdoll.Active())
    {
        enemy.ragdoll.Begin(loaded.model.Skeleton(), enemy.bones.data(), { 0.0f, 0.0f, 0.0f });
    }

    const float gravity = (loaded.scale > 1e-4f) ? Config::Gravity/loaded.scale : Config::Gravity;

    enemy.ragdoll.Update(delta, gravity*Config::RagdollGravityScale, 0.0f);
    enemy.ragdoll.WriteBones(enemy.bones.data());

    return true;
}

//----------------------------------------------------------------------------------
// Whether a one-shot clip is still running the body.
//
// Two things ask this and they must agree: the animation picks its clip from it,
// and the feet decide whether they are allowed to move. Splitting the two is what
// made the enemy slide - it kept walking forward through a swing that is animated
// standing still, so the model skated across the floor.
//
// A pack with no such clip never owns the body, so a missing animation degrades
// to walking rather than to standing frozen.
//----------------------------------------------------------------------------------
bool EnemyManager::ClipOwnsBody(const Enemy &enemy) const
{
    const LoadedType &loaded = TypeOf(enemy);
    const int clip = ClipFor(loaded, enemy.anim, enemy.animVariant);

    // A state the pack has no clip for never owns anything: a missing animation
    // has to degrade to walking, not to standing frozen forever
    if (clip < 0) return false;

    switch (enemy.anim)
    {
        // Emerging, flinching, catching a blow on the guard, or working a crossbow:
        // play out and let go. A shot gets no hold - the archer wants the rest of
        // its long cooldown free to reposition in, which is the whole shape of
        // fighting one.
        case EnemyAnim::Spawn:
        case EnemyAnim::Hit:
        case EnemyAnim::BlockHit:
        case EnemyAnim::Shoot:
            return enemy.animTime < loaded.model.ClipDuration(clip);

        case EnemyAnim::Attack:
            if (enemy.animTime < loaded.model.ClipDuration(clip)) return true;

            // The swing clip and the attack cooldown are two clocks that very
            // nearly agree - 1.17s of animation against 1.2s of cooldown - so
            // between two swings the body would drop into its idle pose for the
            // two frames in between and snap straight back out. Hold when the
            // next swing is close.
            return (enemy.attackCooldown > 0.0f) && (enemy.attackCooldown < Config::EnemyAnimHoldSlack);

        // Death holds its last frame until the ragdoll takes over, and the corpse
        // is not walking anywhere either way, so ownership is not the question
        case EnemyAnim::Death:
        // Loops, chosen fresh every frame. Claiming these would deadlock the state
        // machine: it only reconsiders when nothing owns the body.
        case EnemyAnim::Idle:
        case EnemyAnim::Walk:
        case EnemyAnim::Block:
            break;
    }

    return false;
}

// A guard is animated in place exactly as a swing is, so it stops the feet too -
// but it loops, so it must not claim ownership of the state machine's decision
bool EnemyManager::FeetPinned(const Enemy &enemy) const
{
    return ClipOwnsBody(enemy) || enemy.blocking;
}

//----------------------------------------------------------------------------------
// What clip a state plays, or -1 if this type has none.
//
// Strict on purpose. The old version fell back to idle here, which made "no hit
// reaction in the pack" indistinguishable from "the hit reaction is over" to
// ClipOwnsBody - and a clip that owns the body until a duration it does not have
// owns it forever. ClipOrIdle is where the fallback lives now.
//----------------------------------------------------------------------------------
int EnemyManager::ClipFor(const LoadedType &loaded, EnemyAnim state, int variant)
{
    switch (state)
    {
        case EnemyAnim::Spawn:    return loaded.spawnClip;
        case EnemyAnim::Block:    return loaded.blockClip;
        case EnemyAnim::BlockHit: return loaded.blockHitClip;

        case EnemyAnim::Idle:
        {
            const bool usable = (variant > 0) && (variant < Config::EnemyIdleVariants) &&
                                (loaded.idleClips[variant] >= 0);

            return usable ? loaded.idleClips[variant] : loaded.idleClips[0];
        }

        // Not alternates but two gaits, so the variant is a speed and not a roll:
        // 0 walks, anything else runs. UpdateAnimation decides which from how hard
        // the feet are being driven.
        case EnemyAnim::Walk:
        {
            const int clip = (variant > 0) ? loaded.runClip : loaded.walkClip;

            return (clip >= 0) ? clip : ((variant > 0) ? loaded.walkClip : loaded.runClip);
        }

        // One set between them: an archetype names its swings and `ranged` decides
        // which of the two states plays them
        case EnemyAnim::Attack:
        case EnemyAnim::Shoot:
        {
            const bool usable = (variant > 0) && (variant < loaded.attackCount);

            return usable ? loaded.attackClips[variant] : loaded.attackClips[0];
        }

        // The alternates: an out-of-range or missing variant falls back to [0],
        // which is the one every pack is expected to carry
        case EnemyAnim::Hit:
        {
            const bool usable = (variant > 0) && (variant < Config::EnemyHitVariants) &&
                                (loaded.hitClips[variant] >= 0);

            return usable ? loaded.hitClips[variant] : loaded.hitClips[0];
        }

        case EnemyAnim::Death:
        {
            const bool usable = (variant > 0) && (variant < Config::EnemyDeathVariants) &&
                                (loaded.deathClips[variant] >= 0);

            return usable ? loaded.deathClips[variant] : loaded.deathClips[0];
        }
    }

    return -1;
}

// Falls back to idle so a pack missing, say, a guard still draws something
int EnemyManager::ClipOrIdle(const LoadedType &loaded, EnemyAnim state, int variant)
{
    const int clip = ClipFor(loaded, state, variant);

    return (clip >= 0) ? clip : loaded.idleClips[0];
}

int EnemyManager::DeathClipOf(const Enemy &enemy) const
{
    return ClipFor(TypeOf(enemy), EnemyAnim::Death, enemy.animVariant);
}

//----------------------------------------------------------------------------------
// Pay for the dead, then re-bucket the living.
//
// A pass of its own rather than a few lines inside Update, and it has to be: the
// last blow of a fight can come from a swept blade, from a mote, or from a debug
// key, and only two of those three run inside Update at all. A payout that lived
// there would silently miss anything killed later in the frame - and a corpse with
// no death clip is swept up the same frame it dies, so "later in the frame" means
// "never paid".
//
// Called immediately before RemoveDead, which is the one point in the frame where
// every source of damage has already had its say and nothing has been thrown away
// yet.
//
// The order inside matters too: the kill that just paid out may be the one that
// levelled the player, and the whole meaning of a tier is how far ahead of the
// player a body is RIGHT NOW. Re-tiering first would leave the room describing the
// character they were a moment ago.
//----------------------------------------------------------------------------------
int EnemyManager::CollectExp(Player &player)
{
    int paid = 0;

    for (Enemy &enemy : enemies)
    {
        if (!enemy.expPending) continue;

        enemy.expPending = false;
        paid += enemy.exp;

        if (player.GainExp(enemy.exp) > 0)
        {
            TraceLog(LOG_INFO, "PLAYER: level %i (%i points to spend)",
                     player.level, player.statPoints);
        }
    }

    // Only when it has actually moved. Re-tiering walks every body and rebuilds
    // what the tier decides, which is not something to do sixty times a second for
    // an answer that changes a few times a run.
    if (player.level != lastPlayerLevel) RetierAll(player.level);

    return paid;
}

//----------------------------------------------------------------------------------
// What the dead pay, beyond experience.
//
// Three currencies and three different delivery mechanisms, and the differences are
// the design rather than an accident of implementation:
//
//   COINS      credit straight to the purse. A physical coin per kill would flood the
//              drop pool the moment a swept blade took a whole pack, and the coins the
//              player walked past would be lost to an overflow rather than to a
//              decision.
//   GEMS       are dropped as objects, for exactly the opposite reason. They are rare,
//              and a gem that arrived as a number among a dozen other numbers is a gem
//              nobody noticed earning.
//   CONTRACTS  are not paid here at all. They come only from resolving an event - see
//              the note in Config - because the whole point of the third currency is
//              that it rewards choosing to walk into an objective.
//
// Mana is credited here too, and at two rates. A body dropped by a blade pays outright;
// one dropped by a mote pays half, which is what keeps a cast from ever funding the
// next one. See the invariant in progress/Spellbook.h.
//----------------------------------------------------------------------------------
void EnemyManager::PayLoot(Player &player, LootManager &loot)
{
    int coins = 0;
    int weaponKills = 0;
    int spellKills = 0;

    for (const Enemy &enemy : enemies)
    {
        if (!enemy.expPending) continue;

        // Off the experience the same body pays, so anything worth more to kill is
        // worth more to loot without a second table to keep in step with the first
        int worth = (int)(enemy.exp*Config::CoinsPerExp + 0.5f);

        // Anything an objective sent, which is its whole wave and not only a
        // bounty's champion - see the note on the constant
        if (enemy.eventTag != 0) worth *= Config::EventCoinMult;

        coins += (worth < 1) ? 1 : worth;

        if (enemy.killedBySpell) spellKills++;
        else                     weaponKills++;

        //--------------------------------------------------------------------------
        // The gem roll.
        //
        // Better off anything above the player's own tier, which is what makes an
        // elite worth walking towards rather than around. On raylib's generator, so
        // a logged seed reproduces the drop along with the fight it came out of.
        //--------------------------------------------------------------------------
        int chance = Config::GemChancePerMille;

        if ((enemy.tier == EnemyTier::Elite) || (enemy.tier == EnemyTier::Champion))
        {
            chance *= Config::GemChanceEliteMult;
        }

        if (GetRandomValue(0, 999) < chance) loot.Spawn(Currency::Gems, 1, enemy.body.position);
    }

    if (coins > 0) player.purse.Add(Currency::Coins, coins);

    player.CreditWeaponKills(weaponKills);
    player.CreditSpellKills(spellKills);
}

void EnemyManager::RemoveDead()
{
    // A corpse sticks around long enough to fall over, which is its own death clip
    // plus the linger. With no death clip there is nothing to watch, so it goes the
    // frame it dies - and the clip length is per corpse, not per type, because
    // Death_A and Death_B are 1.8 seconds apart. The linger is what the ragdoll
    // gets to settle in, so it stays the same either way.
    enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
                                 [this](const Enemy &enemy)
                                 {
                                     if (enemy.IsAlive()) return false;

                                     const LoadedType &loaded = TypeOf(enemy);
                                     const int deathClip = DeathClipOf(enemy);
                                     float corpseLife = 0.0f;

                                     if (loaded.ready && (deathClip >= 0))
                                     {
                                         corpseLife = loaded.model.ClipDuration(deathClip) +
                                                      Config::EnemyCorpseLinger;
                                     }

                                     return enemy.deathTime >= corpseLife;
                                 }),
                  enemies.end());
}

int EnemyManager::AliveCount() const
{
    int count = 0;
    for (const Enemy &enemy : enemies) { if (enemy.IsAlive()) count++; }

    return count;
}

void EnemyManager::Draw(const Camera3D &camera) const
{
    // Before the bodies, so a pool of light lies UNDER the skeleton standing in it
    // rather than over its shins
    DrawAuras(camera);

    for (const Enemy &enemy : enemies)
    {
        // The pose was built in Update; this only reads it, which is what keeps
        // one shared Model per type enough for the whole crowd
        const LoadedType &loaded = TypeOf(enemy);

        if (loaded.ready && !enemy.bones.empty())
        {
            // The hurt flash overrides the tier tint rather than blending with it.
            // Being hit is a one-frame event the player needs to read instantly,
            // and a champion's red flashing a slightly different red is not a
            // signal - it is the same colour twice.
            const Color tint = (enemy.hurtFlash > 0.0f) ? (Color){ 255, 130, 130, 255 }
                                                        : enemy.tint;

            loaded.model.Draw(enemy.bones.data(), enemy.body.position,
                              enemy.yaw + Config::EnemyModelYaw*DEG2RAD,
                              loaded.scale*enemy.scale, tint);

            DrawProps(enemy);
            continue;
        }

        if (!enemy.IsAlive()) continue;

        // DrawCapsule wants the centres of the sphere caps, not the extremes
        const float radius = enemy.body.radius;
        const Vector3 bottom = { enemy.body.position.x, enemy.body.position.y + radius, enemy.body.position.z };
        const Vector3 top = { enemy.body.position.x, enemy.body.position.y + enemy.height - radius, enemy.body.position.z };

        const Color body = (enemy.hurtFlash > 0.0f) ? RAYWHITE : (Color){ 190, 80, 90, 255 };

        DrawCapsule(bottom, top, radius, 12, 6, body);
        DrawCapsuleWires(bottom, top, radius, 12, 6, MAROON);

        // Which way it is facing, so its attacks are readable
        const Vector3 eyes = { enemy.body.position.x, enemy.body.position.y + enemy.height*0.85f, enemy.body.position.z };
        const Vector3 nose = Vector3Add(eyes, { -sinf(enemy.yaw)*radius*1.6f, 0.0f, -cosf(enemy.yaw)*radius*1.6f });

        DrawLine3D(eyes, nose, BLACK);
    }
}
