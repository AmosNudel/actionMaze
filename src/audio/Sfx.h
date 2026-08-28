#pragma once

//----------------------------------------------------------------------------------
// One-shot sound effects: UI, movement, combat and magic.
//
// A fire-and-forget bank: the game asks for a Sfx by name and the module picks a free
// voice, gives it a small random pitch and plays it. Nothing is tracked afterwards -
// there is no handle to stop or query, because no caller wants one.
//
// Ported from the mobile game's sfx.cpp along with the sample pack itself, which is
// the point: the two are meant to sound like one studio's work. What is different
// here is the LIST. That game's Skill* rows were its eight skills; this game's eight
// MAGIC SCHOOLS are the same eight names (see combat/MagicKind.h), so those rows came
// across unchanged and only got renamed to the vocabulary this project uses. Around
// them sit the things this game has and that one did not - a parry, a bow, a floor to
// descend, a vendor to buy from - drawn from the same pack.
//
// Every clip is loaded once and then aliased Voices times (see Sfx.cpp). Aliases share
// the sample data but have their own playback state, so a rapid repeat - running
// footsteps, two skeletons swinging at once - layers instead of restarting the one
// voice and clipping itself short.
//
// Every play gets a random pitch within the clip's own spread, so the same sample
// heard back to back does not read as a loop. Pitch also shifts playback speed
// slightly, which is exactly the variation wanted here.
//
// Levels ride on raylib's master volume, so the mute on the options page silences
// these along with the music and nothing here needs to know about it.
//
// Lifecycle: Load() after InitAudioDevice, Unload() before CloseAudioDevice.
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// The bank. Order must match the table in Sfx.cpp - the static_assert there catches a
// mismatch at compile time.
//----------------------------------------------------------------------------------
enum class Sfx
{
    None = -1,          // "no sound": Play ignores it, for a slot deliberately unset

    // --- Interface ---------------------------------------------------------------
    UiConfirm = 0,      // A menu entry or a page button taken
    UiDenied,           // A purchase or a spend that could not be afforded
    UiPause,            // The pause menu opening
    UiUnpause,          // ...and closing
    UiBuy,              // A vendor purchase going through

    // --- The player's body -------------------------------------------------------
    Step,               // A footstep while moving on the ground
    Jump,
    Land,               // Touching down after a real fall

    // --- Melee -------------------------------------------------------------------
    // Swing is the weapon going out and lands on nothing; Hit is it connecting.
    // Kept apart because a miss is information: this game's fights are read off
    // whether the blade found anything, and a swing that sounded identical either
    // way would take that away.
    Swing,
    Hit,
    Crit,               // A critical landing - over the top of Hit, not instead of it
    Block,              // An enemy's guard stopping the blow
    Parry,              // The player's own parry landing - see Config::ParryStunTime
    Shoot,              // A bow, a staff bolt or a thrown weapon leaving the hand

    // --- What the enemies do -----------------------------------------------------
    // The player fights with their back to most of the room, so every one of these
    // is a WARNING before it is a texture: a swing starting behind you, an archer
    // loosing across the room, a support mage winding up a buff you want to
    // interrupt. All three are played positionally - see PlayAt - because where
    // they came from is the whole information.
    //
    // Each is deliberately the player's own equivalent at a different pitch rather
    // than an unrelated sample. The vocabulary stays one vocabulary - a wind-up is a
    // wind-up, a loosed shot is a loosed shot - and the pitch is what says whose it
    // was.
    EnemySwing,
    EnemyShoot,
    EnemyCast,

    // --- Taking and dealing death ------------------------------------------------
    PlayerHurt,
    PlayerDeath,

    // An enemy taking a hit from something that FLEW - an arrow, a thrown dagger.
    // Melee already sounds through Hit, which is at the player's own arm's length
    // and needs no distance; this is the same event happening somewhere else.
    //
    // A mote does not use this. A spell landing sounds through SpellImpact below,
    // once, wherever it went off - see the note there.
    EnemyHurt,
    EnemyDeath,

    // --- Magic -------------------------------------------------------------------
    // Cast is the wind-up, played the moment a cast starts - it is what tells the
    // player the button took, a few frames before the mote appears. Its pitch spread
    // is the widest in the table because it is heard on every single cast.
    Cast,

    // Then one per school, in Magic order. Named for the school rather than for the
    // sample so the table reads as itself.
    MagicFlame,
    MagicSpark,
    MagicToxin,
    MagicBlast,
    MagicSplash,
    MagicFlash,
    MagicNova,
    MagicRend,

    //------------------------------------------------------------------------------
    // A mote arriving, wherever it stopped - a body, a wall, the floor.
    //
    // ONE sound per cast rather than one per body caught. Every school but SPARK
    // lands on an area now and a burst routinely catches half a pack; a flesh hit
    // per body turned a single cast into five overlapping impacts, which is louder
    // than the cast itself and says nothing the first one had not already said.
    //
    // It is also why a mote does not use EnemyHurt: the spell landing is the event,
    // and how many bodies it happened to find is what the damage numbers are for.
    //------------------------------------------------------------------------------
    SpellImpact,

    // --- The world ---------------------------------------------------------------
    Interact,           // A chest lid, a container being opened
    Pickup,             // A bottle or a coin walked over

    //------------------------------------------------------------------------------
    // A GEM or a CONTRACT walked over, which Pickup above is deliberately not.
    //
    // The two rare currencies are the whole reason loot is a physical thing lying on
    // the floor rather than a number that appears (see world/Loot.h) - a gem the
    // player did not notice earning is a gem that may as well have been a stat
    // increment. Giving them the same click as a health bottle undoes exactly that.
    //------------------------------------------------------------------------------
    LootRare,
    Heal,
    Buff,               // A buff taking effect, the player's or an enemy's
    LevelUp,
    Descend,            // The portal taking the player a floor down

    Count
};

namespace GameSfx
{
    // Loads every clip and its voices. Safe with no audio device or missing files: the
    // bank then stays empty and Play does nothing, so the game runs silent rather
    // than crashing.
    void Load();

    // Frees every clip and alias. Call while the audio device is still open.
    void Unload();

    // Plays `id` once on the next free voice, at a randomised pitch. Sfx::None and
    // clips that failed to load are ignored.
    void Play(Sfx id);

    //------------------------------------------------------------------------------
    // The same thing, attenuated by how far away it happened.
    //
    // This game is in 3D and the mobile one was not, which is the one real gap in the
    // port: a skeleton dying across the map at the volume of one dying at your feet
    // turns a quiet floor into a wall of noise. Not true positional audio - there is
    // no panning here, only distance - because what the player actually needs from a
    // sound over there is to know it happened, not where.
    //
    // Silent past Config::SfxHearingRange, so a fight the player walked away from
    // costs nothing at all.
    //------------------------------------------------------------------------------
    void PlayAt(Sfx id, float distance);

    //------------------------------------------------------------------------------
    // The effects slider, 0..1. Scales each clip's own balanced level, so 1.0 is the
    // tuned mix rather than full scale. Read at the moment a sound is played, so it
    // takes effect on the next one - a sound already ringing keeps the level it
    // started at, which is what you want while dragging the slider.
    //------------------------------------------------------------------------------
    float Volume();
    void SetVolume(float volume);   // Clamped to 0..1
}
