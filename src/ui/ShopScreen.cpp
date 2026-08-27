#include "ui/ShopScreen.h"

#include "combat/Magic.h"
#include "combat/Weapon.h"
#include "entities/Player.h"
#include "progress/Arsenal.h"
#include "progress/Spellbook.h"
#include "progress/Traits.h"
#include "ui/UiText.h"
#include "ui/UiTheme.h"

namespace
{
    // Design pixels, fitted to the window - see the note in CharacterSheet.cpp
    constexpr float DesignHeight = 500.0f;
    constexpr float DesignWidth  = 720.0f;
    constexpr float MaxScale     = 2.4f;

    constexpr float TitleSize = 40.0f;
    constexpr float HeadSize  = 17.0f;
    constexpr float RowSize   = 19.0f;
    constexpr float NoteSize  = 13.0f;

    constexpr float RowHeight = 52.0f;
    constexpr float RowGap    = 6.0f;
    constexpr float RowPad    = 14.0f;
    constexpr float ButtonW   = 130.0f;
    constexpr float ButtonH   = 34.0f;

    constexpr float TitleTop  = 8.0f;
    constexpr float ListTop   = 96.0f;
    constexpr float FooterH   = 44.0f;

    // What a service row uses as its id, so it cannot collide with a table index.
    // Negative because every real id in every table is zero or above.
    constexpr int RespecId = -1;
}

void ShopScreen::Open(NpcKind which)
{
    open = true;
    vendor = which;
    scroll = 0;
    justOpened = true;
}

ShopScreen::Layout ShopScreen::Measure() const
{
    Layout out;

    out.ls = UiPageScale(DesignHeight, MaxScale);

    const float screenW = (float)GetScreenWidth();
    const float screenH = (float)GetScreenHeight();

    float width = DesignWidth*out.ls;
    const float widest = screenW*0.92f;

    if (width > widest) width = widest;

    const float height = DesignHeight*out.ls;

    out.page = { (screenW - width)*0.5f, (screenH - height)*0.5f, width, height };

    out.rowHeight = RowHeight*out.ls;

    const float listTop = out.page.y + ListTop*out.ls;
    const float listBottom = out.page.y + out.page.height - FooterH*out.ls;

    out.list = { out.page.x, listTop, out.page.width, listBottom - listTop };

    // How many whole rows fit. A partial row at the bottom is worse than a gap: the
    // player cannot tell a clipped row from one that has nothing left to say.
    const float step = out.rowHeight + RowGap*out.ls;

    out.visible = (step > 0.0f) ? (int)(out.list.height/step) : 0;
    if (out.visible < 1) out.visible = 1;

    out.close = { out.page.x + out.page.width - 180.0f*out.ls,
                  out.page.y + out.page.height - ButtonH*out.ls,
                  180.0f*out.ls, ButtonH*out.ls };

    return out;
}

Rectangle ShopScreen::Layout::RowAt(int slot) const
{
    const float step = rowHeight + RowGap*ls;

    return { list.x, list.y + slot*step, list.width, rowHeight };
}

int ShopScreen::ClampScroll(int rowCount, int visible) const
{
    const int most = rowCount - visible;

    if (most <= 0) return 0;
    if (scroll < 0) return 0;
    if (scroll > most) return most;

    return scroll;
}

