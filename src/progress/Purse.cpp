#include "progress/Purse.h"

const char *CurrencyName(Currency currency)
{
    switch (currency)
    {
        case Currency::Coins:     return "coins";
        case Currency::Gems:      return "gems";
        case Currency::Contracts: return "contracts";
        default:                  return "?";
    }
}

int Purse::Amount(Currency currency) const
{
    switch (currency)
    {
        case Currency::Coins:     return coins;
        case Currency::Gems:      return gems;
        case Currency::Contracts: return contracts;
        default:                  return 0;
    }
}

void Purse::Add(Currency currency, int amount)
{
    // The floor is against a future caller rather than against any path that exists
    // today - Spend is the only thing that subtracts, and it checks first
    int *slot = nullptr;

    switch (currency)
    {
        case Currency::Coins:     slot = &coins; break;
        case Currency::Gems:      slot = &gems; break;
        case Currency::Contracts: slot = &contracts; break;
        default: return;
    }

    *slot += amount;

    if (*slot < 0) *slot = 0;
}

bool Purse::Spend(Currency currency, int price)
{
    if (price <= 0) return true;
    if (!CanAfford(currency, price)) return false;

    Add(currency, -price);

    return true;
}
