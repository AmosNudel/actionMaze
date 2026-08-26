#include "render/Beam.h"

#include "core/Config.h"
#include "raymath.h"
#include "rlgl.h"

#include <cmath>

namespace
{
    // How many billboards the column is stacked out of. Stacked rather than drawn
    // as one tall quad because the glow is round: stretched into a tall rectangle it
    // becomes an ellipse with a hard waist, where stacked it becomes a soft shaft
    // that is brightest where the discs overlap - which is what a column of light
    // actually looks like.
    constexpr int ColumnSteps = 12;
}

void DrawBeam(const Camera3D &camera, const Texture2D &glow, Vector3 at, const BeamLook &look)
{
    if ((glow.id == 0) || (look.lit <= 0.0f)) return;

    const Rectangle whole = { 0.0f, 0.0f, (float)glow.width, (float)glow.height };

    // The screen-aligned up axis. raylib takes its right vector from the view matrix
    // and needs telling which way is up, and the obvious answer of {0,1,0} collapses
    // the quad to a sliver for anything looked at from directly above or below.
    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    const Vector3 up = Vector3CrossProduct(right, forward);

    const Color c = look.colour;

    // A slow breath, so a beam nobody is looking at is still visibly alive. Small -
    // at more than about a tenth it reads as a fault rather than as a pulse.
    const float pulse = 1.0f + 0.06f*sinf(look.spin*0.8f);

    rlDisableDepthMask();
    BeginBlendMode(BLEND_ADDITIVE);

    //------------------------------------------------------------------------------
    // The pool, as a flat quad rather than a billboard.
    //
    // It is the one part that is NOT facing the player: it lies on the floor, and a
    // pool of light that turned to follow the camera would be the giveaway that none
    // of this is solid.
    //------------------------------------------------------------------------------
    {
        const float half = look.radius*1.2f*look.lit*pulse;

        // A hair off the flags, so it is not fighting the floor tile for the same
        // depth. Nothing here writes depth, but the test still runs.
        const float y = at.y + 0.02f;

        rlSetTexture(glow.id);
        rlBegin(RL_QUADS);
            rlColor4ub(c.r, c.g, c.b, (unsigned char)(look.poolAlpha*look.lit));

            rlTexCoord2f(0.0f, 1.0f); rlVertex3f(at.x - half, y, at.z + half);
            rlTexCoord2f(1.0f, 1.0f); rlVertex3f(at.x + half, y, at.z + half);
            rlTexCoord2f(1.0f, 0.0f); rlVertex3f(at.x + half, y, at.z - half);
            rlTexCoord2f(0.0f, 0.0f); rlVertex3f(at.x - half, y, at.z - half);
        rlEnd();
        rlSetTexture(0);
    }

    // The column, tapering. Squared rather than linear, so the narrowing is fast at
    // the bottom and slow at the top - a linear taper reads as a cone, which is a
    // searchlight rather than a doorway.
    for (int i = 0; i < ColumnSteps; ++i)
    {
        const float up01 = i/(float)(ColumnSteps - 1);
        const float taper = 1.0f - up01*up01;

        const float size = look.radius*2.0f*taper*look.lit*pulse;
        if (size <= 0.01f) continue;

        const Vector3 centre = { at.x, at.y + up01*look.height*look.lit, at.z };

        const unsigned char alpha = (unsigned char)(look.columnAlpha*taper*look.lit);

        DrawBillboardPro(camera, glow, whole, centre, up, { size, size },
                         { size*0.5f, size*0.5f }, 0.0f, (Color){ c.r, c.g, c.b, alpha });
    }

    //------------------------------------------------------------------------------
    // The ring: small bright motes carried up the column and fading out at the top.
    //
    // These are what make a beam read as RUNNING rather than as decoration, and it is
    // the motion that does it and not the brightness - a still column of light is a
    // light fitting.
    //------------------------------------------------------------------------------
    for (int i = 0; i < look.motes; ++i)
    {
        const float angle = look.spin + (i/(float)look.motes)*2.0f*PI;

        // Each mote at a different height and all rising at the same rate, so the
        // ring reads as being carried upwards rather than as spinning in place
        const float climb = fmodf(look.spin*0.35f + i/(float)look.motes, 1.0f);

        const Vector3 centre =
        {
            at.x + cosf(angle)*look.radius*0.85f,
            at.y + climb*look.height*look.lit,
            at.z + sinf(angle)*look.radius*0.85f
        };

        // Fading as they reach the top, so they arrive rather than vanish
        const float fade = (1.0f - climb)*look.lit;
        const float size = look.moteSize*(0.6f + 0.4f*fade);

        DrawBillboardPro(camera, glow, whole, centre, up, { size, size },
                         { size*0.5f, size*0.5f }, 0.0f,
                         (Color){ 255, 255, 255, (unsigned char)(230*fade) });
    }

    EndBlendMode();
    rlEnableDepthMask();
}
