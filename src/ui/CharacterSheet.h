#pragma once

#include "combat/Stats.h"
#include "progress/Traits.h"
#include "raylib.h"

class Player;

//----------------------------------------------------------------------------------
// Where points are spent, and where traits are worn.
//
// A whole SCREEN rather than a panel floating in the middle of one, and it takes the
// mouse back to do it. Spending a point is the only decision in the game that is not
// made with the crosshair, and trying to make it with one - a key per stat, a readout
// in the debug box - turns a build into a keyboard shortcut nobody can see the effect
// of.
//
// It PAUSES. The world does not tick while it is up, which is what makes it safe to
// open in the middle of a floor: the alternative is a screen the player can only read
// once the room is clear, which is exactly when the decision matters least.
//
// --- Why fullscreen ----------------------------------------------------------------
// A 460x300 panel over a live dungeon was legible and quietly wrong. The dungeon
// carried on LOOKING like a thing that was happening, so the panel read as an overlay
// the player could play through, and the one decision on it competed for attention
// with a room full of skeletons that were not moving. A page that takes the window
// tells the truth: the game has stopped, and this is the only thing to do.
//
// --- What it shows -----------------------------------------------------------------
// The four stats and, beside each, the number that stat actually moves. That pairing
// is the whole design - a row that says "ARMS 14" and nothing else is asking the
// player to trust a rate they have never seen. The right hand column is what changes
// when they press the button, so the button is a thing they can reason about.
//
// Below them, the trait slots. Traits are BOUGHT from the captain and WORN here, and
// that split is deliberate: the counter is where contracts are spent and this is
// where the build is decided, and swapping between traits already owned is free. A
// trait is a build decision, and charging to undo one turns experimenting into a
// punishment.
//
// The purse is on this page too, and only on this page. A shop shows the one currency
// it accepts - three numbers over a counter that takes one of them is two numbers of
// noise - so somewhere has to show all three, and this is the screen that is about
// the run rather than about a transaction.
//----------------------------------------------------------------------------------
class CharacterSheet
{
public:
    // Toggled by the caller, which owns the key binding. Opening releases the cursor
    // and closing takes it back - the caller does that, because the same cursor is
    // the camera's and the page has no business knowing that.
    void Toggle();
    bool IsOpen() const { return open; }
    void Close() { open = false; }

    // Reads the mouse, spends points and moves traits. Only call while open.
    void Update(Player &player, TraitLoadout &traits);

    // Screen space, after EndMode3D
    void Draw(const Player &player, const TraitLoadout &traits) const;

private:
    //------------------------------------------------------------------------------
    // Everything on the page, in screen pixels, worked out from the window.
    //
    // One struct built by one function, read by both Update and Draw. The layout was
    // a scattering of constants and two rectangle helpers before, which is the one
    // bug this class can have: a button drawn in one place and clicked in another.
    // Now there is a single answer, and it is impossible for the two passes to
    // disagree about it.
    //
    // Measured every frame rather than cached. The window is resizable, and a cached
    // layout is one alt-tab away from being wrong.
    //------------------------------------------------------------------------------
    struct Layout
    {
        float ls = 1.0f;            // Design pixels to screen pixels

        Rectangle page{};           // The content column, centred in the window
        Rectangle rows[(int)Stat::Count]{};
        Rectangle plus[(int)Stat::Count]{};

        // The four trait slots, side by side, and the list of owned traits that
        // opens under them when one is picked.
        Rectangle slots[TraitSlots]{};
        Rectangle list{};
        float listRowHeight = 0.0f;
        int listVisible = 0;

        Rectangle respec{};
        Rectangle close{};

        float derivedX = 0.0f;      // Where the right hand column starts
        float titleY = 0.0f;
        float pointsY = 0.0f;
        float traitsY = 0.0f;       // The TRAITS heading
        float purseY = 0.0f;

        Rectangle ListRowAt(int slot) const;
    };

    Layout Measure() const;

    // Everything the player owns that could go in the open slot, in table order.
    // Rebuilt per frame from the loadout for the same reason the shop's rows are.
    void BuildPickable(const TraitLoadout &traits, int owned[MaxTraits], int &count) const;

    bool open = false;

    // Which trait slot's list is open, or -1. A slot rather than a trait: the list is
    // "what could go HERE", and a player who has clicked a slot has already decided
    // which one they are filling.
    int picking = -1;

    // How far the pick list is scrolled. Clamped against the count every frame, since
    // selling a trait shortens it under the cursor.
    int scroll = 0;

    // True on the frame the page appeared, so the click that OPENED it cannot also
    // be counted as a click on whatever is under the cursor inside it.
    bool justOpened = false;
};
