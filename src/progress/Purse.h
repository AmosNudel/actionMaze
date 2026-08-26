#pragma once

//----------------------------------------------------------------------------------
// The three currencies, and what the player is carrying of each.
//
// Three rather than one, and the split IS the design. Each vendor sells a different
// kind of power for a different currency, so the three cannot be substituted for
// each other and a run is shaped by which of them it happened to earn:
//
//     COINS       every kill pays a few          -> the merchant: weapons and forging
//     GEMS        a rare drop, better off elites -> the mystic: schools and empowering
//     CONTRACTS   events only                    -> the captain: traits and a respec
//
// So coin is the steady income, gems are luck, and contracts are the reward for
// choosing to walk into an objective rather than past it. A player who never resolves
// an event never buys a trait, and that is a real decision rather than a difficulty
// setting.
//
// The point of the separation is that it cannot be arbitraged. There is deliberately
// no exchange between them: the moment one buys another, the three collapse into one
// currency with three names, and every vendor is selling the same thing.
//----------------------------------------------------------------------------------
enum class Currency
{
    Coins = 0,
    Gems,
    Contracts,

    Count
};

const char *CurrencyName(Currency currency);

//----------------------------------------------------------------------------------
// What is in the purse.
//
// A struct of three ints rather than an array, because they are read by name far more
// often than they are looped over - and the two places that DO want to loop go
// through Amount/Add below rather than reaching in.
//
// It carries across floors. Only a new run empties it: a currency reset by the portal
// would make everything the player did not spend before descending a waste, which
// turns the walk to the exit into an errand.
//----------------------------------------------------------------------------------
struct Purse
{
    int coins = 0;
    int gems = 0;
    int contracts = 0;

    int Amount(Currency currency) const;
    void Add(Currency currency, int amount);

    bool CanAfford(Currency currency, int price) const { return Amount(currency) >= price; }

    // Takes `price` if it is there, and returns whether it was. One function rather
    // than a check and a subtraction at every call site, because a purchase that
    // tested one currency and debited another is a bug that would look like a price
    // being wrong.
    bool Spend(Currency currency, int price);

    void Clear() { coins = 0; gems = 0; contracts = 0; }
};
