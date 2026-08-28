#pragma once

#include "raylib.h"

//----------------------------------------------------------------------------------
// Who made the art and the face this game is wearing - see CREDITS.md, which this
// is a short in-game reading of. Static: nothing on this page changes at runtime,
// so unlike every other fullscreen page here it has no Choice worth naming beyond
// Back.
//----------------------------------------------------------------------------------
class CreditsScreen
{
public:
    enum class Choice { None, Back };

    void Show();

    Choice Update();
    void Draw() const;

private:
    struct Layout
    {
        float ls = 1.0f;

        Rectangle page{};
        Rectangle back{};

        float titleY = 0.0f;
        float listTop = 0.0f;
    };

    static Layout Measure();

    bool justShown = false;
};
