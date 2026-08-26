#pragma once

#include "raylib.h"

#include <vector>

class AssetManager;
class Level;
class Player;
class EnemyManager;
class VfxManager;
class LootManager;
struct ChaosState;

//----------------------------------------------------------------------------------
// The rooms that are not just rooms.
//
// A floor's ordinary business is a pool of chaos worked through by killing whatever
// is standing in the way. An event is the exception: a room that has one specific
// thing in it, which the player has to do, and which the way down waits for.
//
// That last part is the whole point. Chaos can hit zero at any moment - it usually
// does before the player has been everywhere - and if the portal opened on that
// alone then a floor's events would be optional decoration nobody bothered with. So
// the portal waits on BOTH: the pool empty, and every event resolved. Which is why
// ChaosState has carried two flags since it was written.
//
// --- Resolved, not succeeded --------------------------------------------------------
// An event that FAILS is resolved. It pays nothing, and the floor moves on. A failed
// objective that went on blocking the portal would be a softlock reachable by
// standing still, and there is no version of this system worth having where the
// player can lose a run to a timer they did not know was running.
//
// --- Pending is a real state --------------------------------------------------------
// An event the player has never visited still blocks. It has to: the alternative is
// that walking past a room is how you skip its objective. Pending events are marked
// with a column of light in their own colour, tall enough to read across a room, so
// "find the ones you have not done" is a thing the player can actually act on.
//----------------------------------------------------------------------------------
enum class EventKind
{
    // Three waves climb out of the floor around you. Kill them all.
    Hunt = 0,
    // Raiders walk past you for the relic. They are not interested in you at all,
    // which is what makes it a different fight rather than a fight in a new hat.
    Defend,
    // One body, several ranks above the floor, with a pool to match. The only
    // event that is a duel.
    Bounty,
    // Runes scattered across the room, a clock, and the ceiling coming down.
    // The one event that is not about killing anything.
    Seal,

    Count
};

enum class EventPhase { Pending, Running, Done, Failed };

//----------------------------------------------------------------------------------
// One kind's whole definition. Everything a kind IS lives in the table in Event.cpp,
// so a fifth is a row rather than a change - the same rule the magic table follows.
//----------------------------------------------------------------------------------
struct EventDef
{
    const char *name;       // Over its marker and on the HUD, so keep it short
    const char *brief;      // One line, shown when it starts. What to do.
    Color colour;           // Marker, banner, and its blip wherever one is drawn
};

const EventDef &EventAt(EventKind kind);

//----------------------------------------------------------------------------------
// One rune of a Seal, or one strike coming down on it.
//
// Both live here rather than in a subsystem of their own because a Seal is the only
// thing that has any, there is only ever one Seal running, and a pickup that exists
// for twenty seconds does not need a manager.
//----------------------------------------------------------------------------------
struct SealRune
{
    Vector3 at{};
    bool taken = false;
};

struct SealBolt
{
    Vector3 at{};
    // Counts UP from zero. Below Config::SealBoltWarn it is a ring on the floor and
    // nothing else; at the warn mark it lands, once. One clock, not two - the
    // telegraph and the strike are the same event seen before and after.
    float age = 0.0f;
    bool struck = false;
};

//----------------------------------------------------------------------------------
// Every event on this floor, and what each of them is up to.
//
// One class rather than a struct plus loose functions, because an event owns bodies
// in the enemy list and effects in the vfx pool, and both of those have to be
// cleaned up when a floor is thrown away. Something has to be responsible for that.
//----------------------------------------------------------------------------------
class EventManager
{
public:
    void Load(AssetManager &assets);

    //------------------------------------------------------------------------------
    // Pick this floor's rooms and register an event in each, all Pending.
    //
    // Called after the level is built and before anything walks on it. Rooms are
    // chosen from the ones that are neither the entrance nor the portal and are big
    // enough to hold a fight - and spread apart, because two events in adjoining
    // rooms is one long fight with a wall in the middle of it.
    //------------------------------------------------------------------------------
    void Place(const Level &level, int depth);

    // Everything an event needs to do its job. It spawns bodies, sets off effects,
    // damages the player, pays out chaos and experience - which is most of the game,
    // and is why this takes most of the game as arguments rather than owning any
    // of it.
    void Update(float delta, const Level &level, Player &player, EnemyManager &enemies,
                VfxManager &vfx, LootManager &loot, ChaosState &chaos, int depth);

    // Inside BeginMode3D. Markers, relics, runes and the strikes coming down.
    void Draw(const Camera3D &camera) const;

    // True while any event is still Pending or Running. What holds the portal shut.
    bool BlockingClear() const;

    // How many are unfinished, and what the one that is RUNNING is doing - null when
    // none is. Both for the HUD; the second is what turns "an event is happening"
    // into "here is what to do about it".
    int Outstanding() const;
    const char *RunningBanner() const;

