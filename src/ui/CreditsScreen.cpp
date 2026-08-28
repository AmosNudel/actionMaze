#include "ui/CreditsScreen.h"

#include "ui/UiText.h"
#include "ui/UiTheme.h"

namespace
{
    //------------------------------------------------------------------------------
    // The table. One row per PACK - see the note in CreditsScreen.h.
    //
    // Sources are written as pages a person could type rather than as full URLs with
    // a scheme on the front: this is read off a screen, not clicked.
    //
    // The licence column is the short form of what CREDITS.md records in full. Two
    // values in it are not courtesies: the menu sky is CC BY 4.0, where credit is a
    // condition, and anything reading "free - see pack" is a licence somebody has to
    // actually read before this ships commercially.
    //------------------------------------------------------------------------------
    const CreditEntry Table[] =
    {
        // --- The game ------------------------------------------------------------
        // Authorship of the work as a whole. Copyright in the code and the design
        // arises when the work is made; this is the line that says whose it is.
        { CreditSection::Game, "ActionMaze", "Amos Nudel", "design, code and game", "" },

        // --- Art -----------------------------------------------------------------
        { CreditSection::Art, "Dungeon art, props and decoration", "Kay Lousberg",
          "kaylousberg.itch.io/kaykit-dungeon", "CC0" },

        { CreditSection::Art, "Skeletons and character animation", "Kay Lousberg",
          "kaylousberg.itch.io", "CC0" },

        // The three adventurers standing in for the vendors - a different pack from
        // the dungeon set above, and an older one
        { CreditSection::Art, "Adventurers, for the vendors", "Kay Lousberg",
          "kaylousberg.itch.io/kaykit-dungeon", "CC0" },

        // The town outside the walls - see world/Skyline.h
        { CreditSection::Art, "Medieval buildings, for the skyline", "Kay Lousberg",
          "kaylousberg.itch.io/medieval-hexagon", "CC0" },

        { CreditSection::Art, "Skybox", "Screaming Brain Studios",
          "screamingbrainstudios.itch.io", "CC0" },

        //----------------------------------------------------------------------
        // The one row on this screen that is a CONDITION rather than a courtesy.
        // CC BY 4.0 requires attribution; leaving this out is a breach, not an
        // impoliteness. See CREDITS.md and ui/MenuBackdrop.h.
        //----------------------------------------------------------------------
        { CreditSection::Art, "Menu sky", "Unicorn Creates",
          "unicorncreates.itch.io/sky-backgrounds", "CC BY 4.0" },

        { CreditSection::Art, "HUD bar art", "brullov",
          "brullov.itch.io/2d-platformer-asset-pack-castle-of-despair",
          "free - see pack" },

        // Recorded honestly rather than quietly used - the pack these came from was
        // never established. See the note in CREDITS.md, which is the open item.
        { CreditSection::Art, "Impact effect sheets", "source not established",
          "brought over from the mobile game", "UNRESOLVED" },

        // --- Audio ---------------------------------------------------------------
        { CreditSection::Audio, "Background music", "ansimuz (Luis Zuno)",
          "ansimuz.itch.io/action-music-pack-1", "free - see pack" },

        { CreditSection::Audio, "Sound effects", "Leohpaz",
          "leohpaz.itch.io/rpg-essentials-sfx-free", "free - see pack" },

        // --- Font ----------------------------------------------------------------
        { CreditSection::Font, "Alagard", "Hewett Tsoi / Ramon Santamaria",
          "dafont.com/alagard.font", "freeware" },

        // --- Engine --------------------------------------------------------------
        { CreditSection::Engine, "raylib", "Ramon Santamaria (@raysan5)",
          "raylib.com", "zlib/libpng" },
    };

    constexpr int TableCount = (int)(sizeof(Table)/sizeof(Table[0]));

    const char *const SectionNames[(int)CreditSection::Count] =
    {
        "A GAME BY",
        "ART",
        "AUDIO",
        "FONT",
        "ENGINE",
    };

    // Design pixels, fitted to the window - see the note in CharacterSheet.cpp
    constexpr float DesignHeight = 460.0f;
    constexpr float DesignWidth  = 560.0f;
    constexpr float MaxScale     = 2.4f;

    constexpr float TitleSize   = 46.0f;
    constexpr float SectionSize = 17.0f;
    constexpr float WhatSize    = 18.0f;
    constexpr float SmallSize   = 13.0f;

    constexpr float TitleTop  = 10.0f;
    constexpr float ListTop   = 96.0f;
    constexpr float RowHeight = 40.0f;
    constexpr float RowGap    = 4.0f;

    constexpr float ButtonH   = 46.0f;
    constexpr float ButtonTop = 12.0f;
}

int CreditCount()
{
    return TableCount;
}

const CreditEntry &CreditAt(int index)
{
    if ((index < 0) || (index >= TableCount)) return Table[0];

    return Table[index];
}

const char *CreditSectionName(CreditSection section)
{
    const int i = (int)section;

    if ((i < 0) || (i >= (int)CreditSection::Count)) return "";

    return SectionNames[i];
}

void CreditsScreen::Show()
{
    justShown = true;

    // Back to the top every visit. A remembered scroll would reopen this page
    // halfway down a list nobody has read yet.
    scroll = 0;
}

//----------------------------------------------------------------------------------
// The table, with a heading inserted wherever the section changes.
//
// Walked in TABLE order rather than gathered per section, so the order rows appear in
// is the order they are written in - which means adding a row is putting it where it
// belongs in the table and nothing else.
//----------------------------------------------------------------------------------
int CreditsScreen::BuildLines(Line *out, int max)
{
    int count = 0;
    int section = -1;

    for (int i = 0; (i < TableCount) && (count < max); ++i)
    {
        if ((int)Table[i].section != section)
        {
            section = (int)Table[i].section;

            Line heading;

            heading.entry = -1;
            heading.section = Table[i].section;

            out[count++] = heading;

            if (count >= max) break;
        }

        Line row;

        row.entry = i;

        out[count++] = row;
    }

    return count;
}

