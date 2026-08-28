#include "ui/ShopScreen.h"

#include "audio/Sfx.h"
#include "render/ViewModel.h"

#include "combat/Magic.h"
#include "combat/Weapon.h"
#include "entities/Player.h"
#include "progress/Arsenal.h"
#include "progress/Spellbook.h"
#include "progress/Traits.h"
#include "render/WeaponPreview.h"
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

    // The merchant's SELL button, which sits to the left of the primary one on an
    // owned weapon's row - see Row::sellable. Narrower, because it carries a shorter
    // label and because the row it shares has to keep room for the weapon's stats.
    constexpr float SellW     = 96.0f;
    constexpr float SellGap   = 8.0f;

    // How far ABOVE the row's centre line the buttons sit, so the strip under them
    // is deep enough to hold a refusal note - see the note draw. A 34-high button
    // centred in a 52-high row leaves 9 either side, and a note needs about 11.
    constexpr float ButtonLift = 5.0f;

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

Rectangle ShopScreen::Layout::MainButtonIn(Rectangle box) const
{
    return { box.x + box.width - (ButtonW + RowPad)*ls,
             box.y + (box.height - ButtonH*ls)*0.5f - ButtonLift*ls,
             ButtonW*ls, ButtonH*ls };
}

Rectangle ShopScreen::Layout::SellButtonIn(Rectangle box) const
{
    const Rectangle main = MainButtonIn(box);

    return { main.x - (SellGap + SellW)*ls, main.y, SellW*ls, ButtonH*ls };
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
                           const ViewModel &viewModel, std::vector<Row> &rows) const
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

                    // The weapon's own table figures, ahead of the tags on every
                    // row for the same reason: damage and reach are what the
                    // player is actually choosing between, and forcing them to
                    // open the character sheet to see either was the shop
                    // showing a name and a price and calling that a decision.
                    //--------------------------------------------------------------
                    // A shield's row reads differently, because its numbers are
                    // different numbers. Damage on a thing that never swings is a
                    // figure the player would have to learn to ignore, and the one
                    // stat that actually separates the three - how many blows the
                    // guard eats - was nowhere on the page at all.
                    //--------------------------------------------------------------
                    const bool shield = (arsenal.TagsAt(i) & TagBlocking) != 0;

                    const std::string statText =
                        shield ? std::string(TextFormat("blocks %i   %.0f%% less taken",
                                                        arsenal.BlockChargesAt(i),
                                                        -arsenal.DamageTakenAt(i)*100.0f))
                               : std::string(TextFormat("%i dmg   %.1f reach",
                                                        arsenal.DamageAt(i), arsenal.ReachAt(i)));

                    row.name = arsenal.NameAt(i);
                    row.id = i;
                    row.tint = UiAccent;

                    if (!owns)
                    {
                        row.deal = Deal::Buy;
                        row.price = arsenal.Price(i);
                        row.detail = "not owned    " + statText
                                   + (tagText.empty() ? "" : ("    " + tagText));
                        row.enabled = purse.CanAfford(Currency::Coins, row.price);
                        row.note = row.enabled ? "" : "not enough coins";
                    }
                    else
                    {
                        //------------------------------------------------------
                        // An owned row: what it is, how far it is forged, and the
                        // tags. Built once for both the forgeable and the maxed
                        // case, which differ only in the level printed and whether
                        // there is a button - writing it twice was how the two
                        // drifted into saying different things about a shield.
                        //
                        // The damage line is dropped for shields. A forge level on
                        // one raises the same DamageMult every weapon has, but a
                        // shield never swings, so "+40% damage" on a buckler is a
                        // true number about something that never happens.
                        //------------------------------------------------------
                        const int level = arsenal.CanForge(i) ? arsenal.Forge(i)
                                                              : WeaponForgeMax;

                        const std::string forged =
                            TextFormat("forged %i / %i", level, WeaponForgeMax);

                        const std::string gain =
                            shield ? std::string()
                                   : std::string(TextFormat("    +%.0f%% damage",
                                        ((arsenal.DamageMult(i) - 1.0f)
                                         + arsenal.HeldBonus(i).damageDealt)*100.0f));

                        row.detail = statText + "    " + forged + gain
                                   + (tagText.empty() ? "" : ("    " + tagText));

                        if (arsenal.CanForge(i))
                        {
                            row.deal = Deal::Upgrade;
                            row.price = arsenal.ForgePrice(i);
                            row.enabled = purse.CanAfford(Currency::Coins, row.price);
                            row.note = row.enabled ? "" : "not enough coins";
                        }
                        else
                        {
                            row.deal = Deal::None;
                            row.note = "fully forged";
                        }
                    }

                    //------------------------------------------------------------------
                    // The second button, on every owned weapon whatever its primary
                    // deal turned out to be - see Row::sellable.
                    //
                    // Two things are refused, and both are refused rather than worked
                    // around because working around either would move something the
                    // player did not ask to move:
                    //
                    //   IN HAND    the Arsenal cannot unequip - it has never heard of
                    //              a ViewModel - and selling out from under a hand
                    //              would leave the wheel pointing at a weapon that no
                    //              longer exists. The player takes it off first.
                    //   THE LAST   the main hand may never be empty (see Equip.h), and
                    //              a run that sold its only weapon would have nothing
                    //              legal to put there.
                    //------------------------------------------------------------------
                    if (owns)
                    {
                        const bool held = (viewModel.SlotIndex(Hand::Right) == i) ||
                                          (viewModel.SlotIndex(Hand::Left) == i);

                        row.sellable = true;
                        row.sellPrice = arsenal.SellPrice(i);
                        row.sellEnabled = !held && (arsenal.OwnedCount() > 1);

                        row.sellNote = held ? "in hand"
                                     : (arsenal.OwnedCount() > 1) ? "" : "your only weapon";
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
                        TraitLoadout &traits, const ViewModel &viewModel)
{
    if (!open) return;

    const UiInput in = UiInput::Read(justOpened);

    justOpened = false;

    const Layout page = Measure();

    std::vector<Row> rows;

    BuildRows(player, arsenal, spells, traits, viewModel, rows);

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

        const Rectangle box = page.RowAt(slot);

        if (!in.Over(box)) continue;

        //----------------------------------------------------------------------
        // The SELL button first, because it sits INSIDE the row and a row-level
        // test would swallow it - the same ordering the options page uses for its
        // steppers, and for the same reason.
        //----------------------------------------------------------------------
        if (row.sellable && in.Over(page.SellButtonIn(box)))
        {
            if (!row.sellEnabled)
            {
                GameSfx::Play(Sfx::UiDenied);
                return;
            }

            // Paid before it is taken, so a figure that moved between the build and
            // the click cannot hand over coins for a weapon that was never removed
            player.purse.Add(Currency::Coins, row.sellPrice);
            arsenal.Take(row.id);

            GameSfx::Play(Sfx::UiBuy);
            return;
        }

        // A row is only enabled when the purse can actually cover it, so a click on
        // a dead one is exactly the "you cannot afford that" the player needs told -
        // and the only place in the game with a use for the refusal sound.
        if (!row.enabled)
        {
            GameSfx::Play(Sfx::UiDenied);
            continue;
        }

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

        // One sound for the whole counter rather than one per branch. Money changing
        // hands is money changing hands, and a respec - which is free, and the only
        // row here that trades nothing - takes the plainer confirm instead.
        GameSfx::Play((row.deal == Deal::Respec) ? Sfx::UiConfirm : Sfx::UiBuy);

        // One row per click, whatever it was. A click that fell through to a second
        // row would spend twice on a list that has just changed shape underneath it.
        return;
    }
}

