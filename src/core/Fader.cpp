#include "core/Fader.h"

#include "core/Config.h"
#include "ui/UiTheme.h"

void Fader::Start()
{
    phase = Phase::Out;
}

void Fader::Update(float delta)
{
    atBlackThisFrame = false;

    const float step = delta/Config::ScreenFadeDuration;

    switch (phase)
    {
        case Phase::Idle:
            break;

        case Phase::Out:
            alpha += step;

            if (alpha >= 1.0f)
            {
                alpha = 1.0f;
                // The frame everything is hidden behind the overlay - the one
                // chance the caller has to swap what's underneath for free.
                atBlackThisFrame = true;
                phase = Phase::In;
            }
            break;

        case Phase::In:
            alpha -= step;

            if (alpha <= 0.0f)
            {
                alpha = 0.0f;
                phase = Phase::Idle;
            }
            break;
    }
}

void Fader::Draw() const
{
    UiFadeOverlay(alpha);
}

bool Fader::AtBlack() const
{
    return atBlackThisFrame;
}
