#include "progress/Arsenal.h"

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

        const int price = 40 + fromDamage + fromReach;

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
    forge.assign((size_t)capped, 0);
    prices.assign((size_t)capped, 0);
    names.assign((size_t)capped, std::string());

    for (int i = 0; i < capped; ++i)
    {
        prices[(size_t)i] = PriceFor(weapons[(size_t)i]);
        names[(size_t)i] = (weapons[(size_t)i].name != nullptr) ? weapons[(size_t)i].name : "";
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

    out.stat.arms = WeaponForgeArms*Forge(index);

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
//----------------------------------------------------------------------------------
int Arsenal::NextOwned(int from, int step) const
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
        if (Owns(slot)) return slot;
    }

    return -1;
}
