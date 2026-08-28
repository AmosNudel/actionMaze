#include "world/Event.h"

#include "audio/Sfx.h"
#include "core/Config.h"
#include "entities/EnemyManager.h"
#include "entities/Player.h"
#include "raymath.h"
#include "render/AssetManager.h"
#include "render/Beam.h"
#include "render/Glow.h"
#include "render/Vfx.h"
#include "rlgl.h"
#include "world/Chaos.h"
#include "world/Level.h"
#include "world/Loot.h"

#include <cmath>
#include <string>

namespace
{

//----------------------------------------------------------------------------------
// Two of the six behaviours, drawn without replacement - see entities/Enemy.h.
//
// Re-rolling rather than shuffling a list: on a table this short a collision settles
// in a couple of draws, and the attempt cap is only there so a table trimmed below
// BountyTraitsPer cannot spin forever.
//
// The two clocks are set here rather than left at zero on purpose. ELUSIVE starts
// CLOSED a full cycle from its first window, and SUMMONER waits half a cycle before
// its first add: a bounty that phased out or summoned on the frame it appeared would
// teach the player nothing about the rhythm they have to play around, which is the
// whole of what those two traits are.
//----------------------------------------------------------------------------------
void RollBountyTraits(Enemy &enemy)
{
    enemy.bountyTraits = 0;

    const int count = (int)BountyTrait::Count;

    for (int picked = 0; (picked < Config::BountyTraitsPer) && (picked < count); ++picked)
    {
        for (int attempt = 0; attempt < 32; ++attempt)
        {
            const BountyTrait trait = (BountyTrait)GetRandomValue(0, count - 1);

            if (enemy.Has(trait)) continue;

            enemy.bountyTraits |= (unsigned char)(1u << (int)trait);
            break;
        }
    }

    enemy.elusiveOpen = 0.0f;
    enemy.elusiveCycle = Config::BountyElusiveCycle;

    enemy.summonCooldown = enemy.Has(BountyTrait::Summoner) ? (Config::BountySummonGap*0.5f)
                                                            : 0.0f;

    // Walked rather than formatted, because "two of six" is a fact about the loop
    // above and not a pair of named slots - a printf with two %s would have to pick
    // which two, which is exactly the branch this table exists to avoid.
    for (int i = 0; i < count; ++i)
    {
        if (!enemy.Has((BountyTrait)i)) continue;

        TraceLog(LOG_INFO, "BOUNTY: %s carries %s", Config::EnemyTypes[enemy.type].name,
                 BountyTraitName((BountyTrait)i));
    }
}
    //------------------------------------------------------------------------------
    // The kinds, in EventKind order.
    //
    // The colours are the whole readout at a distance: what the player sees across a
    // room is a coloured column, and by the second floor they know which is which
    // without reading the label under it. So they are as far apart as four hues can
    // be, and none of them is the portal's cold blue - the way OFF the floor and a
    // thing to do ON it must never be confused at a glance.
    //
    // The briefs are written as instructions rather than as descriptions. A player
    // who has just triggered something has one question, and it is what to do.
    //------------------------------------------------------------------------------
    constexpr EventDef Table[(int)EventKind::Count] =
    {
        { "HUNT",   "three waves - kill them all",   { 255, 150,  60, 255 } },
        { "DEFEND", "stop them reaching the relic",  { 110, 200, 255, 255 } },
        { "BOUNTY", "one champion - kill it",        { 255,  90, 100, 255 } },
        { "SEAL",   "gather the runes",              { 190, 130, 255, 255 } },
    };

    // Which enemy kinds an event may send. Every row of the archetype table except
    // nothing - an event is a harder version of the floor's own population, not a
    // different bestiary, and the difficulty comes from the rank bonus.
    int RollType()
    {
        return GetRandomValue(0, Config::EnemyTypeCount - 1);
    }

    //------------------------------------------------------------------------------
    // Which rooms a Seal scatters its runes across: the event's own room, plus up
    // to two more nearby ones - nearest first, the same idiom Map::ChooseEventRooms
    // already uses for spacing objectives apart.
    //
    // Eligible means big enough to search (Config::EventRoomArea, same bar an
    // event room itself has to clear) and not already spoken for: not the
    // Entrance or the Portal, not another event's room, not a vendor's. A rune in
    // a vendor's room would put the storm's bolts there too, and a shop is
    // supposed to be somewhere safe to stand.
    //
    // Confined to a handful of rooms rather than the whole floor on purpose - the
    // objective should read as "the runes are spread through this WING of the
    // dungeon", which the player can learn by walking it once, not as a scavenger
    // hunt across a map they have not fully explored yet.
    //------------------------------------------------------------------------------
    void ChooseSealRooms(const Level &level, int ownRoom, std::vector<int> &out)
    {
        out.clear();
        out.push_back(ownRoom);

        const std::vector<Room> &rooms = level.Grid().Rooms();
        const std::vector<int> &eventRooms = level.Grid().EventRooms();
        const std::vector<int> &vendorRooms = level.Grid().VendorRooms();

        if ((ownRoom < 0) || (ownRoom >= (int)rooms.size())) return;

        const Room &home = rooms[(size_t)ownRoom];

        constexpr int MaxExtra = 2;

        for (int pick = 0; pick < MaxExtra; ++pick)
        {
            int best = -1;
            int bestAway = -1;

            for (int i = 0; i < (int)rooms.size(); ++i)
            {
                if (i == ownRoom) continue;

                const Room &room = rooms[(size_t)i];

                if ((room.kind == RoomKind::Entrance) || (room.kind == RoomKind::Portal)) continue;
                if (room.Area() < Config::EventRoomArea) continue;

                bool taken = false;

                for (int r : out) { if (r == i) taken = true; }
                for (int r : eventRooms) { if (r == i) taken = true; }
                for (int r : vendorRooms) { if (r == i) taken = true; }

                if (taken) continue;

                const int away = std::abs(room.CenterX() - home.CenterX())
                                + std::abs(room.CenterZ() - home.CenterZ());

                if ((best < 0) || (away < bestAway)) { best = i; bestAway = away; }
            }

            if (best < 0) break;   // Nothing left the floor can offer

            out.push_back(best);
        }
    }
}

