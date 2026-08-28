#include "ui/LoadingScreen.h"

#include "raylib.h"
#include "ui/UiText.h"
#include "ui/UiTheme.h"

#include <cmath>

namespace
{
    constexpr float LabelSize = 28.0f;
    constexpr float StepSize  = 15.0f;
    constexpr float DotPeriod = 0.45f;   // Seconds per dot added
    constexpr int   MaxDots   = 3;

    // The bar, in design pixels - see UiPageScale. Drawn as plain rectangles rather
    // than through UiBars: that art is the HUD's, and the HUD is one of the things
    // this screen is up in order to load. A progress bar that could only be drawn
    // once loading had finished would be no use to anyone.
    constexpr float BarWidth  = 260.0f;
    constexpr float BarHeight = 10.0f;
    constexpr float BarTop    = 26.0f;   // Below the label
    constexpr float StepTop   = 14.0f;   // Below the bar
    constexpr float BarEdge   = 1.0f;

    // How fast the fill is allowed to look like it is moving. The steps behind it
    // are wildly uneven - one is a cubemap and one is six rigged characters - so
    // the bar is eased toward the true figure instead of jumping to it, which is
    // the difference between a bar that reads as progress and one that reads as
    // stalling and then teleporting.
    //
    // Kept here as a static rather than on the class because it is a property of
    // the DRAWING and not of the load: the page stays stateless to its owner, and
    // a reset is simply the fraction going back down, which the ease follows.
    float Eased(float target)
    {
        static float shown = 0.0f;

        // Backwards is a new load starting, and there is nothing to smooth about
        // that - the bar belongs at the bottom immediately
        if (target < shown) shown = target;

        shown += (target - shown)*fminf(GetFrameTime()*8.0f, 1.0f);

        return shown;
    }
}

void LoadingScreen::Draw(const char *label, float elapsed, float progress, const char *step) const
{
    UiPageBackdrop();

    const float screenW = (float)GetScreenWidth();
    const float screenH = (float)GetScreenHeight();
    const float ls = UiPageScale(300.0f, 2.4f);

    const int dots = ((int)(elapsed/DotPeriod)) % (MaxDots + 1);

    char text[32];
    int n = 0;
    for (; label[n] != '\0' && n < 24; ++n) text[n] = label[n];
    for (int i = 0; i < dots; ++i) text[n++] = '.';
    text[n] = '\0';

    // The label sits where it always did when there is no bar, and rises by half
    // the bar's block when there is - so the two together stay centred on the same
    // line the label alone was, rather than the page shifting between the two
    // screens that use it.
    const bool bar = (progress >= 0.0f);
    const float block = bar ? (BarTop + BarHeight + StepTop + StepSize)*ls : 0.0f;

    const float labelY = screenH*0.5f - (LabelSize*ls + block)*0.5f;

    UiLabelCentered(text, screenW*0.5f, labelY, LabelSize*ls, UiInk);

    if (!bar) return;

    const float width = BarWidth*ls;
    const float height = BarHeight*ls;
    const float x = (screenW - width)*0.5f;
    const float y = labelY + LabelSize*ls + BarTop*ls;

    const float filled = Eased(fminf(progress, 1.0f));

    // Track, then fill, then the frame over both - so the fill is boxed in by the
    // edge at every width instead of poking out of the corners at full
    DrawRectangleRec({ x, y, width, height }, UiPanel);
    DrawRectangleRec({ x, y, width*filled, height }, UiAccent);
    DrawRectangleLinesEx({ x, y, width, height }, BarEdge*ls, Fade(UiDim, 0.55f));

    if (step != nullptr)
    {
        UiLabelCentered(step, screenW*0.5f, y + height + StepTop*ls, StepSize*ls, UiDim);
    }
}
