#include "ui/OptionsScreen.h"

#include "ui/UiText.h"
#include "ui/UiTheme.h"

namespace
{
    // Design pixels, fitted to the window - see the note in CharacterSheet.cpp
    constexpr float DesignHeight = 360.0f;
    constexpr float DesignWidth  = 480.0f;
    constexpr float MaxScale     = 2.4f;

    constexpr float TitleSize = 46.0f;
    constexpr float LabelSize = 22.0f;
    constexpr float ValueSize = 20.0f;

    constexpr float TitleTop  = 10.0f;
    constexpr float RowsTop   = 110.0f;
    constexpr float RowHeight = 66.0f;
    constexpr float RowGap    = 14.0f;
    constexpr float RowPad    = 20.0f;

    constexpr float GlyphSize = 40.0f;
    constexpr float GlyphGap  = 10.0f;

    constexpr float ButtonTop = 20.0f;   // Below the two rows
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

    out.fullscreenRow = { out.page.x, out.page.y + RowsTop*out.ls,
                          out.page.width, RowHeight*out.ls };

    out.volumeRow = { out.fullscreenRow.x,
                      out.fullscreenRow.y + out.fullscreenRow.height + RowGap*out.ls,
                      out.page.width, RowHeight*out.ls };

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
    if (in.Over(page.volumeMinus))   return Choice::VolumeDown;
    if (in.Over(page.volumePlus))    return Choice::VolumeUp;
    if (in.Over(page.back))          return Choice::Back;

    return Choice::None;
}

void OptionsScreen::Draw(bool fullscreenOn, float volume) const
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

    // Fullscreen - the whole row is the control, the same way a PauseMenu entry is
    UiRow(page.fullscreenRow, ls, in.Over(page.fullscreenRow), UiAccent);

    UiLabel("FULLSCREEN", page.fullscreenRow.x + RowPad*ls,
            page.fullscreenRow.y + (page.fullscreenRow.height - LabelSize*ls)*0.5f,
            LabelSize*ls, UiInk);

    UiLabelRight(fullscreenOn ? "ON" : "OFF",
                page.fullscreenRow.x + page.fullscreenRow.width - RowPad*ls,
                page.fullscreenRow.y + (page.fullscreenRow.height - ValueSize*ls)*0.5f,
                ValueSize*ls, fullscreenOn ? UiReady : UiDim);

    // Volume
    UiRow(page.volumeRow, ls, false, UiAccent);

    UiLabel("VOLUME", page.volumeRow.x + RowPad*ls,
            page.volumeRow.y + (page.volumeRow.height - LabelSize*ls)*0.5f,
            LabelSize*ls, UiInk);

    const int percent = (int)(volume*100.0f + 0.5f);

    UiLabelRight(TextFormat("%i%%", percent), page.volumeMinus.x - GlyphGap*ls,
                page.volumeRow.y + (page.volumeRow.height - ValueSize*ls)*0.5f,
                ValueSize*ls, UiInk);

    UiGlyphButton(page.volumeMinus, volume > 0.0f, ls, in, "-", UiPanel);
    UiGlyphButton(page.volumePlus, volume < 1.0f, ls, in, "+", UiPanel);

    UiButton(page.back, true, ls, in, "BACK TO MENU", UiPanel, UiInk);
}
