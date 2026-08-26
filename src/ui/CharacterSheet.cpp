#include "ui/CharacterSheet.h"

#include "core/Config.h"
#include "entities/Player.h"
#include "ui/UiText.h"
#include "ui/UiTheme.h"

namespace
{
    //------------------------------------------------------------------------------
    // The page, in DESIGN pixels. Everything below is multiplied by a layout scale
    // fitted to the window, so these are proportions rather than sizes.
    //
    // The design height is what the fit divides by, and it is the sum of everything
    // stacked below - title, points line, four stat rows, the trait block, the pick
    // list and the footer - plus the air between them. Change a metric here and
    // change this, or the page fits to a height it no longer has.
    //------------------------------------------------------------------------------
    constexpr float DesignHeight = 640.0f;

    // A page that ran the full width of a 21:9 monitor would put the [+] buttons a
    // foot of mouse travel from the numbers they change. The column is capped, and
    // the window's own width caps it again on anything narrow.
    constexpr float DesignWidth  = 720.0f;

    // Past about 2.2 the page stops being a page and becomes controls the size of
    // dinner plates. A 1440p window sits at the cap.
    constexpr float MaxScale     = 2.2f;

    constexpr float TitleSize   = 40.0f;
    constexpr float HeadSize    = 17.0f;
    constexpr float RowSize     = 21.0f;
    constexpr float SmallSize   = 14.0f;

    constexpr float RowHeight   = 44.0f;
    constexpr float RowGap      = 7.0f;
    constexpr float RowPad      = 14.0f;   // Inside a row, left and right
    constexpr float PlusSize    = 28.0f;
    constexpr float ButtonH     = 38.0f;

    constexpr float TitleTop    = 8.0f;
    constexpr float PointsTop   = 62.0f;
    constexpr float RowsTop     = 104.0f;

    // The trait block: a heading, a row of four slots, then the pick list
    constexpr float TraitsGap   = 22.0f;   // Under the last stat row
    constexpr float SlotHeight  = 54.0f;
    constexpr float SlotGap     = 6.0f;
    constexpr float ListGap     = 10.0f;
    constexpr float ListRow     = 40.0f;

    constexpr float FooterH     = 48.0f;

    // Where the right hand column starts, as a fraction of the page. One number, so
    // the four rows cannot drift out of alignment with each other.
    constexpr float DerivedShare = 0.52f;

    //------------------------------------------------------------------------------
    // One colour per stat, in Stat order.
    //
    // Borrowed unchanged from the mobile game's sheet, which is the point: a player
    // who knows that red is the one that swings harder should not have to learn it
    // twice. The hue is carried on the row's left edge rather than on the label, so
    // it reads as a tab down the side of the page instead of four different coloured
    // words.
    //------------------------------------------------------------------------------
    constexpr Color StatColour[(int)Stat::Count] =
    {
        { 120, 210, 130, 255 },     // Con   - green, the one that keeps you alive
        { 235, 110,  90, 255 },     // Arms  - red, the one that hits
        { 250, 220, 120, 255 },     // Skill - gold, the one that crits
        { 150, 160, 255, 255 },     // Arc   - blue, the one that casts
    };

    // What each stat actually moves, in the units the player sees it in. Read back
    // through the same functions combat uses, so the page cannot promise something
    // the fight does not deliver.
    const char *DerivedFor(Stat stat, const Player &player, const StatBlock &fighting)
    {
        switch (stat)
        {
            case Stat::Con:
                return TextFormat("health   %i", player.MaxHealth());

            case Stat::Arms:
                // As a multiplier, because the base is the WEAPON's and there are
                // twenty of those. What arms is worth is the same fraction on every
                // one of them, and that is the honest way to say it.
                return TextFormat("weapons  x%.2f",
                                  1.0f + Config::StatDamagePerPoint
                                        *(StatValue(fighting, Stat::Arms) - Config::StatBase));

            case Stat::Skill:
                return TextFormat("crit  %.0f%%  x%.2f",
                                  StatCritChance(fighting)*100.0f, StatCritDamage(fighting));

            default:
                // Arcane buys two things now, and the second one is the reason a
                // caster spends on it at all - so both are printed. A page that
                // showed only spell power would leave the mana pool as a number the
                // player watches move for no visible reason.
                return TextFormat("spell %i   mana %i", player.SpellPower(), player.MaxMana());
        }
    }
}

