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

    blockCooldown = 0.0f;
    blockActiveTime = 0.0f;
}

void Player::ResetCharacter()
{
    stats = StatBlock{};

    level = 1;
    exp = 0;
    expToNext = Config::PlayerExpFirstLevel;
    statPoints = 0;

    spellKillCarry = 0;
    mana = 0;

    RefreshHealth();
    health = maxHealth;
}

void Player::Update(float delta, const InputState &input, float lookYaw)
{
    yaw = lookYaw;

    UpdateAfflictions(delta);

    //------------------------------------------------------------------------------
    // What FLEET and a speed buff are actually worth.
    //
    // Set here every frame rather than at the moment something is equipped, for the
    // same reason SetGearMods is pushed every frame: the sum can change from four
    // directions - a trait slot, a level unlocking one, a weapon, a buff running out
    // - and a body that cached its own speed would keep whichever of them it was
    // told about last.
    //
    // Floored well above zero. `moveSpeed` is a signed fraction like every other in
    // Modifiers, so a future row that slowed the player has somewhere to live, and a
    // stack of them that reached -100% would be a character that cannot move at all.
    //------------------------------------------------------------------------------
    const float speedScale = 1.0f + Combined().moveSpeed;

    body.maxSpeed = Config::MaxSpeed*((speedScale < 0.25f) ? 0.25f : speedScale);

    // A staggered body takes no input at all - see Player::staggerTime. The crouch
    // is left alone deliberately: it is a POSE rather than an action, and snapping
    // the camera up out of a crouch because something hit you would move the view
    // at the exact moment the player is trying to read where the blow came from.
    const Vector2 move = IsStaggered() ? Vector2{ 0.0f, 0.0f } : input.move;
    const bool jump = !IsStaggered() && input.jump;

    body.Update(delta, yaw, move, jump, input.crouch);

    lastHitAge += delta;

    //------------------------------------------------------------------------------
    // The running buff's clock, if there is one.
    //
    // Cleared outright the frame it reaches zero rather than left at a spent
    // Modifiers checked against HasBuff, so Combined() never has to ask - a
    // buffMods sitting there at its old values with buffTimeLeft at exactly 0.0
    // is one float comparison away from being summed in for one more frame than
    // it should be.
    //------------------------------------------------------------------------------
    if (buffTimeLeft > 0.0f)
    {
        buffTimeLeft -= delta;

        if (buffTimeLeft <= 0.0f)
        {
            buffTimeLeft = 0.0f;
            buffMods = Modifiers{};

            RefreshHealth();
            if (mana > MaxMana()) mana = MaxMana();
        }
    }
}

//----------------------------------------------------------------------------------
// The poison and the stagger, ticking down.
//
// Both run on the same rule the enemy's own DOTs follow (see EnemyManager::Update):
// this is happening TO the body rather than being something it is doing, so nothing
// - not blocking, not a page being open, not the blow that lands next - pauses it.
//
// Ordered before everything else in Update so a tick that kills is a death this
// frame rather than one carried into the next, which is the same reason the world's
// death check sits at the bottom of UpdateInGame.
//----------------------------------------------------------------------------------
void Player::UpdateAfflictions(float delta)
{
    if (staggerTime > 0.0f) staggerTime -= delta;

    if (venomTime <= 0.0f) return;

    venomTime -= delta;
    venomTickTimer -= delta;

    if (venomTickTimer > 0.0f) return;

    venomTickTimer += Config::BountyVenomTick;

    // Through TakeDamage, so it is one funnel with every other source - a poison
    // that wrote to `health` directly would skip the death the rest of the game
    // reads off IsAlive()
    TakeDamage(venomDamagePerTick);
}

