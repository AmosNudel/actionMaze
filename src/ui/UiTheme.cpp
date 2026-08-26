#include "ui/UiTheme.h"

#include "core/Config.h"
#include "ui/UiText.h"

namespace
{
    // Mixing a colour toward white or black rather than fading it. A Fade()'d control
    // shows the world through itself, and a stack of translucent plates over a
    // dungeon is how a UI ends up looking like a decal.
    Color Mix(Color a, Color b, float t)
    {
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;

        return { (unsigned char)(a.r + (b.r - a.r)*t),
                 (unsigned char)(a.g + (b.g - a.g)*t),
                 (unsigned char)(a.b + (b.b - a.b)*t),
                 a.a };
    }

    Color Darken(Color c, float t) { return Mix(c, { 0, 0, 0, c.a }, t); }
}

float UiScale()
{
    if (Config::UiDesignHeight <= 0.0f) return 1.0f;

    const float scale = GetScreenHeight()/Config::UiDesignHeight;

    // A window shorter than half the reference height is a window nobody is playing
    // in, and letting the scale fall with it would put the labels under the font's
    // own floor and turn the HUD into noise. Held at a half.
    return (scale < 0.5f) ? 0.5f : scale;
}

UiInput UiInput::Read(bool opened)
{
    UiInput in;

    in.pos = GetMousePosition();
    in.clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    in.down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    in.justOpened = opened;

    // The click that opened the page is not a click inside it. Without this, opening
    // the character page with the mouse spends a point on whichever row the cursor
    // happened to be over.
    if (in.justOpened) in.clicked = false;

    return in;
}

bool UiInput::Over(Rectangle r) const
{
    return CheckCollisionPointRec(pos, r);
}

//----------------------------------------------------------------------------------
// The world, blacked out.
//
// Nearly opaque rather than a tint. These pages are modal and should look it: a panel
// floating over a live-looking dungeon reads as an overlay the player can play
// through, which is exactly what it is not. A little of the room is left showing so
// the page still reads as being IN the game rather than in front of it.
//----------------------------------------------------------------------------------
void UiPageBackdrop()
{
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(UiBg, 0.965f));
}

float UiPageScale(float designHeight, float maxScale)
{
    if (designHeight <= 0.0f) return 1.0f;

    // 0.94 of the window, so a page never runs edge to edge - the margin is what
    // tells the eye it is a page and not the game's own frame.
    float scale = (GetScreenHeight()*0.94f)/designHeight;

    if (scale > maxScale) scale = maxScale;
    if (scale < 0.35f) scale = 0.35f;

    return scale;
}

void UiPanelBox(Rectangle r, float ls, Color fill, Color edge)
{
    DrawRectangleRec(r, fill);
    DrawRectangleLinesEx(r, 1.0f*ls, edge);
}

void UiLabel(const char *text, float x, float y, float size, Color c)
{
    UiTextShadow(text, x, y, UiFontFloor(size), c);
}

void UiLabelRight(const char *text, float right, float y, float size, Color c)
{
    const float fs = UiFontFloor(size);

    UiTextShadow(text, right - UiTextWidth(text, fs), y, fs, c);
}

void UiLabelCentered(const char *text, float cx, float y, float size, Color c)
{
    UiTextCenteredShadow(text, cx, y, UiFontFloor(size), c);
}

//----------------------------------------------------------------------------------
// A button, and the one move that makes the whole UI feel like one thing.
//
// A pressed button SINKS: the face insets by two design pixels and darkens, and the
// text goes with it. Every clickable object on these pages does the same, which is
// what stops a page of controls reading as a page of rectangles.
//----------------------------------------------------------------------------------
bool UiButton(Rectangle r, bool enabled, float ls, const UiInput &in,
              const char *text, Color fill, Color textCol)
{
    const bool held = enabled && in.down && in.Over(r);

    const Rectangle face = held
        ? Rectangle{ r.x + 2*ls, r.y + 2*ls, r.width - 4*ls, r.height - 4*ls }
        : r;

    const Color body = held ? Darken(fill, 0.18f) : fill;

    DrawRectangleRec(face, Fade(body, enabled ? 0.92f : 0.45f));
    DrawRectangleLinesEx(face, 1.5f*ls, Fade(enabled ? RAYWHITE : UiDim, held ? 0.5f : 0.8f));

    const float fs = UiFontFloor(face.height*0.42f);

    UiTextCentered(text, face.x + face.width*0.5f, face.y + (face.height - fs)*0.5f,
                   fs, enabled ? textCol : UiDim);

    return enabled && in.clicked && in.Over(r);
}

bool UiGlyphButton(Rectangle r, bool enabled, float ls, const UiInput &in,
                   const char *glyph, Color fill)
{
    const bool held = enabled && in.down && in.Over(r);

    const Rectangle face = held
        ? Rectangle{ r.x + 2*ls, r.y + 2*ls, r.width - 4*ls, r.height - 4*ls }
        : r;

    const Color body = enabled ? (held ? Darken(fill, 0.18f) : fill)
                               : Color{ 60, 64, 80, 255 };

    DrawRectangleRec(face, Fade(body, enabled ? 0.9f : 0.6f));
    DrawRectangleLinesEx(face, 1.5f*ls, Fade(enabled ? RAYWHITE : UiDim, held ? 0.5f : 0.8f));

    const float fs = UiFontFloor(face.height*0.62f);

    // Dark ink on a lit face. A glyph in the page's own near-white would vanish into
    // the button's fill, which is a light colour by the time it means anything.
    UiTextCentered(glyph, face.x + face.width*0.5f, face.y + (face.height - fs)*0.5f,
                   fs, enabled ? Color{ 30, 28, 20, 255 } : UiDim);

    return enabled && in.clicked && in.Over(r);
}

//----------------------------------------------------------------------------------
// A row.
//
// The highlight goes ON TOP of the row's own edge rather than replacing it. A row
// that changed outline colour would be ambiguous against the disabled styling,
// whereas a second heavier ring only ever means "this is the one you are on".
//----------------------------------------------------------------------------------
void UiRow(Rectangle r, float ls, bool highlighted, Color accent)
{
    DrawRectangleRec(r, Fade(UiSlotCol, highlighted ? 0.95f : 0.72f));
    DrawRectangleLinesEx(r, 1.0f*ls, Fade(UiDim, 0.35f));

    if (highlighted) DrawRectangleLinesEx(r, 2.0f*ls, Fade(accent, 0.9f));
}
