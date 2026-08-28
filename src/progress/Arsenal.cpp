#include "progress/Arsenal.h"

#include "core/Config.h"
#include "raylib.h"

#include <cctype>
#include <cstring>

namespace
{
    //------------------------------------------------------------------------------
    // What one weapon costs, from what it does.
    //
    // Priced off its own damage and reach rather than authored per weapon, because
    // the weapon list is whatever is in the asset folder: a hand-written price table
    // would be a file to forget to update every time a model is added, and a weapon
    // with no row would be free.
    //
    // Reach is in there because it is the half of a weapon that does not show up in
    // its damage figure. A knife and a greatsword at the same damage are not the same
    // purchase - one of them lets you stay out of the swing coming back.
    //------------------------------------------------------------------------------
    int PriceFor(const WeaponListing &weapon)
    {
        const int fromDamage = weapon.damage*7;
        const int fromReach = (int)(weapon.reach*22.0f);

        // Config::WeaponPriceScale is the one knob for "the merchant costs too
        // much" rather than reworking the three constants above by feel - see the
        // note on it in Config.h.
        const int price = (int)((40 + fromDamage + fromReach)*Config::WeaponPriceScale);

        // Rounded to something a player reads as a price rather than as a
        // measurement. 137 coins and 140 coins buy the same sword; only one of them
        // looks like it was decided by a person.
        return ((price + 5)/10)*10;
    }

    // Case-insensitive substring. `haystack` is a file-derived weapon name and
    // `needle` the constant naming one, which is deliberately allowed to be shorter.
    bool NameContains(const char *haystack, const char *needle)
    {
        if ((haystack == nullptr) || (needle == nullptr) || (needle[0] == '\0')) return false;

        for (const char *at = haystack; *at != '\0'; ++at)
        {
            const char *a = at;
            const char *b = needle;

            while ((*b != '\0') && (tolower((unsigned char)*a) == tolower((unsigned char)*b)))
            {
                ++a;
                ++b;
            }

            if (*b == '\0') return true;
        }

        return false;
    }
}

void Arsenal::Reset(const std::vector<WeaponListing> &weapons, const char *startingName)
{
    const int count = (int)weapons.size();
    const int capped = (count > MaxWeapons) ? MaxWeapons : count;

    if (count > MaxWeapons)
    {
        TraceLog(LOG_WARNING, "ARSENAL: %i weapons loaded, only %i can be owned",
                 count, MaxWeapons);
    }

    owned.assign((size_t)capped, 0);
    offered.assign((size_t)capped, 0);
    forge.assign((size_t)capped, 0);
    prices.assign((size_t)capped, 0);
    names.assign((size_t)capped, std::string());
    blockCharges.assign((size_t)capped, 1);
    damageTaken.assign((size_t)capped, 0.0f);
    tags.assign((size_t)capped, 0u);
    damages.assign((size_t)capped, 0);
    reaches.assign((size_t)capped, 0.0f);

    for (int i = 0; i < capped; ++i)
    {
        prices[(size_t)i] = PriceFor(weapons[(size_t)i]);
        names[(size_t)i] = (weapons[(size_t)i].name != nullptr) ? weapons[(size_t)i].name : "";
        blockCharges[(size_t)i] = weapons[(size_t)i].blockCharges;
        damageTaken[(size_t)i] = weapons[(size_t)i].damageTaken;
        tags[(size_t)i] = weapons[(size_t)i].tags;
        damages[(size_t)i] = weapons[(size_t)i].damage;
        reaches[(size_t)i] = weapons[(size_t)i].reach;
    }

    if (capped <= 0) return;

    // The starting kit. Falls back to the first weapon rather than to nothing: a run
    // that opens with two empty hands is a run whose first fight cannot be won, and a
    // renamed asset is not worth that.
    int start = -1;

    for (int i = 0; i < capped; ++i)
    {
        if (!NameContains(weapons[(size_t)i].name, startingName)) continue;

        start = i;
        break;
    }

    if (start < 0)
    {
        TraceLog(LOG_WARNING, "ARSENAL: no weapon matching \"%s\" - starting with %s",
                 startingName, weapons[0].name);

        start = 0;
    }

    owned[(size_t)start] = 1;

    TraceLog(LOG_INFO, "ARSENAL: %i weapons, starting with %s", capped, weapons[(size_t)start].name);
}

