#include "ui/OptionsScreen.h"

#include "ui/UiText.h"
#include "ui/UiTheme.h"

namespace
{
    // Design pixels, fitted to the window - see the note in CharacterSheet.cpp
    constexpr float DesignHeight = 500.0f;
    constexpr float DesignWidth  = 480.0f;
    constexpr float MaxScale     = 2.4f;

    constexpr float TitleSize = 46.0f;
    constexpr float LabelSize = 22.0f;
    constexpr float ValueSize = 20.0f;

    constexpr float TitleTop  = 10.0f;
    constexpr float RowsTop   = 100.0f;
    constexpr float RowHeight = 54.0f;
    constexpr float RowGap    = 10.0f;
    constexpr float RowPad    = 20.0f;

    constexpr float GlyphSize = 34.0f;
    constexpr float GlyphGap  = 9.0f;

    constexpr float ButtonTop = 22.0f;   // Below the last row
    constexpr float ButtonH   = 50.0f;

    // The three level rows, in the order they are drawn. Named here so the layout,
    // the click test and the paint all walk one list - the single bug this page can
    // have is a row drawn in one place and clicked in another.
    constexpr const char *LevelName[OptionsScreen::Layout::Levels] =
    {
        "MASTER", "MUSIC", "EFFECTS"
    };
}

void OptionsScreen::Show()
{
    justShown = true;
}

OptionsScreen::Layout OptionsScreen::Measure()
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

    // Every row sits on one pitch, walked rather than written out - so another
    // setting is one more entry and not a block of edited constants
    const float pitch = (RowHeight + RowGap)*out.ls;
    const float rowsY = out.page.y + RowsTop*out.ls;

    out.fullscreenRow = { out.page.x, rowsY,         out.page.width, RowHeight*out.ls };
    out.muteRow       = { out.page.x, rowsY + pitch, out.page.width, RowHeight*out.ls };

    for (int i = 0; i < Layout::Levels; ++i)
    {
        const float y = rowsY + pitch*(float)(i + 2);

        out.levelRow[i] = { out.page.x, y, out.page.width, RowHeight*out.ls };

        const float glyphY = y + (RowHeight*out.ls - GlyphSize*out.ls)*0.5f;

        out.levelPlus[i] = { out.page.x + out.page.width - RowPad*out.ls - GlyphSize*out.ls,
                             glyphY, GlyphSize*out.ls, GlyphSize*out.ls };

        out.levelMinus[i] = { out.levelPlus[i].x - GlyphGap*out.ls - GlyphSize*out.ls,
                              glyphY, GlyphSize*out.ls, GlyphSize*out.ls };
    }

    const Rectangle last = out.levelRow[Layout::Levels - 1];

    out.back = { out.page.x, last.y + last.height + ButtonTop*out.ls,
                 out.page.width, ButtonH*out.ls };

    return out;
}

OptionsScreen::Choice OptionsScreen::Update()
{
    const UiInput in = UiInput::Read(justShown);
    const Layout page = Measure();

    justShown = false;

    if (IsKeyPressed(KEY_ESCAPE)) return Choice::Back;

    if (!in.clicked) return Choice::None;

    if (in.Over(page.fullscreenRow)) return Choice::ToggleFullscreen;
    if (in.Over(page.muteRow))       return Choice::ToggleMute;

    //------------------------------------------------------------------------------
    // The steppers, walked in the same order the enum lists them - row i's pair is
    // VolumeDown + 2i and one past it. Tied to the enum's own order on purpose, the
    // same way PauseMenu ties its entry table to Choice, so adding a level means
    // adding one name to each list and nothing else.
    //------------------------------------------------------------------------------
    for (int i = 0; i < Layout::Levels; ++i)
    {
        if (in.Over(page.levelMinus[i])) return (Choice)((int)Choice::VolumeDown + i*2);
        if (in.Over(page.levelPlus[i]))  return (Choice)((int)Choice::VolumeUp + i*2);

        // After the two glyphs, so a click that landed on one is a step and not a row
        // press. A level row is not a toggle - it is a label with two buttons on it -
        // and this test only exists to swallow the misses.
        if (in.Over(page.levelRow[i])) return Choice::None;
    }

    if (in.Over(page.back)) return Choice::Back;

    return Choice::None;
}