void Player::ApplyVenom(int damagePerTick)
{
    if ((damagePerTick <= 0) || !IsAlive()) return;

    // Refreshed rather than stacked, the same rule Enemy::ApplyMagicEffect follows
    // for its own DOTs: a second dose is the clock going back to full, not a second
    // poison queued behind the first.
    if (venomTime <= 0.0f) venomTickTimer = Config::BountyVenomTick;

    venomTime = Config::BountyVenomTime;

    // The harder of the two blows carries. A weak follow-up must not be able to
    // downgrade the poison the first one left.
    if (damagePerTick > venomDamagePerTick) venomDamagePerTick = damagePerTick;
}

void Player::Stagger(float seconds)
{
    if ((seconds <= 0.0f) || !IsAlive()) return;

    // Refreshed to the longer of the two, never summed - see Enemy::Stun, which is
    // the same decision for the same reason
    if (seconds > staggerTime) staggerTime = seconds;
}

void Player::UpdateAttacks(float delta, const bool pressedIn[2], const bool heldIn[2], const AttackStyle newStyles[2])
{
    bool pressed[HandCount];
    bool held[HandCount];

    for (int h = 0; h < HandCount; h++)
    {
        styles[h] = newStyles[h];
        pressed[h] = pressedIn[h];
        held[h] = heldIn[h];
    }

    if (blockCooldown > 0.0f)
    {
        blockCooldown -= delta;
        if (blockCooldown < 0.0f) blockCooldown = 0.0f;
    }

    const int off = (int)Hand::Left;
    const int main = (int)Hand::Right;

    if (styles[off] == AttackStyle::Block)
    {
        // Still recovering from the last one - a fresh press does not raise it
        // early. Only checked at rest: a shield already on its way down finishes
        // that motion on its own rather than being yanked back up mid-drop.
        if ((blockCooldown > 0.0f) && (attacks[off].phase == AttackState::Phase::Idle))
        {
            pressed[off] = false;
        }

        // A shield that is up, raising or lowering is both hands' business - no
        // swinging while it is out. See TakeDamageFrom for the other half: the
        // shield forcing itself back down the instant it actually stops a blow.
        if (attacks[off].phase != AttackState::Phase::Idle)
        {
            pressed[main] = false;
            held[main] = false;
        }
    }

    for (int h = 0; h < HandCount; h++)
    {
        attacks[h].Update(delta, pressed[h], held[h], styles[h]);
    }

    if ((styles[off] == AttackStyle::Block) && IsBlocking()) blockActiveTime += delta;
    else blockActiveTime = 0.0f;

    //------------------------------------------------------------------------------
    // The guard's charges, refilled while the arm is at rest.
    //
    // Here rather than on the frame the shield reaches full, because "at rest" is
    // the one state that cannot be reached without the guard having come all the
    // way down - which is exactly the condition a fresh set of charges is owed to.
    // A refill on the way up would let a player who feathered the button hold an
    // endless guard.
    //------------------------------------------------------------------------------
    if (attacks[off].phase == AttackState::Phase::Idle) blockChargesLeft = blockCharges;
}

void Player::SetBlockCharges(int charges)
{
    // Floored at one: a shield that stopped nothing at all would be a hand given up
    // for a damage reduction, which is not what any row in the table says it is
    blockCharges = (charges < 1) ? 1 : charges;

    // A guard already up keeps what it has left, but can never hold more than the
    // shield now in the hand is worth - swapping down mid-block must not carry the
    // heavier shield's remaining charges onto the lighter one
    if (blockChargesLeft > blockCharges) blockChargesLeft = blockCharges;
}

