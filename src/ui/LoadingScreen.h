#pragma once

//----------------------------------------------------------------------------------
// The page shown while Game is doing work with nothing to click - booting, or
// building a fresh run behind the scenes. One page for both: they look the same,
// only the label differs.
//
// Stateless on purpose: Game already has to track how long it has been in this
// stage to know when to move on (see Config::BootLoadingMinTime and
// RunLoadingMinTime), so the dots are driven off that same elapsed time rather
// than a second clock kept here that could disagree with it.
//----------------------------------------------------------------------------------
class LoadingScreen
{
public:
    void Draw(const char *label, float elapsed) const;
};
