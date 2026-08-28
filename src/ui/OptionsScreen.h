#pragma once

#include "raylib.h"

//----------------------------------------------------------------------------------
// Settings, reachable from the main menu: fullscreen and the master volume.
//
// The same shape as PauseMenu and CharacterSheet - fullscreen page, UiTheme's
// palette - but the two rows here are not a list to arrow through, they are
// controls to click, the same way a stat row's [+] on the character sheet is. So
// there is no keyboard cursor: Escape maps straight to Back, and everything else
// is the mouse.
//
// Owns no state of its own. `fullscreenOn` and `volume` live on Game - the same
// reason PauseMenu does not own `canDescend` - because turning them into a raylib
// call (ToggleBorderlessWindowed, SetMasterVolume) is the composition root's job,
// this page only reports which control was clicked.
//----------------------------------------------------------------------------------
class OptionsScreen
{
public:
    enum class Choice { None, Back, ToggleFullscreen, VolumeDown, VolumeUp };

    // See MainMenu::Show - the click that opened this page is not a click in it.
    void Show();

    Choice Update();
    void Draw(bool fullscreenOn, float volume) const;

private:
    struct Layout
    {
        float ls = 1.0f;

        Rectangle page{};
        Rectangle fullscreenRow{};
        Rectangle volumeRow{};
        Rectangle volumeMinus{};
        Rectangle volumePlus{};
        Rectangle back{};

        float titleY = 0.0f;
    };

    static Layout Measure();

    bool justShown = false;
};
