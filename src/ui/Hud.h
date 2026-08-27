#pragma once

#include "combat/Magic.h"
#include "entities/EnemyManager.h"
#include "render/ViewModel.h"
#include "ui/Minimap.h"
#include "ui/UiBars.h"
#include "world/Chaos.h"
#include "world/Event.h"
#include "world/Treasure.h"
#include "world/Vendors.h"

class AssetManager;
class Level;
class Player;
class Spellbook;

//----------------------------------------------------------------------------------
// Screen space overlay: minimap, crosshair, and the four bars.
// Drawn after EndMode3D(), so everything here is in pixels.
//
// --- Design pixels, not screen pixels ---------------------------------------------
// Every size below is a DESIGN figure multiplied by UiScale() - see UiTheme.h. It
// used to be raw pixels, and raw pixels are wrong on any monitor that is not the one
// they were typed on: a 236px health bar is most of a corner at 720p and a smear at
// 1440p, and there is no single number that is right on both. One scale moves all of
// it together.
//
// --- What goes where, and why -----------------------------------------------------
// The layout is the mobile game's, deliberately, so the two read as one game:
//
//   top left       the minimap, the depth under it, then health, then the schools
//   top, beside    the floor  - how much of it is left to quell, and the running event
//   bottom, wide   the run    - level and progress, across the whole screen
//   dead centre    what they are pointing at
//
// That is a change from three corners and a middle, and it is worth stating why. The
// old arrangement put health bottom-left and progress bottom-right, which reads well
// on a 16:9 monitor and falls apart on a wide one: the two corners end up a metre
// apart and the eye has to travel the whole screen to take in the HUD. Stacking the
// player's own readouts down the left under the map keeps everything that is about
// the CHARACTER in one column, and leaves the run's bar - the one thing that is
// genuinely about the whole screen - spanning it.
//
// Health still does not share its line with anything. It is the number read
// constantly and under pressure, and anything beside it is competing for attention at
// exactly the moment it must not.
//----------------------------------------------------------------------------------
class Hud
{
public:
    // Puts the bar art on the GPU. Once, after the window exists - the textures are
    // the AssetManager's from that point on.
    void Load(AssetManager &assets);

    // Fog of war only ever lifts, so it has to be told when the map underneath it
    // has been replaced
    void ResetMap(const Level &level);

    // The one thing on the HUD that carries state from frame to frame
    void Update(const Level &level, const Player &player);

    // `inPortal` is whether the player is standing in the way down right now, which
    // is the one piece of world state the HUD reacts to rather than reports.
    void Draw(const Player &player, const ViewModel &viewModel, const Level &level,
              Magic magic, const ChaosState &chaos, const EventManager &events,
              const Spellbook &spells, const VendorManager &vendors,
              const TreasureManager &treasure, const EnemyManager &enemies,
              const Camera3D &camera, bool inPortal) const;

private:
    //------------------------------------------------------------------------------
    // One bar, drawn the same way every time.
    //
    // The ornamented frame out of the shared art when it loaded, and a flat track and
    // fill when it did not. Every bar on this HUD is this function at a different
    // width and hue, which is what stops four bars written four times from drifting
    // into four different shapes - and it is why adding a fifth (stamina, a boss's
    // health) is a call rather than a function.
    //
    // `colour` is what the FALLBACK fills with, and is also the tint on the event
    // bar's own white strip. The other three hues live in the art.
    //
    // `fill` is clamped, so a dead or over-healed player still draws sanely.
    //------------------------------------------------------------------------------
    void DrawBar(BarHue hue, float x, float y, float width, float fill, Color colour) const;

    // How tall a bar comes out, art or no art. Every vertical offset on this HUD is
    // measured from it, so the two answers cannot lay the corner out differently.
    float BarPixels() const;

    void DrawCrosshair() const;

