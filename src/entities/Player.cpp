#include "entities/Player.h"

#include "core/Input.h"
#include "raymath.h"

#include <cmath>

void Player::Spawn(Vector3 position)
{
    body = Body();
    body.position = position;

    // Not stats, level or exp. A spawn is a placement, not a new character - it is
    // what the debug regenerate key calls, and losing a character sheet every time
    // a new floor is generated would make the whole system untestable.
    RefreshHealth();
    health = maxHealth;
    yaw = 0.0f;

    for (AttackState &attack : attacks) attack.Reset();
}

void Player::Update(float delta, const InputState &input, float lookYaw)
{
    yaw = lookYaw;

    body.Update(delta, yaw, input.move, input.jump, input.crouch);
}

void Player::UpdateAttacks(float delta, const bool pressed[2], const bool held[2], const AttackStyle newStyles[2])
{
    for (int h = 0; h < HandCount; h++)
    {
        styles[h] = newStyles[h];
        attacks[h].Update(delta, pressed[h], held[h], styles[h]);
    }
}

Vector3 Player::EyePosition() const
{
    // The constant standing height, not the camera's crouch-lerped one: gameplay
    // reach should not quietly change while the view is settling
    return { body.position.x, body.position.y + Config::PlayerEyeHeight, body.position.z };
}

Vector3 Player::Forward() const
{
    // Matches Body::Update's convention, where moving forward runs along -front
    return { -sinf(yaw), 0.0f, -cosf(yaw) };
}

bool Player::IsBlocking() const
{
    const int off = (int)Hand::Left;

    return (styles[off] == AttackStyle::Block) && (attacks[off].blend > 0.75f);
}

void Player::TakeDamage(int amount)
{
    if (amount <= 0) return;

    health -= amount;
    if (health < 0) health = 0;
}

void Player::TakeDamageFrom(int amount, Vector3 source)
{
    // A raised shield only covers what it is pointed at. Reach is irrelevant
    // here - InCone is being used purely for the angle.
    if (IsBlocking() && InCone(EyePosition(), Forward(), source, 1000.0f, Config::BlockArc))
    {
        amount = (int)(amount*Config::BlockDamageScale);
        if (amount < 1) amount = 1;
    }

    TakeDamage(amount);
}

void Player::Heal(int amount)
{
    if ((amount <= 0) || !IsAlive()) return;

    health += amount;
    if (health > maxHealth) health = maxHealth;
}

//----------------------------------------------------------------------------------
// The line the fight actually uses: what was spent, what is held, and what has been
// learned.
//
// Three sources and one answer. The held bonus and the modifiers are both OFFSETS -
// adding two stat lines would make a character holding two neutral weapons a
// character of 30 in everything - and the conversions in ApplyModifiers run LAST, on
// the total, so "30% of arcane" reads the arcane the character ended up with rather
// than the arcane they spent points on.
//----------------------------------------------------------------------------------
StatBlock Player::Fighting() const
{
    StatBlock fighting = stats;

    fighting.con  += heldBonus.con;
    fighting.arms += heldBonus.arms;
    fighting.skl  += heldBonus.skl;
    fighting.arc  += heldBonus.arc;

    return ApplyModifiers(fighting, mods);
}

void Player::SetModifiers(const Modifiers &bonus)
{
    mods = bonus;

    // The same rule a spent point follows: the pool grows and the health in it grows
    // with it, so a trait granting constitution is felt on the frame it goes on.
    // Unlike a held weapon this is a permanent choice, so it carries UP rather than
    // being clamped - see the note on SetHeldBonus for why the two differ.
    RefreshHealth();

    if (mana > MaxMana()) mana = MaxMana();
}

void Player::SetHeldBonus(const StatBlock &bonus)
{
    heldBonus = bonus;

    // Clamped down, never carried up - see the note on SetHeldBonus. Putting the
    // shield away shortens the bar; picking it up lengthens it and leaves the
    // health where it was, which is the only version of this that a player cannot
    // farm by cycling the mouse wheel.
    maxHealth = MaxHealth();
    if (health > maxHealth) health = maxHealth;
}

int Player::MaxHealth() const
{
    const StatBlock fighting = Fighting();

    const int total = Config::PlayerMaxHealth
                    + StatBonusHealth(fighting, Config::PlayerMaxHealth)
                    + mods.flatHealth;

    // A character can be built down as well as up - an enemy kind's line goes below
    // neutral and nothing stops a debug spend from doing the same - and a pool of
    // zero is a character that is dead before the first frame
    return (total < 1) ? 1 : total;
}

