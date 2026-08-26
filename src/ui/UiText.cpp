#include "ui/UiText.h"

#include "render/AssetManager.h"

namespace
{
    constexpr char FontFile[] = "fonts/alagard.png";

    Font face = { 0 };

    // Tracks OWNERSHIP, not merely success. raylib's LoadFont already falls back to
    // the default font on a missing file and reports id 0 only when even that
    // failed - and the default font is raylib's, so unloading it would take the
    // whole engine's text with it.
    bool owned = false;
}

void LoadUiFont()
{
    if (owned) return;

    const Font loaded = LoadFont(AssetManager::Resolve(FontFile).c_str());

    if (loaded.texture.id == 0)
    {
        TraceLog(LOG_WARNING, "UI: could not load %s - falling back to the default font",
                 FontFile);

        return;
    }

    // Every size on the fullscreen pages is a design size times a fitted scale, so
    // glyphs land on fractional sizes on nearly every window. Point filtering makes
    // that shimmer as the number moves; bilinear keeps the upscale clean.
    SetTextureFilter(loaded.texture, TEXTURE_FILTER_BILINEAR);

    face = loaded;
    owned = true;
}

void UnloadUiFont()
{
    if (!owned) return;

    // UnloadFont and not UnloadTexture: LoadFontFromImage heap-allocates the glyph
    // array and the source rectangles alongside the atlas, and the texture is only
    // one of the three.
    UnloadFont(face);

    face = Font{ 0 };
    owned = false;
}

Font UiFont()
{
    return owned ? face : GetFontDefault();
}

// raylib's own DrawText uses size/10 rounded up to at least 1. Matching it keeps
// every layout measuring the way it did before the swap, so nothing has to be
// re-tuned twice.
float UiTextSpacing(float size)
{
    const float s = size/10.0f;

    return (s < 1.0f) ? 1.0f : s;
}

float UiTextWidth(const char *text, float size)
{
    if ((text == nullptr) || (text[0] == '\0')) return 0.0f;

    return MeasureTextEx(UiFont(), text, size, UiTextSpacing(size)).x;
}

void UiText(const char *text, float x, float y, float size, Color c)
{
    DrawTextEx(UiFont(), text, { x, y }, size, UiTextSpacing(size), c);
}

void UiTextShadow(const char *text, float x, float y, float size, Color c)
{
    UiText(text, x + 1.0f, y + 1.0f, size, Fade(BLACK, 0.7f));
    UiText(text, x, y, size, c);
}

void UiTextOutline(const char *text, float x, float y, float size, Color c)
{
    // Four cardinal offsets rather than eight: the diagonals cost two more draws for
    // a thickening that is invisible at these sizes.
    const Color edge = Fade(BLACK, 0.85f);

    UiText(text, x - 1.0f, y, size, edge);
    UiText(text, x + 1.0f, y, size, edge);
    UiText(text, x, y - 1.0f, size, edge);
    UiText(text, x, y + 1.0f, size, edge);
    UiText(text, x, y, size, c);
}

void UiTextCentered(const char *text, float cx, float y, float size, Color c)
{
    UiText(text, cx - UiTextWidth(text, size)*0.5f, y, size, c);
}

void UiTextCenteredShadow(const char *text, float cx, float y, float size, Color c)
{
    UiTextShadow(text, cx - UiTextWidth(text, size)*0.5f, y, size, c);
}

void UiTextCenteredOutline(const char *text, float cx, float y, float size, Color c)
{
    UiTextOutline(text, cx - UiTextWidth(text, size)*0.5f, y, size, c);
}

void UiTextRight(const char *text, float right, float y, float size, Color c)
{
    UiText(text, right - UiTextWidth(text, size), y, size, c);
}

float UiFontFloor(float size)
{
    return (size < UiMinTextPx) ? UiMinTextPx : size;
}
