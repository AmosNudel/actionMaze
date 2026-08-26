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