//----------------------------------------------------------------------------------
// Everything the vendor has, in list order.
//
// One function for all three, because the three lists are the same shape: a thing,
// what it does, and one button whose meaning depends on whether the player already
// owns it. What differs is only which table is walked.
//
// The rows hold pointers into TextFormat's ring buffer, which is why this is rebuilt
// every frame and why nothing outside one frame may hold a Row.
//----------------------------------------------------------------------------------
void ShopScreen::BuildRows(const Player &player, const Arsenal &arsenal,
                           const Spellbook &spells, const TraitLoadout &traits,
                           std::vector<Row> &rows) const
{
    rows.clear();

    const Purse &purse = player.purse;

    switch (vendor)
    {
        //--------------------------------------------------------------------------
        // The merchant: every weapon, bought or forged.
        //
        // Unowned first, then owned - so the half of the list the player came here
        // for is the half they can see without scrolling. Within each half the
        // table's own order is kept, which is the order the wheel cycles in, so a
        // weapon's place in the shop is where it will be in the hand.
        //--------------------------------------------------------------------------
        case NpcKind::Merchant:
        {
            for (int pass = 0; pass < 2; ++pass)
            {
                for (int i = 0; i < arsenal.Count(); ++i)
                {
                    const bool owns = arsenal.Owns(i);

                    if (owns != (pass == 1)) continue;

                    // Limited stock: an unowned weapon the counter is not currently
                    // selling simply is not in the list this floor. An owned one is
                    // never gated by this - see the note on Arsenal::IsOffered.
                    if (!owns && !arsenal.IsOffered(i)) continue;

                    Row row;

                    // The tags travel on every row regardless of deal, so a
                    // weapon reads as "1H  CAST" whether it is being sold,
                    // forged, or already maxed out.
                    const std::string tagText = WeaponTagsText(arsenal.TagsAt(i));

                    row.name = arsenal.NameAt(i);
                    row.id = i;
                    row.tint = UiAccent;

                    if (!owns)
                    {
                        row.deal = Deal::Buy;
                        row.price = arsenal.Price(i);
                        row.detail = tagText.empty() ? "not owned" : ("not owned    " + tagText);
                        row.enabled = purse.CanAfford(Currency::Coins, row.price);
                        row.note = row.enabled ? "" : "not enough coins";
                    }
                    else if (arsenal.CanForge(i))
                    {
                        row.deal = Deal::Upgrade;
                        row.price = arsenal.ForgePrice(i);
                        row.detail = TextFormat("forged %i / %i    +%.0f%% damage, +%i arms    %s",
                                                arsenal.Forge(i), WeaponForgeMax,
                                                (arsenal.DamageMult(i) - 1.0f)*100.0f,
                                                arsenal.Forge(i)*WeaponForgeArms, tagText.c_str());
                        row.enabled = purse.CanAfford(Currency::Coins, row.price);
                        row.note = row.enabled ? "" : "not enough coins";
                    }
                    else
                    {
                        row.deal = Deal::None;
                        row.detail = TextFormat("forged %i / %i    +%.0f%% damage, +%i arms    %s",
                                                WeaponForgeMax, WeaponForgeMax,
                                                (arsenal.DamageMult(i) - 1.0f)*100.0f,
                                                arsenal.Forge(i)*WeaponForgeArms, tagText.c_str());
                        row.note = "fully forged";
                    }

                    rows.push_back(row);
                }
            }

            break;
        }

        //--------------------------------------------------------------------------
        // The mystic: every school, bought or empowered.
        //
        // Table order rather than owned-last, because there are only eight and the
        // number keys index them - a list that reordered itself would break the one
        // thing tying the shop to the key the player presses to cast.
        //--------------------------------------------------------------------------
        case NpcKind::Mystic:
        {
            for (int i = 0; i < (int)Magic::Count; ++i)
            {
                const Magic magic = (Magic)i;
                const MagicDef &def = MagicAt(magic);

                // Limited stock - see the note on the merchant's loop above.
                if (!spells.Owns(magic) && !spells.IsOffered(magic)) continue;

                Row row;

                row.name = TextFormat("%i  %s", i + 1, def.name);
                row.id = i;
                row.tint = def.colour;

                if (!spells.Owns(magic))
                {
                    row.deal = Deal::Buy;
                    row.price = spells.Price(magic);
                    row.detail = TextFormat("x%.2f spell power    %i mana a cast",
                                            def.damageMult, spells.CostOf(magic, player.Mods()));
                    row.enabled = purse.CanAfford(Currency::Gems, row.price);
                    row.note = row.enabled ? "" : "not enough gems";
                }
                else if (spells.CanEmpower(magic))
                {
                    row.deal = Deal::Upgrade;
                    row.price = spells.EmpowerPrice(magic);
                    row.detail = TextFormat("empowered %i / %i    x%.2f damage, %i mana a cast",
                                            spells.Empower(magic), SpellEmpowerMax,
                                            def.damageMult*spells.DamageMult(magic),
                                            spells.CostOf(magic, player.Mods()));
                    row.enabled = purse.CanAfford(Currency::Gems, row.price);
                    row.note = row.enabled ? "" : "not enough gems";
                }
                else
                {
                    row.deal = Deal::None;
                    row.detail = TextFormat("empowered %i / %i    x%.2f damage",
                                            SpellEmpowerMax, SpellEmpowerMax,
                                            def.damageMult*spells.DamageMult(magic));
                    row.note = "fully empowered";
                }

                rows.push_back(row);
            }

            break;
        }

        //--------------------------------------------------------------------------
        // The captain: traits, and the respec.
        //
        // The respec goes FIRST and is free. It is free everywhere else in this game
        // already - the character page has always offered it at no cost, on the
        // grounds that the rates are new and a player who cannot undo a spend is
        // being asked to commit to numbers that have not settled - and a captain who
        // charged for the same button would be two prices for one action.
        //
        // Selling a trait back pays half. That is a way out of a build you have
        // changed your mind about, not an arbitrage loop: buying and selling the same
        // row loses contracts, so there is nothing to farm.
        //--------------------------------------------------------------------------
        default:
        {
            Row respec;

            respec.name = "RESPEC";
            respec.detail = "every spent point back in the pool";
            respec.note = "free, any time";
            respec.tint = UiReady;
            respec.deal = Deal::Respec;
            respec.id = RespecId;
            respec.price = 0;
            respec.enabled = true;

            rows.push_back(respec);

            for (int i = 0; i < TraitCount(); ++i)
            {
                const TraitDef &def = TraitAt(i);

                // Limited stock - see the note on the merchant's loop above. RESPEC
                // above is never gated by this; it is not a table row.
                if (!traits.Owns(i) && !traits.IsOffered(i)) continue;

                Row row;

                row.name = def.name;
                row.detail = def.desc;
                row.id = i;
                row.tint = def.colour;

                if (!traits.Owns(i))
                {
                    row.deal = Deal::Buy;
                    row.price = def.price;
                    row.enabled = purse.CanAfford(Currency::Contracts, row.price);
                    row.note = row.enabled ? "" : "not enough contracts";
                }
                else
                {
                    row.deal = Deal::Sell;
                    row.price = (def.price*TraitSellPercent)/100;
                    row.enabled = true;
                    row.note = traits.IsEquipped(i) ? "owned, worn" : "owned";
                }

                rows.push_back(row);
            }

            break;
        }
    }
}