const char *Arsenal::NameAt(int index) const
{
    if ((index < 0) || (index >= Count())) return "";

    return names[(size_t)index].c_str();
}

bool Arsenal::Owns(int index) const
{
    if ((index < 0) || (index >= Count())) return false;

    return owned[(size_t)index] != 0;
}

void Arsenal::Give(int index)
{
    if ((index < 0) || (index >= Count())) return;

    owned[(size_t)index] = 1;
}

void Arsenal::Take(int index)
{
    if ((index < 0) || (index >= Count())) return;

    owned[(size_t)index] = 0;

    // The forge level goes with it - see the note on the declaration. Without this,
    // selling a maxed weapon and buying it back at list price would return it fully
    // forged for a fraction of what the forging cost.
    forge[(size_t)index] = 0;
}

//----------------------------------------------------------------------------------
// What the merchant pays for it back.
//
// Off DamageMult rather than off the list price alone, so the forge levels the player
// paid for are worth something on the way out. It is still a loss - half of a figure
// that never included what forging cost - which is what keeps this a way out of a
// weapon rather than a way to launder coins through one.
//----------------------------------------------------------------------------------
int Arsenal::SellPrice(int index) const
{
    if ((index < 0) || (index >= Count())) return 0;

    const int worth = (int)(Price(index)*DamageMult(index)*(WeaponSellPercent/100.0f) + 0.5f);

    // A weapon that sold for nothing would read as the button being broken
    return (worth < 1) ? 1 : worth;
}

int Arsenal::IndexOfName(const char *name) const
{
    for (int i = 0; i < Count(); ++i)
    {
        if (NameContains(names[(size_t)i].c_str(), name)) return i;
    }

    return -1;
}

void Arsenal::GiveByName(const char *name)
{
    const int index = IndexOfName(name);

    if (index < 0)
    {
        TraceLog(LOG_WARNING, "ARSENAL: no weapon matching \"%s\" to start with", name);
        return;
    }

    owned[(size_t)index] = 1;
}

bool Arsenal::IsOffered(int index) const
{
    if ((index < 0) || (index >= Count())) return false;

    return offered[(size_t)index] != 0;
}

unsigned Arsenal::TagsAt(int index) const
{
    if ((index < 0) || (index >= Count())) return 0;

    return tags[(size_t)index];
}

int Arsenal::DamageAt(int index) const
{
    if ((index < 0) || (index >= Count())) return 0;

    return damages[(size_t)index];
}

int Arsenal::BlockChargesAt(int index) const
{
    if ((index < 0) || (index >= Count())) return 1;

    return blockCharges[(size_t)index];
}

float Arsenal::DamageTakenAt(int index) const
{
    if ((index < 0) || (index >= Count())) return 0.0f;

    return damageTaken[(size_t)index];
}

float Arsenal::ReachAt(int index) const
{
    if ((index < 0) || (index >= Count())) return 0.0f;

    return reaches[(size_t)index];
}

//----------------------------------------------------------------------------------
// A fresh floor, a fresh handful of unowned weapons on the counter.
//
// Picked by repeated rejection rather than a shuffled index list: the candidate
// pool is small (a couple of dozen weapons at most) and already owned weapons need
// no slot of their own, so rerolling a pick that lands on one already offered or
// already owned is cheaper than building and shuffling a whole list for a floor
// that only wants three or four entries from it.
//----------------------------------------------------------------------------------
void Arsenal::RerollOffers(int count, unsigned guaranteeTag)
{
    offered.assign(offered.size(), 0);

    int unowned = 0;
    for (int i = 0; i < Count(); ++i) { if (!Owns(i)) unowned++; }

    if (unowned <= 0) return;

    int wanted = count;
    if (wanted > unowned) wanted = unowned;

    int placed = 0;

    //------------------------------------------------------------------------------
    // The guarantee, spent first.
    //
    // Collected into a candidate list rather than rolled-and-rejected like the
    // ordinary fill below, because a tag as specific as "castable" can be rare
    // enough in a short weapon list that rejection sampling would spin for a
    // while before giving up - counting the candidates once and picking among
    // them is the same guarantee without the spin.
    //------------------------------------------------------------------------------
    if ((guaranteeTag != 0) && (placed < wanted))
    {
        std::vector<int> candidates;

        for (int i = 0; i < Count(); ++i)
        {
            if (Owns(i)) continue;
            if ((tags[(size_t)i] & guaranteeTag) != guaranteeTag) continue;

            candidates.push_back(i);
        }

        if (!candidates.empty())
        {
            const int pick = candidates[(size_t)GetRandomValue(0, (int)candidates.size() - 1)];

            offered[(size_t)pick] = 1;
            placed++;
        }
    }

    int guard = 0;      // Bails out rather than spinning forever on a bad count

    while ((placed < wanted) && (guard < 1000))
    {
        guard++;

        const int pick = GetRandomValue(0, Count() - 1);

        if (Owns(pick) || IsOffered(pick)) continue;

        offered[(size_t)pick] = 1;
        placed++;
    }
}

