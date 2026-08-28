#pragma once

#include "raylib.h"

//----------------------------------------------------------------------------------
// The palette and the handful of controls the fullscreen pages share.
//
// The character page and the pause page are the same kind of object: an opaque page
// over a stopped world, laid out at a fitted scale, driven by the mouse. They were
// always going to want the same drop-shadowed label, the same draw-and-hit-test
// button and the same row, and this is where those live so the two cannot drift into
// looking like two different games.
//
// Ported from the mobile game's ui_widgets, deliberately unchanged in look. The two
// projects share a face, a palette and a set of controls; what differs is the pointer
// driving them, which is why UiInput below carries a hover the touch version had no
// use for.
//
// Everything here draws AND hit-tests in one call. There is no retained widget tree,
// no ids and no separate layout pass: a control returns true on the frame it was
// clicked, and the caller acts on it immediately.
//
// `ls` throughout is the caller's LAYOUT SCALE - design pixels to screen pixels,
// already fitted to the window by whoever owns the page. Every size here is
// multiplied by it.
//----------------------------------------------------------------------------------

// The shared colours. The pages are meant to look like one game, so these live here
// rather than being redefined per page with slightly different values.
constexpr Color UiBg      = {  22,  24,  34, 255 };
constexpr Color UiPanel   = {  38,  41,  56, 255 };
constexpr Color UiAccent  = { 255, 205, 110, 255 };
constexpr Color UiDim     = { 150, 156, 176, 255 };
constexpr Color UiSlotCol = {  48,  52,  70, 255 };
constexpr Color UiInk     = { 235, 238, 245, 255 };

// Green, and the only colour on either page that ever means "you can act now".
// Points to spend, an entry the cursor is on, a floor that is finally cleared.
constexpr Color UiReady   = { 130, 225, 140, 255 };

// A control that is drawn but cannot be used. Greyed rather than hidden, because an
// entry that appears and disappears is one the player has to notice twice.
constexpr Color UiOff     = {  95, 100, 112, 255 };

//----------------------------------------------------------------------------------
// One pointer, as a page sees it.
//
// `justOpened` is true on the frame the page appeared, and exists so the click that
// OPENED a page cannot also be counted as a click on whatever happens to be under the
// cursor inside it.
//
// `clicked` is the down-EDGE and is what a control acts on; `down` is what a control
// looks pressed by. They are separate because a button must fire once and look
// pressed for as long as the button is held, and one flag cannot do both.
//----------------------------------------------------------------------------------
struct UiInput
{
    Vector2 pos{};
    bool clicked = false;
    bool down = false;
    bool justOpened = false;

    // Reads the mouse. `opened` is passed straight through to justOpened.
    static UiInput Read(bool opened);

    bool Over(Rectangle r) const;
};

//----------------------------------------------------------------------------------
// How many screen pixels a DESIGN pixel is worth right now.
//
// screenHeight over Config::UiDesignHeight, and it is the one number the whole HUD
// is written against. Every size on screen is a design figure times this, so the HUD
// keeps its proportions from a small window to a 1440p monitor instead of shrinking
// into a hairline on the second - which is exactly what a HUD written in raw pixels
// does, because there is no raw number that is right on both.
//
// Height and not width, deliberately. A 21:9 monitor is not a screen with a bigger
// HUD on it, it is a screen with more room beside the same HUD, and scaling by width
// would grow every bar by a third the moment the window got wider.
//
// Read per frame rather than cached. The window is resizable and a cached scale is
// one alt-tab away from laying the corner out for a size the window no longer is.
//----------------------------------------------------------------------------------
float UiScale();

//----------------------------------------------------------------------------------
// The page itself: the world blacked out, and a fitted layout scale to draw on.
//
// UiPageScale answers the only question a fullscreen page has - how big is a design
// pixel here - by fitting `designHeight` into the window with a margin. It never
// returns more than `maxScale`, because a page designed at 450px tall stretched onto
// a 1440p monitor at 3.2x is four controls the size of dinner plates.
//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
// The dark plate every fullscreen page lies on.
//
// `alpha` is how much of what is BEHIND it survives, and the two callers want very
// different answers. A page over a live world - the pause menu, the shop, the
// character sheet - wants the world almost gone: it is a modal page and a legible
// dungeon behind it reads as something the player could still be acting on. A page
// over the FRONT END wants the opposite, because what is behind it there is the menu
// sky (see ui/MenuBackdrop.h), which is the only picture those pages have.
//
// The default is the modal one, so every in-game page keeps exactly what it had.
//----------------------------------------------------------------------------------
void UiPageBackdrop(float alpha = 0.965f, Color colour = UiBg);

// What a front-end page passes: enough to read text against, little enough that the
// sky behind it is still a picture rather than a tint.
constexpr float UiFrontBackdrop = 0.52f;

//----------------------------------------------------------------------------------
// ...and in a warm dark rather than UiBg's blue.
//
// The plate is doing half the colour work on the front end, which is not obvious
// until it is wrong: the menu sky is tinted red (see ui/MenuBackdrop.h) and then
// half covered by a dark BLUE sheet, which pulls the whole thing back to the neutral
// grey it was trying to leave. Warming the plate as well is what makes the two agree.
//
// Still dark enough to read white text against - it is the same value as UiBg, only
// rotated toward the red the dungeon's own sky is.
//----------------------------------------------------------------------------------
constexpr Color UiFrontBg = { 34, 18, 20, 255 };
float UiPageScale(float designHeight, float maxScale);

// A panel: a filled plate with a hairline edge, the shape every block on a page is.
void UiPanelBox(Rectangle r, float ls, Color fill, Color edge);

// Text with a drop shadow, so it stays readable over a panel or over the world.
void UiLabel(const char *text, float x, float y, float size, Color c);
void UiLabelRight(const char *text, float right, float y, float size, Color c);
void UiLabelCentered(const char *text, float cx, float y, float size, Color c);

// A filled rectangular button with centred text. True on the frame it is clicked, and
// only when `enabled` - a disabled button still draws, greyed.
bool UiButton(Rectangle r, bool enabled, float ls, const UiInput &in,
              const char *text, Color fill, Color textCol);

// A small square button holding one glyph - the [+] of a stat row.
bool UiGlyphButton(Rectangle r, bool enabled, float ls, const UiInput &in,
                   const char *glyph, Color fill);

// A row: a plate with a title along the left and a value along the right. What every
// stat and every menu entry is made of.
void UiRow(Rectangle r, float ls, bool highlighted, Color accent);

// A fullscreen black overlay at `alpha` (0 clear, 1 opaque) - what every fade
// between screens is drawn out of, whether that is a menu transition (see
// core/Fader.h), the portal dwell going dark, or the player's own death.
void UiFadeOverlay(float alpha);
