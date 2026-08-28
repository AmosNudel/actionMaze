#include "combat/Equip.h"

#include "combat/Weapon.h"
#include "progress/Arsenal.h"
#include "render/ViewModel.h"

void EquipWeapon(ViewModel &viewModel, const Arsenal &arsenal, Hand hand, int index,
                 bool freeTwoHander)
{
    // See the class note: a shield handed to the main hand would be a button
    // that does nothing forever, so the request is refused rather than acted
    // on halfway.
    if ((index >= 0) && (hand == Hand::Right) &&
        ((arsenal.TagsAt(index) & TagBlocking) != 0))
    {
        return;
    }

    // The main hand may never end up empty - there is nothing else for LMB to
    // do, and a run always owns at least the starting sword, so there is always
    // something real to put here instead. See Game::UpdateWorld's wheel handler
    // for the other half: it continues the cycle past the empty slot rather
    // than stopping on it, so scrolling never just sticks here.
    if ((index < 0) && (hand == Hand::Right)) return;

    const Hand other = (hand == Hand::Left) ? Hand::Right : Hand::Left;

    // One weapon, one hand - putting it here means it is not also still there.
    if ((index >= 0) && (viewModel.SlotIndex(other) == index)) viewModel.SetSlot(other, -1);

    viewModel.SetSlot(hand, index);

    //------------------------------------------------------------------------------
    // A two-handed weapon takes both hands, so there is never a frame where one is
    // in view alongside a second item - see Weapon.cpp's IsTwoHanded for which
    // weapons these are.
    //
    // Two directions, both handled the same way: landing a two-hander in `hand`
    // empties whatever `other` was holding, and landing anything real in `other`
    // - cycling past empty does not count, but a weapon does - bumps a two-hander
    // that was sitting in `hand` back out. The player is always the one who just
    // acted; the hand that goes empty is always the one that did not.
    //
    // Unless the player bought their way out of it - see the note on the
    // declaration. The check is here at the bottom rather than at the top because
    // everything above it still applies: TITAN GRIP lifts the two-handed rule and
    // nothing else, so a shield is still off-hand only and one weapon is still not
    // two weapons.
    //------------------------------------------------------------------------------
    if (freeTwoHander) return;

    const bool newIsTwoHanded = (index >= 0) && ((arsenal.TagsAt(index) & TagTwoHanded) != 0);

    const int otherIndex = viewModel.SlotIndex(other);
    const bool otherIsTwoHanded = (otherIndex >= 0) && ((arsenal.TagsAt(otherIndex) & TagTwoHanded) != 0);

    if (newIsTwoHanded || (otherIsTwoHanded && (index >= 0)))
    {
        viewModel.SetSlot(other, -1);
    }
}
