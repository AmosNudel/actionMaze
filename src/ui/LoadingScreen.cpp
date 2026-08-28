#include "ui/LoadingScreen.h"

#include "raylib.h"
#include "ui/UiText.h"
#include "ui/UiTheme.h"

namespace
{
    constexpr float LabelSize = 28.0f;
    constexpr float DotPeriod = 0.45f;   // Seconds per dot added
    constexpr int   MaxDots   = 3;
}

void LoadingScreen::Draw(const char *label, float elapsed) const
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

    UiLabelCentered(text, screenW*0.5f, screenH*0.5f - LabelSize*ls*0.5f, LabelSize*ls, UiInk);
}
