#pragma once

#include "core/Hand.h"

class ViewModel;
class Arsenal;

//----------------------------------------------------------------------------------
// The two rules every equip has to follow, wherever the request came from - the
// mouse wheel (Game::UpdateWorld) or the Inventory tab's hand buttons
// (CharacterSheet::Update):
//
//   - A shield only works in the off hand. Player::IsBlocking and everything
//     built on it - the block cooldown, the parry window (see Player.cpp) -
//     read Hand::Left specifically, so a shield handed to the main hand would
//     raise on a click and never do anything: no damage reduction, no parry,
//     nothing to show for the button press. Refused outright rather than
//     silently rerouted to the other hand, which would move something the
//     player did not ask to move.
//   - One weapon never occupies both hands at once. Equipping it into `hand`
//     clears it out of `other` first, the same as a two-handed weapon already
//     empties the other hand - a dagger is not two daggers just because both
//     hands happened to cycle onto the same index.
//   - The main hand is never empty. There is nothing for LMB to do with
//     nothing in it, so `index` of -1 for Hand::Right is refused outright.
//
// `index` of -1 otherwise empties `hand` and is allowed - the off hand is
// where a shield or a free hand for casting comes from.
//----------------------------------------------------------------------------------
void EquipWeapon(ViewModel &viewModel, const Arsenal &arsenal, Hand hand, int index);
