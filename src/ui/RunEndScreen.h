#pragma once

#include "raylib.h"

//----------------------------------------------------------------------------------
// The one screen a run ends on, win or lose.
//
// Defeat and victory are the same kind of moment - the run is over, here is how far
// it got, and here is the only thing left to decide - so this is one page with a
// mode rather than two near-identical ones. It is the same shape as PauseMenu and
// CharacterSheet: fullscreen, UiTheme's palette, driven by the mouse, and it takes
// the cursor back the same way they do.
//
// Unlike them it OWNS the moment it is drawn: nothing else in the game is happening
// underneath it. There is no floor left to accidentally play through, so unlike the
// pause page this one does not need to say "the game has stopped" - it says why it
// stopped instead.
//----------------------------------------------------------------------------------
class RunEndScreen
{
public:
    enum class Choice { None, Restart, Quit };

    // Captures the run's summary at the moment it ends - depth reached and the
    // character's level, the two numbers that actually say how far this run got.
    void Open(bool victorious, int depthReached, int characterLevel);
    void Close() { open = false; }
    bool IsOpen() const { return open; }

    // Reads the mouse and returns what was chosen this frame, or None. Only call
    // while open.
    Choice Update();

    // Screen space, after EndMode3D
    void Draw() const;

private:
    struct Layout
    {
        float ls = 1.0f;

        Rectangle page{};
        Rectangle restart{};
        Rectangle quit{};

        float titleY = 0.0f;
        float summaryY = 0.0f;
    };

    static Layout Measure();

    bool open = false;
    bool victorious = false;
    int depthReached = 1;
    int characterLevel = 1;

    // See CharacterSheet.h - the click that opened the page is not a click in it.
    bool justOpened = false;
};
