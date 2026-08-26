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
    bool toggleCombatDebug = false; // Pressed this frame (F5, the combat overlay)

    // Throws the level away and builds another from a fresh seed. Judging what a
    // generator produces means seeing twenty of its maps, and without this that
    // means twenty restarts - which in practice means never seeing twenty.
    bool regenerateLevel = false;   // Pressed this frame (F6)

    // Opens and closes the character sheet, which pauses the world and takes the
    // mouse back. Tab because it is the only key on the board that already means
    // this to everyone who will ever play it.
    bool characterSheet = false;    // Pressed this frame (Tab)

    // Which school of magic to switch to, or -1 for "leave it alone". The number
    // row, 1 through 8, indexing combat/Magic.h's enum from zero.
    //
    // A debug binding and honestly labelled as one: there is no spellbook, no
    // unlock and no cost, so this is not how a player will ever choose their magic.
    // It exists because the alternative while the schools are being tuned is a
    // recompile per school, and eight recompiles to compare eight colours is how
    // colours end up never being compared.
    int magic = -1;

    static InputState Poll();
};
