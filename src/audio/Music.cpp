#include "audio/Music.h"

#include "raylib.h"
#include "render/AssetManager.h"

#include <string>

namespace
{
    const std::string Dir = "bg_music/ACTION PACK 1 OGG/";

    //------------------------------------------------------------------------------
    // Volume of a fully faded-in track, at a slider of 1.0.
    //
    // Deliberately low: the music is scenery behind the game, not a layer over it.
    // The mobile game found 0.55 drowned the effects its levels are balanced against
    // and settled here, and the Sfx table was ported at those same levels - so this
    // number and that table have to move together or not at all.
    //------------------------------------------------------------------------------
    constexpr float MusicVolume = 0.28f;

    // Seconds a track takes to fade up at its start and back down before its end, and
    // the length of the crossing-through-silence when the set changes. Long enough to
    // read as a deliberate transition rather than a glitch; short enough that walking
    // into a floor does not spend half the fade in silence.
    constexpr float MusicFadeTime = 2.0f;

    //------------------------------------------------------------------------------
    // Every track, in play order within its set.
    //
    // The Dungeon rows are the playlist and are cycled top to bottom; the other two
    // sets hold a single track each, which simply restarts through a fade.
    //
    // ENDED is this game's own addition. The mobile game sent its game-over screen
    // back to the menu track, which works when the menu is one button away - here a
    // run ends on its own page with a summary to read, and dropping the battle
    // playlist straight into the front-end loop made the end of a run feel like a
    // menu transition rather than like something finishing. It gets the preparation
    // track, which is the calmest thing in the pack.
    //------------------------------------------------------------------------------
    struct Track
    {
        const char *file;
        MusicSet set;
        Music music;
        bool loaded;
    };

    Track Tracks[] =
    {
        { "Long Preparation.ogg", MusicSet::Menu,    { 0 }, false },
        { "Battle Encounter.ogg", MusicSet::Dungeon, { 0 }, false },
        { "Bipedal Mech.ogg",     MusicSet::Dungeon, { 0 }, false },
        { "Magic Fx 7.ogg",       MusicSet::Dungeon, { 0 }, false },
        { "Long Preparation.ogg", MusicSet::Ended,   { 0 }, false },
    };

    constexpr int TrackCount = (int)(sizeof(Tracks)/sizeof(Tracks[0]));

    // Where the current track is in its fade envelope.
    enum class Phase
    {
        Silent,     // Nothing playing
        FadeIn,     // Gain rising to 1
        Hold,       // Full volume, waiting for the end of the track or a set change
        FadeOut     // Gain falling to 0; at 0 the next track starts
    };

    bool     ready      = false;            // Device open and at least one track loaded
    int      current    = -1;               // Index into Tracks, -1 is silence
    MusicSet currentSet = MusicSet::None;   // Set the current track belongs to
    MusicSet wantedSet  = MusicSet::None;   // Set the game is asking for
    Phase    phase      = Phase::Silent;
    float    gain       = 0.0f;             // 0..1 fade multiplier over MusicVolume
    float    setting    = 1.0f;             // The options slider, 0..1

    // First playable track of `set`, or -1 if it has none - an empty set, or one whose
    // files failed to load, which then degrades to silence rather than crashing.
    int FirstOf(MusicSet set)
    {
        for (int i = 0; i < TrackCount; i++)
        {
            if ((Tracks[i].set == set) && Tracks[i].loaded) return i;
        }

        return -1;
    }

    // The track after `index` within its own set, wrapping back to that set's first.
    // For a one-track set that is the same track again, which is how a loop restarts.
    int NextOf(int index)
    {
        const MusicSet set = Tracks[index].set;

        for (int i = index + 1; i < TrackCount; i++)
        {
            if ((Tracks[i].set == set) && Tracks[i].loaded) return i;
        }

        return FirstOf(set);
    }

    // Starts `index` from silence, fading in. Always entered at gain 0 - see Music.h.
    void Start(int index)
    {
        current    = index;
        currentSet = Tracks[index].set;
        phase      = Phase::FadeIn;
        gain       = 0.0f;

        SetMusicVolume(Tracks[index].music, 0.0f);
        PlayMusicStream(Tracks[index].music);
        SeekMusicStream(Tracks[index].music, 0.0f);   // Back to the top on a repeat
    }
}

