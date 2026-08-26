#pragma once

#include "combat/Modifiers.h"
#include "raylib.h"

//----------------------------------------------------------------------------------
// Passive bonuses, bought with contracts and swapped freely.
//
// A trait is a permanent, passive change to how the character works, held in one of a
// few slots. Every trait is one row of the table in Traits.cpp, and a row is a name, a
// line of text and a Modifiers - so there is no trait-specific code anywhere in the
// game, exactly as there is no school-specific code in the cast path. Adding one is a
// row.
//
// --- Slots are earned, traits are bought ---------------------------------------------
// Two separate things, deliberately. SLOTS come from levelling and cannot be rushed, so
// how much passive power a run carries is a function of how far it got. WHICH traits
// fill them is bought from the captain with contracts, which come only from resolving
// events - so a run that walks past every objective has slots it cannot fill, and that
// is the trade the events exist to offer.
//
// Swapping between traits already owned is FREE. A trait is a build decision, and
// charging to undo one turns experimenting into a punishment; the contract was the
// price of admission.
//
// --- What traits are for ---------------------------------------------------------------
// Not "more damage" - points and forging already sell that, and a third source of the
// same number is just a bigger number. A trait changes a RULE: what a kill pays into
// the mana pool, whether arcane feeds arms, whether a blow drinks. That is why several
// of them are worth far more to one build than any amount of raw points.
//----------------------------------------------------------------------------------

// How many traits can be worn at once at the end of a long run. Four because that is
// what fits down the character page beside the four stats without either half
// scrolling.
constexpr int TraitSlots = 4;

//----------------------------------------------------------------------------------
// The levels the slots unlock at. The first is free from the start - a run with no
// trait slot at all would leave the captain selling something the player cannot use.
//
// Priced against how long a RUN is rather than how long a character could theoretically
// live. A floor is worth a level and a half or so, so 3/5/7 puts all four inside a
// normal descent with the last landing about four floors down, where the player has
// the contracts to fill it. A slot the run cannot reach is not difficulty, it is a
// feature that does not exist.
//----------------------------------------------------------------------------------
constexpr int TraitSlotLevels[TraitSlots] = { 1, 3, 5, 7 };

struct TraitDef
{
    const char *name;       // Drawn in the slot - short, there are no icons
    const char *desc;       // One line, for the page and the captain's list
    Color colour;
    int price;              // Contracts
    Modifiers mods;         // Everything it does
};

int TraitCount();
const TraitDef &TraitAt(int id);    // Clamps out of range to row 0

// How many slots a player at `level` has unlocked, 1..TraitSlots, and the level a
// given slot needs - which is what the page prints on a locked one.
int TraitSlotsUnlocked(int level);
int TraitSlotLevel(int slot);

// What the captain pays for a trait sold back, as a percentage of its price. Half,
// so selling is a way out of a build you have changed your mind about rather than an
// arbitrage loop against a vendor who buys and sells the same row.
constexpr int TraitSellPercent = 50;

//----------------------------------------------------------------------------------
// What the player is carrying.
//----------------------------------------------------------------------------------
constexpr int MaxTraits = 32;

class TraitLoadout
{
public:
    void Clear();

    bool Owns(int id) const;
    void Give(int id);
    void Take(int id);      // Sold back. Also unequips it, wherever it was worn.

    int Equipped(int slot) const;

    //------------------------------------------------------------------------------
    // Puts `id` into `slot`, returning what came out (-1 if it was empty).
    //
    // Refuses - returning -1 and changing nothing - for an unowned trait, a bad slot,
    // or a trait already worn in a DIFFERENT slot. That last one matters: two copies
    // of the same row would be double its bonus for the price of one, and the loadout
    // is the only place that can see it happening.
    //------------------------------------------------------------------------------
    int Equip(int slot, int id);
    int Unequip(int slot);

    bool IsEquipped(int id) const;
    int OwnedCount() const;

    // The summed bonus from every worn trait in an UNLOCKED slot. `level` is passed
    // because slots unlock with it, and a trait sitting in a slot the character has
    // not reached yet grants nothing.
    Modifiers Bonus(int level) const;

private:
    int slots[TraitSlots] = { -1, -1, -1, -1 };
    unsigned char owned[MaxTraits] = { 0 };
};