const EventDef &EventAt(EventKind kind)
{
    const int index = (int)kind;
    if (index < 0 || index >= (int)EventKind::Count) return Table[0];

    return Table[index];
}

//----------------------------------------------------------------------------------
// The one model this system owns, and the light everything else is drawn out of.
//
// The relic is loaded the way Level::Piece loads a wall: against the ONE canonical
// copy of the dungeon atlas, and rebound onto the lit shader. Both matter. Without
// the atlas argument glTF's relative-URI texture gives this a private 1024-square
// copy of a PNG the level already has; without the shader it comes out flat white
// beside furniture that is being lit, and reads as a hole rather than an object.
//
// The shader is fetched rather than passed in because the AssetManager caches by
// path: this is the same Shader the level configured at load, not a second one.
//----------------------------------------------------------------------------------
void EventManager::Load(AssetManager &assets)
{
    glow = &GlowTexture(assets);

    const std::string dir = "models/dungeon/";
    const std::string path = dir + Config::DefendRelicModel;

    if (!FileExists(AssetManager::Resolve(path).c_str()))
    {
        TraceLog(LOG_WARNING, "EVENTS: no %s - the defend relic is light only", path.c_str());

        return;
    }

    Shader &lit = assets.GetShader("shaders/lit.vs", "shaders/lit.fs");

    relic = &assets.GetModel(path, dir + "dungeon_texture.png");

    for (int i = 0; i < relic->materialCount; ++i) relic->materials[i].shader = lit;
}

void EventManager::Clear()
{
    events.clear();
    running = -1;
}

//----------------------------------------------------------------------------------
// Register an event in each room the map set aside for one.
//
// WHICH rooms is the map's decision, not this one, and that is not an arbitrary
// split. The dressing pass has to keep the middle of an event room clear, and it
// runs at load time long before anything knows what an event is - so the rooms are
// chosen in Map::ChooseEventRooms and this reads them back. Choosing them here
// instead is what put a marker inside a table.
//
// What IS decided here is which kind goes where, because that is the only part the
// geometry has no opinion about: any room the map set aside is as good a place for a
// hunt as for a seal.
//----------------------------------------------------------------------------------
void EventManager::Place(const Level &level, int depth)
{
    Clear();

    (void)depth;    // Kinds are rolled evenly; depth decides how hard, not which

    const std::vector<Room> &rooms = level.Grid().Rooms();

    //------------------------------------------------------------------------------
    // The stocked first floor takes one of EVERY kind, in order, rather than rolling
    // - see Config::StockedFirstFloor. The map gave it one room per kind
    // (Config::StockedEventCount), and this is the half that fills them.
    //
    // The assert is the seam between the two: Map.cpp cannot see this enum, so the
    // count it reserves rooms against is a plain number in Config, and a fifth kind
    // added here stops the build until that number is raised to match.
    //------------------------------------------------------------------------------
    static_assert(Config::StockedEventCount == (int)EventKind::Count,
                  "Config::StockedEventCount and the EventKind enum are out of step");

    const bool stocked = level.Grid().Stocked();

    int next = 0;

    for (int index : level.Grid().EventRooms())
    {
        if ((index < 0) || (index >= (int)rooms.size())) continue;

        Event event;

        event.kind = stocked ? (EventKind)(next%(int)EventKind::Count)
                             : (EventKind)GetRandomValue(0, (int)EventKind::Count - 1);

        next++;
        event.phase = EventPhase::Pending;
        event.room = index;
        event.at = level.Grid().CellCenter(rooms[index].CenterX(), rooms[index].CenterZ());
        event.tag = nextTag++;

        events.push_back(event);

        TraceLog(LOG_INFO, "EVENTS: %s in the %s at (%i, %i)",
                 EventAt(event.kind).name, Rooms::Spec(rooms[index].kind).name,
                 rooms[index].CenterX(), rooms[index].CenterZ());
    }
}

