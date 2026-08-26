#pragma once

#include "combat/Collider.h"
#include "combat/Weapon.h"
#include "core/Hand.h"
#include "raylib.h"

class EnemyManager;
class Level;
class Player;

//----------------------------------------------------------------------------------
// F5: what the collision and combat code actually sees.
//
// "Why did that swing miss" needs an answer you can look at rather than reason
// about, and that is far more true of a swept blade than it was of a cone: the
// cone was a shape you could hold in your head, where a blade capsule depends on
// the pose, the magnification and the arc it travelled through.
//
// So it draws the exact capsules that were tested - both hands' blades and every
// enemy's body, from the same BodyCapsule the hit code calls - plus the wall cells
// the grid thinks are solid. Nothing here approximates the real volume; if the
// overlay and the hit disagree, one of them is a bug.
//----------------------------------------------------------------------------------
class CombatDebug
{
public:
    void Toggle() { visible = !visible; }
    bool IsVisible() const { return visible; }

    // Called when a sweep lands, so the blade can flash on contact
    void NoteHit(int count);

    // This frame's blade capsules, straight from the sweep that used them. Kept
    // rather than rebuilt at draw time: the overlay must show the volume that was
    // tested, and rebuilding it would be a second chance to get it wrong.
    void NoteBlades(const Capsule blades[2]);

    void Update(float delta);
    void Draw(const Player &player, const WeaponStats stats[2], const Level &level,
              const EnemyManager &enemies) const;    // Inside BeginMode3D
    void DrawUi(const Player &player, const EnemyManager &enemies) const;
private:
    void DrawBlade(const Capsule &blade, const WeaponStats &stats, Color color) const;

    bool visible = false;
    float hitFlash = 0.0f;
    int lastHitCount = 0;
    Capsule blades[2];
};
