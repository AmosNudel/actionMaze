#include "progress/Traits.h"

#include "combat/Stats.h"

namespace
{
    // Shorthand for a row that grants stat points. Written out rather than a macro so
    // the table below stays something a reader can check against the struct.
    constexpr StatBlock Points(int con, int arms, int skl, int arc)
    {
        return { con, arms, skl, arc };
    }

    //------------------------------------------------------------------------------
    // The table.
    //
    // Twelve rows, and they are deliberately not a ladder. Four of them are stat
    // sticks and cheap; the rest change a rule, and those are what a build is
    // actually shopping for. A player with four contracts should have a real question
    // about whether to spend them on one rule or four points.
    //
    // The prices are in CONTRACTS, which come only from events - so a whole floor
    // cleared of objectives is two or three of them. A four-contract trait is two
    // floors of choosing to walk into the marker, which is the right price for
    // something that changes how the character works for the rest of the run.
    //
    // Nothing here grants raw damage. Points and forging already sell that, and a
    // third source of the same number would only move the same slider.
    //------------------------------------------------------------------------------
    const TraitDef Table[] =
    {
        //--------------------------------------------------------------------------
        // The cheap stat sticks. They exist so that a player with one contract and no
        // opinion has something to spend it on, and so that the expensive rules have
        // something to be measured against.
        //--------------------------------------------------------------------------
        { "STOUT", "+3 constitution", { 120, 210, 130, 255 }, 1,
          { Points(3, 0, 0, 0) } },

        { "STRONG", "+3 arms", { 235, 110, 90, 255 }, 1,
          { Points(0, 3, 0, 0) } },

        { "DEFT", "+3 skill", { 250, 220, 120, 255 }, 1,
          { Points(0, 0, 3, 0) } },

        { "LEARNED", "+3 arcane", { 150, 160, 255, 255 }, 1,
          { Points(0, 0, 0, 3) } },

        //--------------------------------------------------------------------------
        // The rules.
        //--------------------------------------------------------------------------

        // The sustain answer, and the one trait that rewards being in the middle of
        // the room rather than at the edge of it. Worth nothing at all if you are
        // not connecting, which is the whole point.
        { "LEECH", "blows return 8% as health", { 200, 60, 70, 255 }, 3,
          { Points(0, 0, 0, 0), 0, 0, 0, 0,
            0.0f, 0.0f, 0.0f, 0.08f } },

        // Buys the crit build its ceiling early. Flat rather than a percentage of
        // the rolled chance, so it is worth the same to a character who has spent
        // nothing on skill - which makes it a genuine alternative to spending on it.
        { "KEEN", "+8% critical chance", { 250, 220, 120, 255 }, 3,
          { Points(0, 0, 0, 0), 0, 0, 0, 0,
            0.0f, 0.0f, 0.08f } },

        // The caster's economy. Casting is paid for by killing, and this is the trait
        // that makes a spell build able to fund itself off its own kills without
        // breaking the rule that one cast can never pay for the next.
        { "SIPHON", "+1 mana for every kill", { 190, 130, 255, 255 }, 3,
          { Points(0, 0, 0, 0), 0, 0, 0, 0,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1 } },

        // The other half of the caster's economy, and priced against SIPHON on
        // purpose: one makes casts cheaper and the other makes them come back
        // faster, and which is better depends entirely on how fast the run kills.
        { "FRUGAL", "spells cost 20% less", { 190, 160, 255, 255 }, 3,
          { Points(0, 0, 0, 0), 0, 0, 0, 0,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -0.20f } },

        // A bigger reservoir rather than a faster faucet. What it buys is the right
        // to open a fight with three spells instead of one, which is a different
        // thing from casting more of them over a floor.
        { "DEEP WELL", "+12 maximum mana", { 130, 180, 255, 255 }, 2,
          { Points(0, 0, 0, 0), 0, 0, 0, 12 } },

        // Speed, on both hands and on the cast. The most generally useful row in the
        // table and priced for it - there is no build this is bad for, which is
        // exactly why it must not be cheap.
        { "QUICK", "swing and cast 12% faster", { 255, 195, 110, 255 }, 4,
          { Points(0, 0, 0, 0), 0, 0, 0, 0,
            0.12f } },

        // The one that makes a floor smaller. Not a combat bonus at all - what it
        // buys is the walk between fights, which on a big map is most of the run.
        { "FLEET", "move 10% faster", { 130, 225, 140, 255 }, 2,
          { Points(0, 0, 0, 0), 0, 0, 0, 0,
            0.0f, 0.10f } },

        //--------------------------------------------------------------------------
        // The conversions. The most expensive rows, and the only ones whose worth
        // depends on what the player has already spent - which is what makes them
        // the last purchase of a build rather than the first.
        //--------------------------------------------------------------------------

        // Turns a caster's investment into a weapon. Reads the arcane the character
        // ENDED UP with, upgrades included - see ApplyModifiers.
        { "SPELLBLADE", "30% of arcane counts as arms", { 200, 150, 255, 255 }, 5,
          { Points(0, 0, 0, 0), 0, 0, 0, 0,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0,
            { { (unsigned char)Stat::Arcane, (unsigned char)Stat::Arms, 0.30f } }, 1 } },

        // And the reverse, for the character who spent on the sword and wants the
        // spells to keep up. Priced the same because neither is better - they are
        // the same trait read from the two ends of the same decision.
        { "WARLOCK", "30% of arms counts as arcane", { 255, 150, 200, 255 }, 5,
          { Points(0, 0, 0, 0), 0, 0, 0, 0,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0,
            { { (unsigned char)Stat::Arms, (unsigned char)Stat::Arcane, 0.30f } }, 1 } },
    };