CreditsScreen::Layout CreditsScreen::Measure()
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

    const float listTop = out.page.y + ListTop*out.ls;
    const float buttonY = out.page.y + out.page.height - ButtonH*out.ls;

    out.list = { out.page.x, listTop, out.page.width,
                 (buttonY - ButtonTop*out.ls) - listTop };

    out.rowHeight = RowHeight*out.ls;

    const float step = out.rowHeight + RowGap*out.ls;

    out.visible = (step > 0.0f) ? (int)(out.list.height/step) : 0;

    if (out.visible < 1) out.visible = 1;

    out.back = { out.page.x, buttonY, out.page.width, ButtonH*out.ls };

    return out;
}

Rectangle CreditsScreen::Layout::RowAt(int slot) const
{
    const float step = rowHeight + RowGap*ls;

    return { list.x, list.y + slot*step, list.width, rowHeight };
}

int CreditsScreen::ClampScroll(int lineCount, int visible) const
{
    const int most = lineCount - visible;

    if (most <= 0) return 0;
    if (scroll < 0) return 0;
    if (scroll > most) return most;

    return scroll;
}

CreditsScreen::Choice CreditsScreen::Update()
{
    const UiInput in = UiInput::Read(justShown);
    const Layout page = Measure();

    justShown = false;

    Line lines[TableCount + (int)CreditSection::Count];

    const int lineCount = BuildLines(lines, (int)(sizeof(lines)/sizeof(lines[0])));

    scroll = ClampScroll(lineCount, page.visible);

    // The wheel scrolls, the same gesture the shop's own list answers
    const float wheel = GetMouseWheelMove();

    if (wheel != 0.0f)
    {
        scroll -= (int)wheel;
        scroll = ClampScroll(lineCount, page.visible);
    }

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) scroll++;
    if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W)) scroll--;

    scroll = ClampScroll(lineCount, page.visible);

    if (IsKeyPressed(KEY_ESCAPE)) return Choice::Back;
    if (!in.clicked) return Choice::None;

    return in.Over(page.back) ? Choice::Back : Choice::None;
}

void CreditsScreen::Draw() const
{
    const Layout page = Measure();
    const float ls = page.ls;

    UiPageBackdrop(UiFrontBackdrop, UiFrontBg);

    UiLabel("CREDITS", page.page.x, page.titleY, TitleSize*ls, UiAccent);

    UiLabelRight("ESC goes back", page.page.x + page.page.width,
                 page.titleY + TitleSize*0.42f*ls, 14.0f*ls, UiDim);

    const float ruleY = page.titleY + (TitleSize + 14.0f)*ls;

    DrawRectangleRec({ page.page.x, ruleY, page.page.width, 1.0f*ls }, Fade(UiDim, 0.45f));

    Line lines[TableCount + (int)CreditSection::Count];

    const int lineCount = BuildLines(lines, (int)(sizeof(lines)/sizeof(lines[0])));

    const int top = ClampScroll(lineCount, page.visible);

    for (int slot = 0; slot < page.visible; ++slot)
    {
        const int index = top + slot;

        if (index >= lineCount) break;

        const Line &line = lines[index];
        const Rectangle box = page.RowAt(slot);

        //--------------------------------------------------------------------------
        // A section heading: the word alone over a rule, no panel behind it. It is
        // not a row of the table and must not look like one, or the list reads as
        // having an entry called "ART".
        //--------------------------------------------------------------------------
        if (line.entry < 0)
        {
            UiLabel(CreditSectionName(line.section), box.x,
                    box.y + box.height - (SectionSize + 6.0f)*ls, SectionSize*ls, UiAccent);

            DrawRectangleRec({ box.x, box.y + box.height - 3.0f*ls, box.width, 1.0f*ls },
                             Fade(UiAccent, 0.30f));
            continue;
        }

        const CreditEntry &entry = CreditAt(line.entry);

        UiRow(box, ls, false, UiAccent);

        // What it is, and who made it - the two halves of the row that matter, on
        // one line so the eye pairs them
        UiLabel(entry.what, box.x + 12.0f*ls, box.y + 5.0f*ls, WhatSize*ls, UiInk);

        UiLabelRight(entry.who, box.x + box.width - 12.0f*ls,
                     box.y + 6.0f*ls, SmallSize*ls, UiDim);

        // Where it came from, and the terms it is held under, underneath
        UiLabel(entry.where, box.x + 12.0f*ls,
                box.y + (5.0f + WhatSize + 2.0f)*ls, SmallSize*ls, UiOff);

        if (entry.licence[0] != '\0')
        {
            UiLabelRight(entry.licence, box.x + box.width - 12.0f*ls,
                         box.y + (5.0f + WhatSize + 2.0f)*ls, SmallSize*ls, UiDim);
        }
    }

    // How far down this is, only when there is somewhere else to be
    if (lineCount > page.visible)
    {
        UiLabel(TextFormat("%i - %i of %i    wheel scrolls", top + 1,
                           ((top + page.visible) < lineCount) ? (top + page.visible) : lineCount,
                           lineCount),
                page.page.x, page.back.y - (SmallSize + 4.0f)*ls, SmallSize*ls, UiDim);
    }

    const UiInput in = UiInput::Read(false);

    UiButton(page.back, true, ls, in, "BACK TO MENU", UiPanel, UiInk);
}
