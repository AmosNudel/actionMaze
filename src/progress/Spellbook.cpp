#include "progress/Spellbook.h"

#include "combat/Magic.h"
#include "core/Config.h"
#include "raylib.h"

namespace
{
    bool InRange(Magic magic)
    {
        return ((int)magic >= 0) && ((int)magic < (int)Magic::Count);
    }
}

void Spellbook::Reset(Magic starting)
{
    for (int i = 0; i < (int)Magic::Count; ++i)
    {
        owned[i] = 0;
        empower[i] = 0;
    }

    if (!InRange(starting)) starting = Magic::Flame;

    owned[(int)starting] = 1;

    TraceLog(LOG_INFO, "SPELLBOOK: starting with %s", MagicAt(starting).name);
}

bool Spellbook::Owns(Magic magic) const
{
    return InRange(magic) && (owned[(int)magic] != 0);
}

void Spellbook::Give(Magic magic)
{
    if (!InRange(magic)) return;

    owned[(int)magic] = 1;
}

bool Spellbook::IsOffered(Magic magic) const
{
    return InRange(magic) && (offered[(int)magic] != 0);
}

// See the note on Arsenal::RerollOffers - same idiom, over eight schools rather
// than a couple of dozen weapons.
void Spellbook::RerollOffers(int count)
{
    for (int i = 0; i < (int)Magic::Count; ++i) offered[i] = 0;

    int unowned = 0;
    for (int i = 0; i < (int)Magic::Count; ++i) { if (owned[i] == 0) unowned++; }

    if (unowned <= 0) return;

    int wanted = count;
    if (wanted > unowned) wanted = unowned;

    int placed = 0;
    int guard = 0;

    while ((placed < wanted) && (guard < 1000))
    {
        guard++;

        const int pick = GetRandomValue(0, (int)Magic::Count - 1);

        if ((owned[pick] != 0) || (offered[pick] != 0)) continue;

        offered[pick] = 1;
        placed++;
    }
}

int Spellbook::Empower(Magic magic) const
{
    if (!InRange(magic)) return 0;

    return empower[(int)magic];
}

bool Spellbook::CanEmpower(Magic magic) const
{
    return Owns(magic) && (Empower(magic) < SpellEmpowerMax);
}

void Spellbook::RaiseEmpower(Magic magic)
{
    if (!CanEmpower(magic)) return;

    empower[(int)magic]++;
}

//----------------------------------------------------------------------------------
// What a school costs, from what it is worth.
//
// Off the damage multiplier, which is the one number in the magic table that says how
// hard a school hits relative to the others. The band is narrow by design (0.8 to 1.7
// - see the note in Magic.h), so the prices come out close together, and that is the
// honest answer: the schools are meant to differ in SPEED and shape rather than in
// power, and a price spread that implied otherwise would be a lie about the table.
//
// Gems are rare, so these are small numbers. A school is a few elite kills, not a
// floor's worth of them.
//----------------------------------------------------------------------------------
int Spellbook::Price(Magic magic) const
{
    if (!InRange(magic)) return 0;

    const float worth = MagicAt(magic).damageMult;

    const int price = (int)(Config::SpellBasePrice*worth + 0.5f);

    return (price < 1) ? 1 : price;
}

int Spellbook::EmpowerPrice(Magic magic) const
{
    if (!CanEmpower(magic)) return 0;

    // Climbing, for the same reason the forge does: the last level has to cost about
    // what another school costs, or deepening one is never a decision against
    // broadening the book.
    const int level = Empower(magic) + 1;

    const int price = (Price(magic)*(40 + 30*level))/100;

    return (price < 1) ? 1 : price;
}

float Spellbook::DamageMult(Magic magic) const
{
    return 1.0f + SpellEmpowerDamage*Empower(magic);
}

float Spellbook::SizeMult(Magic magic) const
{
    return 1.0f + SpellEmpowerSize*Empower(magic);
}

//----------------------------------------------------------------------------------
// What one cast costs.
//
// Off the school's own damage multiplier again, so the heavy end of the table is the
// expensive end to cast as well as to buy - which is what keeps SPARK worth having
// after the player owns BLAST. A pool of twenty holds about four blasts or seven
// sparks, and the difference between those two numbers is the decision.
//
// Empowering does NOT raise the cost. It is bought with a different currency and it
// is the mystic's whole second half; a level that made the school more expensive to
// use would be a purchase that partly undid itself.
//----------------------------------------------------------------------------------
int Spellbook::CostOf(Magic magic, const Modifiers &mods) const
{
    if (!InRange(magic)) return Config::SpellBaseCost;

    const float worth = MagicAt(magic).damageMult;

    float cost = Config::SpellBaseCost*worth;

    // Negative is cheaper - see the note on Modifiers::manaCost
    cost *= (1.0f + mods.manaCost);

    const int rounded = (int)(cost + 0.5f);

    return (rounded < 1) ? 1 : rounded;
}

int Spellbook::OwnedCount() const
{
    int count = 0;

    for (int i = 0; i < (int)Magic::Count; ++i) { if (owned[i] != 0) count++; }

    return count;
}

Magic Spellbook::NextOwned(Magic from, int step) const
{
    if (step == 0) return from;

    const int count = (int)Magic::Count;

    int at = InRange(from) ? (int)from : 0;

    for (int i = 0; i < count; ++i)
    {
        at += (step > 0) ? 1 : -1;

        if (at >= count) at = 0;
        if (at < 0) at = count - 1;

        if (owned[at] != 0) return (Magic)at;
    }

    // Owns one school, or none. Either way there is nothing to step to, and handing
    // back an unowned school would be a cast the player cannot pay for.
    return from;
}