void CharacterSheet::Toggle()
{
    open = !open;
    justOpened = open;

    // The pick list closes with the page. A list left open would reopen onto a slot
    // the player picked several floors ago, which reads as the page having
    // remembered something it has no business remembering.
    if (open)
    {
        picking = -1;
        scroll = 0;
    }
}

//----------------------------------------------------------------------------------
// The whole page, measured from the window.
//
// The scale fits the design height into whatever is there, and the column is centred
// at whichever is narrower - the design width at that scale, or the window less its
// gutters. On anything from a small window to a wide monitor the result is the same
// page, and the only thing that changes is how big it is.
//----------------------------------------------------------------------------------
CharacterSheet::Layout CharacterSheet::Measure() const
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

    out.titleY = out.page.y + TitleTop*out.ls;
    out.pointsY = out.page.y + PointsTop*out.ls;
    out.derivedX = out.page.x + out.page.width*DerivedShare;

    for (int row = 0; row < (int)Stat::Count; ++row)
    {
        const float y = out.page.y + (RowsTop + row*(RowHeight + RowGap))*out.ls;

        out.rows[row] = { out.page.x, y, out.page.width, RowHeight*out.ls };

        // Just left of the derived column, so the button that changes a number sits
        // beside the number it changes rather than at the far end of the row.
        out.plus[row] = { out.derivedX - (PlusSize + RowPad)*out.ls,
                          y + (RowHeight - PlusSize)*0.5f*out.ls,
                          PlusSize*out.ls, PlusSize*out.ls };
    }

    //------------------------------------------------------------------------------
    // The trait block, starting under the last stat row.
    //
    // Measured from the row above rather than from a constant, so adding a fifth stat
    // moves the traits down instead of putting them on top of it.
    //------------------------------------------------------------------------------
    const Rectangle last = out.rows[(int)Stat::Count - 1];

    out.traitsY = last.y + last.height + TraitsGap*out.ls;

    const float slotsY = out.traitsY + (HeadSize + 8.0f)*out.ls;
    const float slotW = (out.page.width - SlotGap*out.ls*(TraitSlots - 1))/TraitSlots;

    for (int i = 0; i < TraitSlots; ++i)
    {
        out.slots[i] = { out.page.x + i*(slotW + SlotGap*out.ls), slotsY,
                         slotW, SlotHeight*out.ls };
    }

    const float listTop = slotsY + SlotHeight*out.ls + ListGap*out.ls;
    const float listBottom = out.page.y + out.page.height - FooterH*out.ls
                           - (SmallSize + 8.0f)*out.ls;

    out.list = { out.page.x, listTop, out.page.width, listBottom - listTop };
    out.listRowHeight = ListRow*out.ls;

    const float step = out.listRowHeight + 4.0f*out.ls;

    out.listVisible = (step > 0.0f) ? (int)(out.list.height/step) : 0;
    if (out.listVisible < 1) out.listVisible = 1;

    out.purseY = listBottom + 4.0f*out.ls;

    const float footerY = out.page.y + out.page.height - ButtonH*out.ls;

    out.respec = { out.page.x, footerY, 150.0f*out.ls, ButtonH*out.ls };
    out.close = { out.page.x + out.page.width - 180.0f*out.ls, footerY,
                  180.0f*out.ls, ButtonH*out.ls };

    return out;
}

Rectangle CharacterSheet::Layout::ListRowAt(int slot) const
{
    const float step = listRowHeight + 4.0f*ls;

    return { list.x, list.y + slot*step, list.width, listRowHeight };
}

