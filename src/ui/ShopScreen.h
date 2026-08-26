#pragma once

#include "raylib.h"
#include "world/Npc.h"

#include <vector>

class Player;
class Arsenal;
class Spellbook;
class TraitLoadout;

//----------------------------------------------------------------------------------
// The vendor's counter: one page, three shops.
//
// The same kind of object as the character page - a fullscreen page over a stopped
// world, laid out at a fitted scale, driven by the mouse - and it shares that page's
// controls and palette (see UiTheme.h) so the two read as one game.
//
// ONE page for all three vendors rather than three. What changes between them is the
// list, the currency and the accent colour; the counter itself is identical, and three
// copies of it would be three places to fix the same bug.
//
//     MERCHANT   weapons + forge levels   <-> coins
//     MYSTIC     schools + empower levels <-> gems
//     CAPTAIN    traits, bought and sold  <-> contracts
//
// --- Buying is not equipping ---------------------------------------------------------
// The counter only moves things into and out of the player's OWNED set. What is in
// which hand, which school is selected, and which trait is in which slot are all
// decided elsewhere - the wheel, the number keys, and the character page. That split
// is what keeps this a counter: the player buys a sword here and decides whether it
// beats the one they are holding on the screen that shows them both.
//
// --- No rolled stock -----------------------------------------------------------------
// The mobile game deals each vendor three random offers per area. This does not: it
// lists everything the vendor has, and the list is short enough to read. Rolled stock
// makes sense when the table is a hundred rows and a run is five areas; here it would
// only mean a player who wants a specific weapon has to descend until it appears,
// which is a slot machine rather than a decision.
//----------------------------------------------------------------------------------
class ShopScreen
{
public:
    // Opened by the caller, which owns the interact key and knows which vendor the
    // player is standing at.
    void Open(NpcKind vendor);
    void Close() { open = false; }
    bool IsOpen() const { return open; }

    NpcKind Vendor() const { return vendor; }

    // Reads the mouse and moves things across the counter. Only call while open.
    void Update(Player &player, Arsenal &arsenal, Spellbook &spells, TraitLoadout &traits);

    // Screen space, after EndMode3D
    void Draw(const Player &player, const Arsenal &arsenal, const Spellbook &spells,
              const TraitLoadout &traits) const;

private:
    //------------------------------------------------------------------------------
    // What one row of the list offers.
    //
    // Built fresh every frame from the three loadouts rather than cached, because a
    // purchase changes what the row underneath the cursor says - a cached list would
    // show the player buying a weapon they already own for one frame, which is
    // exactly the frame they are looking at.
    //
    // `deal` is what the button does. It is an enum rather than a function pointer so
    // that Draw and Update can build the identical list and only one of them acts on
    // it - the same rule the character page's layout follows, and for the same
    // reason: a button drawn in one place and clicked in another is the one bug this
    // class can have.
    //------------------------------------------------------------------------------
    enum class Deal { None, Buy, Upgrade, Sell, Respec };

    struct Row
    {
        const char *name = "";
        const char *detail = "";        // What it does, or what the next level buys
        const char *note = "";          // Owned / forged / the reason it is refused

        Color tint = WHITE;

        Deal deal = Deal::None;
        int id = 0;                     // Weapon index, Magic, or trait id
        int price = 0;

        bool enabled = false;           // Affordable, and there is something to do
    };

    // Everything the vendor has, in list order. Static-lifetime strings only - the
    // rows hold pointers into TextFormat's ring buffer, which survives long enough
    // for one frame's draw and no longer.
    void BuildRows(const Player &player, const Arsenal &arsenal, const Spellbook &spells,
                   const TraitLoadout &traits, std::vector<Row> &rows) const;

    struct Layout
    {
        float ls = 1.0f;

        Rectangle page{};
        Rectangle list{};               // The scrolling area
        Rectangle close{};

        float rowHeight = 0.0f;
        int visible = 0;                // Rows that fit in `list`

        Rectangle RowAt(int slot) const;
    };

    Layout Measure() const;

    // How far down the list is scrolled, in rows. Clamped against the row count every
    // frame rather than when it changes, because the count moves under it: selling a
    // trait shortens the captain's list, and a scroll left past the end would show an
    // empty page the player has no obvious way out of.
    int ClampScroll(int rowCount, int visible) const;

    bool open = false;
    NpcKind vendor = NpcKind::Merchant;

    int scroll = 0;

    // See CharacterSheet.h - the click that opened the page is not a click in it.
    bool justOpened = false;
};