    constexpr int Count = (int)(sizeof(Table)/sizeof(Table[0]));

    static_assert(Count <= MaxTraits, "the trait table is longer than a loadout can own");
}

int TraitCount()
{
    return Count;
}

const TraitDef &TraitAt(int id)
{
    if ((id < 0) || (id >= Count)) return Table[0];

    return Table[id];
}

int TraitSlotsUnlocked(int level)
{
    int unlocked = 0;

    for (int i = 0; i < TraitSlots; ++i)
    {
        if (level >= TraitSlotLevels[i]) unlocked++;
    }

    // The first is free from the start, whatever the table says - see the note on
    // TraitSlotLevels
    return (unlocked < 1) ? 1 : unlocked;
}

int TraitSlotLevel(int slot)
{
    if ((slot < 0) || (slot >= TraitSlots)) return 1;

    return TraitSlotLevels[slot];
}

void TraitLoadout::Clear()
{
    for (int i = 0; i < TraitSlots; ++i) slots[i] = -1;
    for (int i = 0; i < MaxTraits; ++i) owned[i] = 0;
}

bool TraitLoadout::Owns(int id) const
{
    if ((id < 0) || (id >= MaxTraits)) return false;

    return owned[id] != 0;
}

void TraitLoadout::Give(int id)
{
    if ((id < 0) || (id >= MaxTraits)) return;

    owned[id] = 1;
}

void TraitLoadout::Take(int id)
{
    if ((id < 0) || (id >= MaxTraits)) return;

    owned[id] = 0;

    // Off the character as well as out of the owned set. A trait sold from under a
    // slot it was still worn in would go on granting its bonus, which is a bug the
    // player would experience as free money.
    for (int i = 0; i < TraitSlots; ++i)
    {
        if (slots[i] == id) slots[i] = -1;
    }
}

int TraitLoadout::Equipped(int slot) const
{
    if ((slot < 0) || (slot >= TraitSlots)) return -1;

    return slots[slot];
}

int TraitLoadout::Equip(int slot, int id)
{
    if ((slot < 0) || (slot >= TraitSlots)) return -1;
    if (!Owns(id)) return -1;

    // Already worn somewhere else. Refused rather than moved, because moving it would
    // silently empty the slot the player was not looking at.
    for (int i = 0; i < TraitSlots; ++i)
    {
        if ((i != slot) && (slots[i] == id)) return -1;
    }

    const int out = slots[slot];

    slots[slot] = id;

    return out;
}

int TraitLoadout::Unequip(int slot)
{
    if ((slot < 0) || (slot >= TraitSlots)) return -1;

    const int out = slots[slot];

    slots[slot] = -1;

    return out;
}

bool TraitLoadout::IsEquipped(int id) const
{
    if (id < 0) return false;

    for (int i = 0; i < TraitSlots; ++i)
    {
        if (slots[i] == id) return true;
    }

    return false;
}

int TraitLoadout::OwnedCount() const
{
    int count = 0;

    for (int i = 0; i < MaxTraits; ++i) { if (owned[i] != 0) count++; }

    return count;
}

//----------------------------------------------------------------------------------
// Summed rather than accumulated.
//
// Rebuilt from the slots every time it is asked for, so there is no way for an
// equip/unequip pair to leave a bonus behind - which is the failure mode of every
// system that adds on equip and subtracts on remove.
//----------------------------------------------------------------------------------
Modifiers TraitLoadout::Bonus(int level) const
{
    const int unlocked = TraitSlotsUnlocked(level);

    Modifiers out;

    for (int i = 0; i < unlocked; ++i)
    {
        if (slots[i] < 0) continue;

        out = ModifiersAdd(out, TraitAt(slots[i]).mods);
    }

    return out;
}
