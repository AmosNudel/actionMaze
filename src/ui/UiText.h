#pragma once

#include "raylib.h"

//----------------------------------------------------------------------------------
// The game's face, and every way text is drawn with it.
//
// raylib's DrawText and MeasureText are hard-wired to its built-in font. There is no
// setter for it, and the font a game uses is one of the loudest things about how it
// looks - so every string in this project goes through here instead, and the face is
// loaded once into this module.
//
// It is Alagard (assets/fonts/alagard.png, see the CREDIT file beside it), the same
// display face the mobile game uses, and that shared identity is the point: the two
// are the same game seen from two distances, and a player who has played one should
// recognise the other before reading a word of it.
//
// --- What that costs the layouts --------------------------------------------------
// Alagard has a taller cap height than raylib's default, so a size tuned against the
// old font reads a pixel or two larger here. Anything that looked tight before this
// swap is tighter now - worth remembering before shrinking a panel to fit.
//
// --- ASCII ONLY -------------------------------------------------------------------
// It is a raylib IMAGE font: 95 glyphs, codepoints 32 to 126, in 16px bands on a
// magenta key. Raylib renders anything outside that range as '?', silently. An em
// dash, a curly quote or an accented letter in a DRAWN string is a question mark on
// screen and a perfectly readable character in the editor. Comments and TraceLog
// strings are exempt - nothing draws them.
//
// --- Sizes are floats -------------------------------------------------------------
// Everything on the fullscreen pages is a design size times a fitted scale, which is
// fractional on nearly every window. Rounding to int at the call site - which is what
// MeasureText forced - threw away most of a pixel on every small label. Nothing here
// rounds until raylib does.
//
// Lifecycle: LoadUiFont() once after InitWindow (the texture needs a GL context),
// UnloadUiFont() before CloseWindow. Before the load, and after a failed one, UiFont()
// is raylib's default - so a build with the asset missing is ugly rather than blank.
//----------------------------------------------------------------------------------
void LoadUiFont();
void UnloadUiFont();

// The face in use: the loaded one, or raylib's default until/unless that worked.
Font UiFont();

// The spacing raylib wants alongside a size. Exposed because a caller laying text out
// itself has to measure with the same number this draws with.
float UiTextSpacing(float size);

// Width of `text` at `size`, in pixels - the MeasureText replacement.
float UiTextWidth(const char *text, float size);

// Plain text, top-left at (x, y).
void UiText(const char *text, float x, float y, float size, Color c);

// The same with a one-pixel black drop shadow: the default for anything over a panel.
void UiTextShadow(const char *text, float x, float y, float size, Color c);

// The same with a four-way black outline. Five draws, so it is for text over the
// WORLD - a banner, a floating number - where a drop shadow alone leaves the glyph
// edge fighting whatever is behind it.
void UiTextOutline(const char *text, float x, float y, float size, Color c);

// Centred horizontally on `cx` - the idiom every button was writing out by hand as
// `x - MeasureText(t, fs)/2`.
void UiTextCentered(const char *text, float cx, float y, float size, Color c);
void UiTextCenteredShadow(const char *text, float cx, float y, float size, Color c);
void UiTextCenteredOutline(const char *text, float cx, float y, float size, Color c);

// Right-aligned, so the text ENDS at `right` - the value columns.
void UiTextRight(const char *text, float right, float y, float size, Color c);

//----------------------------------------------------------------------------------
// The smallest a label may be drawn at, in screen pixels.
//
// Alagard is a 16px bitmap face, so under about eleven screen pixels the glyphs stop
// being letters and start being texture noise. Every fullscreen page here shrinks its
// layout scale to fit a short window, which is exactly the case that drives sizes
// down there.
//
// It is a floor on the RESULT and not on the design size: a caller asks for `13*ls`
// and gets back whichever of the two is larger. Better a page that has to be scrolled
// than one that cannot be read.
//----------------------------------------------------------------------------------
constexpr float UiMinTextPx = 11.0f;

float UiFontFloor(float size);