bool EventManager::BlockingClear() const
{
    for (const Event &event : events)
    {
        if ((event.phase == EventPhase::Pending) || (event.phase == EventPhase::Running))
        {
            return true;
        }
    }

    return false;
}

int EventManager::Outstanding() const
{
    int count = 0;

    for (const Event &event : events)
    {
        if ((event.phase == EventPhase::Pending) || (event.phase == EventPhase::Running)) count++;
    }

    return count;
}

const char *EventManager::RunningBanner() const
{
    if (running < 0) return nullptr;

    const Event &event = events[running];

    switch (event.kind)
    {
        case EventKind::Hunt:
            return TextFormat("HUNT   wave %i of %i", event.wavesSent, Config::EventWaves);

        case EventKind::Defend:
            return TextFormat("DEFEND   relic %i / %i", event.relicHealth, event.relicMaxHealth);

        case EventKind::Bounty:
            return "BOUNTY   kill it";

        default:
            return TextFormat("SEAL   %i / %i runes", event.collected, (int)event.runes.size());
    }
}

const char *EventManager::RunningBrief() const
{
    if (running < 0) return nullptr;

    const Event &event = events[running];

    // Long enough to read and no longer. After that the bar and its banner are the
    // readout, and a standing instruction beside them is clutter.
    if (event.sinceStarted > Config::EventBriefTime) return nullptr;

    return EventAt(event.kind).brief;
}

EventManager::Blip EventManager::BlipAt(int index) const
{
    Blip blip;

    if ((index < 0) || (index >= (int)events.size())) return blip;

    const Event &event = events[(size_t)index];

    blip.at = event.at;
    blip.colour = EventAt(event.kind).colour;
    blip.resolved = (event.phase == EventPhase::Done) || (event.phase == EventPhase::Failed);

    return blip;
}

int EventManager::RuneCount() const
{
    if (running < 0) return 0;

    const Event &event = events[running];

    if (event.kind != EventKind::Seal) return 0;

    return (int)event.runes.size();
}

EventManager::RuneBlip EventManager::RuneAt(int index) const
{
    RuneBlip blip;

    if (running < 0) return blip;

    const Event &event = events[running];

    if ((event.kind != EventKind::Seal) || (index < 0) || (index >= (int)event.runes.size()))
    {
        return blip;
    }

    blip.at = event.runes[(size_t)index].at;
    blip.taken = event.runes[(size_t)index].taken;

    return blip;
}

bool EventManager::RunningState(Color &colour, float &progress, float &timeLeft) const
{
    if (running < 0) return false;

    const Event &event = events[running];

    colour = EventAt(event.kind).colour;
    timeLeft = event.timeLeft;

    //------------------------------------------------------------------------------
    // Every kind's bar FILLS towards success, whatever it is actually counting.
    //
    // Defend is the one that has to be inverted to manage it - what it is counting
    // is a pool going down - and inverting it here rather than drawing that kind's
    // bar backwards is what keeps one bar meaning one thing. A readout that
    // sometimes empties as you win is a readout the player has to check the label of
    // before they can read it.
    //------------------------------------------------------------------------------
    switch (event.kind)
    {
        case EventKind::Hunt:
            progress = (Config::EventWaves > 0)
                     ? ((event.wavesSent - 1)/(float)Config::EventWaves) : 0.0f;
            break;

        case EventKind::Defend:
            progress = 1.0f - ((Config::DefendTimeLimit > 0.0f)
                     ? (event.timeLeft/Config::DefendTimeLimit) : 0.0f);
            break;

        case EventKind::Bounty:
            progress = 0.0f;    // One target: it is either alive or the event is over
            break;

        default:
            progress = event.runes.empty()
                     ? 0.0f : (event.collected/(float)event.runes.size());
            break;
    }

    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    return true;
}

//----------------------------------------------------------------------------------
// Somewhere in the event's room a body or a rune can go.
//
// Rejects anything in a wall, in the furniture, or off the room altogether. Tried a
// bounded number of times and then given up on: a room can genuinely be full, and
// the answer to that is to place fewer runes rather than to search forever.
//----------------------------------------------------------------------------------
bool EventManager::FindSpotIn(const Level &level, const Event &event, float radius,
                              Vector3 &spot) const
{
    const std::vector<Room> &rooms = level.Grid().Rooms();

    if ((event.room < 0) || (event.room >= (int)rooms.size())) return false;

    const Room &room = rooms[event.room];

    for (int attempt = 0; attempt < 24; ++attempt)
    {
        const int cx = room.x + GetRandomValue(0, room.w - 1);
        const int cz = room.z + GetRandomValue(0, room.d - 1);

        const Vector3 at = level.Grid().CellCenter(cx, cz);

        if (level.Grid().SolidAtWorld(at.x, at.z)) continue;

        // The prop test is the one that matters here. The cells an event marker
        // stands on are kept clear by the dressing pass, but the rest of the room is
        // furnished like any other - and a rune inside a shelf is one the player
        // cannot walk into, which on a seal is an objective that cannot be finished.
        if (level.PropBlocksAt(at, radius)) continue;

        spot = at;

        return true;
    }

    return false;
}

