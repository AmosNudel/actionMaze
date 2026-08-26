#pragma once

#include "raylib.h"

//----------------------------------------------------------------------------------
// The two axes that turn one table row into a spread of fights.
//
// An enemy is a KIND and a RANK. Config::EnemyTypes holds the kind's line at rank 1;
// the rank is rolled per body off the level's DEPTH and scales that line up. The
// rolled rank is then re-bucketed into a TIER against the player's live level, and
// the tier is what the player can actually see.
//
// TWO INPUTS, TWO JOBS, and keeping them apart is the whole design:
//
//   DEPTH   owns the NUMBER. The roll centres on the depth, so going down a floor
//           is a difficulty step whether or not the player just levelled. The map
//           carries the curve.
//   PLAYER  owns the FEEL. The tier compares that rank to the player's level, so
//           levelling turns the same floor into more cleanup without rewriting what
//           the floor is made of. That is what "I got stronger" looks like: the
//           world stayed put and the character pulled ahead of it.
//
// Anchoring the roll on the player instead keeps every fight relevant forever, and
// makes every level-up hollow - new spawns climb with the character, so the reward
// for levelling is that nothing changed.
//----------------------------------------------------------------------------------

// Mode of the rank roll on a given depth (1-based). Always at least 1.
int RankCentreForDepth(int depth);

// Rolls the rank for one body about to spawn on `depth`. Always at least 1.
//
// Drawn from a window around the centre, weighted so the centre is the mode, and
// the two sides fall off DIFFERENTLY - which is the design, not an accident:
//
//   BELOW   a slow, heavy-tailed decay. Low ranks on a deep floor are cleanup once
//           the player has levelled past the centre, which is the reward for
//           growing mid-floor and what makes a level-up land without moving the map.
//   ABOVE   a steep geometric one. A body above the centre is a SPIKE, not a norm:
//           it should be the fight remembered from that floor, which means rare.
int RollRankForDepth(int depth);

// A kind's rank 1 figure, scaled to `rank`. Growth is a fraction of the kind's OWN
// base and is not compounded, which is what preserves the durability-against-threat
// roles the table is built on - a flat step would converge the Minion and the
// Warrior within a dozen ranks.
int RankedHealth(int baseHealth, int rank);
int RankedDamage(int baseDamage, int rank);

// What the kill pays. The one figure here that COMPOUNDS, and the asymmetry is
// deliberate: level costs are geometric, so a linearly growing reward means the
// number of bodies a level demands runs away with itself. The late game would not
// be harder, it would be longer, and there is no amount of skill that answers that.
int RankedExp(int baseExp, int rank);

//----------------------------------------------------------------------------------
// Tiers - what makes a rank ahead of the player a different FIGHT.
//
// Rank alone is not enough, and the arithmetic says why. Rank growth is linear off
// the base, so rank 1 -> 2 is +30% health but rank 20 -> 21 is +4.5%. By mid-run
// every body the player meets sits within a few percent of every other one, and a
// tint promising a difference the numbers have stopped delivering is worse than no
// tint at all.
//
// A tier is the DISTANCE from the player's level, bucketed, carrying MULTIPLIERS on
// top of the linear rank curve. That fixes the compression at its root: the spread
// between a washed-out body and a hot one is the same proportion at level 40 as at
// level 1, because it is a multiplier and not an offset.
//
// The tier is also the single source of the tint, so what an enemy looks like and
// what it is made of cannot drift apart.
//----------------------------------------------------------------------------------
enum class EnemyTier
{
    Worn = 0,       // Two or more ranks behind the player: cleanup
    Even,           // At their level or one behind: the ordinary fight
    Elite,          // One ahead
    Champion,       // Two or more ahead: the fight remembered from the floor

    Count
};

struct EnemyTierDef
{
    const char *name;
    float health;
    float damage;
    float speed;    // Patrol and chase speed
    float scale;    // Draw scale, and the body radius and height derived from it
    float swing;    // Multiplies the cooldown's RECIPROCAL: above 1 swings faster

    //------------------------------------------------------------------------------
    // POISE: how much damage this body absorbs before it flinches, as a fraction of
    // its own maximum health. Zero means it flinches at every blow.
    //
    // It exists because the flinch is a stun the player controls. Every hit puts a
    // body into its Hit clip, and a body in its Hit clip is not swinging - so against
    // anything that takes a dozen blows to kill, the winning move is to stand in
    // front of it and attack as fast as possible, and the fight never happens. The
    // more health a tier has, the more completely that works on it.
    //
    // So the tier that has the most health is the one that has to be able to ignore
    // it. A champion soaks a share of its pool before giving ground, which turns the
    // exchange back into something the player has to time rather than out-click.
    //
    // As a FRACTION of health rather than a flat figure, so it keeps meaning the same
    // thing at rank 30 as at rank 3 - the same rule every other column here follows.
    //------------------------------------------------------------------------------
    float poise;

    Color tint;     // WHITE at Even
};

// Which tier a rank-`rank` body is for a player at `playerLevel`
EnemyTier TierFor(int rank, int playerLevel);

// A tier's row. Clamps out of range to Even.
const EnemyTierDef &TierAt(EnemyTier tier);
