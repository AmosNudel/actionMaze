#include "audio/Sfx.h"

#include "core/Config.h"
#include "raylib.h"
#include "render/AssetManager.h"

#include <cmath>
#include <string>

namespace
{
    const std::string Dir = "sfx/";

    //------------------------------------------------------------------------------
    // Copies of each clip that can sound at the same time.
    //
    // Four covered the mobile game's worst realistic overlap. This one is louder: a
    // burst catches a pack, every body in it takes a hit, and half of them die on the
    // same frame - so it is six here. Beyond that the oldest voice is reused, which
    // at these lengths is inaudible.
    //------------------------------------------------------------------------------
    constexpr int Voices = 6;

    //------------------------------------------------------------------------------
    // One entry per Sfx, in enum order.
    //
    // `volume` levels the clips against each other - the source wavs are recorded at
    // quite different loudness - and `pitchSpread` is the plus-or-minus range the
    // random pitch is drawn from: wide for sounds heard many times a second, narrow
    // for UI, where a big swing reads as a different button.
    //
    // These sit high, most near the top of their range, because the mix is set by
    // MusicVolume in Music.cpp sitting LOW rather than by holding the effects down.
    // The two were balanced against each other in the mobile game and ported as a
    // pair; moving one without the other undoes that.
    //
    // `pitchCenter` is the pitch before the random spread - 1.0 is the sample as
    // recorded. Dropping it makes a sample sound bigger and heavier (and, since pitch
    // drives playback speed here, longer), which is how BLAST and FLAME share one
    // explosion recording and still read as two different schools.
    //------------------------------------------------------------------------------
    struct Def
    {
        const char *file;
        float volume;
        float pitchCenter;
        float pitchSpread;
    };