//----------------------------------------------------------------------------------
// The player walked into a marker.
//
// One event runs at a time, and that is not a simplification to be lifted later: two
// running events would be two banners, two clocks and two sets of waves, and the room
// the player was not standing in would be failing quietly while they were busy in the
// other one. A clock the player cannot see is a clock they cannot play against.
//----------------------------------------------------------------------------------
void EventManager::TryStart(Event &event, const Level &level, const Player &player,
                            EnemyManager &enemies, int depth)
{
    event.phase = EventPhase::Running;
    event.wavesSent = 0;
    event.waveTimer = 0.0f;
    event.sinceStarted = 0.0f;

    switch (event.kind)
    {
        case EventKind::Hunt:
            event.timeLeft = Config::HuntTimeLimit;
            break;

        case EventKind::Defend:
        {
            event.timeLeft = Config::DefendTimeLimit;

            // On the marker's own spot, which the player is standing on: the relic
            // takes the place of the thing they walked into, so where it is was
            // never in doubt.
            event.relicAt = event.at;
            event.relicMaxHealth = Config::DefendRelicHealth;
            event.relicHealth = event.relicMaxHealth;
            break;
        }

        case EventKind::Bounty:
        {
            event.timeLeft = Config::BountyTimeLimit;

            Vector3 spot = event.at;
            FindSpotIn(level, event, 0.5f, spot);

            const int id = enemies.SpawnEventEnemy(RollType(), spot, depth, player.level,
                                                   Config::BountyRankBonus, event.tag);

            // A bounty with no body is an event that can never be finished, so the
            // failure is taken now rather than left as a timer running out on
            // nothing. Rare - it needs a room the placement pass called big enough
            // and the spot search then found no room in.
            if (id == 0)
            {
                event.phase = EventPhase::Failed;
                break;
            }

            // The only body in the game with a pool of its own on top of its tier.
            // A duel against one target the player can bring everything to bear on
            // is over in seconds at ordinary health, and an event a floor waits on
            // has to last longer than that.
            for (Enemy &enemy : enemies.All())
            {
                if (enemy.id != id) continue;

                enemy.maxHealth = (int)(enemy.maxHealth*Config::BountyHealthScale);
                enemy.health = enemy.maxHealth;

                RollBountyTraits(enemy);
                break;
            }

            break;
        }

        default:
        {
            event.timeLeft = Config::SealTimeLimit;
            event.collected = 0;
            event.boltTimer = Config::SealBoltGap;

            event.runes.clear();
            event.bolts.clear();

            //------------------------------------------------------------------
            // Spread across a few rooms rather than one - see ChooseSealRooms.
            // Runes are handed out round robin across whichever rooms it found,
            // so a floor that could only offer one extra room still gets a
            // reasonable split rather than piling everything into the first.
            //------------------------------------------------------------------
            std::vector<int> sealRooms;
            ChooseSealRooms(level, event.room, sealRooms);

            const std::vector<Room> &rooms = level.Grid().Rooms();

            for (int i = 0; i < Config::SealRunes; ++i)
            {
                const int roomIndex = sealRooms[(size_t)(i%(int)sealRooms.size())];

                if ((roomIndex < 0) || (roomIndex >= (int)rooms.size())) continue;

                const Vector3 spot = level.FindOpenSpotIn(rooms[(size_t)roomIndex], 0.3f);

                SealRune rune;
                // Waist height, so they are picked up by walking rather than by
                // standing on them - a pickup at floor level in a first person view
                // is a pickup the player cannot see once they are close to it
                rune.at = { spot.x, spot.y + 1.1f, spot.z };

                event.runes.push_back(rune);
            }

            // Same reasoning as the bounty above: a seal with nothing to gather can
            // never be completed
            if (event.runes.empty()) event.phase = EventPhase::Failed;

            break;
        }
    }

    TraceLog(LOG_INFO, "EVENTS: %s started", EventAt(event.kind).name);
}

