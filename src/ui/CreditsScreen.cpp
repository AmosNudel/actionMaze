#include "ui/CreditsScreen.h"

#include "ui/UiText.h"
#include "ui/UiTheme.h"

namespace
{
    struct Line
    {
        const char *who;
        const char *what;
    };

    // The short reading of CREDITS.md - see that file at the repo root for the
    // full licence terms each of these is actually held under. Kept as two lines
    // per name rather than the full detail: this page is a thank-you, not the
    // legal record.
    constexpr Line Lines[] =
    {
        { "Kay Lousberg",              "dungeon art, enemies and animation" },
        { "Screaming Brain Studios",   "the skybox" },
        { "brullov",                   "HUD bar art" },
        { "Hewett Tsoi",               "Alagard, the display font" },
        { "Ramon Santamaria and contributors", "raylib" },
    };

    constexpr int Count = (int)(sizeof(Lines)/sizeof(Lines[0]));

    // Design pixels, fitted to the window - see the note in CharacterSheet.cpp
    constexpr float DesignHeight = 420.0f;
    constexpr float DesignWidth  = 520.0f;
    constexpr float MaxScale     = 2.4f;

    constexpr float TitleSize = 46.0f;
    constexpr float WhoSize   = 20.0f;
    constexpr float WhatSize  = 14.0f;

    constexpr float TitleTop  = 10.0f;
    constexpr float ListTop   = 110.0f;
    constexpr float LineGap   = 46.0f;

    constexpr float ButtonH   = 50.0f;
}

void CreditsScreen::Show()
{
    justShown = true;
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
    out.listTop = out.page.y + ListTop*out.ls;

    const float buttonY = out.page.y + out.page.height - ButtonH*out.ls;

    out.back = { out.page.x, buttonY, out.page.width, ButtonH*out.ls };

    return out;
}

CreditsScreen::Choice CreditsScreen::Update()
{
    const UiInput in = UiInput::Read(justShown);
    const Layout page = Measure();

    justShown = false;

    if (IsKeyPressed(KEY_ESCAPE)) return Choice::Back;
    if (!in.clicked) return Choice::None;

    return in.Over(page.back) ? Choice::Back : Choice::None;
}

void CreditsScreen::Draw() const
{
    const Layout page = Measure();
    const float ls = page.ls;

    UiPageBackdrop();

    UiLabel("CREDITS", page.page.x, page.titleY, TitleSize*ls, UiAccent);

    UiLabelRight("ESC goes back", page.page.x + page.page.width,
                 page.titleY + TitleSize*0.42f*ls, 14.0f*ls, UiDim);

    const float ruleY = page.titleY + (TitleSize + 14.0f)*ls;

    DrawRectangleRec({ page.page.x, ruleY, page.page.width, 1.0f*ls }, Fade(UiDim, 0.45f));

    for (int i = 0; i < Count; ++i)
    {
        const float y = page.listTop + i*LineGap*ls;

        UiLabel(Lines[i].who, page.page.x, y, WhoSize*ls, UiInk);
        UiLabel(Lines[i].what, page.page.x, y + (WhoSize + 4.0f)*ls, WhatSize*ls, UiDim);
    }

    const UiInput in = UiInput::Read(false);

    UiButton(page.back, true, ls, in, "BACK TO MENU", UiPanel, UiInk);
}