void Player::DropBlock()
{
    const int off = (int)Hand::Left;

    if (attacks[off].phase != AttackState::Phase::Idle)
    {
        attacks[off].phase = AttackState::Phase::Back;
        attacks[off].timer = 0.0f;
    }

    blockCooldown = Config::BlockRecoveryTime;
    blockActiveTime = 0.0f;
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

    // What a shield is actually worth now - see the long note at the top of
    // combat/Modifiers.h on why this is a fraction read off Combined() rather
    // than a stat point that used to just inflate the health pool. Floored at
    // 1 the same way a landed blow always is elsewhere (see ResolveDamage) -
    // a hit that connects and does nothing reads as the game dropping it.
    const float scale = 1.0f + Combined().damageTaken;

    amount = (int)(amount*((scale > 0.0f) ? scale : 0.0f) + 0.5f);
    if (amount < 1) amount = 1;

    const int before = health;

    health -= amount;
    if (health < 0) health = 0;

    if (health < before)
    {
        lastHitAge = 0.0f;
        lastHitDirectional = false;
        hitPending = true;
    }
}

bool Player::TakeDamageFrom(int amount, Vector3 source, bool melee)
{
    // A raised shield only covers what it is pointed at. Reach is irrelevant
    // here - InCone is being used purely for the angle.
    const bool guarded = IsBlocking() && InCone(EyePosition(), Forward(), source, 1000.0f, Config::BlockArc);

    bool parried = false;

    if (guarded)
    {
        // A parry is a melee blow caught in the instant the shield went up, not
        // one caught after a wait - see the note on blockActiveTime. A shield
        // raised early and held still stops the blow, just not for free.
        parried = melee && (blockActiveTime <= Config::ParryWindow);

        amount = parried ? 0 : (int)(amount*Config::BlockDamageScale);
        if (!parried && (amount < 1)) amount = 1;

        //--------------------------------------------------------------------------
        // A charge spent, and the guard dropped only when the last one goes.
        //
        // What separates the three shields - see WeaponStats::blockCharges. A guard
        // that fell to the first blow made a shield worth least in the one situation
        // it is for, which is a pack: the first skeleton was blocked and the two
        // behind it were not.
        //
        // A PARRY spends nothing. It is a timed read rather than something the
        // shield absorbed, and charging it would mean the better the player's timing
        // the sooner their guard broke.
        //--------------------------------------------------------------------------
        if (!parried) blockChargesLeft--;

        if (parried || (blockChargesLeft <= 0)) DropBlock();
    }

    const int before = health;

    TakeDamage(amount);

    // Overrides what TakeDamage just set: this blow DOES have a direction, so the
    // indicator gets the one thing a bare TakeDamage cannot give it.
    if (health < before)
    {
        lastHitFrom = source;
        lastHitDirectional = true;
    }

    return parried;
}

void Player::Heal(int amount)
{
    if ((amount <= 0) || !IsAlive()) return;

    health += amount;
    if (health > maxHealth) health = maxHealth;
}

void Player::GiveMana(int amount)
{
    if (amount <= 0) return;

    mana += amount;
    if (mana > MaxMana()) mana = MaxMana();
}

void Player::ApplyBuff(BuffKind kind)
{
    activeBuff = kind;
    buffMods = BuffAt(kind).mods;
    buffTimeLeft = Config::BuffDuration;

    // Felt on the frame it lands, the same rule SetModifiers follows for a
    // trait - see the note there.
    RefreshHealth();

    if (mana > MaxMana()) mana = MaxMana();
}

//----------------------------------------------------------------------------------
// The line the fight actually uses: what was spent, with the trait loadout's own
// point bonuses and conversions applied on top.
//
// Held gear no longer reaches this function - see the note on SetGearMods. It
// works through Combined() instead, read directly by whatever it actually
// changes, so a weapon in the hand can never inflate the STAT line the way a
// shield's old constitution point used to inflate the health pool it was
// silently derived from.
//----------------------------------------------------------------------------------
StatBlock Player::Fighting() const
{
    return ApplyModifiers(stats, mods);
}

//----------------------------------------------------------------------------------
// What combat actually reads - see the class note on ApplyBuff. Built fresh
// rather than cached: `mods`, `buffMods` and `gearMods` change on different
// clocks (an equip, a tick of a timer, a weapon swap) and a cached sum is a
// fourth value that can fall out of step with any of them.
//----------------------------------------------------------------------------------
Modifiers Player::Combined() const
{
    return ModifiersAdd(ModifiersAdd(mods, buffMods), gearMods);
}