//----------------------------------------------------------------------------------
// Finish one, either way.
//
// A failure is RESOLVED. It pays nothing and the floor moves on, because an
// objective that went on blocking the portal after it had been lost would be a
// softlock reachable by standing still.
//
// Both paths clear up what the event still has standing, and for the same reason: a
// pack left behind by a finished hunt is a pack the player can farm for a reward the
// event no longer pays.
//----------------------------------------------------------------------------------
void EventManager::Resolve(Event &event, bool succeeded, Player &player, ChaosState &chaos,
                           EnemyManager &enemies, LootManager &loot)
{
    event.phase = succeeded ? EventPhase::Done : EventPhase::Failed;

    enemies.ClearTag(event.tag);

    event.runes.clear();
    event.bolts.clear();

    if (!succeeded)
    {
        TraceLog(LOG_INFO, "EVENTS: %s failed", EventAt(event.kind).name);

        return;
    }

    // Chaos first. Quell is what notices the pool emptying, and paying the exp
    // before it would put a level-up in the log ahead of the floor it cleared.
    Quell(chaos, Config::EventChaosReward);

    player.GainExp(Config::EventExpReward);

    //------------------------------------------------------------------------------
    // The payout the whole third currency exists for.
    //
    // CONTRACTS come from here and from nowhere else - not from kills at any rate,
    // however small. The point of a currency the captain alone accepts is that it
    // rewards choosing to walk INTO an objective rather than past it, and a trickle
    // off ordinary bodies would quietly make that choice optional.
    //
    // Gems too, because a resolved objective should move more than one vendor's
    // counter. Both are dropped as objects on the marker rather than credited: the
    // event just ended, the player is standing in the middle of it, and walking over
    // what it paid is how the reward reads as coming out of the thing they did.
    //------------------------------------------------------------------------------
    const int contracts = GetRandomValue(Config::EventContractsMin, Config::EventContractsMax);
    const int gems = GetRandomValue(Config::EventGemsMin, Config::EventGemsMax);

    const int total = contracts + gems;

    for (int i = 0; i < contracts; ++i)
    {
        loot.Spawn(Currency::Contracts, 1, event.at, i, total);
    }

    for (int i = 0; i < gems; ++i)
    {
        loot.Spawn(Currency::Gems, 1, event.at, contracts + i, total);
    }

    TraceLog(LOG_INFO, "EVENTS: %s cleared (+%i chaos, +%i exp, %i contracts, %i gems)",
             EventAt(event.kind).name, Config::EventChaosReward, Config::EventExpReward,
             contracts, gems);
}

//----------------------------------------------------------------------------------
// Hunt: three waves out of the floor, and the next one comes early if the last is
// already down.
//
// That early-clear rule is what stops a player who is winning from standing in an
// empty room waiting for a clock. It is also the only thing making the wave gap a
// pacing number rather than a floor on how long the event takes.
//----------------------------------------------------------------------------------
void EventManager::UpdateHunt(Event &event, float delta, EnemyManager &enemies, int depth,
                              int playerLevel)
{
    const int standing = enemies.AliveWithTag(event.tag);

    if (event.wavesSent >= Config::EventWaves)
    {
        // Every wave is out. The event ends when the last of them is down, and not
        // on a clock - the timer is only there to stop a player who has walked away
        // from leaving the floor blocked forever.
        if (standing == 0) event.phase = EventPhase::Done;

        return;
    }

    event.waveTimer -= delta;

    // Due, or the room is already empty. The second is the early clear.
    if ((event.waveTimer > 0.0f) && (standing > 0)) return;

    event.wavesSent++;
    event.waveTimer = Config::HuntWaveGap;

    for (int i = 0; i < Config::HuntWaveSize; ++i)
    {
        // Scattered about the room's centre rather than placed by the spot search:
        // they climb out of the FLOOR, so what matters is that five of them are not
        // standing inside each other, and a wall is a place a skeleton can climb out
        // in front of quite happily.
        const float angle = (i/(float)Config::HuntWaveSize)*2.0f*PI
                          + GetRandomValue(0, 100)/100.0f;

        const float away = Config::HuntSpawnSpread*(0.4f + GetRandomValue(0, 100)/166.0f);

        const Vector3 at = { event.at.x + cosf(angle)*away, event.at.y,
                             event.at.z + sinf(angle)*away };

        enemies.SpawnEventEnemy(RollType(), at, depth, playerLevel, Config::EventRankBonus,
                                event.tag);
    }
}

