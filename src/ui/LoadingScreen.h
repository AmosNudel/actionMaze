#pragma once

//----------------------------------------------------------------------------------
// The page shown while Game is doing work with nothing to click - booting, or
// building a fresh run behind the scenes. One page for both: they look the same,
// only the label and whether there is a bar differ.
//
// Stateless on purpose: Game already has to track how long it has been in this
// stage to know when to move on (see Config::BootLoadingMinTime and
// RunLoadingMinTime), so the dots are driven off that same elapsed time rather
// than a second clock kept here that could disagree with it. The progress is the
// same arrangement - Game is the only thing that knows how much of the run load is
// done, so it passes the fraction in rather than this asking.
//----------------------------------------------------------------------------------
class LoadingScreen
{
public:
    //------------------------------------------------------------------------------
    // `progress` is 0..1 to draw a bar, or NEGATIVE for none - which is what the
    // boot screen passes, because it has nothing to measure against. A bar that
    // filled by guesswork would be worse than the dots: the one thing a progress
    // bar promises is that it means something.
    //
    // `step` is the line under the bar saying what is being loaded right now, and
    // may be null. Ignored entirely when there is no bar.
    //------------------------------------------------------------------------------
    void Draw(const char *label, float elapsed, float progress = -1.0f,
              const char *step = nullptr) const;
};
