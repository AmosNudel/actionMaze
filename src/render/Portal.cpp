#include "render/Portal.h"

#include "core/Config.h"
#include "render/AssetManager.h"
#include "render/Beam.h"
#include "render/Glow.h"

void Portal::Load(AssetManager &assets)
{
    glow = &GlowTexture(assets);
}

void Portal::PlaceAt(Vector3 point)
{
    at = point;
}

void Portal::Reset()
{
    open = false;
    lit = 0.0f;
}

void Portal::Open()
{
    open = true;
}

void Portal::Update(float delta)
{
    // Kept turning whether or not it is up - see the note on `spin`
    spin += delta*Config::PortalSpinRate;

    if (spin > 2.0f*PI) spin -= 2.0f*PI;

    // Up over PortalRaise, and instantly down. There is no fading a portal out: the
    // only thing that shuts one is a new floor, and a new floor has no old portal
    // to fade.
    if (!open)
    {
        lit = 0.0f;

        return;
    }

    if (lit < 1.0f)
    {
        lit += delta/Config::PortalRaise;
        if (lit > 1.0f) lit = 1.0f;
    }
}

bool Portal::Contains(Vector3 point) const
{
    if (!open) return false;

    // A vertical cylinder, height ignored. Testing the height too would mean a
    // portal the player can miss by jumping through it, and the floor it stands on
    // is the only floor there is.
    const float dx = point.x - at.x;
    const float dz = point.z - at.z;

    return (dx*dx + dz*dz) <= (Config::PortalRadius*Config::PortalRadius);
}

//----------------------------------------------------------------------------------
// The way down is a beam like any other - see render/Beam.h.
//
// It is the biggest and the only cold one, which is the whole of what distinguishes
// it from an event marker: every other light in the game is warm, so a cold column
// at the end of a corridor is legible as the way out from a long way off.
//----------------------------------------------------------------------------------
void Portal::Draw(const Camera3D &camera) const
{
    if (glow == nullptr) return;

    BeamLook look;
    look.colour = { Config::PortalColour[0], Config::PortalColour[1],
                    Config::PortalColour[2], 255 };
    look.radius = Config::PortalRadius;
    look.height = Config::PortalHeight;
    look.lit = lit;
    look.spin = spin;
    look.motes = Config::PortalMotes;
    look.moteSize = Config::PortalMoteSize;
    look.columnAlpha = Config::PortalColumnAlpha;

    DrawBeam(camera, *glow, at, look);
}
