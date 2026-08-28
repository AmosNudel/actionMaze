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
    float volume = 1.0f;

    // Where BACK goes, in the caller's own words. The page is opened from the main
    // menu and from a paused run, and a button that said "BACK TO MENU" over a
    // dungeon would be promising to end the run.
    const char *backLabel = "BACK";
};

class OptionsScreen
{
public:
    enum class Choice { None, Back, ToggleFullscreen, ToggleMute, VolumeDown, VolumeUp };

    // See MainMenu::Show - the click that opened this page is not a click in it.
    void Show();

    Choice Update();
    void Draw(const OptionsView &view) const;

private:
    struct Layout
    {
        float ls = 1.0f;

        Rectangle page{};
        Rectangle fullscreenRow{};
        Rectangle muteRow{};
        Rectangle volumeRow{};
        Rectangle volumeMinus{};
        Rectangle volumePlus{};
        Rectangle back{};

        float titleY = 0.0f;
    };

    static Layout Measure();

    bool justShown = false;
};