    const Def Defs[] =
    {
        // --- Interface -----------------------------------------------------------
        /* UiConfirm  */ { "10_UI_Menu_SFX/013_Confirm_03.wav",            0.85f, 1.00f, 0.15f },
        /* UiDenied   */ { "10_UI_Menu_SFX/033_Denied_03.wav",             0.80f, 1.00f, 0.06f },
        /* UiPause    */ { "10_UI_Menu_SFX/092_Pause_04.wav",              0.75f, 1.00f, 0.04f },
        /* UiUnpause  */ { "10_UI_Menu_SFX/098_Unpause_04.wav",            0.75f, 1.00f, 0.04f },
        /* UiBuy      */ { "10_UI_Menu_SFX/079_Buy_sell_01.wav",           0.90f, 1.00f, 0.08f },

        // --- The player's body ---------------------------------------------------
        // Footsteps stay well under the one-off sounds: they fire twice a second
        // while running, and anything at the level of a sword swing becomes the
        // loudest thing in the game within one corridor.
        /* Step       */ { "12_Player_Movement_SFX/08_Step_rock_02.wav",   0.45f, 1.00f, 0.15f },
        /* Jump       */ { "12_Player_Movement_SFX/30_Jump_03.wav",        0.85f, 1.00f, 0.10f },
        /* Land       */ { "12_Player_Movement_SFX/45_Landing_01.wav",     0.80f, 1.00f, 0.10f },

        // --- Melee ---------------------------------------------------------------
        // Swing is a slash through air and Hit is the impact. Two samples rather than
        // one played at two levels, because a miss and a connection are the two
        // things a fight here is actually read off.
        /* Swing      */ { "12_Player_Movement_SFX/56_Attack_03.wav",      0.70f, 1.05f, 0.15f },
        /* Hit        */ { "12_Player_Movement_SFX/61_Hit_03.wav",         0.95f, 1.00f, 0.20f },

        // A crit is layered OVER Hit rather than replacing it - see the call site -
        // so it is pitched up and kept quiet enough not to double the impact into
        // something twice as loud as an ordinary blow.
        /* Crit       */ { "10_Battle_SFX/22_Slash_04.wav",                0.75f, 1.25f, 0.10f },

        /* Block      */ { "10_Battle_SFX/39_Block_03.wav",                0.85f, 1.00f, 0.12f },

        // The parry is the same block sample pitched UP and played louder: it is the
        // best thing that can happen in an exchange and has to sound like it, while
        // still obviously being the same event as a block.
        /* Parry      */ { "10_Battle_SFX/39_Block_03.wav",                1.00f, 1.35f, 0.08f },

        /* Shoot      */ { "8_Atk_Magic_SFX/25_Wind_01.wav",               0.60f, 1.40f, 0.12f },

        // --- What the enemies do -------------------------------------------------
        // A claw rather than the player's own blade sample: the thing swinging at
        // you is a skeleton, and the one sound the player most needs to place in a
        // room is the one they cannot see coming. Loud for its distance, and given
        // the widest spread of the three - in a pack this fires constantly.
        /* EnemySwing */ { "10_Battle_SFX/03_Claw_03.wav",                 0.85f, 1.00f, 0.20f },

        // The player's own Shoot at a lower pitch - see the note in Sfx.h. Same
        // sample deliberately: a thing loosed across a room is a thing loosed
        // across a room, and the pitch is what says whose it was.
        /* EnemyShoot */ { "8_Atk_Magic_SFX/25_Wind_01.wav",               0.75f, 1.00f, 0.12f },

        // Likewise the charge, pitched well down. A support mage winding up is
        // something the player is meant to hear and go and interrupt, so it is the
        // loudest of the three and the narrowest - it has to be recognisable every
        // single time rather than varied.
        /* EnemyCast  */ { "8_Atk_Magic_SFX/45_Charge_05.wav",             0.55f, 0.70f, 0.06f },

        // --- Taking and dealing death --------------------------------------------
        /* PlayerHurt */ { "10_Battle_SFX/15_Impact_flesh_02.wav",         0.85f, 1.00f, 0.20f },
        /* PlayerDeath*/ { "10_Battle_SFX/55_Encounter_02.wav",            1.00f, 1.00f, 0.04f },

        // The melee Hit sample again, pitched down and played by distance. The same
        // event - something connecting with a body - so the same voice; what makes
        // an arrow landing over there different from a sword landing here is how
        // far away it is, and PlayAt already says that.
        /* EnemyHurt  */ { "12_Player_Movement_SFX/61_Hit_03.wav",         0.80f, 0.88f, 0.18f },

        /* EnemyDeath */ { "10_Battle_SFX/69_Enemy_death_01.wav",          0.90f, 1.00f, 0.15f },

        // --- Magic ---------------------------------------------------------------
        // The charge is heard on every single cast, so its pitch spread is the widest
        // here: an identical wind-up every few seconds is what makes a sound grate.
        /* Cast       */ { "8_Atk_Magic_SFX/45_Charge_05.wav",             0.25f, 1.00f, 0.22f },

        // One per school, in Magic order. Each was matched to what the school DOES in
        // this game rather than to what the mobile game's skill of the same name did,
        // and two of those diverged: NOVA is the knockback here, so it takes the
        // earth impact, and BLAST is the heavy one, so it takes the explosion pitched
        // furthest down.
        /* MagicFlame */ { "8_Atk_Magic_SFX/04_Fire_explosion_04_medium.wav", 0.90f, 1.10f, 0.12f },
        /* MagicSpark */ { "8_Atk_Magic_SFX/18_Thunder_02.wav",            1.00f, 1.00f, 0.10f },
        /* MagicToxin */ { "8_Atk_Magic_SFX/46_Poison_01.wav",             0.90f, 1.00f, 0.12f },
        /* MagicBlast */ { "8_Atk_Magic_SFX/04_Fire_explosion_04_medium.wav", 1.00f, 0.72f, 0.08f },
        /* MagicSplash*/ { "8_Atk_Magic_SFX/22_Water_02.wav",              0.90f, 1.00f, 0.15f },
        /* MagicFlash */ { "8_Atk_Magic_SFX/25_Wind_01.wav",               0.90f, 1.15f, 0.12f },
        /* MagicNova  */ { "8_Atk_Magic_SFX/30_Earth_02.wav",              1.00f, 0.95f, 0.10f },

        // REND is the only school that is not magic to look at - it is drawn as
        // matter, so it takes a flesh sound from the battle pack. A different one
        // from PlayerHurt's, so a school landing never sounds like taking a hit.
        /* MagicRend  */ { "10_Battle_SFX/77_flesh_02.wav",                0.95f, 0.90f, 0.15f },

        // A spell arriving - one per cast, wherever it stopped. See the note in
        // Sfx.h for why this is not per body. Ice rather than any of the eight
        // school samples: those are what CASTING sounds like and are already
        // playing, so the arrival has to be its own short crack or the two blur
        // into one long noise.
        /* SpellImpact*/ { "8_Atk_Magic_SFX/13_Ice_explosion_01.wav",      0.80f, 1.05f, 0.14f },

        // --- The world -----------------------------------------------------------
        // Interact is the block sample pitched down: a wooden, hollow knock, which is
        // what a chest lid wants. Narrow spread - it is a deliberate action the player
        // took, and those should sound the same every time.
        /* Interact   */ { "10_Battle_SFX/39_Block_03.wav",                0.85f, 0.82f, 0.08f },
        /* Pickup     */ { "10_UI_Menu_SFX/051_use_item_01.wav",           0.70f, 1.10f, 0.12f },

        // Bright, and the narrowest spread of the world sounds: this fires a handful
        // of times a floor at most, so it never gets a chance to grate, and it is
        // supposed to be the same recognisable chime every time it does.
        /* LootRare   */ { "8_Buffs_Heals_SFX/39_Absorb_04.wav",           0.95f, 1.25f, 0.05f },
        /* Heal       */ { "8_Buffs_Heals_SFX/02_Heal_02.wav",             0.95f, 1.00f, 0.06f },
        /* Buff       */ { "8_Buffs_Heals_SFX/16_Atk_buff_04.wav",         0.90f, 1.00f, 0.06f },
        /* LevelUp    */ { "8_Buffs_Heals_SFX/30_Revive_03.wav",           1.00f, 1.00f, 0.04f },
        /* Descend    */ { "12_Player_Movement_SFX/88_Teleport_02.wav",    0.90f, 0.90f, 0.06f },
    };