void GameMusic::Load()
{
    // Game::Init owns the device. If it could not open one, load nothing and stay
    // silent rather than filling memory with streams that can never be heard.
    if (!IsAudioDeviceReady())
    {
        TraceLog(LOG_WARNING, "MUSIC: no audio device, running silent");
        return;
    }

    int loaded = 0;

    for (int i = 0; i < TrackCount; i++)
    {
        const std::string path = AssetManager::Resolve(Dir + Tracks[i].file);

        Tracks[i].music = LoadMusicStream(path.c_str());
        Tracks[i].loaded = IsMusicValid(Tracks[i].music);

        if (!Tracks[i].loaded)
        {
            TraceLog(LOG_WARNING, "MUSIC: could not load %s", Tracks[i].file);
            continue;
        }

        // Never let the stream loop itself: reaching the end is the SIGNAL to fade
        // out and move on, and raylib's looping would splice the seam back in.
        Tracks[i].music.looping = false;
        SetMusicVolume(Tracks[i].music, 0.0f);

        ready = true;
        loaded++;
    }

    TraceLog(LOG_INFO, "MUSIC: %i of %i streams loaded", loaded, TrackCount);
}

void GameMusic::Unload()
{
    if (ready && (current >= 0)) StopMusicStream(Tracks[current].music);

    for (int i = 0; i < TrackCount; i++)
    {
        if (Tracks[i].loaded) UnloadMusicStream(Tracks[i].music);

        Tracks[i].loaded = false;
    }

    current    = -1;
    currentSet = MusicSet::None;
    phase      = Phase::Silent;
    ready      = false;
}

void GameMusic::Want(MusicSet set)
{
    // Acted on by Update. A no-op if it is already playing - see Music.h.
    wantedSet = set;
}

void GameMusic::Update(float delta)
{
    if (!ready) return;

    if (current >= 0) UpdateMusicStream(Tracks[current].music);

    // Silent: pick up whatever set is being asked for. Nothing to fade this frame -
    // Start leaves the volume at 0 and the envelope runs from the next one.
    if (current < 0)
    {
        const int next = FirstOf(wantedSet);

        if (next >= 0) Start(next);

        return;
    }

    Music &music = Tracks[current].music;

    //------------------------------------------------------------------------------
    // Three things start a fade-out: the game asked for another set, the track is
    // within one fade of its end, or the stream stopped on its own - a short track,
    // or the decoder reaching the end. The "stopped" test is limited to Hold so it
    // cannot misfire on the first frames, before playback has actually begun.
    //------------------------------------------------------------------------------
    const float remaining = GetMusicTimeLength(music) - GetMusicTimePlayed(music);

    if ((phase != Phase::FadeOut) &&
        ((wantedSet != currentSet) ||
         (remaining <= MusicFadeTime) ||
         ((phase == Phase::Hold) && !IsMusicStreamPlaying(music))))
    {
        phase = Phase::FadeOut;
    }

    switch (phase)
    {
        case Phase::FadeIn:
            gain += delta/MusicFadeTime;

            if (gain >= 1.0f) { gain = 1.0f; phase = Phase::Hold; }
            break;

        case Phase::FadeOut:
            gain -= delta/MusicFadeTime;

            if (gain <= 0.0f)
            {
                StopMusicStream(music);

                // Either cross to the set the game asked for, or advance the playlist
                const int next = (wantedSet != currentSet) ? FirstOf(wantedSet)
                                                           : NextOf(current);

                current    = -1;
                currentSet = MusicSet::None;
                phase      = Phase::Silent;
                gain       = 0.0f;

                if (next >= 0) Start(next);

                return;
            }
            break;

        default: break;   // Hold: full volume until one of the tests above trips
    }

    SetMusicVolume(Tracks[current].music, MusicVolume*gain*setting);
}

float GameMusic::Volume()
{
    return setting;
}

void GameMusic::SetVolume(float volume)
{
    // Stored only. The level reaches the stream through the fade envelope at the end
    // of Update, so a slider drag cannot fight with a fade already in progress.
    setting = (volume < 0.0f) ? 0.0f : ((volume > 1.0f) ? 1.0f : volume);
}