    // What to DO about the running event, or null once it has been up long enough to
    // have been read. A player who has just triggered something has one question and
    // it is this one - and a line that stayed on screen for the whole event would be
    // instructions nobody is reading competing with the readout they are.
    const char *RunningBrief() const;
    // Colour and progress of the running event, for the HUD's own bar. Progress runs
    // 0 to 1 towards success, so every kind's bar fills the same way whatever it is
    // actually counting.
    bool RunningState(Color &colour, float &progress, float &timeLeft) const;

    //------------------------------------------------------------------------------
    // Every event on the floor as a point, a colour and whether it is finished.
    //
    // For the minimap, which cannot be handed the events themselves: what it wants
    // is where to put a blip and what colour to make it, and the private Event
    // struct is thirty fields of clock and wave state it has no business seeing.
    //
    // Whether the room has been DISCOVERED is not decided here. That is the map's
    // own fog, and this reports every event on the floor including ones in rooms
    // nobody has walked into - the caller filters. Deciding it here would mean this
    // class knowing what has been explored, which is the one thing it does not.
    //------------------------------------------------------------------------------
    struct Blip
    {
        Vector3 at{};
        Color colour{};
        bool resolved = false;      // Done or Failed - either way, nothing left to do
    };

    int BlipCount() const { return (int)events.size(); }
    Blip BlipAt(int index) const;

    void Clear();

private:
    struct Event
    {
        EventKind kind = EventKind::Hunt;
        EventPhase phase = EventPhase::Pending;

        Vector3 at{};           // Middle of its room, where the marker stands
        int room = -1;

        // Which bodies belong to it. Enemy ids, not indices - RemoveDead compacts
        // the vector between frames and an index that meant one skeleton last frame
        // means its neighbour this one.
        int tag = 0;

        float timeLeft = 0.0f;  // Seconds before it fails. Below zero is no limit.
        float sinceStarted = 0.0f;

        // Waves. Hunt and Defend share these: both are a fight that arrives in
        // packs on a clock, and the difference between them is what the packs are
        // interested in rather than how they turn up.
        int wavesSent = 0;
        float waveTimer = 0.0f;

        // Defend
        Vector3 relicAt{};
        int relicHealth = 0;
        int relicMaxHealth = 0;

        // Seal
        std::vector<SealRune> runes;
        std::vector<SealBolt> bolts;
        int collected = 0;
        float boltTimer = 0.0f;

        float spin = 0.0f;      // The marker's, and the relic's
    };

    // Starts the event the player is standing on, if any is Pending. One at a time:
    // two running events would be two banners, two clocks and two sets of waves,
    // and the room the player is not in would be failing while they were busy.
    void TryStart(Event &event, const Level &level, const Player &player,
                  EnemyManager &enemies, int depth);

    // `playerLevel` tiers whatever they send. Threaded through rather than read off
    // the Player, because these take an EnemyManager and a level for the spawn and
    // there is no reason for them to need a whole character as well.
    void UpdateHunt(Event &event, float delta, EnemyManager &enemies, int depth,
                    int playerLevel);
    void UpdateDefend(Event &event, float delta, const Level &level, Player &player,
                      EnemyManager &enemies, VfxManager &vfx, int depth, int playerLevel);
    void UpdateBounty(Event &event, float delta, EnemyManager &enemies);
    void UpdateSeal(Event &event, float delta, Player &player, VfxManager &vfx);

    // Finish one, either way. Pays out on success and nothing on failure, and in
    // both cases clears up whatever the event still has standing.
    void Resolve(Event &event, bool succeeded, Player &player, ChaosState &chaos,
                 EnemyManager &enemies, LootManager &loot);

    // Somewhere inside the event's room a body or a rune can go. Rejects anything
    // in a wall or inside the furniture, so a wave cannot arrive inside a table.
    bool FindSpotIn(const Level &level, const Event &event, float radius, Vector3 &spot) const;

    std::vector<Event> events;

    // How long the running defend has been up, for the relic's bob. Its own clock
    // rather than `spin`, which is shared with the marker and turns at the
    // portal's rate - a relic bobbing at 1.4 rad/s looks like it is being shaken.
    float relicBob = 0.0f;

    // Which event is Running, or -1. One at a time - see TryStart.
    int running = -1;

    // Handed out and never reused, so a body from a finished hunt cannot be counted
    // towards a later one that happened to get the same number.
    int nextTag = 1;

    Texture2D *glow = nullptr;      // Shared, owned by the AssetManager

    // The defend relic's body. Owned by the AssetManager, and null when the
    // dungeon pack is missing - in which case the column of light underneath it is
    // the whole marker again, which is what it was before this existed.
    Model *relic = nullptr;
};