//----------------------------------------------------------------------------------
// Defend: raiders walk past the player for the relic.
//
// The player is not being attacked. That is the fight, and everything here is in
// service of it - the raiders come in from outside the ring the player will be
// standing in, they do not stop for anything, and the only way to keep the relic is
// to kill them before they arrive.
//
// It ends on the CLOCK rather than on a body count, which is the other half of what
// makes it different from a hunt: there is no last enemy to kill, only a minute and
// a quarter to survive with the thing intact.
//----------------------------------------------------------------------------------
void EventManager::UpdateDefend(Event &event, float delta, const Level &level, Player &player,
                                EnemyManager &enemies, VfxManager &vfx, int depth,
                                int playerLevel)
{
    (void)player;

    //------------------------------------------------------------------------------
    // Collect the swings that landed.
    //
    // Raiders set a flag and know nothing about relics - the same idiom a kill uses
    // for its experience, and for the same reason: the body that swung has no idea
    // what it hit.
    //------------------------------------------------------------------------------
    for (Enemy &enemy : enemies.All())
    {
        if (!enemy.raidHitPending) continue;

        enemy.raidHitPending = false;

        if (enemy.eventTag != event.tag) continue;

        event.relicHealth -= Config::DefendHitDamage;

        // On the relic, so a blow the player did not see land is still a blow they
        // can see has landed
        vfx.Spawn(VfxKind::Splash, event.relicAt, 1.4f, EventAt(EventKind::Defend).colour);

        if (event.relicHealth <= 0)
        {
            event.relicHealth = 0;
            event.phase = EventPhase::Failed;

            return;
        }
    }

    // Held it. The clock running out is the win condition here, which is the reverse
    // of every other kind and is why the timeout is handled inside this function
    // rather than by the shared one in Update.
    if (event.timeLeft <= 0.0f)
    {
        event.phase = EventPhase::Done;

        return;
    }

    if (event.wavesSent >= Config::EventWaves) return;

    event.waveTimer -= delta;
    if (event.waveTimer > 0.0f) return;

    event.wavesSent++;
    event.waveTimer = Config::DefendWaveGap;

    for (int i = 0; i < Config::DefendWaveSize; ++i)
    {
        const float angle = (i/(float)Config::DefendWaveSize)*2.0f*PI
                          + GetRandomValue(0, 100)/100.0f;

        Vector3 at = { event.at.x + cosf(angle)*Config::DefendSpawnRing, event.at.y,
                       event.at.z + sinf(angle)*Config::DefendSpawnRing };

        // The ring may reach through a wall. Falling back into the room is better
        // than not sending the raider at all - a wave that silently spawned nothing
        // is a defend the player wins by doing nothing.
        if (level.Grid().SolidAtWorld(at.x, at.z))
        {
            if (!FindSpotIn(level, event, 0.5f, at)) continue;
        }

        const int id = enemies.SpawnEventEnemy(RollType(), at, depth, playerLevel,
                                               Config::EventRankBonus, event.tag);

        // Most of the wave raids; the rest come for the player instead, through
        // the same AI the floor's own population already fights with - see the
        // note on Config::DefendRaiderFraction.
        const bool raids = GetRandomValue(0, 999) < (int)(Config::DefendRaiderFraction*1000.0f);

        if (raids)
        {
            for (Enemy &enemy : enemies.All())
            {
                if (enemy.id != id) continue;

                enemy.raiding = true;
                enemy.raidTarget = event.relicAt;
                break;
            }
        }
    }
}

void EventManager::UpdateBounty(Event &event, float delta, EnemyManager &enemies)
{
    (void)delta;

    // One body. It is either standing or the event is won - there is no wave, no
    // count and nothing else to check.
    if (enemies.AliveWithTag(event.tag) == 0) event.phase = EventPhase::Done;
}

//----------------------------------------------------------------------------------
// Seal: gather the runes while the room comes down on you.
//
// The only event that is not about killing anything, and the only one whose failure
// is a clock rather than a loss. What it asks for is movement under pressure, which
// is the one thing the other three never test - a hunt and a defend both reward
// standing in a good spot and swinging.
//
// The bolts are a storm rather than a metronome: several live at once, each cheap,
// so what the player reads is the room rather than any one telegraph.
//----------------------------------------------------------------------------------
void EventManager::UpdateSeal(Event &event, float delta, Player &player, VfxManager &vfx)
{
    const Vector3 feet = player.Position();

    for (SealRune &rune : event.runes)
    {
        if (rune.taken) continue;

        // Flat distance. A rune at chest height that had to be reached in three
        // dimensions would be one the player walks through and does not collect.
        const float dx = rune.at.x - feet.x;
        const float dz = rune.at.z - feet.z;

        if ((dx*dx + dz*dz) > (Config::SealPickupRadius*Config::SealPickupRadius)) continue;

        rune.taken = true;
        event.collected++;

        vfx.Spawn(VfxKind::Splash, rune.at, 1.2f, EventAt(EventKind::Seal).colour);

        // The one objective in the game the player collects by hand, and it had no
        // sound at all - so the only confirmation a rune had registered was the
        // counter on the HUD, which is the last place someone running through a
        // bombardment is looking. Full level: it is happening at their feet.
        GameSfx::Play(Sfx::LootRare);
    }

    if (event.collected >= (int)event.runes.size())
    {
        event.phase = EventPhase::Done;

        return;
    }

    //------------------------------------------------------------------------------
    // The storm.
    //
    // Aimed at a still-uncollected RUNE, scattered a little, rather than at the
    // player. The runes are what the storm is guarding now that they are spread
    // across several rooms - a volley that chased the player's feet would strike
    // wherever they happened to be standing and say nothing about where the
    // objective actually is. This still asks for the same thing the event
    // always has, which is moving under pressure: standing next to a rune to
    // wait out its own bolts is not free.
    //------------------------------------------------------------------------------
    event.boltTimer -= delta;

    if (event.boltTimer <= 0.0f)
    {
        event.boltTimer = Config::SealBoltGap;

        int live = -1;
        int liveCount = 0;

        for (int r = 0; r < (int)event.runes.size(); ++r) { if (!event.runes[r].taken) liveCount++; }

        if (liveCount > 0)
        {
            int pick = GetRandomValue(0, liveCount - 1);

            for (int r = 0; r < (int)event.runes.size(); ++r)
            {
                if (event.runes[r].taken) continue;
                if (pick == 0) { live = r; break; }

                pick--;
            }
        }

        if (live >= 0)
        {
            const Vector3 target = event.runes[(size_t)live].at;

            for (int i = 0; i < Config::SealBoltsPerVolley; ++i)
            {
                const float angle = GetRandomValue(0, 359)*DEG2RAD;
                const float away = GetRandomValue(0, 300)/100.0f;

                SealBolt bolt;
                bolt.at = { target.x + cosf(angle)*away, event.at.y, target.z + sinf(angle)*away };

                event.bolts.push_back(bolt);
            }
        }
    }

    for (SealBolt &bolt : event.bolts)
    {
        bolt.age += delta;

        if (bolt.struck || (bolt.age < Config::SealBoltWarn)) continue;

        bolt.struck = true;

        //------------------------------------------------------------------------
        // The strike. Bigger than a straight read of the radius would suggest and
        // layered with a second, wider burst underneath it - the sprite sheet
        // alone at its old size was a fast, small, white flash that was easy to
        // miss against a lit dungeon, which is not what "the ceiling coming down"
        // should read as.
        //------------------------------------------------------------------------------
        vfx.Spawn(VfxKind::Lightning, { bolt.at.x, bolt.at.y + 1.0f, bolt.at.z },
                  Config::SealBoltRadius*3.2f, WHITE);

        vfx.Spawn(VfxKind::Splash, { bolt.at.x, bolt.at.y + 0.05f, bolt.at.z },
                  Config::SealBoltRadius*2.4f, EventAt(EventKind::Seal).colour);

        const float dx = bolt.at.x - player.Position().x;
        const float dz = bolt.at.z - player.Position().z;

        if ((dx*dx + dz*dz) > (Config::SealBoltRadius*Config::SealBoltRadius)) continue;

        // A fraction of the pool rather than a flat figure, so a bolt costs the same
        // share of a deep character as of a shallow one. Floored at 1: a strike that
        // connected and did nothing reads as the game dropping a hit.
        int damage = (int)(player.MaxHealth()*Config::SealBoltDamageFrac + 0.5f);
        if (damage < 1) damage = 1;

        // TakeDamage and not TakeDamageFrom - there is nothing to point a shield at.
        // It comes from above.
        player.TakeDamage(damage);
    }

    // Swept once they have landed and their effect has had time to play
    for (size_t i = event.bolts.size(); i-- > 0; )
    {
        if (event.bolts[i].age < Config::SealBoltWarn + Config::SealBoltLinger) continue;

        event.bolts.erase(event.bolts.begin() + i);
    }
}