void Player::SetModifiers(const Modifiers &bonus)
{
    mods = bonus;

    // The same rule a spent point follows: the pool grows and the health in it grows
    // with it, so a trait granting constitution is felt on the frame it goes on.
    // Unlike a held weapon this is a permanent choice, so it carries UP rather than
    // being clamped - see the note on SetGearMods for why the two differ.
    RefreshHealth();

    if (mana > MaxMana()) mana = MaxMana();
}

void Player::SetGearMods(const Modifiers &bonus)
{
    gearMods = bonus;

    // Clamped down, never carried up - see the note on SetGearMods. Deliberately
    // NOT RefreshHealth, which does the opposite for a trait on purpose: a
    // trait is a permanent choice and its constitution should carry the current
    // health up with it, where gear is put down again a moment later and must
    // not be a heal with a cooldown of one frame. Nothing on today's table
    // grants flatHealth from a weapon, but the rule is written here rather than
    // assumed.
    maxHealth = MaxHealth();
    if (health > maxHealth) health = maxHealth;

    if (mana > MaxMana()) mana = MaxMana();
}

bool Player::CanOneHandTwoHanders() const
{
    return Combined().freeTwoHander > 0;
}

int Player::MaxHealth() const
{
    const StatBlock fighting = Fighting();

    const int total = Config::PlayerMaxHealth
                    + StatBonusHealth(fighting, Config::PlayerMaxHealth)
                    + Combined().flatHealth;

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
    const Modifiers combined = Combined();

    int total = Config::BaseSpellPower
              + StatBonusSpellPower(fighting, Config::BaseSpellPower)
              + combined.flatSpell;

    // A fraction on top of what arcane already bought, added rather than compounded -
    // the same rule every other modifier fraction follows
    if (combined.spellPower != 0.0f) total = (int)(total*(1.0f + combined.spellPower) + 0.5f);

    return (total < 1) ? 1 : total;
}

int Player::WeaponDamage(int weaponDamage) const
{
    const Modifiers combined = Combined();

    int total = WeaponDamageWith(Fighting(), weaponDamage) + combined.flatDamage;

    // The melee/ranged counterpart of SpellPower's own fraction, added rather
    // than compounded - the same rule every other modifier fraction follows.
    if (combined.damageDealt != 0.0f) total = (int)(total*(1.0f + combined.damageDealt) + 0.5f);

    return (total < 1) ? 1 : total;
}

//----------------------------------------------------------------------------------
// Mana.
//
// The pool does NOT move with arcane - see the note in Config for why it used to
// and why it no longer does. What is left is a flat budget every character has the
// same amount of, plus whatever gear and traits are granting on top.
//
// Still a RESERVOIR and not a faucet: the only way to fill it is to kill something.
//----------------------------------------------------------------------------------
int Player::MaxMana() const
{
    const int total = Config::ManaMax + Combined().flatMana;

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

    mana += kills*(Config::ManaPerKill + Combined().manaPerKill);

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

    mana += whole*(Config::ManaPerKill + Combined().manaPerKill);

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

    // A level is a fresh start, not just a bigger one - see RefreshHealth's own
    // note on why raising the pool alone is not the same as filling it. Once at
    // the end rather than once per level crossed: a kill that pays out three at
    // once still only heals once, on the level the character actually lands on.
    if (gained > 0)
    {
        RefreshHealth();
        health = maxHealth;
        mana = MaxMana();
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

void Player::RevertPoints(const StatBlock &pending)
{
    for (int i = 0; i < (int)Stat::Count; ++i)
    {
        const Stat stat = (Stat)i;

        const int spent = StatValue(pending, stat);
        if (spent <= 0) continue;

        StatAdd(stats, stat, -spent);
        statPoints += spent;
    }

    RefreshHealth();
}
