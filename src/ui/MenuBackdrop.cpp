#include "ui/MenuBackdrop.h"

#include "core/Config.h"
#include "render/AssetManager.h"

#include <cmath>

void MenuBackdrop::Load(AssetManager &assets)
{
    sky = nullptr;

    if (!FileExists(AssetManager::Resolve(Config::MenuSkyImage).c_str()))
    {
        TraceLog(LOG_WARNING, "MENU: %s not found, the front end keeps its flat backdrop",
                 Config::MenuSkyImage);
        return;
    }

    sky = &assets.GetTexture(Config::MenuSkyImage);

    // It is always drawn LARGER than its native size - a 1080p source covering
    // whatever window it is given - so the upscale is smoothed rather than shown as
    // pixels. The same call the mobile game makes, for the same reason.
    SetTextureFilter(*sky, TEXTURE_FILTER_BILINEAR);
}

void MenuBackdrop::Draw() const
{
    if (sky == nullptr) return;

    const float screenW = (float)GetScreenWidth();
    const float screenH = (float)GetScreenHeight();

    const float iw = (float)sky->width;
    const float ih = (float)sky->height;

    if ((iw < 1.0f) || (ih < 1.0f)) return;

    //------------------------------------------------------------------------------
    // Scaled to COVER: the larger of the two ratios, so whichever axis is short
    // decides and the other overflows. A window wider than 16:9 is limited by the
    // width, a taller one by the height, and neither ever letterboxes.
    //------------------------------------------------------------------------------
    const float scale = fmaxf(screenW/iw, screenH/ih);

    const float dw = iw*scale;
    const float dh = ih*scale;

    // Centred on both axes, so what is cropped is taken evenly off both edges
    const Rectangle src = { 0.0f, 0.0f, iw, ih };
    const Rectangle dst = { (screenW - dw)*0.5f, (screenH - dh)*0.5f, dw, dh };

    const Color tint = { Config::MenuSkyTint[0], Config::MenuSkyTint[1],
                         Config::MenuSkyTint[2], 255 };

    DrawTexturePro(*sky, src, dst, { 0.0f, 0.0f }, 0.0f, tint);
}
