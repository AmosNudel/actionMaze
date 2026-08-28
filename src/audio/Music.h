#pragma once

//----------------------------------------------------------------------------------
// Background music: one playlist per screen, and the fades between them.
//
// Ported from the mobile game's audio.cpp, which is where the design notes below
// were written and proven - see the note on Sfx.h for what the two projects share
// and why. What changed here is the SETS: that game had a menu and a level, this one
// has a front end, a dungeon and a run that has ended, and the last of those wants
// its own music rather than the menu's.
//
// The player is driven declaratively: every frame the game says which SET it wants
// (see Game::Update) and calls Update(). Asking for the set that is already playing
// does nothing, so screen transitions need no bookkeeping of their own.
//
// Nothing is ever cut off or looped by the stream itself. Every start and every end
// goes through MusicFadeTime, and a track that reaches its end is followed by the
// next of its set, wrapping - so a one-track set restarts itself. That is deliberate:
// these loops do not join cleanly, so the seam is never played. The track fades to
// silence and the next fades up.
//
// Lifecycle: Load() once after InitAudioDevice, Unload() before CloseAudioDevice -
// the same rule the AssetManager follows for GL resources.
//----------------------------------------------------------------------------------

// Which group of tracks the game wants playing.
enum class MusicSet
{
    None = 0,   // Silence
    Menu,       // The front end: main menu, options, credits, both loading screens
    Dungeon,    // The run itself, played in order and looped
    Ended       // A run that is over, won or lost - see the note on the table

};

namespace GameMusic
{
    // Loads every stream. Safe with no audio device or missing files: the player then
    // does nothing and the game runs silent.
    void Load();
    void Unload();

    //------------------------------------------------------------------------------
    // Requests a set. Re-requesting the current one is a no-op - it does NOT restart
    // it - so this can be called unconditionally every frame, which is exactly how
    // Game::Update uses it.
    //------------------------------------------------------------------------------
    void Want(MusicSet set);

    // Advances the stream and the fade state machine. Once a frame, INCLUDING while
    // paused: the music keeps playing behind the pause menu and the shop.
    void Update(float delta);

    //------------------------------------------------------------------------------
    // The music slider, 0..1. Scales the module's own mix level rather than replacing
    // it, so 1.0 means "as loud as the mix intends" - music sitting behind the game -
    // and not full scale. Applied on the next Update, which runs every frame, so
    // dragging it is heard immediately.
    //------------------------------------------------------------------------------
    float Volume();
    void SetVolume(float volume);   // Clamped to 0..1
}
