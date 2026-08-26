#include "ui/UiBars.h"

#include "render/AssetManager.h"

#include <string>

namespace
{
    constexpr char Dir[] = "ui/";

    // Source geometry, in the art's own pixels.
    constexpr float FrameW = 118.0f;
    constexpr float FrameH = 13.0f;
    constexpr float FillW  = 100.0f;
    constexpr float FillH  = 7.0f;

    // Where the frame's hollow window sits inside bar.png. The window is exactly the
    // size of the fill strips, so they drop in without any stretching. The border is
    // symmetric: 118 - 9 - 100 leaves the same 9px cap on the right.
    constexpr float InnerX = 9.0f;
    constexpr float InnerY = 3.0f;
    constexpr float CapW   = 9.0f;

    //------------------------------------------------------------------------------
    // Recolouring one strip.
    //
    // The art is a RED RAMP - its dark border is a dark red, its highlight a pale
    // red - so the red channel is the one carrying all the shading, and every recolour
    // here is a way of moving that ramp into another channel rather than replacing
    // pixels. That is what keeps the artist's shading intact.
    //------------------------------------------------------------------------------
    using Recolour = void (*)(Color &);

    // Red <-> blue: (238,68,31) becomes (31,68,238).
    void ToBlue(Color &px)
    {
        const unsigned char t = px.r;
        px.r = px.b;
        px.b = t;
    }

    // Violet: the ramp is copied into blue and damped in red, so the bright body
    // becomes (155,68,238) - purple, not the magenta a straight copy gives.
    void ToViolet(Color &px)
    {
        px.b = px.r;
        px.r = (unsigned char)(px.r*0.65f);
    }

    // The event bar is tinted per call, so its strip has to start WHITE - a tint
    // multiplies, and any hue left in the art would drag every event colour towards
    // it. The ramp is flattened to its own luminance and nothing else.
    void ToGrey(Color &px)
    {
        const unsigned char g = (unsigned char)(px.r*0.60f + px.g*0.30f + px.b*0.10f);

        // Lifted, because a straight luminance of a red ramp tops out near 130 and a
        // bar tinted from that is half as bright as the three that are not.
        const int raised = (int)(g*1.65f);

        px.r = px.g = px.b = (unsigned char)((raised > 255) ? 255 : raised);
    }

    //------------------------------------------------------------------------------
    // A recoloured copy of `path`, adopted under `name`.
    //
    // Adopted rather than loaded, because nothing on disk answers to the name: these
    // are generated, and GetTexture would try the name as a file, fail, and cache the
    // failure. The AssetManager owns them either way, which is what matters - they
    // are GPU resources and have to go before the context does.
    //------------------------------------------------------------------------------
    Texture2D *Recoloured(AssetManager &assets, const std::string &path,
                          const char *name, Recolour recolour)
    {
        if (Texture2D *found = assets.FindTexture(name)) return found;

        Image image = LoadImage(AssetManager::Resolve(path).c_str());

        if (image.data == nullptr) return nullptr;

        ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

        Color *pixels = (Color *)image.data;

        for (int i = 0; i < image.width*image.height; ++i) recolour(pixels[i]);

        Texture2D &texture = assets.AdoptTexture(name, LoadTextureFromImage(image));

        UnloadImage(image);

        return &texture;
    }
}

float UiBars::Height(float scale)
{
    return FrameH*scale;
}

void UiBars::Load(AssetManager &assets)
{
    const std::string dir = Dir;

    if (!FileExists(AssetManager::Resolve(dir + "bar.png").c_str()))
    {
        TraceLog(LOG_WARNING, "UI: no %sbar.png - the HUD draws plain bars", Dir);

        return;
    }

    frame = &assets.GetTexture(dir + "bar.png");
    track = &assets.GetTexture(dir + "bar_background.png");

    const std::string strip = dir + "health_bar.png";

    fills[(int)BarHue::Health] = &assets.GetTexture(strip);
    fills[(int)BarHue::Exp]    = Recoloured(assets, strip, "bar.fill.exp", ToBlue);
    fills[(int)BarHue::Chaos]  = Recoloured(assets, strip, "bar.fill.chaos", ToViolet);
    fills[(int)BarHue::Event]  = Recoloured(assets, strip, "bar.fill.event", ToGrey);
}

//----------------------------------------------------------------------------------
// Track, fill, frame, in that order.
//
// The frame goes on LAST because its end ornaments overlap the window slightly: drawn
// on top, the bar's ends are tucked behind the metalwork instead of running out from
// under it.
//----------------------------------------------------------------------------------
void UiBars::Draw(BarHue hue, float fill, float x, float y, float width, float scale,
                  Color tint) const
{
    if (!Ready()) return;

    const Texture2D *strip = fills[(int)hue];

    if (strip == nullptr) return;

    if (fill < 0.0f) fill = 0.0f;
    if (fill > 1.0f) fill = 1.0f;

    const float cap = CapW*scale;
    const float innerW = width - 2.0f*cap;

    if (innerW < 1.0f) return;      // Too narrow to draw anything sane

    const float ix = x + InnerX*scale;
    const float iy = y + InnerY*scale;
    const float ih = FillH*scale;

    DrawTexturePro(*track, { 0, 0, FillW, FillH }, { ix, iy, innerW, ih },
                   { 0, 0 }, 0.0f, WHITE);

    // Cropped from the LEFT of the source by the same fraction as the destination, so
    // the bar drains to the right without the art squashing as it shrinks.
    if (fill > 0.0f)
    {
        DrawTexturePro(*strip, { 0, 0, FillW*fill, FillH },
                       { ix, iy, innerW*fill, ih }, { 0, 0 }, 0.0f, tint);
    }

    const float h = FrameH*scale;

    DrawTexturePro(*frame, { 0, 0, CapW, FrameH }, { x, y, cap, h }, { 0, 0 }, 0.0f, WHITE);
    DrawTexturePro(*frame, { CapW, 0, FrameW - 2.0f*CapW, FrameH },
                   { x + cap, y, width - 2.0f*cap, h }, { 0, 0 }, 0.0f, WHITE);
    DrawTexturePro(*frame, { FrameW - CapW, 0, CapW, FrameH },
                   { x + width - cap, y, cap, h }, { 0, 0 }, 0.0f, WHITE);
}
