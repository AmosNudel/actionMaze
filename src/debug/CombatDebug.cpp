#include "debug/CombatDebug.h"

#include "core/Config.h"
#include "entities/EnemyManager.h"
#include "entities/Player.h"
#include "raymath.h"
#include "rlgl.h"
#include "world/Level.h"

#include <cmath>

namespace
{
    constexpr float FlashTime = 0.25f;
}

void CombatDebug::NoteHit(int count)
{
    if (count <= 0) return;

    hitFlash = FlashTime;
    lastHitCount = count;
}

void CombatDebug::NoteBlades(const Capsule capsules[2])
{
    blades[0] = capsules[0];
    blades[1] = capsules[1];
}

void CombatDebug::Update(float delta)
{
    if (hitFlash > 0.0f) hitFlash -= delta;
}

// The blade exactly as the sweep tested it, plus a mark on the tip - which is the
// end you aim, and the one that tells you at a glance whether the swing went
// where you thought it did.
void CombatDebug::DrawBlade(const Capsule &blade, const WeaponStats &stats, Color color) const
{
    if (!stats.melee || (blade.radius <= 0.0f)) return;

    DrawCapsuleWires(blade.a, blade.b, blade.radius, 8, 4, color);
    DrawSphereWires(blade.b, blade.radius*0.5f, 5, 5, color);
}

void CombatDebug::Draw(const Player &player, const WeaponStats stats[2], const Level &level,
                       const EnemyManager &enemies) const
{
    if (!visible) return;

    rlDrawRenderBatchActive();
    rlDisableDepthTest();       // Overlay: it is here to be read, not occluded

    const float floorY = level.FloorHeight();

    // Wall cells as the grid understands them, not as they are modelled
    const Map &grid = level.Grid();
    const float size = grid.CellSize();

    for (int cz = 0; cz < grid.Depth(); cz++)
    {
        for (int cx = 0; cx < grid.Width(); cx++)
        {
            if (!grid.IsWall(cx, cz)) continue;

            const Vector3 center = grid.CellCenter(cx, cz);
            DrawCubeWiresV({ center.x, floorY + 0.05f, center.z }, { size, 0.1f, size }, Fade(ORANGE, 0.6f));
        }
    }

    // Enemy capsules and what they can see. Built by the same helper the hit code
    // calls, so what is drawn is what was tested.
    for (const Enemy &enemy : enemies.All())
    {
        if (!enemy.IsAlive()) continue;

        const Capsule body = BodyCapsule(enemy.body.position, enemy.height, enemy.body.radius);

        DrawCapsuleWires(body.a, body.b, body.radius, 10, 5, GREEN);

        // The sight line, coloured by how far the detection meter has filled: red
        // has noticed nothing, yellow is mid-fill, lime is certain. The line
        // itself is still the raw one - whether stone is in the way - so a red
        // line through clear air means the player is outside the cone.
        const bool clear = level.LineOfSight(enemy.Center(), player.EyePosition());
        const Color heat = ColorLerp(RED, LIME, enemy.detection);

        DrawLine3D(enemy.Center(), player.EyePosition(), Fade(clear ? heat : Fade(RED, 0.4f), 0.5f));

        // The cone it is actually looking down, on the floor at its feet, so which
        // way a body is facing is something the player can be shown rather than told
        const float half = Config::EnemyViewCone*0.5f*DEG2RAD;
        const Vector3 feet = { enemy.body.position.x, enemy.body.position.y + 0.05f,
                               enemy.body.position.z };

        for (int side = -1; side <= 1; side += 2)
        {
            const float edge = enemy.yaw + side*half;
            const Vector3 arm = { feet.x - sinf(edge)*Config::EnemyAggroRange, feet.y,
                                  feet.z - cosf(edge)*Config::EnemyAggroRange };

            DrawLine3D(feet, arm, Fade(heat, 0.35f));
        }
    }

    // Both hands' blades, so an off hand weapon is not a mystery
    const Color flash = (hitFlash > 0.0f) ? YELLOW : SKYBLUE;

    DrawBlade(blades[(int)Hand::Right], stats[(int)Hand::Right], flash);
    DrawBlade(blades[(int)Hand::Left], stats[(int)Hand::Left], Fade(VIOLET, 0.7f));

    rlDrawRenderBatchActive();
    rlEnableDepthTest();
}

void CombatDebug::DrawUi(const Player &player, const EnemyManager &enemies) const
{
    if (!visible) return;

    const int width = 250;
    const int height = 74;
    const int x = 10;
    const int y = GetScreenHeight() - height - 40;

    DrawRectangle(x, y, width, height, Fade(BLACK, 0.6f));
    DrawRectangleLines(x, y, width, height, GREEN);

    DrawText("COMBAT DEBUG (F5)", x + 10, y + 8, 10, GREEN);
    DrawText(TextFormat("enemies alive: %i", enemies.AliveCount()), x + 10, y + 24, 10, RAYWHITE);
    DrawText(TextFormat("last swing hit: %i", lastHitCount), x + 10, y + 40, 10, RAYWHITE);
    DrawText(player.IsBlocking() ? "BLOCKING" : "not blocking", x + 10, y + 56, 10,
             player.IsBlocking() ? YELLOW : GRAY);
}