//----------------------------------------------------------------------------------
// The pool, brought back in line with the stats.
//
// The current health rises WITH it. A point of constitution that lengthened the bar
// and left the character on the same number is a point that made them no safer at
// the moment they spent it, which is not what it was spent for - and the alternative
// of simply refilling would make constitution a heal as well as a pool.
//----------------------------------------------------------------------------------
void Player::RefreshHealth()
{
    const int was = maxHealth;

    maxHealth = MaxHealth();

    const int gained = maxHealth - was;
    if (gained > 0) health += gained;

    if (health > maxHealth) health = maxHealth;
    if (health < 0) health = 0;
}

int Player::SpellPower() const
{
    const StatBlock fighting = Fighting();

    int total = Config::BaseSpellPower
              + StatBonusSpellPower(fighting, Config::BaseSpellPower)
              + mods.flatSpell;

    // A fraction on top of what arcane already bought, added rather than compounded -
    // the same rule every other modifier fraction follows
    if (mods.spellPower != 0.0f) total = (int)(total*(1.0f + mods.spellPower) + 0.5f);

    return (total < 1) ? 1 : total;
}

int Player::WeaponDamage(int weaponDamage) const
{
    return WeaponDamageWith(Fighting(), weaponDamage) + mods.flatDamage;
}

//----------------------------------------------------------------------------------
// Mana.
//
// The pool is arcane's second job - see the note in Config. It is a RESERVOIR and
// not a faucet: spending on arcane banks more casts, and the only way to fill what
// has been banked is still to kill something.
//----------------------------------------------------------------------------------
int Player::MaxMana() const
{
    const StatBlock fighting = Fighting();

    const int over = fighting.arc - Config::StatBase;

    const int total = Config::ManaMax + (int)(over*Config::ManaPerArcane + 0.5f)
                    + mods.flatMana;

    return (total < 1) ? 1 : total;
}

bool Player::SpendMana(int cost)
{
    if (cost <= 0) return true;
    if (mana < cost) return false;

    mana -= cost;

    return true;
}

void Player::CreditWeaponKills(int kills)
{
    if (kills <= 0) return;

    mana += kills*(Config::ManaPerKill + mods.manaPerKill);

    if (mana > MaxMana()) mana = MaxMana();
}

void Player::CreditSpellKills(int kills)
{
    if (kills <= 0) return;

    // The remainder is CARRIED, not dropped. See the note on spellKillCarry: rounding
    // each batch down on its own pays a caster who kills one body at a time nothing
    // at all, which is the build the half rate exists to keep alive.
    spellKillCarry += kills;

    const int per = (Config::SpellKillsPerMana > 0) ? Config::SpellKillsPerMana : 1;
    const int whole = spellKillCarry/per;

    if (whole <= 0) return;

    spellKillCarry -= whole*per;

    mana += whole*(Config::ManaPerKill + mods.manaPerKill);

    if (mana > MaxMana()) mana = MaxMana();
}

//----------------------------------------------------------------------------------
// Experience in, levels out.
//
// A while loop rather than a single test: a rank far above the player pays enough
// to cross more than one threshold at once, and paying out only the first would
// silently swallow the rest of it.
//----------------------------------------------------------------------------------
int Player::GainExp(int amount)
{
    if (amount <= 0) return 0;

    exp += amount;

    int gained = 0;

    while (exp >= expToNext)
    {
        exp -= expToNext;
        ++level;
        ++gained;

        statPoints += Config::PlayerStatPointsPerLevel;

        // Rounded up, so the cost is always a whole number that actually grew -
        // truncation at a growth rate this shallow can leave two levels costing
        // the same
        expToNext = (int)ceilf(expToNext*Config::PlayerExpGrowth);
    }

    return gained;
}

bool Player::SpendPoint(Stat stat)
{
    if (statPoints <= 0) return false;
    if (stat < Stat::Con || stat >= Stat::Count) return false;

    StatAdd(stats, stat, 1);
    --statPoints;

    // Immediately, not at the next level. A point of constitution whose effect
    // waited would be indistinguishable from one that did nothing.
    RefreshHealth();

    return true;
}

void Player::RespecStats()
{
    for (int i = 0; i < (int)Stat::Count; ++i)
    {
        const Stat stat = (Stat)i;

        // Only what was actually SPENT comes back. A stat sitting below neutral is
        // not a refund owed - nothing here can spend a point downwards - and
        // paying one out would mint points from nothing.
        const int spent = StatValue(stats, stat) - Config::StatBase;
        if (spent <= 0) continue;

        StatAdd(stats, stat, -spent);
        statPoints += spent;
    }

    RefreshHealth();
}