//----------------------------------------------------------------------------------
// Everything owned that is not already worn somewhere else.
//
// Traits already in ANOTHER slot are left out rather than shown greyed: the loadout
// refuses them anyway (two copies of one row would be double its bonus for the price
// of one), and a list of rows that cannot be picked is a list the player has to learn
// the rule of. The one in THIS slot stays, because picking it again is how a slot is
// emptied.
//----------------------------------------------------------------------------------
void CharacterSheet::BuildPickable(const TraitLoadout &traits, int owned[MaxTraits],
                                   int &count) const
{
    count = 0;

    const int here = (picking >= 0) ? traits.Equipped(picking) : -1;

    for (int i = 0; (i < TraitCount()) && (count < MaxTraits); ++i)
    {
        if (!traits.Owns(i)) continue;
        if (traits.IsEquipped(i) && (i != here)) continue;

        owned[count++] = i;
    }
}

void CharacterSheet::Update(Player &player, TraitLoadout &traits)
{
    if (!open) return;

    const UiInput in = UiInput::Read(justOpened);

    justOpened = false;

    const Layout page = Measure();

    int owned[MaxTraits];
    int ownedCount = 0;

    BuildPickable(traits, owned, ownedCount);

    // The wheel scrolls the pick list, and only while one is open. Read before the
    // click so a frame that does both acts on the row the player was looking at.
    if (picking >= 0)
    {
        const float wheel = GetMouseWheelMove();

        if (wheel != 0.0f) scroll -= (wheel > 0.0f) ? 1 : -1;

        const int most = ownedCount - page.listVisible;

        if (scroll > most) scroll = most;
        if (scroll < 0) scroll = 0;
    }

    if (!in.clicked) return;

    //------------------------------------------------------------------------------
    // The stat buttons.
    //------------------------------------------------------------------------------
    for (int row = 0; row < (int)Stat::Count; ++row)
    {
        if (!in.Over(page.plus[row])) continue;
        if (player.statPoints <= 0) return;

        player.SpendPoint((Stat)row);

        // One point per click, and one button per click. A held button that spent
        // continuously would empty a level's whole budget into one stat faster than
        // the number under it could be read.
        return;
    }

    //------------------------------------------------------------------------------
    // The trait slots. Clicking one opens its list; clicking it again closes it.
    //
    // A locked slot does nothing at all rather than opening an empty list. The level
    // it needs is printed on it, which is the whole answer to why it did nothing.
    //------------------------------------------------------------------------------
    const int unlocked = TraitSlotsUnlocked(player.level);

    for (int i = 0; i < TraitSlots; ++i)
    {
        if (!in.Over(page.slots[i])) continue;
        if (i >= unlocked) return;

        picking = (picking == i) ? -1 : i;
        scroll = 0;

        return;
    }

    //------------------------------------------------------------------------------
    // The pick list.
    //
    // Clicking the trait already in the slot takes it OFF, which is the only way to
    // empty a slot and is why that row is left in the list at all.
    //------------------------------------------------------------------------------
    if (picking < 0) return;

    for (int slot = 0; slot < page.listVisible; ++slot)
    {
        const int index = scroll + slot;

        if (index >= ownedCount) break;
        if (!in.Over(page.ListRowAt(slot))) continue;

        const int id = owned[index];

        if (traits.Equipped(picking) == id) traits.Unequip(picking);
        else                                traits.Equip(picking, id);

        picking = -1;

        return;
    }

    if (in.Over(page.respec)) { player.RespecStats(); return; }
    if (in.Over(page.close)) open = false;
}