void EventManager::Update(float delta, const Level &level, Player &player,
                          EnemyManager &enemies, VfxManager &vfx, LootManager &loot,
                          ChaosState &chaos, int depth)
{
    running = -1;

    // The relic's own clock. Advanced here and not from GetTime() so that a paused
    // game - which is every frame the sheet or the menu is up - leaves the thing
    // hanging still rather than turning behind the panel.
    relicBob += delta;

    for (int i = 0; i < (int)events.size(); ++i)
    {
        Event &event = events[i];

        event.spin += delta*Config::PortalSpinRate;

        if (event.phase == EventPhase::Pending)
        {
            // Walked into. The marker is the same shape as the portal and is walked
            // into the same way, which is the point: the player learns the gesture
            // once.
            const Vector3 feet = player.Position();

            const float dx = feet.x - event.at.x;
            const float dz = feet.z - event.at.z;

            const bool inside = (dx*dx + dz*dz)
                              <= (Config::EventMarkerRadius*Config::EventMarkerRadius);

            // One at a time - see TryStart. A player standing in a second marker
            // while one is already running is simply standing in a marker.
            if (inside && (running < 0)) TryStart(event, level, player, enemies, depth);
        }

        if (event.phase != EventPhase::Running) continue;

        running = i;
        event.sinceStarted += delta;

        //--------------------------------------------------------------------------
        // The clock.
        //
        // Ticked before the kind's own update, so a kind that WINS on the clock
        // running out - defend - sees it at zero on the same frame everything else
        // would see it as a failure. That ordering is why defend handles its own
        // timeout rather than falling through to the shared one below.
        //--------------------------------------------------------------------------
        if (event.timeLeft > 0.0f)
        {
            event.timeLeft -= delta;
            if (event.timeLeft < 0.0f) event.timeLeft = 0.0f;
        }

        switch (event.kind)
        {
            case EventKind::Hunt:
                UpdateHunt(event, delta, enemies, depth, player.level);
                break;

            case EventKind::Defend:
                UpdateDefend(event, delta, level, player, enemies, vfx, depth, player.level);
                break;

            case EventKind::Bounty: UpdateBounty(event, delta, enemies); break;
            default:                UpdateSeal(event, delta, player, vfx); break;
        }

        // Out of time, for the three kinds that lose by it. Defend has already
        // turned its own zero into a win above and left Running behind it.
        if ((event.phase == EventPhase::Running) && (event.timeLeft <= 0.0f))
        {
            event.phase = EventPhase::Failed;
        }

        if (event.phase == EventPhase::Done)   Resolve(event, true, player, chaos, enemies, loot);
        if (event.phase == EventPhase::Failed) Resolve(event, false, player, chaos, enemies, loot);

        if (event.phase != EventPhase::Running) running = -1;
    }
}

