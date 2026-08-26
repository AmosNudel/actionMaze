#pragma once

#include "combat/StatBlock.h"
#include "core/Config.h"

//----------------------------------------------------------------------------------
// The four numbers everything that fights carries, and the one place a blow becomes
// a number.
//
//     CONSTITUTION   health
//     ARMS           weapon damage
//     SKILL          critical strikes - both how often and how hard
//     ARCANE         spell power, which is what magic is multiplied against
//
// This is its own module rather than fields on Player because the moment enemies
// carry the same four, "how much does this hit for" stops belonging to either side
// of the fight. A Warrior with high constitution is genuinely tougher and a Rogue
// with high skill genuinely crits, out of the same arithmetic the player uses.
//
// --- StatBase is a PIVOT, not a floor ---------------------------------------------
// Everything starts at 10 in all four, and 10 contributes exactly nothing: a fresh
// character has the base health and the weapon damage written elsewhere, no more.
// Above 10 adds. BELOW 10 SUBTRACTS, and that is not a degenerate case - it is how
// an enemy kind gets a weakness. The Minion sits under the line on constitution and
// is made of paper for it, which is a fact about the kind and not a rank it can
// grow out of.
//
// --- Why every bonus is a percentage of a BASE -------------------------------------
// A stat bonus is a fraction of the character's own base line - the weapon's own
// damage, the player's own health - added, never compounded. That is the same idiom
// the enemy rank growth uses, and it is load-bearing. Enemy health grows LINEARLY
// with rank; if a stat instead multiplied a damage figure that was itself climbing
// per level, the player's damage would be quadratic against linear enemy health and
// would lap the curve inside a run.
//
// So: points spent are linear, and the two curves stay the same shape.
//
// --- What is deliberately NOT here ------------------------------------------------
// Elements and resistances. The mobile game this is ported from has five damage
// elements, each answered by one of the four stats, and it is what makes carrying a
// particular spell into a particular room a decision. It is not in this pass and it
// is not forgotten: ResolveDamage below is the single funnel every point of damage
// already goes through, so an element argument and a defender's block is one
// signature change here rather than a search through every call site. Magic already
// has eight schools waiting to be given one.
//----------------------------------------------------------------------------------
// StatBlock itself is in combat/StatBlock.h - see the note there for why the type
// and the rates are in different files.

// Which row a character sheet is editing, and the order the four are shown in
enum class Stat { Con = 0, Arms, Skill, Arcane, Count };

const char *StatName(Stat stat);

// Read and write one stat by index, so the sheet can loop rather than naming all
// four in every function it has
int  StatValue(const StatBlock &block, Stat stat);
void StatAdd(StatBlock &block, Stat stat, int amount);

//----------------------------------------------------------------------------------
// Derived values.
//
// Each of these is the BONUS a block grants, not the final figure - the caller adds
// it to its own base, because the player's base health and a Minion's are different
// numbers and neither belongs in here.
//----------------------------------------------------------------------------------
int StatBonusHealth(const StatBlock &block, int baseHealth);
int StatBonusDamage(const StatBlock &block, int baseDamage);

// A weapon's own damage raised by ARMS, floored at 1. The base is the WEAPON's,
// never the character's, so arms scales what is being held rather than replacing
// it - a dagger stays a dagger in the hands of a strong character. One function
// because two paths ask for it, the swing and the throw, and a formula written
// twice is a formula that gets tuned once.
int WeaponDamageWith(const StatBlock &block, int baseDamage);
int StatBonusSpellPower(const StatBlock &block, int baseSpellPower);

// Chance in [0, 1], capped. Crit multiplies damage, so SKILL alone has nothing to
// multiply and must lose to ARMS alone early - the cap is deliberately expensive so
// that it wins late instead, which is what makes the split a decision.
float StatCritChance(const StatBlock &block);
// The multiplier on a critical hit. Uncapped, and floored at 1: a crit that hit for
// less than an ordinary blow would be a worse outcome than not critting.
float StatCritDamage(const StatBlock &block);

//----------------------------------------------------------------------------------
// Resolving a blow.
//
// The one place damage is turned into a number, used by every path that deals any:
// both hands' weapons, every school of magic, and every enemy swing.
//
//   raw       what the attacker's own weapon, spell or table row already worked out
//   attacker  whose crit multiplier applies when `crit` is set
//   crit      whether this blow rolled a critical - see StatRollCrit
//
// Always returns at least 1: a blow that connects has to do something, or a heavily
// reduced hit reads as the game having dropped an input.
//
// It deliberately does NOT touch the block reduction or the death test - those live
// in Enemy::TakeDamageFrom and Player::TakeDamageFrom, so that every source of
// damage is covered by them without each caller remembering, and this is upstream
// of both.
//----------------------------------------------------------------------------------
int ResolveDamage(int raw, const StatBlock &attacker, bool crit);

// Rolls whether a blow from `attacker` crits, on raylib's generator - so a logged
// seed reproduces a fight along with the level it happened in.
bool StatRollCrit(const StatBlock &attacker);

// The same roll with a flat bonus on top, which is what a weapon's `critBonus`
// column feeds. The total is re-clamped against StatCritChanceCap, so the ceiling
// stays one number in one place and no stack of weapons can carry a character past
// it however many of them are held.
bool RollWeaponCrit(const StatBlock &attacker, float bonusChance);
