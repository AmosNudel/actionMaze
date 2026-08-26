#pragma once

//----------------------------------------------------------------------------------
// Which hand something is in.
//
// Lives in core because both sides need it: gameplay tracks an attack per hand,
// rendering tracks a pose per hand, and neither should have to include the other
// to say which one it means. The values double as array indices.
//----------------------------------------------------------------------------------
enum class Hand { Right = 0, Left = 1 };

constexpr int HandCount = 2;
