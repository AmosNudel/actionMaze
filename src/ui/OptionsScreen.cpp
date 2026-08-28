#include "ui/OptionsScreen.h"

#include "ui/UiText.h"
#include "ui/UiTheme.h"

namespace
{
    // Design pixels, fitted to the window - see the note in CharacterSheet.cpp
    constexpr float DesignHeight = 400.0f;
    constexpr float DesignWidth  = 480.0f;
    constexpr float MaxScale     = 2.4f;

    constexpr float TitleSize = 46.0f;
    constexpr float LabelSize = 22.0f;
    constexpr float ValueSize = 20.0f;

    constexpr float TitleTop  = 10.0f;
    constexpr float RowsTop   = 110.0f;
    constexpr float RowHeight = 60.0f;
    constexpr float RowGap    = 12.0f;
    constexpr float RowPad    = 20.0f;

    constexpr float GlyphSize = 38.0f;
    constexpr float GlyphGap  = 10.0f;

    constexpr float ButtonTop = 22.0f;   // Below the three rows
    constexpr float ButtonH   = 50.0f;
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

    // The three rows, stacked on one pitch. Walked rather than written out, so a
    // fourth setting is one more rectangle and not three edited constants.
    const float pitch = (RowHeight + RowGap)*out.ls;
    const float rowsY = out.page.y + RowsTop*out.ls;

    out.fullscreenRow = { out.page.x, rowsY,             out.page.width, RowHeight*out.ls };
    out.muteRow       = { out.page.x, rowsY + pitch,     out.page.width, RowHeight*out.ls };
    out.volumeRow     = { out.page.x, rowsY + pitch*2.0f, out.page.width, RowHeight*out.ls };

    const float glyphY = out.volumeRow.y + (out.volumeRow.height - GlyphSize*out.ls)*0.5f;

    out.volumePlus = { out.volumeRow.x + out.volumeRow.width - RowPad*out.ls - GlyphSize*out.ls,
                       glyphY, GlyphSize*out.ls, GlyphSize*out.ls };

    out.volumeMinus = { out.volumePlus.x - GlyphGap*out.ls - GlyphSize*out.ls,
                        glyphY, GlyphSize*out.ls, GlyphSize*out.ls };

    const float buttonY = out.volumeRow.y + out.volumeRow.height + ButtonTop*out.ls;

    out.back = { out.page.x, buttonY, out.page.width, ButtonH*out.ls };

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
    if (in.Over(page.volumeMinus))   return Choice::VolumeDown;
    if (in.Over(page.volumePlus))    return Choice::VolumeUp;

    // After the two glyphs, so a click that landed on one of them is a step and not
    // a row press. The volume row is not a toggle - it is a label with two buttons
    // on it - and the whole-row test only exists to swallow the misses.
    if (in.Over(page.volumeRow)) return Choice::None;

    if (in.Over(page.back)) return Choice::Back;

    return Choice::None;
}

void OptionsScreen::Draw(const OptionsView &view) const
{
    const Layout page = Measure();
    const float ls = page.ls;

    UiPageBackdrop();

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
    // Mute, kept SEPARATE from dragging the volume to zero rather than folded into
    // it. They are different things a player wants: mute is "not right now" and
    // remembers what the volume was, zero is "this is how loud I want it". A mute
    // that clobbered the volume would make un-muting a second decision.
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
    // Volume. Drawn dim while muted - the number is still the truth about what will
    // come back when the sound does, and greying it is how the row says so without
    // a second sentence.
    //------------------------------------------------------------------------------
    UiRow(page.volumeRow, ls, false, UiAccent);

    UiLabel("VOLUME", page.volumeRow.x + RowPad*ls,
            page.volumeRow.y + (page.volumeRow.height - LabelSize*ls)*0.5f,
            LabelSize*ls, view.muted ? UiOff : UiInk);

    const int percent = (int)(view.volume*100.0f + 0.5f);

    UiLabelRight(TextFormat("%i%%", percent), page.volumeMinus.x - GlyphGap*ls,
                page.volumeRow.y + (page.volumeRow.height - ValueSize*ls)*0.5f,
                ValueSize*ls, view.muted ? UiOff : UiInk);

    UiGlyphButton(page.volumeMinus, view.volume > 0.0f, ls, in, "-", UiPanel);
    UiGlyphButton(page.volumePlus, view.volume < 1.0f, ls, in, "+", UiPanel);

    UiButton(page.back, true, ls, in, view.backLabel, UiPanel, UiInk);
}
