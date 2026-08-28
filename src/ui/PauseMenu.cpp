#include "ui/PauseMenu.h"

#include "ui/UiText.h"
#include "ui/UiTheme.h"

namespace
{
    //------------------------------------------------------------------------------
    // The entries, in order, and what each one is for.
    //
    // OPTIONS is where DESCEND used to be. The shortcut past the walk was a
    // shortcut past the one act the game is built around - the portal IS how you
    // go down, and a menu entry that did it instead meant the walk only ever
    // happened to players who had not found the menu. It went; the portal stayed.
    //
    // What belongs in its place is the thing a paused player most often actually
    // wants, which is not a game action at all: the window, the sound, the volume.
    // Those lived only on the front end before, which meant changing them meant
    // ending the run - the single worst place to put a mute button.
    //------------------------------------------------------------------------------
    struct Entry
    {
        const char *label;
        const char *note;       // Second line, smaller. Null for none.

        // The accent an entry is picked out in when the cursor is on it. The first
        // three are ordinary business and share the page's own gold; Quit is red,
        // because it is the one entry that cannot be taken back.
        Color accent;
    };

    constexpr Color QuitRed = { 235, 110, 90, 255 };

    constexpr Entry Entries[] =
    {
        { "RESUME",    "back to the floor",         UiAccent },
        { "CHARACTER", "spend points",              UiAccent },
        { "OPTIONS",   "window, sound and volume",  UiAccent },
        { "QUIT",      "to desktop",                QuitRed },
    };

    constexpr int Count = (int)(sizeof(Entries)/sizeof(Entries[0]));

    // See the note in CharacterSheet.cpp - design pixels, fitted to the window.
    constexpr float DesignHeight = 430.0f;
    constexpr float DesignWidth  = 480.0f;
    constexpr float MaxScale     = 2.4f;

    constexpr float TitleSize   = 46.0f;
    constexpr float LabelSize   = 24.0f;
    constexpr float NoteSize    = 14.0f;

    constexpr float EntryHeight = 62.0f;
    constexpr float EntryGap    = 10.0f;
    constexpr float EntryPad    = 20.0f;

    constexpr float TitleTop    = 10.0f;
    constexpr float EntriesTop  = 110.0f;
}

void PauseMenu::Toggle()
{
    open = !open;
    justOpened = open;

    // Back to the top every time it opens. A menu that remembered the cursor would
    // put QUIT under Enter for the player who last used it, which is the one place
    // a remembered cursor can cost something.
    if (open) cursor = 0;
}

PauseMenu::Layout PauseMenu::Measure()
{
    Layout out;

    out.ls = UiPageScale(DesignHeight, MaxScale);

    const float screenW = (float)GetScreenWidth();
    const float screenH = (float)GetScreenHeight();

    float width = DesignWidth*out.ls;
    const float widest = screenW*0.92f;

    if (width > widest) width = widest;

    const float height = DesignHeight*out.ls;

    out.page = { (screenW - width)*0.5f, (screenH - height)*0.5f, width, height };
    out.titleY = out.page.y + TitleTop*out.ls;

    for (int i = 0; i < Count; ++i)
    {
        out.entries[i] = { out.page.x,
                           out.page.y + (EntriesTop + i*(EntryHeight + EntryGap))*out.ls,
                           out.page.width, EntryHeight*out.ls };
    }

    return out;
}

PauseMenu::Choice PauseMenu::Update()
{
    if (!open) return Choice::None;

    const UiInput in = UiInput::Read(justOpened);
    const Layout page = Measure();

    justOpened = false;

    //------------------------------------------------------------------------------
    // Keyboard first, then the mouse, and the mouse MOVES the cursor rather than
    // having a highlight of its own. Two highlights on one menu is a menu the player
    // has to work out the rules of.
    //------------------------------------------------------------------------------
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) cursor++;
    if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W)) cursor--;

    // Wrapped rather than clamped: four entries is short enough that running off the
    // bottom to reach the top is quicker than turning round
    if (cursor < 0) cursor = Count - 1;
    if (cursor >= Count) cursor = 0;

    for (int i = 0; i < Count; ++i)
    {
        if (in.Over(page.entries[i])) cursor = i;
    }

    bool chosen = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);

    if (in.clicked)
    {
        // Only on an entry. A click on the page's own background must not fire
        // whatever the keyboard happened to be sitting on.
        chosen = false;

        for (int i = 0; i < Count; ++i)
        {
            if (!in.Over(page.entries[i])) continue;

            cursor = i;
            chosen = true;
            break;
        }
    }

    if (!chosen) return Choice::None;

    // The enum runs None, Resume, Character, Options, Quit - so entry i is i + 1.
    // Tied to the table's order on purpose: adding an entry means adding both, and
    // the two sitting next to each other in this file is what makes that obvious.
    return (Choice)(cursor + 1);
}

void PauseMenu::Draw() const
{
    if (!open) return;

    const Layout page = Measure();
    const float ls = page.ls;

    UiPageBackdrop();

    UiLabel("PAUSED", page.page.x, page.titleY, TitleSize*ls, UiAccent);

    UiLabelRight("ESC resumes", page.page.x + page.page.width,
                 page.titleY + TitleSize*0.42f*ls, NoteSize*ls, UiDim);

    const float ruleY = page.titleY + (TitleSize + 14.0f)*ls;

    DrawRectangleRec({ page.page.x, ruleY, page.page.width, 1.0f*ls },
                     Fade(UiDim, 0.45f));

    for (int i = 0; i < Count; ++i)
    {
        const Rectangle box = page.entries[i];
        const bool selected = (i == cursor);

        UiRow(box, ls, selected, Entries[i].accent);

        // The accent as a tab down the left edge
        DrawRectangleRec({ box.x, box.y, 4.0f*ls, box.height }, Entries[i].accent);

        UiLabel(Entries[i].label, box.x + EntryPad*ls, box.y + 10.0f*ls,
                LabelSize*ls, selected ? Entries[i].accent : UiInk);

        UiLabel(Entries[i].note, box.x + EntryPad*ls, box.y + (10.0f + LabelSize + 4.0f)*ls,
                NoteSize*ls, UiDim);
    }

    UiLabelCentered("arrows or the mouse   enter to choose",
                    page.page.x + page.page.width*0.5f,
                    page.page.y + page.page.height - NoteSize*ls, NoteSize*ls, UiDim);
}