void CharacterSheet::Draw(const Player &player, const TraitLoadout &traits) const
{
    if (!open) return;

    const Layout page = Measure();
    const float ls = page.ls;

    // Read again in Draw rather than passed down from Update, because Draw is const
    // and the two are one frame apart at most. It is only used for the hover and the
    // pressed look; nothing here acts on it.
    const UiInput in = UiInput::Read(false);

    UiPageBackdrop();

    //------------------------------------------------------------------------------
    // The head: who this is, and the two numbers that are about the character rather
    // than about any one stat.
    //------------------------------------------------------------------------------
    UiLabel("CHARACTER", page.page.x, page.titleY, TitleSize*ls, UiAccent);

    UiLabelRight(TextFormat("LEVEL %i", player.level),
                 page.page.x + page.page.width, page.titleY + TitleSize*0.32f*ls,
                 HeadSize*ls, UiInk);

    // A rule under the title. The page has no border of its own - it is the whole
    // window - so this is the only thing telling the eye where the content starts.
    const float ruleY = page.titleY + (TitleSize + 10.0f)*ls;

    DrawRectangleRec({ page.page.x, ruleY, page.page.width, 1.0f*ls },
                     Fade(UiDim, 0.45f));

    //------------------------------------------------------------------------------
    // The points line, in green while there is anything to spend.
    //
    // It is the one thing on this page that is ever urgent, so it is the one thing
    // that changes colour.
    //------------------------------------------------------------------------------
    const bool spendable = (player.statPoints > 0);

    UiLabel(spendable ? TextFormat("%i POINTS TO SPEND", player.statPoints)
                      : "NO POINTS TO SPEND",
            page.page.x, page.pointsY, HeadSize*ls, spendable ? UiReady : UiDim);

    UiLabelRight(TextFormat("%i / %i exp", player.exp, player.expToNext),
                 page.page.x + page.page.width, page.pointsY, HeadSize*ls, UiDim);

    //------------------------------------------------------------------------------
    // The four rows.
    //
    // Printed from Fighting() and not from `stats`, because that is the line the
    // fight uses. What the WEAPONS and the traits are adding is called out separately
    // beside it, so the player can tell what they earned from what they are carrying
    // - and so putting the greatsword down is not a mystery drop in their damage.
    //------------------------------------------------------------------------------
    const StatBlock fighting = player.Fighting();

    for (int row = 0; row < (int)Stat::Count; ++row)
    {
        const Stat stat = (Stat)row;
        const Rectangle box = page.rows[row];

        const int base = StatValue(player.stats, stat);
        const int held = StatValue(fighting, stat) - base;

        const bool hovered = in.Over(box);

        UiRow(box, ls, hovered && spendable, StatColour[row]);

        // The stat's colour as a bar down the row's left edge. It is a tab rather
        // than a coloured label because four coloured words on one page is four
        // things shouting, and this is a thing to find a row by rather than read.
        DrawRectangleRec({ box.x, box.y, 4.0f*ls, box.height }, StatColour[row]);

        const float textY = box.y + (box.height - RowSize*ls)*0.5f;

        UiLabel(StatName(stat), box.x + (RowPad + 8.0f)*ls, textY, RowSize*ls, UiInk);

        // The base, and what everything else is adding, as two figures rather than
        // one total. A single number cannot be spent against: the player has no way
        // to tell the part they chose from the part they are carrying.
        const char *value = (held != 0) ? TextFormat("%i %+i", base, held)
                                        : TextFormat("%i", base);

        UiLabel(value, box.x + box.width*0.28f, textY, RowSize*ls, UiDim);

        // Drawn whether or not it can be used, so the row does not change shape as
        // points come and go
        UiGlyphButton(page.plus[row], spendable, ls, in, "+", StatColour[row]);

        UiLabel(DerivedFor(stat, player, fighting), page.derivedX + RowPad*ls, textY,
                RowSize*ls, UiInk);
    }

    //------------------------------------------------------------------------------
    // The trait slots.
    //
    // Four boxes, and the ones the character has not reached yet say the level they
    // unlock at rather than being hidden. A slot that appeared out of nowhere is one
    // the player has to notice twice - once to learn it exists and again to work out
    // what brought it.
    //------------------------------------------------------------------------------
    const int unlocked = TraitSlotsUnlocked(player.level);

    UiLabel("TRAITS", page.page.x, page.traitsY, HeadSize*ls, UiAccent);

    UiLabelRight(TextFormat("%i owned    the captain sells them", traits.OwnedCount()),
                 page.page.x + page.page.width, page.traitsY, SmallSize*ls, UiDim);

    for (int i = 0; i < TraitSlots; ++i)
    {
        const Rectangle box = page.slots[i];
        const bool locked = (i >= unlocked);
        const int worn = traits.Equipped(i);

        const Color accent = (worn >= 0) ? TraitAt(worn).colour : UiDim;

        UiRow(box, ls, (picking == i), accent);

        if (locked)
        {
            UiLabelCentered(TextFormat("LEVEL %i", TraitSlotLevel(i)),
                            box.x + box.width*0.5f,
                            box.y + (box.height - SmallSize*ls)*0.5f, SmallSize*ls, UiOff);

            continue;
        }

        if (worn < 0)
        {
            UiLabelCentered("empty", box.x + box.width*0.5f,
                            box.y + (box.height - SmallSize*ls)*0.5f, SmallSize*ls, UiDim);

            continue;
        }

        const TraitDef &def = TraitAt(worn);

        DrawRectangleRec({ box.x, box.y, 4.0f*ls, box.height }, def.colour);

        UiLabelCentered(def.name, box.x + box.width*0.5f, box.y + 8.0f*ls,
                        SmallSize*ls + 2.0f*ls, def.colour);

        UiLabelCentered(def.desc, box.x + box.width*0.5f,
                        box.y + box.height - (SmallSize + 8.0f)*ls, SmallSize*ls - 1.0f*ls,
                        UiDim);
    }

    //------------------------------------------------------------------------------
    // The pick list, when a slot is open.
    //
    // When it is not, the space belongs to whatever the traits are actually doing -
    // one line summing every bonus the character is carrying. That line is the answer
    // to "what are my four traits worth", which four separate slot captions cannot
    // give.
    //------------------------------------------------------------------------------
    if (picking >= 0)
    {
        int owned[MaxTraits];
        int ownedCount = 0;

        BuildPickable(traits, owned, ownedCount);

        if (ownedCount <= 0)
        {
            UiLabel("nothing owned - the captain sells traits for contracts",
                    page.list.x, page.list.y, SmallSize*ls, UiDim);
        }

        for (int slot = 0; slot < page.listVisible; ++slot)
        {
            const int index = scroll + slot;

            if (index >= ownedCount) break;

            const int id = owned[index];
            const TraitDef &def = TraitAt(id);
            const Rectangle box = page.ListRowAt(slot);

            const bool wornHere = (traits.Equipped(picking) == id);

            UiRow(box, ls, in.Over(box), def.colour);

            DrawRectangleRec({ box.x, box.y, 4.0f*ls, box.height }, def.colour);

            UiLabel(def.name, box.x + (RowPad + 8.0f)*ls,
                    box.y + (box.height - SmallSize*ls)*0.5f, SmallSize*ls + 2.0f*ls,
                    def.colour);

            UiLabel(def.desc, box.x + box.width*0.30f,
                    box.y + (box.height - SmallSize*ls)*0.5f, SmallSize*ls, UiInk);

            UiLabelRight(wornHere ? "click to remove" : "click to wear",
                         box.x + box.width - RowPad*ls,
                         box.y + (box.height - SmallSize*ls)*0.5f, SmallSize*ls, UiDim);
        }
    }
    else
    {
        const char *summary = ModifiersText(player.Mods());

        UiLabel((summary[0] != '\0') ? summary : "no traits worn",
                page.list.x, page.list.y, SmallSize*ls,
                (summary[0] != '\0') ? UiInk : UiDim);
    }

    //------------------------------------------------------------------------------
    // The purse.
    //
    // All three, and only here. A shop shows the one currency it accepts, so this is
    // the one screen that can say what the run has actually earned - and the three
    // sitting side by side is what makes it obvious that they are not
    // interchangeable.
    //------------------------------------------------------------------------------
    const char *money = TextFormat("%i coins      %i gems      %i contracts",
                                   player.purse.coins, player.purse.gems,
                                   player.purse.contracts);

    UiLabel(money, page.page.x, page.purseY, SmallSize*ls, UiAccent);

    //------------------------------------------------------------------------------
    // The respec, and what it costs, which is nothing.
    //
    // Said on the button's own line rather than left to be discovered. The rates in
    // this system are new and will move; a player who cannot undo a spend is being
    // asked to commit to numbers that have not settled, and one who can undo it but
    // does not know that is in exactly the same position.
    //------------------------------------------------------------------------------
    UiButton(page.respec, true, ls, in, "RESPEC", UiPanel, UiInk);

    UiLabel("free, any time", page.respec.x + page.respec.width + 12.0f*ls,
            page.respec.y + (page.respec.height - SmallSize*ls)*0.5f, SmallSize*ls, UiDim);

    UiButton(page.close, true, ls, in, "CLOSE   TAB", UiPanel, UiInk);
}
