#include "core/Input.h"

#include "combat/Magic.h"

InputState InputState::Poll()
{
    InputState in;

    in.move.x = (float)(IsKeyDown(KEY_D) - IsKeyDown(KEY_A));
    in.move.y = (float)(IsKeyDown(KEY_W) - IsKeyDown(KEY_S));
    in.look = GetMouseDelta();
    in.wheel = GetMouseWheelMove();
    in.offhand = IsKeyDown(KEY_LEFT_SHIFT);

    in.jump       = IsKeyPressed(KEY_SPACE);
    in.crouch     = IsKeyDown(KEY_LEFT_CONTROL);
    in.attack     = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    in.attackHeld = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    in.altAttack  = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    in.altAttackHeld = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    in.interact   = IsKeyPressed(KEY_E);
    in.characterSheet = IsKeyPressed(KEY_TAB);

    // KEY_ONE..KEY_NINE are contiguous in raylib, so the row is a loop rather than
    // nine cases. Bounded by the table's own length, not by the keyboard's: a
    // ninth school becomes reachable by adding the row and nothing else, and a
    // shorter table cannot be indexed past its end from the number row.
    for (int i = 0; i < (int)Magic::Count && i < 9; ++i)
    {
        if (IsKeyPressed(KEY_ONE + i)) in.magic = i;
    }

    return in;
}
