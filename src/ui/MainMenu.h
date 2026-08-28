#pragma once

#include "raylib.h"

//----------------------------------------------------------------------------------
// The front door: what the game shows before there is a run to be in.
//
// The same shape as PauseMenu - a fullscreen page, UiTheme's palette, an entry
// list driven by the keyboard or the mouse - because this and the pause page are
// both "the game is stopped, pick what happens next", just at opposite ends of a
// run. Unlike PauseMenu nothing here is ever greyed out: every entry is always
// available, there being no floor state yet for one of them to depend on.
//
// Owns no state beyond the cursor. What each entry DOES is Game's job to read
// back and act on - see the note on PauseMenu::Choice for why.
//----------------------------------------------------------------------------------
class MainMenu
{
public:
    enum class Choice { None, Start, Options, Credits, Exit };

    // Called the frame the main menu becomes the thing on screen, so the click
    // that ended the previous screen cannot also be read as a click in this one -
    // see the note on UiInput::justOpened.
    void Show();

    Choice Update();
    void Draw() const;

private:
    struct Layout
    {
        float ls = 1.0f;

        Rectangle page{};
        Rectangle entries[4]{};

        float titleY = 0.0f;
    };

    static Layout Measure();

    int cursor = 0;
    bool justShown = false;
};
