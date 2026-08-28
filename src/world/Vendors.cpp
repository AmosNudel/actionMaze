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

//----------------------------------------------------------------------------------
// The three adventurers, fitted to a height and bound to the level's own shading.
//
// Every figure here is MEASURED rather than declared - see the note on
// VendorManager::Look. The pack does not author its characters at one scale and does
// not stand them on their origin, so a table of hand-tuned numbers would be a table
// that silently goes wrong the day the pack is re-exported.
//
// The lit shader is the same one the walls use, which is the whole point: a
// character lit by a different rule than the room they are standing in reads as a
// sticker on the screen. They carry no texture of their own - the pack colours these
// with flat materials - and the shader multiplies the default white texture by that
// colour, so they come through as authored.
//----------------------------------------------------------------------------------
void VendorManager::Load(AssetManager &assets)
{
    glow = &GlowTexture(assets);

    Shader &lit = assets.GetShader("shaders/lit.vs", "shaders/lit.fs");

    for (int i = 0; i < (int)NpcKind::Count; ++i)
    {
        Look &look = looks[i];

        look = Look{};

        const char *path = NpcAt((NpcKind)i).modelPath;

        if ((path == nullptr) || !FileExists(AssetManager::Resolve(path).c_str()))
        {
            TraceLog(LOG_WARNING, "VENDORS: %s has no model, drawing the aura alone",
                     NpcAt((NpcKind)i).name);
            continue;
        }

        look.model = &assets.GetModel(path);

        for (int m = 0; m < look.model->materialCount; m++) look.model->materials[m].shader = lit;

        const BoundingBox box = GetModelBoundingBox(*look.model);
        const float tall = box.max.y - box.min.y;

        // A model with no height at all would divide by zero and then be drawn at
        // infinity, which is a great deal harder to diagnose than a wrong size
        look.scale = (tall > 1e-3f) ? (Config::VendorHeight/tall) : 1.0f;

        // Where its feet are relative to its origin, already scaled - these are
        // authored around the waist, so without this every vendor stands shin-deep
        // in the floor
        look.foot = box.min.y*look.scale;

        TraceLog(LOG_INFO, "VENDORS: %s is %s, %.2f tall at x%.2f",
                 NpcAt((NpcKind)i).name, GetFileName(path), tall, look.scale);
    }
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

        // Out of step with each other on purpose - see the note on Vendor::phase
        vendor.phase = (float)GetRandomValue(0, 628)*0.01f;

        vendors.push_back(vendor);
    }
}

void VendorManager::Update(float delta)
{
    // The portal's rate, because it is the portal's object. Three auras turning at
    // three speeds would read as three different mechanisms.
    spin += delta*Config::PortalSpinRate;

    // Not wrapped. The breath is a sine of it and a float holds a run's worth of
    // seconds with far more precision than a body moving 4cm needs.
    clock += delta;
}

//----------------------------------------------------------------------------------
// The vendor: their aura, and them standing in it.
//
// Aura first, body second. Both are additive-blended objects over a solid one, and
// drawing the body last means the light is behind them rather than smeared across
// their face - which is the difference between somebody lit from below and somebody
// with a lens flare stuck to them.
//----------------------------------------------------------------------------------
void VendorManager::Draw(const Camera3D &camera) const
{
    for (const Vendor &vendor : vendors)
    {
        const NpcDef &def = NpcAt(vendor.kind);

        if (glow != nullptr)
        {
            BeamLook look;

            look.colour = def.colour;
            look.radius = Config::VendorMarkerRadius;
            look.height = Config::VendorAuraHeight;
            look.spin = spin;
            look.motes = 6;
            look.moteSize = 0.20f;

            //------------------------------------------------------------------
            // Almost all pool, barely any column - which is what turns the marker
            // this used to be into an aura somebody is standing in.
            //
            // The column has to go: it occupied exactly the space the character
            // now does, and a haze drawn up through a body reads as the body
            // being on fire rather than as it being lit. What is worth keeping is
            // the part that says WHERE - the pool on the floor and the motes
            // turning round it - so those stay at full strength.
            //------------------------------------------------------------------
            look.columnAlpha = 34;
            look.poolAlpha = 235;

            DrawBeam(camera, *glow, vendor.at, look);
        }

        DrawFigure(vendor, camera);
    }
}

//----------------------------------------------------------------------------------
// One vendor's body: facing the player, breathing, swaying.
//
// All three are procedural because the model has no rig to animate - see the note in
// Vendors.h. What that buys is still worth having: a figure that turns to watch you
// cross the room is doing the one thing a shopkeeper has to do, and it costs a yaw.
//
// The breath and the sway are deliberately on periods that do not divide into each
// other, so the pair never settles into a visible loop the way two multiples would.
//----------------------------------------------------------------------------------
void VendorManager::DrawFigure(const Vendor &vendor, const Camera3D &camera) const
{
    const Look &look = looks[(int)vendor.kind];

    if (look.model == nullptr) return;

    //------------------------------------------------------------------------------
    // Facing, from where the camera actually is - not from where it is looking.
    //
    // Flattened to the horizontal, so a vendor does not tip over backwards when the
    // player stands on something or looks up at them. The negation is Body::Update's
    // own convention for facing TOWARD a point, and it is the same call the enemies
    // make when they turn to close.
    //------------------------------------------------------------------------------
    const float dx = camera.position.x - vendor.at.x;
    const float dz = camera.position.z - vendor.at.z;

    float yaw = 0.0f;

    if ((dx*dx + dz*dz) > 1e-4f) yaw = atan2f(dx, dz);

    yaw += sinf(vendor.phase + clock*(2.0f*PI/Config::VendorSwayPeriod))*Config::VendorSwaySwing;

    const float breath = sinf(vendor.phase + clock*(2.0f*PI/Config::VendorBreathPeriod));

    // Half the swing added back as a bias, so the body rises and falls ABOUT the
    // floor rather than spending half its cycle sunk into it
    const float lift = (breath + 1.0f)*0.5f*Config::VendorBreathRise;

    const Vector3 stand = { vendor.at.x, vendor.at.y - look.foot + lift, vendor.at.z };

    // Through the model's own transform rather than DrawModelEx's axis-angle, so
    // the scale and the rotation compose in one matrix and the shared asset is
    // handed back exactly as it was found - the same idiom Level::DrawProp uses
    look.model->transform = MatrixRotateY(yaw);
    DrawModel(*look.model, stand, look.scale, WHITE);
    look.model->transform = MatrixIdentity();
}

//----------------------------------------------------------------------------------
// Their names, floating over their heads.
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
        // Off the CHARACTER's height rather than the marker's - the tall column
        // this used to be anchored to is gone, and a name still floating where its
        // top used to be would be a name hanging in the air above nobody
        const Vector3 head = { vendor.at.x,
                               vendor.at.y + Config::VendorHeight + Config::VendorLabelLift,
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
