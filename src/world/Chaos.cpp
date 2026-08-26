#include "world/Chaos.h"

#include "core/Config.h"

#include <cmath>

int ChaosForDepth(int depth)
{
    if (depth < 1) depth = 1;

    // Ceiled, so the pool always grows by at least one and is always a whole number
    // of kills. A growth rate this shallow truncates two adjacent floors to the
    // same figure often enough to notice.
    return (int)ceilf(Config::ChaosBase*powf(Config::ChaosGrowth, (float)(depth - 1)));
}

void ResetChaos(ChaosState &chaos, int depth)
{
    chaos.max = ChaosForDepth(depth);
    chaos.left = chaos.max;

    chaos.quelled = false;
    chaos.cleared = false;
    chaos.sinceCleared = 0.0f;
    chaos.dwell = -1.0f;
}

bool Quell(ChaosState &chaos, int amount)
{
    if ((amount <= 0) || chaos.quelled) return false;

    chaos.left -= amount;
    if (chaos.left > 0) return false;

    chaos.left = 0;
    chaos.quelled = true;

    // Not cleared. The floor's events may still be outstanding, and TryClear is what
    // knows - see the note in the header on why the two are separate flags.
    return true;
}

bool TryClear(ChaosState &chaos, bool eventsOutstanding)
{
    if (chaos.cleared) return false;
    if (!chaos.quelled || eventsOutstanding) return false;

    chaos.cleared = true;
    chaos.sinceCleared = 0.0f;

    return true;
}

bool UpdateChaos(ChaosState &chaos, float delta, bool inPortal)
{
    if (!chaos.cleared) return false;

    chaos.sinceCleared += delta;

    //------------------------------------------------------------------------------
    // The dwell.
    //
    // Reset the moment the player steps out, and reset to "not counting" rather
    // than to zero so the two states are distinguishable. A countdown that
    // remembered its progress would let the player stand in the portal, leave to
    // finish a fight, and come back to a floor that ended under them.
    //
    // A dwell and not a touch, because the portal stands in a room the player has
    // every reason to walk through: it is at the far end of the map, which is where
    // the vault and the crypt are too.
    //------------------------------------------------------------------------------
    if (!inPortal)
    {
        chaos.dwell = -1.0f;

        return false;
    }

    if (chaos.dwell < 0.0f) chaos.dwell = 0.0f;

    chaos.dwell += delta;

    if (chaos.dwell < Config::PortalDwell) return false;

    // Consumed here rather than left for the caller to clear. Descending rebuilds
    // the whole floor and resets this state anyway, but a flag that stayed true
    // would fire again on any frame the rebuild did not happen.
    chaos.dwell = -1.0f;

    return true;
}

float BannerAlpha(float sinceCleared)
{
    if (sinceCleared < Config::BannerFadeIn)
    {
        return sinceCleared/Config::BannerFadeIn;
    }

    const float settling = (sinceCleared - Config::BannerFadeIn - Config::BannerHold)
                         / Config::BannerSettle;

    if (settling <= 0.0f) return 1.0f;
    if (settling >= 1.0f) return Config::BannerIdleAlpha;

    // Smoothstep, so it eases out of full rather than stepping down off it
    const float eased = settling*settling*(3.0f - 2.0f*settling);

    return 1.0f + (Config::BannerIdleAlpha - 1.0f)*eased;
}