void ShopScreen::Update(Player &player, Arsenal &arsenal, Spellbook &spells,
                        TraitLoadout &traits)
{
    if (!open) return;

    const UiInput in = UiInput::Read(justOpened);

    justOpened = false;

    const Layout page = Measure();

    std::vector<Row> rows;

    BuildRows(player, arsenal, spells, traits, rows);

    // The wheel scrolls the list. Read before the click, so a frame that does both
    // acts on the row the player was looking at rather than the one that slid under
    // the cursor.
    const float wheel = GetMouseWheelMove();

    if (wheel != 0.0f) scroll -= (wheel > 0.0f) ? 1 : -1;

    scroll = ClampScroll((int)rows.size(), page.visible);

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) scroll++;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) scroll--;

    scroll = ClampScroll((int)rows.size(), page.visible);

    if (!in.clicked) return;

    if (in.Over(page.close)) { open = false; return; }

    for (int slot = 0; slot < page.visible; ++slot)
    {
        const int index = scroll + slot;

        if (index >= (int)rows.size()) break;

        const Row &row = rows[(size_t)index];

        if (!row.enabled) continue;
        if (!in.Over(page.RowAt(slot))) continue;

        //----------------------------------------------------------------------
        // The one place anything crosses the counter.
        //
        // Every branch pays FIRST and delivers second. Spend returns whether the
        // purse actually had it, so a row whose price moved between the build and
        // the click cannot hand over a weapon for nothing.
        //----------------------------------------------------------------------
        switch (row.deal)
        {
            case Deal::Buy:
                if (vendor == NpcKind::Merchant)
                {
                    if (player.purse.Spend(Currency::Coins, row.price)) arsenal.Give(row.id);
                }
                else if (vendor == NpcKind::Mystic)
                {
                    if (player.purse.Spend(Currency::Gems, row.price)) spells.Give((Magic)row.id);
                }
                else
                {
                    if (player.purse.Spend(Currency::Contracts, row.price)) traits.Give(row.id);
                }
                break;

            case Deal::Upgrade:
                if (vendor == NpcKind::Merchant)
                {
                    if (player.purse.Spend(Currency::Coins, row.price)) arsenal.RaiseForge(row.id);
                }
                else
                {
                    if (player.purse.Spend(Currency::Gems, row.price))
                    {
                        spells.RaiseEmpower((Magic)row.id);
                    }
                }
                break;

            case Deal::Sell:
                // Take it off the character before paying for it. Take() unequips as
                // well as un-owns, and a trait paid for while still worn would go on
                // granting its bonus until something else happened to recompute.
                traits.Take(row.id);
                player.purse.Add(Currency::Contracts, row.price);
                break;

            case Deal::Respec:
                player.RespecStats();
                break;

            default:
                break;
        }

        // One row per click, whatever it was. A click that fell through to a second
        // row would spend twice on a list that has just changed shape underneath it.
        return;
    }
}

