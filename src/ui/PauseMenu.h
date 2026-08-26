#pragma once

#include "raylib.h"

//----------------------------------------------------------------------------------
// Escape stops the game instead of ending it.
//
// raylib closes the window on Escape by default, and for a demo that is right: there
// is nothing to lose. There is now - a character, a floor part-cleared, points
// unspent - and a key that throws all of it away without asking is not a key anyone
// should be able to hit by accident.
//
// So Game calls SetExitKey(KEY_NULL) at startup and Escape comes through as an
// ordinary input, which this answers. Quitting is still one keypress away; it is just
// a keypress that says what it does first.
//
// --- Why fullscreen ------------------------------------------------------------
// The same reason the character page is - see CharacterSheet.h. A menu floating in
// the middle of a dungeon that is visibly still there reads as something you can
// play through, and this is the one screen in the game whose entire job is to say
// that the game has stopped. It is also the page a player is on when they are
// deciding whether to keep playing, and a small box is a bad place to hold that
// thought.
//
// The menu owns no state beyond which entry is highlighted and whether it is up.
// What each entry DOES is the caller's - it reads the choice back and acts on it -
// because two of the four (restart, quit) are things only the composition root can
// do, and a menu that could do them would need to know about every system there is.
//----------------------------------------------------------------------------------
class PauseMenu
{
public:
    // What the player picked this frame, or None. Consumed by reading: Update
    // returns it once and the menu goes back to None on the next frame.
    enum class Choice { None, Resume, Character, Descend, Quit };

    void Toggle();
    bool IsOpen() const { return open; }
    void Close() { open = false; }

    // `canDescend` greys the Descend entry out when the floor is not cleared. Shown
    // rather than hidden, because an entry that appears and disappears is one the
    // player has to notice twice - once to learn it exists and again to work out
    // what makes it come back.
    Choice Update(bool canDescend);

    void Draw(bool canDescend) const;

private:
    //------------------------------------------------------------------------------
    // The page, in screen pixels. One struct, built once, read by both the click
    // test and the draw - because a button drawn in one place and clicked in another
    // is the one bug a menu can have.
    //------------------------------------------------------------------------------
    struct Layout
    {
        float ls = 1.0f;

        Rectangle page{};
        Rectangle entries[4]{};

        float titleY = 0.0f;
    };

    static Layout Measure();

    // Whether `index` can be chosen right now
    static bool EntryEnabled(int index, bool canDescend);

    bool open = false;

    // Which entry the keyboard is on. The mouse moves it too, so that arrowing to an
    // entry and then reaching for the mouse does not leave two different things
    // looking selected.
    int cursor = 0;

    // See CharacterSheet.h - the click that opened the page is not a click in it.
    bool justOpened = false;
};
