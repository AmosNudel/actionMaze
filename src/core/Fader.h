#pragma once

//----------------------------------------------------------------------------------
// A black-screen cut between one thing being shown and the next.
//
// Every hop between the front-end screens - boot loading to the main menu, the
// menu to options or credits, loading to the game itself - goes through the same
// out-then-in: the current screen fades to black, and on the one frame it is
// fully black the caller is free to swap what is drawn under it, because nothing
// is visible to cut. It then fades back in on whatever that turns out to be.
//
// Owns only the timer and which half of the cut it is in - not WHAT is showing,
// which stays the caller's business. See Game::RequestAppState for how a caller
// drives it: set what should show once black, call Start, and check AtBlack()
// each frame to know when to actually swap.
//----------------------------------------------------------------------------------
class Fader
{
public:
    // Begins fading out from whatever is on screen now. Safe to call again mid
    // fade - a second Start before the first has resolved just restarts the cut,
    // which only happens if a caller changes its mind about where it is going.
    void Start();

    void Update(float delta);

    // The overlay itself, at the current alpha. Draw last, over everything else
    // the screen this frame drew.
    void Draw() const;

    // True on the single frame the screen is fully black - see the class note.
    bool AtBlack() const;

    bool IsIdle() const { return phase == Phase::Idle; }

private:
    enum class Phase { Idle, Out, In };

    Phase phase = Phase::Idle;
    float alpha = 0.0f;          // Doubles as the overlay's alpha - 0 clear, 1 black
    bool atBlackThisFrame = false;
};
