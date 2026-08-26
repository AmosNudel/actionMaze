#include "render/Glow.h"

#include "core/Config.h"
#include "render/AssetManager.h"

#include <cmath>
#include <string>

Texture2D &GlowTexture(AssetManager &assets)
{
    // A name, not a path - see AssetManager::AdoptTexture. Nothing on disk answers
    // to it, so it goes in through the door meant for generated art rather than
    // through GetTexture, which would try to load it as a file and cache the
    // failure.
    static const std::string key = "generated:glow";

    if (Texture2D *cached = assets.FindTexture(key)) return *cached;

    constexpr int size = Config::MoteTextureSize;

    Image image = GenImageColor(size, size, BLANK);

    const float centre = (size - 1)*0.5f;

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const float dx = (x - centre)/centre;
            const float dy = (y - centre)/centre;
            const float radius = sqrtf(dx*dx + dy*dy);

            if (radius >= 1.0f) continue;   // Left BLANK: no lit corners

            // Squared-ish falloff rather than linear. A linear ramp blended
            // additively reads as a flat grey disc - the eye wants most of the
            // brightness in the middle third.
            const float falloff = powf(1.0f - radius, Config::MoteSoftness);

            // White, so a tint at the draw is the caller's colour exactly rather
            // than the caller's colour times whatever was baked in here
            ImageDrawPixel(&image, x, y,
                           (Color){ 255, 255, 255, (unsigned char)(falloff*255.0f + 0.5f) });
        }
    }

    Texture2D &texture = assets.AdoptTexture(key, LoadTextureFromImage(image));
    UnloadImage(image);

    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);

    return texture;
}