void ShopScreen::Draw(const Player &player, const Arsenal &arsenal, const Spellbook &spells,
                      const TraitLoadout &traits, WeaponPreview &preview,
                      const ViewModel &viewModel) const
{
    if (!open) return;

    const Layout page = Measure();
    const float ls = page.ls;

    const UiInput in = UiInput::Read(false);

    const NpcDef &def = NpcAt(vendor);

    std::vector<Row> rows;

    BuildRows(player, arsenal, spells, traits, viewModel, rows);

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

        //----------------------------------------------------------------------
        // The merchant's rows show the weapon itself, turning, instead of
        // printing its name - the name is still what BuildRows carries in
        // `row.name`, but here it is a model to look up rather than a string to
        // print. Everyone else's rows are unchanged: a school or a trait has no
        // model to show in its place.
        //----------------------------------------------------------------------
        if (vendor == NpcKind::Merchant)
        {
            const float iconSize = box.height - 6.0f*ls;
            const Rectangle icon = { box.x + 3.0f*ls, box.y + 3.0f*ls, iconSize, iconSize };

            // The MODEL name, which is what the preview looks the mesh up by
            preview.Draw(row.name, icon);

            //------------------------------------------------------------------
            // The name over the stats, the same two-line shape every other vendor
            // uses. The merchant showed neither before - an icon and a row of
            // figures - which meant the one list in the game where the player is
            // choosing between twenty similar objects was the one that did not
            // say what any of them were called.
            //------------------------------------------------------------------
            UiLabel(WeaponDisplayName(row.name.c_str()), icon.x + icon.width + RowPad*ls,
                    box.y + 8.0f*ls, RowSize*ls, UiInk);

            UiLabel(row.detail.c_str(), icon.x + icon.width + RowPad*ls,
                    box.y + (8.0f + RowSize + 3.0f)*ls, NoteSize*ls, UiDim);
        }
        else
        {
            UiLabel(row.name.c_str(), box.x + (RowPad + 8.0f)*ls, box.y + 8.0f*ls, RowSize*ls, UiInk);

            UiLabel(row.detail.c_str(), box.x + (RowPad + 8.0f)*ls,
                    box.y + (8.0f + RowSize + 3.0f)*ls, NoteSize*ls, UiDim);
        }

        const Rectangle button = page.MainButtonIn(box);

        //----------------------------------------------------------------------
        // The merchant's SELL button, drawn before the primary one and never
        // instead of it - an owned weapon can be forged and sold, and those are
        // two answers to two questions. See Row::sellable.
        //
        // Its refusal note goes UNDER the button rather than beside it, where the
        // primary button's own note already lives.
        //----------------------------------------------------------------------
        if (row.sellable)
        {
            const Rectangle sell = page.SellButtonIn(box);

            UiButton(sell, row.sellEnabled, ls, in,
                     TextFormat("SELL  %i", row.sellPrice), UiPanel, UiDim);

            if (!row.sellEnabled && !row.sellNote.empty())
            {
                UiLabelCentered(row.sellNote.c_str(), sell.x + sell.width*0.5f,
                                sell.y + sell.height + 1.0f*ls, NoteSize*0.85f*ls, UiOff);
            }
        }

        if (row.deal == Deal::None)
        {
            // Nothing left to do with this one. The note is drawn where the button
            // would be, so the list keeps its shape rather than growing a ragged
            // right edge as things are bought - unless a SELL button is standing
            // there, in which case it goes to the left of that instead.
            const float noteRight = row.sellable ? (page.SellButtonIn(box).x - 10.0f*ls)
                                                 : (button.x + button.width);

            UiLabelRight(row.note.c_str(), noteRight,
                         button.y + (button.height - NoteSize*ls)*0.5f, NoteSize*ls, UiOff);

            continue;
        }

        const char *label = (row.deal == Deal::Buy)     ? TextFormat("BUY   %i", row.price)
                          : (row.deal == Deal::Upgrade) ? TextFormat("%s   %i",
                                (vendor == NpcKind::Merchant) ? "FORGE" : "EMPOWER", row.price)
                          : (row.deal == Deal::Sell)    ? TextFormat("SELL   %i", row.price)
                          : "RESPEC";

        UiButton(button, row.enabled, ls, in, label, UiPanel, row.tint);

        //----------------------------------------------------------------------
        // Why it is refused, UNDER the button rather than beside it. A greyed button
        // with no reason next to it is a rule the player has to guess.
        //
        // Under, because beside does not fit: a merchant row already carries the
        // weapon's stats and tags across the middle and now a SELL button as well,
        // and a right-aligned note between them ran straight through both. The
        // strip below the button is the one piece of a row nothing else uses, and
        // putting every note there means no row can ever grow into a collision.
        //----------------------------------------------------------------------
        if (!row.note.empty())
        {
            UiLabelCentered(row.note.c_str(), button.x + button.width*0.5f,
                            button.y + button.height + 1.0f*ls, NoteSize*0.85f*ls,
                            row.enabled ? UiDim : UiOff);
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
