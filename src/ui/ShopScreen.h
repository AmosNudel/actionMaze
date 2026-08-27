#pragma once

#include "raylib.h"
#include "world/Npc.h"

#include <string>
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
// --- Limited stock -------------------------------------------------------------------
// Unlike an early version of this counter, stock IS rolled: what is UNOWNED is
// limited to a small offered subset that rerolls every floor (see
// Arsenal::RerollOffers and its neighbours on Spellbook and TraitLoadout), so a
// vendor is worth checking again on the way to the next one rather than a list the
// player has already read in full. Nothing OWNED is ever gated by it - an owned
// weapon's Upgrade row and a worn trait's Sell row show every time regardless of
// what this floor happens to be offering.
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

    //--------------------------------------------------------------------------
    // Owned strings, not pointers into raylib's TextFormat ring.
    //
    // TextFormat's buffer is a hard-capped ring of 4 (MAX_TEXTFORMAT_BUFFERS in
    // raylib's rcore.c), and BuildRows calls it once or twice per row for as many
    // as nineteen weapons or eight schools in one pass. A `const char *` alias
    // into that ring is only good until the FIFTH call after it was made, so row
    // 1's pointer was already pointing at row 5's text by the time this list had
    // finished building - which is exactly the "wrong string next to the row"
    // bug this was rewritten to fix. std::string copies the text out immediately,
    // so each row owns its own.
    //--------------------------------------------------------------------------
    struct Row
    {
        std::string name;
        std::string detail;             // What it does, or what the next level buys
        std::string note;               // Owned / forged / the reason it is refused

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
