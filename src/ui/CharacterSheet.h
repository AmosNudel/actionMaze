#pragma once

#include "combat/Stats.h"
#include "progress/Traits.h"
#include "raylib.h"

class Player;
class Arsenal;
class Spellbook;
class WeaponPreview;

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
// Three tabs. STATS is the original page: the four stats and, beside each, the
// number that stat actually moves - a row that says "ARMS 14" and nothing else is
// asking the player to trust a rate they have never seen. Below them, the trait
// slots - traits are BOUGHT from the captain and WORN here, and swapping between
// ones already owned is free, because a trait is a build decision and charging to
// undo one turns experimenting into a punishment.
//
// INVENTORY and MAGIC are read-only reference pages beside it: every weapon owned
// and what it does (damage, reach, forge level, its tags), and every school owned
// and what its signature effect actually is - the answer to "what does casting this
// do" that used to live only in code comments. Equipping still happens on the mouse
// wheel and casting still happens on the number keys; these two tabs are where you
// go to check what you are carrying before you decide to change it.
//
// The purse is on this page too, and only on this page, on every tab rather than
// one of them - it is not "the tab about currency", it is a fact about the run that
// belongs on the screen that is about the run. A shop shows the one currency it
// accepts; three numbers over a counter that takes one of them is two numbers of
// noise, so somewhere has to show all three.
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

    // Reads the mouse, spends points, moves traits, and switches tabs. Only call
    // while open. Arsenal and Spellbook are read-only here - Inventory and Magic
    // are reference tabs, nothing on them is bought or equipped.
    void Update(Player &player, const Arsenal &arsenal, const Spellbook &spells,
               TraitLoadout &traits);

    // Screen space, after EndMode3D. `preview` draws the Inventory tab's rotating
    // weapon icons - see render/WeaponPreview.h - and is mutable despite this
    // being a const method for the same reason ShopScreen::Draw's is: it owns a
    // render target it redraws every row, and nothing about the sheet's own
    // state changes because of it.
    void Draw(const Player &player, const Arsenal &arsenal, const Spellbook &spells,
             const TraitLoadout &traits, WeaponPreview &preview) const;

private:
    enum class Tab { Stats, Inventory, Magic };
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

        // The three tabs, always drawn regardless of which is active - a tab you
        // cannot see is a tab you do not know exists.
        Rectangle tabs[3]{};

        Rectangle rows[(int)Stat::Count]{};
        Rectangle plus[(int)Stat::Count]{};

        // The four trait slots, side by side, and the list of owned traits that
        // opens under them when one is picked.
        Rectangle slots[TraitSlots]{};
        Rectangle list{};
        float listRowHeight = 0.0f;
        int listVisible = 0;

        //--------------------------------------------------------------------------
        // The big scrollable area Inventory and Magic each use for their one list
        // of rows. Spans the same vertical band the Stats tab fills with its stat
        // rows and trait block - the three tabs are the same page at three
        // different things to show, not three different page shapes.
        //--------------------------------------------------------------------------
        Rectangle content{};
        float contentRowHeight = 0.0f;
        int contentVisible = 0;

        Rectangle cancel{};
        Rectangle confirm{};
        Rectangle close{};

        float derivedX = 0.0f;      // Where the right hand column starts
        float titleY = 0.0f;
        float pointsY = 0.0f;
        float traitsY = 0.0f;       // The TRAITS heading
        float purseY = 0.0f;

        Rectangle ListRowAt(int slot) const;
        Rectangle ContentRowAt(int slot) const;
    };

    Layout Measure() const;

    // Everything the player owns that could go in the open slot, in table order.
    // Rebuilt per frame from the loadout for the same reason the shop's rows are.
    void BuildPickable(const TraitLoadout &traits, int owned[MaxTraits], int &count) const;

    // The Inventory and Magic tabs. Read-only reference lists, so unlike the Stats
    // tab's rows these take no UiInput and act on nothing - Draw is their whole
    // job.
    void DrawInventoryTab(const Layout &page, const Arsenal &arsenal, WeaponPreview &preview) const;
    void DrawMagicTab(const Layout &page, const Player &player, const Spellbook &spells) const;

    bool open = false;

    // Which tab is showing. Reset to Stats on open - see Toggle - so the page
    // never opens onto whichever reference tab was last read.
    Tab tab = Tab::Stats;

    // Which trait slot's list is open, or -1. A slot rather than a trait: the list is
    // "what could go HERE", and a player who has clicked a slot has already decided
    // which one they are filling.
    int picking = -1;

    // How far the current tab's list is scrolled - the trait pick list on Stats,
    // or the Inventory/Magic content list on theirs. Shared rather than one field
    // per list because exactly one of them is ever visible at a time, and reset to
    // 0 on every tab switch so a scrolled Inventory list does not leave Magic
    // opening halfway down. Clamped against the count every frame, since selling a
    // trait shortens the Stats list under the cursor.
    int scroll = 0;

    // True on the frame the page appeared, so the click that OPENED it cannot also
    // be counted as a click on whatever is under the cursor inside it.
    bool justOpened = false;

    //------------------------------------------------------------------------------
    // What has been spent THIS SESSION - since the page last opened, or since the
    // last CONFIRM or CANCEL, whichever was more recent.
    //
    // A point still goes into Player::stats the moment its [+] is clicked - see
    // the note on Player::RevertPoints for why it has to, the same reason
    // SpendPoint always has. This is a second, parallel record of exactly that
    // click's worth, kept only so CANCEL has something precise to give back:
    // without it, cancelling could only mean a full RespecStats, which throws
    // away a build the player was happy with along with the level they just
    // second-guessed.
    //
    // Not persisted anywhere outside this page. Closing the sheet without an
    // explicit CONFIRM or CANCEL leaves the points spent - the page is a place to
    // decide, not a lock on the character between visits.
    //------------------------------------------------------------------------------
    StatBlock pending{};
    int pendingCount = 0;    // Sum of `pending`, so the buttons know at a glance whether there is anything to do
};