    //------------------------------------------------------------------------------
    // The red bar for the last blow that landed, on the side of the screen it came
    // in from - see Player::lastHitFrom and its neighbours.
    //
    // Reads the player's own record of the hit rather than taking one as an
    // argument: Player already has to remember it for as long as the fade lasts,
    // and a second copy kept here would be the same three fields drifting out of
    // step with the first the moment a floor change or a respawn touched one and
    // not the other.
    //------------------------------------------------------------------------------
    void DrawHurtIndicator(const Player &player, const Camera3D &camera) const;

    // Where the floor's two bars start and how wide they run. Shared, because the
    // chaos bar and the event bar under it have to agree to the pixel.
    float FloorBarLeft() const;
    float FloorBarWidth() const;

    //------------------------------------------------------------------------------
    // The left column, top to bottom: the depth, health, and the schools.
    //
    // One function rather than three, because they are a STACK - each one starts
    // where the last one ended - and three functions that each worked out their own
    // top from the same constants is three places for that sum to drift. It returns
    // nothing; the column is self-contained and nothing else lays out against it.
    //------------------------------------------------------------------------------
    void DrawLeftColumn(const Player &player, const Level &level, Magic magic,
                        const Spellbook &spells) const;

    //------------------------------------------------------------------------------
    // "E to trade", when the player is standing at a vendor.
    //
    // Near the crosshair rather than in a corner, because it is the one line on this
    // HUD that is about something the player is standing IN. A prompt in the corner
    // for a thing under your feet is a prompt nobody reads.
    //------------------------------------------------------------------------------
    void DrawVendorPrompt(const Player &player, const VendorManager &vendors) const;

    //------------------------------------------------------------------------------
    // "E to open", standing at an unopened chest, and "FOUND: <name>" fading out
    // for a few seconds after it is opened - see TreasureManager::LastFoundName.
    // The same near-the-crosshair placement as the vendor prompt above, for the
    // same reason.
    //------------------------------------------------------------------------------
    void DrawTreasurePrompt(const Player &player, const TreasureManager &treasure) const;

    //------------------------------------------------------------------------------
    // A health bar over every CHAMPION in view, and over nothing else.
    //
    // Only champions, and that restraint is the whole design. A bar over every body
    // is a screen of bars with the important one lost among them; a bar that appears
    // over exactly the thing that takes forty swings to kill says "this one is
    // different" before it says anything about numbers.
    //
    // It answers the question the game could not answer before: a champion absorbs so
    // many blows that with no readout at all it is indistinguishable from one that
    // takes no damage. Which is exactly what it looked like.
    //
    // Screen space, so it runs after EndMode3D - which is why it needs the camera
    // handed to it rather than reading one.
    //------------------------------------------------------------------------------
    void DrawChampionBars(const EnemyManager &enemies, const Camera3D &camera) const;

    // The run's bar, spanning the bottom of the screen. Level, progress to the next
    // one, and the unspent points - which is the only part that ever shouts, and has
    // to: points arrive silently in the middle of a fight, and a player who never
    // learns they have any is playing a game whose progression does nothing.
    void DrawProgress(const Player &player) const;

    // What is left of the floor. Beside the map rather than over the middle of the
    // screen, because it is about the level rather than about the player - and the
    // middle of the screen belongs to the crosshair.
    void DrawChaos(const ChaosState &chaos, const EventManager &events) const;

    // The running objective: what it is, how it is going, and how long is left.
    // Under the chaos bar, because it is the other thing that is about the floor -
    // and only while one is actually running, since a slot that is empty most of the
    // time is one the player stops looking at.
    void DrawEvent(const EventManager &events) const;

    // The cleared banner, and the prompt while standing in the portal. One function
    // because they are two halves of one message - the floor is done, here is what
    // to do about it - and they are never both at full strength at once.
    void DrawFloorState(const ChaosState &chaos, const EventManager &events,
                        bool inPortal) const;

    Minimap minimap;
    UiBars bars;
};