int Arsenal::Forge(int index) const
{
    if ((index < 0) || (index >= Count())) return 0;

    return forge[(size_t)index];
}

bool Arsenal::CanForge(int index) const
{
    return Owns(index) && (Forge(index) < WeaponForgeMax);
}

void Arsenal::RaiseForge(int index)
{
    if (!CanForge(index)) return;

    forge[(size_t)index]++;
}

int Arsenal::Price(int index) const
{
    if ((index < 0) || (index >= Count())) return 0;

    return prices[(size_t)index];
}

//----------------------------------------------------------------------------------
// What the next forge level costs.
//
// Climbing, and steeply. A flat price would make the fifth level the obvious purchase
// the moment the player had the coin, and the whole point of a forge ceiling is that
// the last level is a commitment to ONE weapon - which is only true if it costs about
// what a new weapon would.
//----------------------------------------------------------------------------------
int Arsenal::ForgePrice(int index) const
{
    if (!CanForge(index)) return 0;

    const int level = Forge(index) + 1;
    const int price = (Price(index)*(30 + 25*level))/100;

    return ((price + 5)/10)*10;
}

float Arsenal::DamageMult(int index) const
{
    return 1.0f + WeaponForgeDamage*Forge(index);
}

Modifiers Arsenal::HeldBonus(int index) const
{
    Modifiers out;

    // See the note on WeaponForgeArmsEquivalent - a fraction now, not a stat
    // point sitting on the character for as long as the weapon stays in hand.
    out.damageDealt = WeaponForgeArmsEquivalent*Forge(index);

    return out;
}

int Arsenal::OwnedCount() const
{
    int count = 0;

    for (unsigned char flag : owned) { if (flag != 0) count++; }

    return count;
}

//----------------------------------------------------------------------------------
// The wheel.
//
// Walks at most one full lap, so a player who owns nothing gets -1 back rather than
// a loop that never terminates. The empty hand is a slot in the cycle and not an
// absence: `Count()` slots plus one, with the last one meaning nothing held.
//
// `excludeTags` skips anything owned that carries one of those bits - see
// Game::UpdateWorld's own use of TagBlocking, which keeps the main hand's wheel
// from ever landing on a shield: Player's block and parry timing is hard-wired
// to the off hand, so a shield in the main hand would raise and never do
// anything. Zero excludes nothing, which is every other caller.
//----------------------------------------------------------------------------------
int Arsenal::NextOwned(int from, int step, unsigned excludeTags) const
{
    const int count = Count();

    if ((count <= 0) || (step == 0)) return -1;

    // -1 sits at the top of the ring, so stepping forward off the last owned weapon
    // lands on the empty hand and one more step comes back round to the first
    const int slots = count + 1;

    int slot = (from < 0) ? count : from;

    for (int i = 0; i < slots; ++i)
    {
        slot += (step > 0) ? 1 : -1;

        if (slot >= slots) slot = 0;
        if (slot < 0) slot = slots - 1;

        if (slot == count) return -1;           // The empty hand
        if (Owns(slot) && ((tags[(size_t)slot] & excludeTags) == 0)) return slot;
    }

    return -1;
}