    constexpr int Count = (int)(sizeof(Defs)/sizeof(Defs[0]));

    static_assert(Count == (int)Sfx::Count, "Sfx.cpp Defs[] and the Sfx enum are out of sync");

    //------------------------------------------------------------------------------
    // The playable copies of one clip. voices[0] owns the samples; the rest are
    // aliases of it - their own playback state, shared data. Static storage, so an
    // unloaded slot is already zeroed and `loaded` is already false.
    //------------------------------------------------------------------------------
    struct Clip
    {
        Sound voices[Voices];
        int next = 0;       // Round-robin cursor into voices
        bool loaded = false;
    };

    Clip Clips[Count];

    float setting = 1.0f;   // The options slider, 0..1

    //------------------------------------------------------------------------------
    // One play, at a level already scaled by whatever the caller wants.
    //
    // `attenuation` is 1.0 for anything happening to the player and less for anything
    // happening at a distance - see GameSfx::PlayAt.
    //------------------------------------------------------------------------------
    void Sound1(Sfx id, float attenuation)
    {
        const int i = (int)id;

        if ((i < 0) || (i >= Count) || !Clips[i].loaded) return;   // Covers Sfx::None
        if (attenuation <= 0.0f) return;

        // Round-robin rather than "first idle voice": it costs no search and spreads
        // repeats across all the voices, so the one being cut off is always the oldest
        Sound &voice = Clips[i].voices[Clips[i].next];

        Clips[i].next = (Clips[i].next + 1)%Voices;

        const float spread = Defs[i].pitchSpread;

        float pitch = Defs[i].pitchCenter + GetRandomValue(-1000, 1000)/1000.0f*spread;

        // A spread wider than the centre would stop playback outright
        if (pitch < 0.05f) pitch = 0.05f;

        SetSoundPitch(voice, pitch);
        SetSoundVolume(voice, Defs[i].volume*setting*attenuation);
        PlaySound(voice);
    }
}

void GameSfx::Load()
{
    // Game::Init owns the device. If it could not open one, load nothing and stay
    // silent rather than filling memory with samples that can never be heard.
    if (!IsAudioDeviceReady())
    {
        TraceLog(LOG_WARNING, "SFX: no audio device, sound effects disabled");
        return;
    }

    int loaded = 0;

    for (int i = 0; i < Count; i++)
    {
        const std::string path = AssetManager::Resolve(Dir + Defs[i].file);

        Sound base = LoadSound(path.c_str());

        if (!IsSoundValid(base))
        {
            TraceLog(LOG_WARNING, "SFX: could not load %s", Defs[i].file);
            continue;
        }

        Clips[i].voices[0] = base;

        for (int v = 1; v < Voices; v++) Clips[i].voices[v] = LoadSoundAlias(base);

        Clips[i].next = 0;
        Clips[i].loaded = true;

        loaded++;
    }

    TraceLog(LOG_INFO, "SFX: %i of %i clips loaded, %i voices each", loaded, Count, Voices);
}

void GameSfx::Unload()
{
    for (int i = 0; i < Count; i++)
    {
        if (!Clips[i].loaded) continue;

        // Aliases first: they borrow voices[0]'s sample data, so it has to outlive them
        for (int v = 1; v < Voices; v++) UnloadSoundAlias(Clips[i].voices[v]);

        UnloadSound(Clips[i].voices[0]);

        Clips[i].loaded = false;
    }
}

void GameSfx::Play(Sfx id)
{
    Sound1(id, 1.0f);
}

//----------------------------------------------------------------------------------
// The same sound, quieter for being further away - see the note in Sfx.h.
//
// Falls off with the SQUARE of the remaining fraction rather than linearly. A linear
// ramp keeps a sound most of the way up for most of its range, which in a dungeon
// where two rooms is already far means everything sounds close; squaring it drops
// away fast and leaves the last stretch as the faint edge of hearing it should be.
//----------------------------------------------------------------------------------
void GameSfx::PlayAt(Sfx id, float distance)
{
    if (distance >= Config::SfxHearingRange) return;

    const float near = Config::SfxFullVolumeRange;

    if (distance <= near)
    {
        Sound1(id, 1.0f);
        return;
    }

    const float left = 1.0f - (distance - near)/(Config::SfxHearingRange - near);

    Sound1(id, left*left);
}

float GameSfx::Volume()
{
    return setting;
}

void GameSfx::SetVolume(float volume)
{
    setting = (volume < 0.0f) ? 0.0f : ((volume > 1.0f) ? 1.0f : volume);
}
