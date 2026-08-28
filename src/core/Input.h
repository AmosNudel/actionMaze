#pragma once

#include "raylib.h"

//----------------------------------------------------------------------------------
// One frame of player intent.
//
// Key polling is isolated here so bindings can change - or input can be faked,
// replayed or driven by a gamepad - without touching gameplay code.
//----------------------------------------------------------------------------------
struct InputState
{
    Vector2 move = { 0.0f, 0.0f };  // x: strafe (+ right), y: forward (+ forward), in [-1, 1]
    Vector2 look = { 0.0f, 0.0f };  // Mouse delta, in pixels
    float wheel = 0.0f;             // Mouse wheel this frame, cycles weapons
    bool offhand = false;           // Shift held: the wheel drives the left hand

    bool jump       = false;        // Pressed this frame
    bool crouch     = false;        // Held
    bool attack     = false;        // Pressed this frame  (melee swing / fire)
    bool attackHeld = false;        // Held                (automatic fire)
    bool altAttack  = false;        // Pressed this frame  (off hand: block / cast)
    bool altAttackHeld = false;     // Held                (keeps a shield up)
    bool interact   = false;        // Pressed this frame  (pick up, open, use)

    // Opens and closes the character sheet, which pauses the world and takes the
    // mouse back. Tab because it is the only key on the board that already means
    // this to everyone who will ever play it.
    bool characterSheet = false;    // Pressed this frame (Tab)

    // Which school of magic to switch to, or -1 for "leave it alone". The number
    // row, 1 through 8, indexing combat/Magic.h's enum from zero.
    int magic = -1;

    static InputState Poll();
};
