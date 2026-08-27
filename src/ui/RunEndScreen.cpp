#include "ui/RunEndScreen.h"

#include "ui/UiText.h"
#include "ui/UiTheme.h"

namespace
{
    // Design pixels, fitted to the window - see the note in CharacterSheet.cpp
    constexpr float DesignHeight = 320.0f;
    constexpr float DesignWidth  = 480.0f;
    constexpr float MaxScale     = 2.4f;

    constexpr float TitleSize   = 42.0f;
    constexpr float SummarySize = 18.0f;
    constexpr float HintSize    = 14.0f;

    constexpr float TitleTop    = 46.0f;
    constexpr float SummaryTop  = 108.0f;
    constexpr float ButtonsTop  = 190.0f;
    constexpr float ButtonH     = 50.0f;
    constexpr float ButtonGap   = 16.0f;

    // Red for the loss, the same gold every other "good outcome" on these pages
    // already uses for the win - see UiReady in UiTheme.h.
    constexpr Color DefeatRed = { 220, 90, 80, 255 };
}

void RunEndScreen::Open(bool win, int depth, int level)
{
    open = true;
    victorious = win;
    depthReached = depth;
    characterLevel = level;
    justOpened = true;
}

RunEndScreen::Layout RunEndScreen::Measure()
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
    out.summaryY = out.page.y + SummaryTop*out.ls;

    const float buttonW = (out.page.width - ButtonGap*out.ls)*0.5f;
    const float buttonY = out.page.y + ButtonsTop*out.ls;

    out.restart = { out.page.x, buttonY, buttonW, ButtonH*out.ls };
    out.quit = { out.page.x + buttonW + ButtonGap*out.ls, buttonY, buttonW, ButtonH*out.ls };

    return out;
}

RunEndScreen::Choice RunEndScreen::Update()
{
    if (!open) return Choice::None;

    const UiInput in = UiInput::Read(justOpened);

    justOpened = false;

    const Layout page = Measure();

    if (!in.clicked) return Choice::None;

    if (in.Over(page.restart)) return Choice::Restart;
    if (in.Over(page.quit))    return Choice::Quit;

    return Choice::None;
}

void RunEndScreen::Draw() const
{
    if (!open) return;

    const Layout page = Measure();
    const float ls = page.ls;

    UiPageBackdrop();

    const char *title = victorious ? "DUNGEON CLEARED" : "YOU DIED";
    const Color titleColour = victorious ? UiReady : DefeatRed;

    UiLabelCentered(title, page.page.x + page.page.width*0.5f, page.titleY,
                    TitleSize*ls, titleColour);

    const char *summary = victorious
        ? TextFormat("floor %i cleared, character level %i", depthReached, characterLevel)
        : TextFormat("fell on floor %i, character level %i", depthReached, characterLevel);

    UiLabelCentered(summary, page.page.x + page.page.width*0.5f, page.summaryY,
                    SummarySize*ls, UiDim);

    const UiInput in = UiInput::Read(false);

    UiButton(page.restart, true, ls, in, "RESTART", UiPanel, UiReady);
    UiButton(page.quit, true, ls, in, "QUIT", UiPanel, UiInk);

    UiLabelCentered("a new run starts fresh - level, purse and gear all reset",
                    page.page.x + page.page.width*0.5f,
                    page.page.y + page.page.height - HintSize*ls, HintSize*ls, UiDim);
}
