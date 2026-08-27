#include "world/Vendors.h"

#include "core/Config.h"
#include "raymath.h"
#include "render/AssetManager.h"
#include "render/Beam.h"
#include "render/Glow.h"
#include "ui/UiText.h"
#include "ui/UiTheme.h"
#include "world/Level.h"

#include <cmath>

void VendorManager::Load(AssetManager &assets)
{
    glow = &GlowTexture(assets);
}

void VendorManager::Clear()
{
    vendors.clear();
}

void VendorManager::Place(const Level &level)
{
    Clear();

    const Map &map = level.Grid();
    const std::vector<Room> &rooms = map.Rooms();

    const std::vector<int> &where = map.VendorRooms();
    const std::vector<int> &which = map.VendorKinds();

    for (int i = 0; i < (int)where.size(); ++i)
    {
        const int room = where[(size_t)i];

        if ((room < 0) || (room >= (int)rooms.size())) continue;
        if (i >= (int)which.size()) continue;

        Vendor vendor;

        vendor.kind = (NpcKind)which[(size_t)i];

        // Random-with-retries against the room's own furniture, rather than the
        // exact centre an anchor prop usually already occupies - see the note on
        // Level::FindOpenSpotIn.
        vendor.at = level.FindOpenSpotIn(rooms[(size_t)room], Config::VendorMarkerRadius);

        vendors.push_back(vendor);
    }
}

void VendorManager::Update(float delta)
{
    // The portal's rate, because it is the portal's object. Three columns of light
    // turning at three speeds would read as three different mechanisms.
    spin += delta*Config::PortalSpinRate;
}

void VendorManager::Draw(const Camera3D &camera) const
{
    if (glow == nullptr) return;

    for (const Vendor &vendor : vendors)
    {
        const NpcDef &def = NpcAt(vendor.kind);

        BeamLook look;

        look.colour = def.colour;
        look.radius = Config::VendorMarkerRadius;
        look.height = Config::VendorMarkerHeight;
        look.spin = spin;
        look.motes = 6;
        look.moteSize = 0.20f;

        // Dimmer in the column than an event marker and brighter in the pool. A
        // vendor is somewhere to stand rather than something to walk into, and the
        // pool is the part that says where the floor is.
        look.columnAlpha = 90;

        DrawBeam(camera, *glow, vendor.at, look);
    }
}

//----------------------------------------------------------------------------------
// Their names, floating over the columns.
//
// Screen space, so this runs after EndMode3D - which is why it is not part of Draw.
// A vendor with no name over it is three identical columns in three colours, and the
// player has no way to learn which colour is which until they have walked into all
// three.
//
// Culled by the camera's own forward axis rather than by the projection, because
// GetWorldToScreen happily returns a point for something BEHIND the camera and the
// label would appear mirrored across the screen. Distance-culled as well: a name
// legible across the whole map is a name that clutters every other room.
//----------------------------------------------------------------------------------
void VendorManager::DrawLabels(const Camera3D &camera) const
{
    const float ui = UiScale();

    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));

    for (const Vendor &vendor : vendors)
    {
        const Vector3 head = { vendor.at.x, vendor.at.y + Config::VendorMarkerHeight + 0.35f,
                               vendor.at.z };

        const Vector3 toward = Vector3Subtract(head, camera.position);

        if (Vector3DotProduct(toward, forward) <= 0.0f) continue;

        const float away = Vector3Length(toward);

        if (away > Config::VendorLabelRange) continue;

        const NpcDef &def = NpcAt(vendor.kind);

        // Fades out over the last quarter of the range rather than snapping off, so
        // a label does not blink as the player walks the boundary
        const float fadeFrom = Config::VendorLabelRange*0.75f;
        const float alpha = (away <= fadeFrom)
                          ? 1.0f
                          : 1.0f - (away - fadeFrom)/(Config::VendorLabelRange - fadeFrom);

        const Vector2 screen = GetWorldToScreen(head, camera);

        UiTextCenteredOutline(def.name, screen.x, screen.y, 16.0f*ui,
                              Fade(def.colour, alpha));
    }
}

NpcKind VendorManager::At(Vector3 position) const
{
    const float reach = Config::VendorReach;

    for (const Vendor &vendor : vendors)
    {
        const float dx = position.x - vendor.at.x;
        const float dz = position.z - vendor.at.z;

        // Horizontal only. A vendor is on the floor and so is the player; adding the
        // vertical would mean a jump could close a counter.
        if ((dx*dx + dz*dz) <= (reach*reach)) return vendor.kind;
    }

    return NpcKind::Count;
}

Vector3 VendorManager::PointAt(int index) const
{
    if ((index < 0) || (index >= (int)vendors.size())) return { 0.0f, 0.0f, 0.0f };

    return vendors[(size_t)index].at;
}

NpcKind VendorManager::KindAt(int index) const
{
    if ((index < 0) || (index >= (int)vendors.size())) return NpcKind::Count;

    return vendors[(size_t)index].kind;
}