void ShopScreen::Draw(const Player &player, const Arsenal &arsenal, const Spellbook &spells,
                      const TraitLoadout &traits) const
{
    if (!open) return;

    const Layout page = Measure();
    const float ls = page.ls;

    const UiInput in = UiInput::Read(false);

    const NpcDef &def = NpcAt(vendor);

    std::vector<Row> rows;

    BuildRows(player, arsenal, spells, traits, rows);

    const int at = ClampScroll((int)rows.size(), page.visible);

    UiPageBackdrop();

    //------------------------------------------------------------------------------
    // Who this is, what they deal in, and what the player has to deal with.
    //
    // The purse line shows only THIS vendor's currency. Three numbers across the top
    // of a shop that only accepts one of them is two numbers of noise beside the
    // answer - and the character page is where the whole purse is read.
    //------------------------------------------------------------------------------
    UiLabel(def.name, page.page.x, page.page.y + TitleTop*ls, TitleSize*ls, def.colour);

    UiLabel(def.title, page.page.x,
            page.page.y + (TitleTop + TitleSize + 4.0f)*ls, HeadSize*ls, UiDim);

    const int held = player.purse.Amount(def.currency);

    UiLabelRight(TextFormat("%i %s", held, CurrencyName(def.currency)),
                 page.page.x + page.page.width,
                 page.page.y + (TitleTop + TitleSize*0.32f)*ls, HeadSize*ls,
                 (held > 0) ? def.colour : UiDim);

    const float ruleY = page.page.y + (ListTop - 10.0f)*ls;

    DrawRectangleRec({ page.page.x, ruleY, page.page.width, 1.0f*ls }, Fade(UiDim, 0.45f));

    //------------------------------------------------------------------------------
    // The list.
    //
    // A row is its name, what it does, and one button. The button carries the price,
    // so there is no separate price column that can fall out of step with what the
    // click actually charges - the two are the same string built from the same field.
    //------------------------------------------------------------------------------
    for (int slot = 0; slot < page.visible; ++slot)
    {
        const int index = at + slot;

        if (index >= (int)rows.size()) break;

        const Row &row = rows[(size_t)index];
        const Rectangle box = page.RowAt(slot);

        const bool hovered = in.Over(box) && row.enabled;

        UiRow(box, ls, hovered, row.tint);

        // The row's colour as a tab down its left edge, the same idiom the character
        // page uses. It is a thing to find a row by rather than to read.
        DrawRectangleRec({ box.x, box.y, 4.0f*ls, box.height }, row.tint);

        UiLabel(row.name.c_str(), box.x + (RowPad + 8.0f)*ls, box.y + 8.0f*ls, RowSize*ls, UiInk);

        UiLabel(row.detail.c_str(), box.x + (RowPad + 8.0f)*ls,
                box.y + (8.0f + RowSize + 3.0f)*ls, NoteSize*ls, UiDim);

        const Rectangle button = { box.x + box.width - (ButtonW + RowPad)*ls,
                                   box.y + (box.height - ButtonH*ls)*0.5f,
                                   ButtonW*ls, ButtonH*ls };

        if (row.deal == Deal::None)
        {
            // Nothing left to do with this one. The note is drawn where the button
            // would be, so the list keeps its shape rather than growing a ragged
            // right edge as things are bought.
            UiLabelRight(row.note.c_str(), button.x + button.width,
                         button.y + (button.height - NoteSize*ls)*0.5f, NoteSize*ls, UiOff);

            continue;
        }

        const char *label = (row.deal == Deal::Buy)     ? TextFormat("BUY   %i", row.price)
                          : (row.deal == Deal::Upgrade) ? TextFormat("%s   %i",
                                (vendor == NpcKind::Merchant) ? "FORGE" : "EMPOWER", row.price)
                          : (row.deal == Deal::Sell)    ? TextFormat("SELL   %i", row.price)
                          : "RESPEC";

        UiButton(button, row.enabled, ls, in, label, UiPanel, row.tint);

        // Why it is refused, beside the button rather than instead of it. A greyed
        // button with no reason next to it is a rule the player has to guess.
        if (!row.enabled && !row.note.empty())
        {
            UiLabelRight(row.note.c_str(), button.x - 10.0f*ls,
                         button.y + (button.height - NoteSize*ls)*0.5f, NoteSize*ls, UiOff);
        }
        else if (row.enabled && !row.note.empty())
        {
            UiLabelRight(row.note.c_str(), button.x - 10.0f*ls,
                         button.y + (button.height - NoteSize*ls)*0.5f, NoteSize*ls, UiDim);
        }
    }

    //------------------------------------------------------------------------------
    // The footer: how far down the list this is, and the way out.
    //
    // The scroll readout is a count rather than a bar. A bar on a list of eight is a
    // decoration; "9 - 16 of 21" is the thing the player actually wants to know,
    // which is whether there is more.
    //------------------------------------------------------------------------------
    const float footerY = page.close.y + (page.close.height - NoteSize*ls)*0.5f;

    if ((int)rows.size() > page.visible)
    {
        const int last = at + page.visible;

        UiLabel(TextFormat("%i - %i of %i     wheel scrolls", at + 1,
                           (last < (int)rows.size()) ? last : (int)rows.size(),
                           (int)rows.size()),
                page.page.x, footerY, NoteSize*ls, UiDim);
    }
    else if (rows.empty())
    {
        UiLabel("nothing to trade", page.page.x, footerY, NoteSize*ls, UiDim);
    }

    UiButton(page.close, true, ls, in, "LEAVE   E", UiPanel, UiInk);
}
