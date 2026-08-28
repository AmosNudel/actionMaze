#pragma once

#include "raylib.h"

//----------------------------------------------------------------------------------
// Settings: the window, the sound, and the master volume.
//
// The same shape as PauseMenu and CharacterSheet - fullscreen page, UiTheme's
// palette - but the rows here are not a list to arrow through, they are controls to
// click, the same way a stat row's [+] on the character sheet is. So there is no
// keyboard cursor: Escape maps straight to Back, and everything else is the mouse.
//
// --- Reached from two places ---------------------------------------------------
// The main menu, and the pause menu mid-run. One page for both, because they are
// the same settings and a second copy would be a second place for them to drift.
// The only thing that differs is where BACK goes, and the page does not decide
// that - see OptionsView::backLabel, which is a caller's word for its own way out.
//
// Owns no state of its own. `fullscreenOn`, `muted` and `volume` live on Game, for
// the same reason PauseMenu owns nothing but its own cursor: turning a click into a
// raylib call (ToggleBorderlessWindowed, SetMasterVolume) is the composition root's
// job. This page only reports which control was clicked.
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// What the page shows. A struct rather than four positional arguments because three
// of them are bools: Draw(true, false, true) is a call nobody can read, and the one
// bug this page can have is a mute toggle that draws the fullscreen state.
//----------------------------------------------------------------------------------
struct OptionsView
{
    bool fullscreenOn = false;
    bool muted = false;

    //------------------------------------------------------------------------------
    // Three levels, not one, because they are three different decisions.
    //
    // MASTER is how loud the game is against everything else on the machine. MUSIC
    // and EFFECTS are the mix INSIDE that - and the pair exists because the two are
    // balanced against each other rather than set independently (see the note on
    // MusicVolume in audio/Music.cpp), so a player who wants the score down without
    // losing the sound of a swing landing has to be able to say so.
    //
    // Mute is separate again, and above all three: it is "not right now", and it
    // spends none of them.
    //------------------------------------------------------------------------------
    float volume = 1.0f;
    float music = 1.0f;
    float effects = 1.0f;

    // Where BACK goes, in the caller's own words. The page is opened from the main
    // menu and from a paused run, and a button that said "BACK TO MENU" over a
    // dungeon would be promising to end the run.
    const char *backLabel = "BACK";
};

class OptionsScreen
{
public:
    //------------------------------------------------------------------------------
    // What the page reports. The three level rows each report their own pair rather
    // than one "a slider moved" plus an index, so the caller reads which control was
    // pressed straight off the enum and cannot mis-order the rows against it.
    //------------------------------------------------------------------------------
    enum class Choice
    {
        None, Back, ToggleFullscreen, ToggleMute,
        VolumeDown, VolumeUp,
        MusicDown, MusicUp,
        EffectsDown, EffectsUp
    };

    // See MainMenu::Show - the click that opened this page is not a click in it.
    void Show();

    Choice Update();
    void Draw(const OptionsView &view) const;

    //------------------------------------------------------------------------------
    // The page, in screen pixels. One struct, built once, read by both the click test
    // and the draw - because a control drawn in one place and clicked in another is
    // the one bug a settings page can have.
    //
    // Public only for `Levels`, which the row-name table in the .cpp is sized by.
    //------------------------------------------------------------------------------
    struct Layout
    {
        float ls = 1.0f;

        Rectangle page{};
        Rectangle fullscreenRow{};
        Rectangle muteRow{};
        // The three level rows and their steppers, in the order they are drawn.
        // Walked as an array rather than named one by one - see Measure - so a
        // fourth level is one more row and not four more members.
        static constexpr int Levels = 3;

        Rectangle levelRow[Levels]{};
        Rectangle levelMinus[Levels]{};
        Rectangle levelPlus[Levels]{};

        Rectangle back{};

        float titleY = 0.0f;
    };

private:
    static Layout Measure();

    bool justShown = false;
};