void EventManager::Draw(const Camera3D &camera) const
{
    if (glow == nullptr) return;

    for (const Event &event : events)
    {
        if (event.phase == EventPhase::Pending)
        {
            // The marker: the same object as the portal in a different colour, so a
            // column of light always means the same thing - somewhere to walk into
            BeamLook look;
            look.colour = EventAt(event.kind).colour;
            look.radius = Config::EventMarkerRadius;
            look.height = Config::EventMarkerHeight;
            look.spin = event.spin;
            look.motes = 5;
            look.moteSize = 0.22f;

            DrawBeam(camera, *glow, event.at, look);

            continue;
        }

        if (event.phase != EventPhase::Running) continue;

        if (event.kind == EventKind::Defend)
        {
            //----------------------------------------------------------------------
            // The relic: a short, fat, bright beam.
            //
            // Deliberately the same object again rather than a model. It has to be
            // legible from across a room through a crowd of raiders standing on it,
            // and a solid object at that size is one that a body in front of it
            // hides. Light is not hidden by much.
            //
            // It DIMS as it is broken, which is the readout: the player never has to
            // look at a number to know how the relic is doing.
            //----------------------------------------------------------------------
            const float left = (event.relicMaxHealth > 0)
                             ? (event.relicHealth/(float)event.relicMaxHealth) : 0.0f;

            BeamLook look;
            look.colour = EventAt(EventKind::Defend).colour;
            look.radius = Config::DefendRelicRadius;
            look.height = 1.6f;
            look.lit = 0.35f + 0.65f*left;
            look.spin = event.spin;
            look.motes = 4;
            look.moteSize = 0.18f;
            look.columnAlpha = 200;

            DrawBeam(camera, *glow, event.relicAt, look);

            //----------------------------------------------------------------------
            // ...and the relic itself, hanging in the middle of it.
            //
            // Drawn AFTER the beam rather than before. The beam is additive light
            // and writes no depth, so a solid object drawn first is still visible
            // through it - and drawn second the model sits in the column instead of
            // being painted over by it.
            //
            // It bobs on the shared clock and turns on its own slow rate, both of
            // which are the point: nothing else in a dungeon is off the ground and
            // moving, so the one thing that is cannot be mistaken for furniture.
            //----------------------------------------------------------------------
            if (relic != nullptr)
            {
                const Vector3 at = { event.relicAt.x,
                                     event.relicAt.y + Config::DefendRelicLift
                                        + sinf(relicBob*Config::DefendRelicBobRate)
                                          *Config::DefendRelicBob,
                                     event.relicAt.z };

                // Dimmed with the light rather than left bright. The whole readout
                // of a defend is that the thing is being worn down, and a relic at
                // full colour over a guttering column says two different things.
                const unsigned char shade = (unsigned char)(150.0f + 105.0f*left);

                DrawModelEx(*relic, at, { 0.0f, 1.0f, 0.0f },
                            relicBob*Config::DefendRelicSpin*RAD2DEG,
                            { Config::DefendRelicScale, Config::DefendRelicScale,
                              Config::DefendRelicScale },
                            { shade, shade, shade, 255 });
            }

            continue;
        }

        if (event.kind != EventKind::Seal) continue;

        // The runes, as small bright motes at waist height
        for (const SealRune &rune : event.runes)
        {
            if (rune.taken) continue;

            BeamLook look;
            look.colour = EventAt(EventKind::Seal).colour;
            look.radius = 0.22f;
            look.height = 0.5f;
            look.spin = event.spin*1.6f;
            look.motes = 3;
            look.moteSize = 0.14f;
            look.poolAlpha = 0;     // It floats; there is nothing under it to light

            DrawBeam(camera, *glow, rune.at, look);
        }

        //----------------------------------------------------------------------
        // The strikes coming down: a ring on the floor that closes as the bolt
        // arrives.
        //
        // A ring and not a beam, because it has to say WHERE and WHEN at the same
        // time and a column says only where. The radius is the true hit radius from
        // the first frame - a telegraph that grew into its own hit box would be one
        // the player could stand in safely right up until they could not.
        //----------------------------------------------------------------------
        for (const SealBolt &bolt : event.bolts)
        {
            const float t = bolt.age/Config::SealBoltWarn;

            const Color colour = bolt.struck ? (Color){ 255, 255, 255, 200 }
                                             : EventAt(EventKind::Seal).colour;

            const float inner = bolt.struck ? 0.0f
                                            : Config::SealBoltRadius*(t < 1.0f ? t : 1.0f);

            DrawCylinderWires({ bolt.at.x, bolt.at.y + 0.03f, bolt.at.z },
                              Config::SealBoltRadius, Config::SealBoltRadius, 0.0f, 24,
                              Fade(colour, 0.75f));

            if (inner > 0.05f)
            {
                DrawCylinderWires({ bolt.at.x, bolt.at.y + 0.04f, bolt.at.z },
                                  inner, inner, 0.0f, 20, Fade(colour, 0.9f));
            }
        }
    }
}