void OptionsScreen::Draw(const OptionsView &view) const
{
    const Layout page = Measure();
    const float ls = page.ls;

    UiPageBackdrop(UiFrontBackdrop, UiFrontBg);

    UiLabel("OPTIONS", page.page.x, page.titleY, TitleSize*ls, UiAccent);

    UiLabelRight("ESC goes back", page.page.x + page.page.width,
                 page.titleY + TitleSize*0.42f*ls, 14.0f*ls, UiDim);

    const float ruleY = page.titleY + (TitleSize + 14.0f)*ls;

    DrawRectangleRec({ page.page.x, ruleY, page.page.width, 1.0f*ls }, Fade(UiDim, 0.45f));

    const UiInput in = UiInput::Read(false);

    //------------------------------------------------------------------------------
    // The window. The whole row is the control, the same way a PauseMenu entry is.
    //
    // One row rather than two, and the OFF state spells out what it actually is -
    // WINDOWED, not "OFF". A player looking for a windowed mode is looking for the
    // WORD, and "FULLSCREEN: OFF" makes them work out that those are the same
    // setting before they can believe they have found it.
    //------------------------------------------------------------------------------
    UiRow(page.fullscreenRow, ls, in.Over(page.fullscreenRow), UiAccent);

    UiLabel("FULLSCREEN", page.fullscreenRow.x + RowPad*ls,
            page.fullscreenRow.y + (page.fullscreenRow.height - LabelSize*ls)*0.5f,
            LabelSize*ls, UiInk);

    UiLabelRight(view.fullscreenOn ? "ON" : "WINDOWED",
                page.fullscreenRow.x + page.fullscreenRow.width - RowPad*ls,
                page.fullscreenRow.y + (page.fullscreenRow.height - ValueSize*ls)*0.5f,
                ValueSize*ls, view.fullscreenOn ? UiReady : UiDim);

    //------------------------------------------------------------------------------
    // Mute, kept SEPARATE from dragging a level to zero rather than folded into it.
    // They are different things a player wants: mute is "not right now" and remembers
    // what the levels were, zero is "this is how loud I want it". A mute that
    // clobbered the levels would make un-muting a second decision.
    //------------------------------------------------------------------------------
    UiRow(page.muteRow, ls, in.Over(page.muteRow), UiAccent);

    UiLabel("SOUND", page.muteRow.x + RowPad*ls,
            page.muteRow.y + (page.muteRow.height - LabelSize*ls)*0.5f,
            LabelSize*ls, UiInk);

    UiLabelRight(view.muted ? "MUTED" : "ON",
                page.muteRow.x + page.muteRow.width - RowPad*ls,
                page.muteRow.y + (page.muteRow.height - ValueSize*ls)*0.5f,
                ValueSize*ls, view.muted ? UiOff : UiReady);

    //------------------------------------------------------------------------------
    // The three levels - see the note on OptionsView. All drawn dim while muted: the
    // numbers are still the truth about what comes back when the sound does, and
    // greying them is how the rows say so without a second sentence.
    //------------------------------------------------------------------------------
    const float level[Layout::Levels] = { view.volume, view.music, view.effects };

    for (int i = 0; i < Layout::Levels; ++i)
    {
        const Rectangle row = page.levelRow[i];

        UiRow(row, ls, false, UiAccent);

        UiLabel(LevelName[i], row.x + RowPad*ls,
                row.y + (row.height - LabelSize*ls)*0.5f,
                LabelSize*ls, view.muted ? UiOff : UiInk);

        const int percent = (int)(level[i]*100.0f + 0.5f);

        UiLabelRight(TextFormat("%i%%", percent), page.levelMinus[i].x - GlyphGap*ls,
                    row.y + (row.height - ValueSize*ls)*0.5f,
                    ValueSize*ls, view.muted ? UiOff : UiInk);

        UiGlyphButton(page.levelMinus[i], level[i] > 0.0f, ls, in, "-", UiPanel);
        UiGlyphButton(page.levelPlus[i], level[i] < 1.0f, ls, in, "+", UiPanel);
    }

    UiButton(page.back, true, ls, in, view.backLabel, UiPanel, UiInk);
}
