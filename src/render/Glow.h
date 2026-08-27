#pragma once

#include "raylib.h"

//----------------------------------------------------------------------------------
// The one soft round light in the game, generated rather than authored.
//
// White, opaque in the middle, falling off to nothing at the rim of a square and cut
// off hard outside the circle so the corners are empty rather than faintly lit.
// Every caller tints it, so it is white here and coloured at the draw.
//
// Generated because it is one falloff curve. A PNG of it would be a file to ship, a
// path to resolve, and a thing that can silently disagree with Config::MoteSoftness
// - which is the number that actually decides whether it reads as a ball of light or
// as a disc with an edge.
//
// Shared rather than owned per system: the magic motes and the portal want exactly
// the same texture, and two copies of it is two things to keep in step for no gain.
// Held by the AssetManager under a name no file has, so it is released with
// everything else and nothing has to remember it.
//----------------------------------------------------------------------------------
class AssetManager;

// Uploads it on the first call and hands back the same texture thereafter. Needs a
// live GL context.
Texture2D &GlowTexture(AssetManager &assets);

//----------------------------------------------------------------------------------
// A small halo of light around a point on the floor - three billboards of the
// texture above: a wide dim halo, a bright core that pulses, and a faint pool of
// light on the floor under it. The same three-billboard shape LootManager's own
// glow fallback already draws for a missing coin prop, factored out here so a
// second caller (a currency drop worth more than an ordinary coin, a pickup
// with no model of its own) does not have to retype the maths.
//
// `at` is where the halo and the core sit - typically a bobbing position - and
// `floorY` is the resting height the pool is drawn flat against, which is not
// always the same number.
//
// Additive and unlit by design, like every other glow in the game: the caller
// is expected to already be inside BeginBlendMode(BLEND_ADDITIVE) with the depth
// mask off, the same as the fallback loop this was pulled out of - wrapping one
// state change around a batch of these is cheaper than toggling it per call.
//
// `intensity` scales the halo and floor pool's opacity and the core's pulse -
// 1 is the plain glow every caller used to get. A pickup or a loot drop wants
// to read as something to walk INTO rather than as dungeon dressing, so both
// pass a number above 1 - see Config::PickupAuraIntensity / LootAuraIntensity.
void DrawAura(const Camera3D &camera, Texture2D &glow, Vector3 at, float floorY,
              Color colour, float coreSize, float haloSize, float pulsePhase,
              float intensity = 1.0f);
