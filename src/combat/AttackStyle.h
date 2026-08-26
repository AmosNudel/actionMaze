#pragma once

//----------------------------------------------------------------------------------
// How a weapon attacks. The motion is always rest -> end -> rest; the style only
// decides the timing, the easing, and whether it waits at full extension.
//
//   Swing   axes, swords, hammers, halberd - arc the head through the crosshair
//   Thrust  spear, sword_D (rapier) - drive the point straight down the view axis
//   Block   shields - raise, hold while the button is held, lower
//   Cast    wand, staffs - push forward and settle back slowly
//   Throw   daggers - short weapons, so they leave the hand
//
// This lives in combat rather than render because attack rate is a gameplay
// number: how long a weapon leaves you committed is a balance decision that the
// animation follows, not the other way round.
//----------------------------------------------------------------------------------
enum class AttackStyle { Swing, Thrust, Block, Cast, Throw };

const char *AttackStyleName(AttackStyle style);

// Seconds out to full extension, seconds back, and whether the weapon waits out
// there while the button is held
struct StyleTiming
{
    float out = 0.13f;
    float back = 0.22f;
    bool holdsAtFullExtension = false;
};

const StyleTiming &TimingFor(AttackStyle style);

// Whether this style resolves a melee hit at all. Cast and Throw do not: they put
// something in the air instead - a mote of the selected magic, or the weapon that
// left the hand - and the damage travels with it.
bool IsMeleeStyle(AttackStyle style);
