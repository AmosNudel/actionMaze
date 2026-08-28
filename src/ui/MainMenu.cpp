#include "audio/Sfx.h"
#include "ui/MainMenu.h"

#include "core/Config.h"
#include "ui/UiText.h"
#include "ui/UiTheme.h"

namespace
{
    struct Entry
    {
        const char *label;
        const char *note;
        Color accent;
    };

    constexpr Color QuitRed = { 235, 110, 90, 255 };

    constexpr Entry Entries[] =
    {
        { "START",   "into the dungeon",  UiReady },
        { "OPTIONS", "fullscreen, sound", UiAccent },
        { "CREDITS", "who made this",     UiAccent },
        { "EXIT",    "to desktop",        QuitRed },
    };

    constexpr int Count = (int)(sizeof(Entries)/sizeof(Entries[0]));

    // Design pixels, fitted to the window - see the note in CharacterSheet.cpp
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

void MainMenu::Show()
{
    cursor = 0;
    justShown = true;
}

MainMenu::Layout MainMenu::Measure()
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

MainMenu::Choice MainMenu::Update()
{
    const UiInput in = UiInput::Read(justShown);
    const Layout page = Measure();

    justShown = false;

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) cursor++;
    if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W)) cursor--;

    if (cursor < 0) cursor = Count - 1;
    if (cursor >= Count) cursor = 0;

    for (int i = 0; i < Count; ++i)
    {
        if (in.Over(page.entries[i])) cursor = i;
    }

    bool chosen = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);

    if (in.clicked)
    {
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

    GameSfx::Play(Sfx::UiConfirm);

    // The enum runs None, Start, Options, Credits, Exit - entry i is i + 1, the
    // same trick PauseMenu's table uses for the same reason: the two have to be
    // edited together, and sitting next to each other in this file is what makes
    // that obvious.
    return (Choice)(cursor + 1);
}

void MainMenu::Draw() const
{
    const Layout page = Measure();
    const float ls = page.ls;

    UiPageBackdrop(UiFrontBackdrop, UiFrontBg);

    UiLabel(Config::WindowTitle, page.page.x, page.titleY, TitleSize*ls, UiAccent);

    const float ruleY = page.titleY + (TitleSize + 14.0f)*ls;

    DrawRectangleRec({ page.page.x, ruleY, page.page.width, 1.0f*ls },
                     Fade(UiDim, 0.45f));

    for (int i = 0; i < Count; ++i)
    {
        const Rectangle box = page.entries[i];
        const bool selected = (i == cursor);

        UiRow(box, ls, selected, Entries[i].accent);

        DrawRectangleRec({ box.x, box.y, 4.0f*ls, box.height }, Entries[i].accent);

        const Color label = selected ? Entries[i].accent : UiInk;

        UiLabel(Entries[i].label, box.x + EntryPad*ls, box.y + 10.0f*ls,
                LabelSize*ls, label);

        UiLabel(Entries[i].note, box.x + EntryPad*ls, box.y + (10.0f + LabelSize + 4.0f)*ls,
                NoteSize*ls, UiDim);
    }

    UiLabelCentered("arrows or the mouse   enter to choose",
                    page.page.x + page.page.width*0.5f,
                    page.page.y + page.page.height - NoteSize*ls, NoteSize*ls, UiDim);
}
