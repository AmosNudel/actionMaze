#pragma once

//----------------------------------------------------------------------------------
// How hostile this floor still is, and what happens when it stops being.
//
// A floor starts full of CHAOS and every kill quells its own worth of it. That is
// the whole of "clear the floor", and expressing it as a pool rather than as a body
// count is what makes it a quantity to work through instead of "kill the things
// that happen to be standing here": the camps go on refilling until the pool is
// empty, so a floor is as long as it is worth, not as long as its first garrison.
//
// A kill quells exactly what it pays in experience. One number, not two - an enemy
// that is worth more to kill IS more of the floor dealt with, and any rate that
// let the two drift apart would be a way to level without progressing or to
// progress without levelling.
//
// It is also where the objectives that are not killing will go. Destroying the
// thing, holding the ground, walking the prisoner out - all of them drain this same
// pool, and a floor becomes a mix of them rather than one long cull. Nothing here
// needs to change for that; only the things that call Quell.
//
// --- Quelled, and cleared ----------------------------------------------------------
// QUELLED is the pool at zero: the camps stop sending anyone, because there is
// nothing left for a new body to be worth. CLEARED is quelled AND every event
// resolved, and it is what opens the portal.
//
// The gap between them is where the events live, and it is load-bearing. The pool
// usually empties before the player has been everywhere, so a floor that opened its
// portal on the pool alone would make its events optional decoration nobody bothered
// with. And the two must not be one flag: with the pool at zero and one event
// outstanding, treating quelled as cleared leaves the camps refilling and paying full
// price for kills that quell nothing.
//----------------------------------------------------------------------------------
struct ChaosState
{
    int max = 0;        // What this floor started with - the bar's full mark
    int left = 0;       // What is left of it; zero quells the floor

    bool quelled = false;
    bool cleared = false;       // Quelled, and nothing else outstanding

    // Seconds since the floor cleared, for the banner's fade. The banner has to
    // outlive its own announcement: the portal is somewhere else on the map and the
    // walk there is most of a minute, so it fades back to a standing reminder
    // rather than going away.
    float sinceCleared = 0.0f;

    // How long the player has been standing in the portal. Below zero means they
    // are not. See Config::PortalDwell - it is a dwell rather than a touch so that
    // walking through the room the portal is in does not end the floor.
    float dwell = -1.0f;
};

// What a floor at `depth` starts with. Rounded up, so the pool always grows and is
// always a whole number of kills.
int ChaosForDepth(int depth);

// Back to "this floor is hostile", at `depth`. Called for a fresh floor and for
// every regenerate, so nothing survives into the next one.
void ResetChaos(ChaosState &chaos, int depth);

// Something died, or an objective resolved. `amount` comes off the pool; the floor
// quells when it empties. Returns true on the frame that happens, so the caller can
// do the once-only work without polling a flag it would then have to remember it had
// already seen.
//
// Quelling is NOT clearing. Whether the floor is finished is TryClear's answer, and
// it depends on something this function cannot see.
bool Quell(ChaosState &chaos, int amount);

// Is the floor finished? Quelled, and nothing outstanding. Returns true on the frame
// it becomes so, which is the frame the portal goes up and the banner starts.
//
// Called every frame rather than at the moment either half changes, because the two
// halves are answered by different systems on different frames - the pool empties in
// the enemy pass and the last event resolves in the event pass, and whichever of them
// happens second is the one that clears the floor.
bool TryClear(ChaosState &chaos, bool eventsOutstanding);

// Advances the banner clock and the portal dwell. `inPortal` is whether the player
// is standing in it right now; the dwell resets the moment they step out, because a
// countdown that remembered progress would let the player leave, fight, and come
// back to a floor that ends instantly.
//
// Returns true on the frame the dwell completes - the one frame the next floor
// should be built on.
bool UpdateChaos(ChaosState &chaos, float delta, bool inPortal);

// How strongly the cleared banner shows: up, held, then eased back to a standing
// reminder that does not go away.
float BannerAlpha(float sinceCleared);
