#include "render/ViewModel.h"

#include "core/Config.h"
#include "raymath.h"
#include "render/AssetManager.h"

namespace
{
    struct TunedPose
    {
        const char *name;
        Hand hand;
        ViewModelPose pose;
    };

    //------------------------------------------------------------------------------
    // Rest poses, dialled in with the in game editor (T/Y to drag, H to swap
    // hands, G to swap rest/end, U to dump).
    // Order: right, up, forward, pitch, yaw, roll, scale
    //
    // A hand with no entry here starts as the mirror of the other one, so only
    // the grips that mirroring gets wrong need listing twice.
    //------------------------------------------------------------------------------
    const TunedPose TunedRestPoses[] =
    {
        { "axe_A",    Hand::Right, {  0.336f, -0.218f, 0.479f,  -50.0f,  -80.3f,  -22.1f, 0.450f } },
        { "axe_B",    Hand::Right, {  0.280f, -0.260f, 0.419f,  -78.0f,  106.5f,   42.3f, 0.450f } },
        { "axe_C",    Hand::Right, {  0.280f, -0.176f, 0.458f,   -7.2f,  -86.8f,   17.9f, 0.450f } },
        { "dagger_A", Hand::Right, {  0.280f, -0.258f, 0.460f,  -73.1f,  -74.1f,   10.7f, 0.450f } },
        { "dagger_B", Hand::Right, {  0.280f, -0.260f, 0.448f,  -78.0f,  -59.9f,    8.0f, 0.450f } },
        { "halberd",  Hand::Right, {  0.403f, -0.120f, 0.578f,  -27.1f,  -75.1f,    8.0f, 0.450f } },
        { "hammer_A", Hand::Right, {  0.370f, -0.202f, 0.466f,  -15.9f,  -63.4f,    8.0f, 0.450f } },
        { "hammer_B", Hand::Right, {  0.280f, -0.226f, 0.486f,    6.7f,    6.0f,  -21.4f, 0.450f } },
        { "hammer_C", Hand::Right, {  0.316f, -0.336f, 0.637f,   24.8f,   -0.8f,  -14.4f, 0.450f } },
        { "spear_A",  Hand::Right, {  0.280f, -0.260f, 0.311f,  -77.7f,   -9.5f,   11.9f, 0.450f } },
        { "staff_A",  Hand::Right, {  0.280f, -0.260f, 0.550f,  -11.5f,    6.0f,  -10.5f, 0.450f } },
        { "staff_B",  Hand::Right, {  0.469f, -0.260f, 0.647f,  -21.0f,    6.0f,    5.9f, 0.450f } },
        { "sword_A",  Hand::Right, {  0.386f, -0.260f, 0.504f,  -21.1f,  102.5f,    8.0f, 0.450f } },
        { "sword_B",  Hand::Right, {  0.368f, -0.211f, 0.462f,  -22.8f,  -90.3f,   13.6f, 0.450f } },
        { "sword_C",  Hand::Right, {  0.466f, -0.326f, 0.613f,   -5.9f,  -59.6f,   16.7f, 0.450f } },
        { "sword_D",  Hand::Right, {  0.373f, -0.266f, 0.322f,  -64.3f,    1.4f,   22.8f, 0.450f } },
        { "sword_E",  Hand::Right, {  0.398f, -0.336f, 0.555f,  -35.9f,  -71.2f,  -17.6f, 0.450f } },
        { "wand_A",   Hand::Right, {  0.280f, -0.260f, 0.425f,  -78.0f,    6.0f,    8.0f, 0.450f } },

        // Shields are off hand items: posed on the left, mirrored for the right
        { "shield_A", Hand::Left,  { -0.221f, -0.180f, 0.391f,   15.8f, -191.7f,   -4.0f, 0.450f } },
        { "shield_B", Hand::Left,  { -0.402f, -0.260f, 0.608f, -181.0f,   10.2f,  178.3f, 0.450f } },
        { "shield_C", Hand::Left,  { -0.400f, -0.260f, 0.550f, -177.0f,    6.0f,    1.0f, 0.450f } },

        // Weapons whose mirrored left hand grip needed straightening out
        { "axe_C",    Hand::Left,  { -0.280f, -0.176f, 0.458f,    0.8f,  255.6f,   11.0f, 0.450f } },
        { "halberd",  Hand::Left,  { -0.403f, -0.120f, 0.578f,  -27.1f,  260.0f,   -9.2f, 0.450f } },
        { "hammer_B", Hand::Left,  { -0.280f, -0.184f, 0.409f,  -21.2f,   -3.7f,   15.5f, 0.450f } },
        { "hammer_C", Hand::Left,  { -0.280f, -0.263f, 0.410f,  -21.7f,   29.2f,   27.1f, 0.450f } },
        { "sword_C",  Hand::Left,  { -0.466f, -0.326f, 0.613f,   -5.9f,  261.1f,    6.3f, 0.450f } },
    };

    //------------------------------------------------------------------------------
    // End poses: where the weapon wants to be at full extension.
    //
    // Only the right hand is listed. A hand with no entry takes the other hand's
    // rest -> end motion, mirrors it, and applies it to its own rest pose - so
    // the left hand swings the same stroke from wherever it happens to rest,
    // even where its rest pose was hand tuned rather than mirrored.
    //
    // Failing that (neither hand tuned) an end pose is generated from the rest
    // pose and the attack style, so nothing is ever without somewhere to swing.
    //------------------------------------------------------------------------------
    const TunedPose TunedEndPoses[] =
    {
        { "axe_A",    Hand::Right, {  0.050f, -0.264f, 0.527f, -123.4f, -105.0f,  -18.3f, 0.450f } },
        { "axe_B",    Hand::Right, {  0.042f, -0.237f, 0.432f,  -91.0f,  102.0f,  -11.4f, 0.450f } },
        { "axe_C",    Hand::Right, {  0.080f, -0.242f, 0.445f,  -52.4f,  -86.8f,   47.0f, 0.450f } },
        { "dagger_A", Hand::Right, {  0.037f, -0.080f, 1.153f,  -60.4f,  -71.9f,   20.1f, 0.450f } },
        { "dagger_B", Hand::Right, {  0.084f, -0.110f, 0.748f,  -63.0f,  -59.9f,   18.6f, 0.450f } },
        { "halberd",  Hand::Right, {  0.060f, -0.351f, 0.768f,   -5.0f,  -75.1f,   78.1f, 0.450f } },
        { "hammer_A", Hand::Right, {  0.056f, -0.294f, 0.566f,    9.1f,  -63.4f,   80.5f, 0.450f } },
        { "hammer_B", Hand::Right, {  0.042f, -0.258f, 0.509f,  -41.9f,  -65.4f,   45.9f, 0.450f } },
        { "hammer_C", Hand::Right, {  0.042f, -0.317f, 0.510f,  -47.3f,  -63.3f,   32.1f, 0.450f } },
        { "shield_A", Hand::Right, {  0.088f, -0.118f, 0.387f,   15.8f,  191.7f,    4.0f, 0.450f } },
        { "shield_B", Hand::Right, {  0.161f, -0.080f, 0.508f, -181.0f,  -10.2f, -178.3f, 0.450f } },
        { "shield_C", Hand::Right, {  0.160f, -0.080f, 0.450f, -177.0f,   -6.0f,   -1.0f, 0.450f } },
        { "spear_A",  Hand::Right, {  0.193f, -0.210f, 0.875f,  -66.6f,  -38.2f,   13.8f, 0.450f } },
        { "staff_A",  Hand::Right, {  0.094f, -0.269f, 0.797f,  -86.0f,  -13.3f,   15.5f, 0.450f } },
        { "staff_B",  Hand::Right, {  0.235f, -0.220f, 0.847f,  -73.7f,    6.0f,    5.9f, 0.450f } },
        { "sword_A",  Hand::Right, {  0.127f, -0.282f, 0.511f,    0.6f,  106.7f,  -92.2f, 0.450f } },
        { "sword_B",  Hand::Right, {  0.055f, -0.220f, 0.381f,  -98.7f,  -76.2f,    9.2f, 0.450f } },
        { "sword_C",  Hand::Right, {  0.099f, -0.226f, 0.413f,   19.1f,  -83.1f,  120.1f, 0.450f } },
        { "sword_D",  Hand::Right, {  0.131f, -0.216f, 0.419f,  -89.4f,    1.4f,   16.9f, 0.450f } },
        { "sword_E",  Hand::Right, {  0.060f, -0.334f, 0.655f,  -59.0f,  -71.2f,   50.6f, 0.450f } },
        { "wand_A",   Hand::Right, {  0.188f, -0.250f, 0.431f,  -83.8f,    6.0f,    8.0f, 0.450f } },

        // Left hands whose transferred stroke was not good enough by eye. Every
        // other weapon's left end pose is still derived, so retuning its right
        // hand carries across; these three are pinned and will not.
        { "axe_C",    Hand::Left,  { -0.080f, -0.220f, 0.405f,  -69.1f,  278.5f,    0.8f, 0.450f } },
        { "halberd",  Hand::Left,  { -0.060f, -0.258f, 0.592f, -205.5f,  290.8f, -129.1f, 0.450f } },
        { "sword_C",  Hand::Left,  { -0.099f, -0.226f, 0.413f, -179.1f,  284.6f,  -97.1f, 0.450f } },
        { "hammer_C", Hand::Left,  { -0.042f, -0.317f, 0.510f,  -42.0f,   21.7f,  -50.2f, 0.450f } },
    };

    //------------------------------------------------------------------------------
    // Which weapon attacks which way. First matching prefix wins, so the more
    // specific rules come first.
    //------------------------------------------------------------------------------
    struct StyleRule
    {
        const char *prefix;
        AttackStyle style;
    };

    const StyleRule StyleRules[] =
    {
        { "sword_D", AttackStyle::Thrust },  // The rapier: point first, not edge
        { "axe_",    AttackStyle::Swing  },
        { "sword_",  AttackStyle::Swing  },
        { "hammer_", AttackStyle::Swing  },
        { "halberd", AttackStyle::Swing  },
        { "spear_",  AttackStyle::Thrust },
        { "shield_", AttackStyle::Block  },
        { "wand_",   AttackStyle::Cast   },
        { "staff_",  AttackStyle::Cast   },
        { "dagger_", AttackStyle::Throw  },
    };

    const ViewModelPose *FindTunedPose(const TunedPose *table, int count, const std::string &name, Hand hand)
    {
        for (int i = 0; i < count; i++)
        {
            if ((table[i].name[0] != '\0') && (table[i].hand == hand) && (name == table[i].name))
            {
                return &table[i].pose;
            }
        }

        return nullptr;
    }

    AttackStyle StyleForName(const std::string &name)
    {
        for (const StyleRule &rule : StyleRules)
        {
            const std::string prefix = rule.prefix;
            if (name.compare(0, prefix.size(), prefix) == 0) return rule.style;
        }

        return AttackStyle::Swing;
    }

    // A first guess at full extension, so every weapon has something to swing
    // before anyone has dragged its end pose into place. Scaling `right` toward
    // zero moves the weapon toward the crosshair from either hand.
    ViewModelPose DefaultEndPose(const ViewModelPose &rest, AttackStyle style)
    {
        ViewModelPose end = rest;

        switch (style)
        {
            case AttackStyle::Swing:
                end.right = rest.right*0.15f;
                end.up = rest.up + 0.10f;
                end.forward = rest.forward + 0.10f;
                end.pitch = rest.pitch + 25.0f;
                end.roll = rest.roll - 35.0f;
                break;

            case AttackStyle::Thrust:
                end.right = rest.right*0.35f;
                end.up = rest.up + 0.05f;
                end.forward = rest.forward + 0.35f;
                end.pitch = rest.pitch + 5.0f;
                break;

            case AttackStyle::Block:
                end.right = rest.right*0.40f;
                end.up = rest.up + 0.18f;
                end.forward = rest.forward - 0.10f;   // Pulled in toward the face
                break;

            case AttackStyle::Cast:
                end.right = rest.right*0.50f;
                end.up = rest.up + 0.12f;
                end.forward = rest.forward + 0.20f;
                end.pitch = rest.pitch - 20.0f;
                break;

            case AttackStyle::Throw:
                end.right = rest.right*0.30f;
                end.up = rest.up + 0.15f;
                end.forward = rest.forward + 0.30f;
                end.pitch = rest.pitch + 15.0f;
                break;
        }

        return end;
    }

    // Models that live in assets/models/weapons but are never held: arrows are
    // projectile art, kept for whatever the ranged magic ends up throwing.
    bool IsHeldWeapon(const std::string &name)
    {
        return (name.compare(0, 6, "arrow_") != 0);
    }

    // Take one hand's rest -> end stroke, mirror it, and apply it to the other
    // hand's rest pose. The two rest poses need not be mirrors of each other,
    // which is the point: a hand tuned grip keeps its grip and still swings the
    // same stroke.
    ViewModelPose TransferMotion(const ViewModelPose &fromRest, const ViewModelPose &fromEnd,
                                 const ViewModelPose &toRest)
    {
        ViewModelPose out = toRest;
        out.right -= (fromEnd.right - fromRest.right);      // Mirrored: sideways flips
        out.up += (fromEnd.up - fromRest.up);
        out.forward += (fromEnd.forward - fromRest.forward);
        out.pitch += (fromEnd.pitch - fromRest.pitch);
        out.yaw -= (fromEnd.yaw - fromRest.yaw);            // Mirrored
        out.roll -= (fromEnd.roll - fromRest.roll);         // Mirrored
        out.scale += (fromEnd.scale - fromRest.scale);

        return out;
    }

    float RandomSigned(float amount)
    {
        return (GetRandomValue(-1000, 1000)/1000.0f)*amount;
    }

    // Fast at first, settling at the end - which is what picking something up and
    // bringing it to rest looks like
    float EaseOutCubic(float t)
    {
        const float inv = 1.0f - t;

        return 1.0f - inv*inv*inv;
    }
}

ViewModelPose MirrorPose(const ViewModelPose &pose)
{
    ViewModelPose mirrored = pose;
    mirrored.right = -pose.right;
    mirrored.yaw = -pose.yaw;
    mirrored.roll = -pose.roll;

    return mirrored;
}

// Angles interpolate raw, not by the shortest route. The editor accumulates
// them without wrapping, so the stored numbers already describe the arc that was
// dragged - a 200 degree swing stays a 200 degree swing instead of being
// "corrected" into a 160 degree one going the other way.
ViewModelPose LerpPose(const ViewModelPose &from, const ViewModelPose &to, float t)
{
    ViewModelPose out;
    out.right = Lerp(from.right, to.right, t);
    out.up = Lerp(from.up, to.up, t);
    out.forward = Lerp(from.forward, to.forward, t);
    out.pitch = Lerp(from.pitch, to.pitch, t);
    out.yaw = Lerp(from.yaw, to.yaw, t);
    out.roll = Lerp(from.roll, to.roll, t);
    out.scale = Lerp(from.scale, to.scale, t);

    return out;
}

void ViewModel::Load(AssetManager &assets)
{
    const std::string dir = "models/weapons";

    FilePathList files = LoadDirectoryFilesEx(AssetManager::Resolve(dir).c_str(), ".gltf", false);

    const int restCount = (int)(sizeof(TunedRestPoses)/sizeof(TunedRestPoses[0]));
    const int endCount = (int)(sizeof(TunedEndPoses)/sizeof(TunedEndPoses[0]));

    for (unsigned int i = 0; i < files.count; i++)
    {
        Weapon weapon;
        weapon.name = GetFileNameWithoutExt(files.paths[i]);

        if (!IsHeldWeapon(weapon.name)) continue;

        weapon.model = &assets.GetModel(dir + "/" + GetFileName(files.paths[i]));
        weapon.style = StyleForName(weapon.name);

        // Weapons are modelled upright with the tip at +Y, which is what makes a
        // pitch of -90 point the blade down the view axis. Combat reads the same
        // two numbers to place its hit capsule along that axis.
        const BoundingBox box = GetModelBoundingBox(*weapon.model);
        weapon.height = box.max.y - box.min.y;
        weapon.bladeMin = box.min.y;
        weapon.bladeMax = box.max.y;

        const ViewModelPose *right = FindTunedPose(TunedRestPoses, restCount, weapon.name, Hand::Right);
        const ViewModelPose *left = FindTunedPose(TunedRestPoses, restCount, weapon.name, Hand::Left);

        if (right != nullptr) weapon.poses[(int)Hand::Right][(int)PoseSlot::Rest] = *right;
        if (left != nullptr) weapon.poses[(int)Hand::Left][(int)PoseSlot::Rest] = *left;

        // An untuned hand starts as the mirror of the other. With neither tuned
        // this mirrors the default, which at least puts the weapon on the correct
        // side of the screen.
        if (left == nullptr)
        {
            weapon.poses[(int)Hand::Left][(int)PoseSlot::Rest] =
                MirrorPose(weapon.poses[(int)Hand::Right][(int)PoseSlot::Rest]);
        }
        else if (right == nullptr)
        {
            weapon.poses[(int)Hand::Right][(int)PoseSlot::Rest] =
                MirrorPose(weapon.poses[(int)Hand::Left][(int)PoseSlot::Rest]);
        }

        // End poses: tuned if listed, otherwise the other hand's stroke mirrored
        // onto this hand's rest pose, otherwise a guess from the style
        const ViewModelPose *tunedEnd[2] =
        {
            FindTunedPose(TunedEndPoses, endCount, weapon.name, Hand::Right),
            FindTunedPose(TunedEndPoses, endCount, weapon.name, Hand::Left),
        };

        for (int h = 0; h < 2; h++)
        {
            const int other = 1 - h;
            const ViewModelPose &rest = weapon.poses[h][(int)PoseSlot::Rest];

            if (tunedEnd[h] != nullptr)
            {
                weapon.poses[h][(int)PoseSlot::End] = *tunedEnd[h];
            }
            else if (tunedEnd[other] != nullptr)
            {
                weapon.poses[h][(int)PoseSlot::End] =
                    TransferMotion(weapon.poses[other][(int)PoseSlot::Rest], *tunedEnd[other], rest);
            }
            else weapon.poses[h][(int)PoseSlot::End] = DefaultEndPose(rest, weapon.style);
        }

        weapons.push_back(weapon);
    }

    UnloadDirectoryFiles(files);

    if (weapons.empty()) slots[0] = -1;

    TraceLog(LOG_INFO, "VIEWMODEL: Loaded %i weapons", (int)weapons.size());
}

void ViewModel::Unload()
{
    if (target.id != 0)
    {
        UnloadRenderTexture(target);
        target = { 0 };
    }
}

//----------------------------------------------------------------------------------
// The isolated pass. The target carries its own depth buffer, so the weapons
// depth sort against each other and against nothing else - a wall pressed
// against the player's face cannot slice the sword in half.
//----------------------------------------------------------------------------------
void ViewModel::BeginPass(const Camera3D &camera)
{
    const int width = GetScreenWidth();
    const int height = GetScreenHeight();

    // Cheap to check, and it means a resized window does not stretch the weapons
    if ((target.id == 0) || (target.texture.width != width) || (target.texture.height != height))
    {
        if (target.id != 0) UnloadRenderTexture(target);
        target = LoadRenderTexture(width, height);
    }

    BeginTextureMode(target);
    ClearBackground(BLANK);
    BeginMode3D(camera);
}

void ViewModel::EndPass()
{
    EndMode3D();
    EndTextureMode();
}

void ViewModel::Composite() const
{
    if (target.id == 0) return;

    // Render textures come out upside down, hence the negative height
    const Rectangle source = { 0.0f, 0.0f, (float)target.texture.width, -(float)target.texture.height };

    DrawTextureRec(target.texture, source, { 0.0f, 0.0f }, WHITE);
}

void ViewModel::SetSlot(Hand hand, int index)
{
    if ((index < 0) || (index >= (int)weapons.size())) index = -1;

    slots[(int)hand] = index;
}

void ViewModel::Update(const ViewModelInput &in)
{
    bobPhase = in.bobPhase;
    bobAmount = in.walkAmount;
    idleTimer += in.delta;

    for (int h = 0; h < 2; h++)
    {
        blends[h] = in.blend[h];

        if (throwing[h])
        {
            throwTimer[h] += in.delta;

            if (throwTimer[h] >= (Config::ThrowHideTime + Config::ThrowReturnTime))
            {
                throwing[h] = false;
            }
        }

        // One roll per attack, so the whole stroke stays coherent
        if (in.attackStarted[h])
        {
            // A new stroke cancels the recovery outright. Without this the hand
            // would still be sliding a knife into place while the next throw's
            // wind-up was supposed to be driving it, and the wind-up would lose.
            throwing[h] = false;
            throwTimer[h] = 0.0f;

            variations[h].right = RandomSigned(Config::AttackVariationOffset);
            variations[h].up = RandomSigned(Config::AttackVariationOffset);
            variations[h].forward = RandomSigned(Config::AttackVariationOffset);
            variations[h].pitch = RandomSigned(Config::AttackVariationAngle);
            variations[h].yaw = RandomSigned(Config::AttackVariationAngle);
            variations[h].roll = RandomSigned(Config::AttackVariationAngle);
        }
    }
}

void ViewModel::Draw(const Camera3D &camera)
{
    // A hand that has just thrown is empty for a moment. Skipped rather than drawn
    // somewhere off screen, because "off screen" depends on the field of view and
    // an empty hand does not.
    if (!ThrowHidden(Hand::Right)) DrawWeapon(slots[(int)Hand::Right], LivePose(Hand::Right), camera, WHITE);
    if (!ThrowHidden(Hand::Left))  DrawWeapon(slots[(int)Hand::Left], LivePose(Hand::Left), camera, WHITE);
}

void ViewModel::DrawPosePreview(Hand hand, PoseSlot slot, const Camera3D &camera, Color tint)
{
    DrawWeapon(SlotIndex(hand), PoseFor(hand, slot), camera, tint);
}

// Rest blended toward the end pose by however far into the attack we are, with
// the walk sway fading out as the weapon commits
ViewModelPose ViewModel::LivePose(Hand hand) const
{
    const int h = (int)hand;
    const float blend = blends[h];
    const ViewModelPose &rest = PoseFor(hand, PoseSlot::Rest);

    // Offsetting the left hand keeps the two from breathing in lockstep
    const float idlePhase = (hand == Hand::Left) ? Config::WeaponIdleHandPhase : 0.0f;

    // Recovering from a throw. This wins over the attack blend outright: the
    // stroke that threw is finished, and what is being drawn now is the NEXT
    // weapon coming into the hand, not the old one easing back from extension.
    if (throwing[h])
    {
        // Off the bottom of the screen and out toward its own hand, so a right
        // hand reaches down and right for the next knife and a left hand does not
        // reach across the body to do it
        const float side = (hand == Hand::Right) ? 1.0f : -1.0f;

        ViewModelPose entry = rest;
        entry.up -= Config::ThrowReturnDrop;
        entry.right += side*Config::ThrowReturnSide;

        const float t = (throwTimer[h] - Config::ThrowHideTime)/Config::ThrowReturnTime;
        const float slide = (t <= 0.0f) ? 0.0f : ((t >= 1.0f) ? 1.0f : EaseOutCubic(t));

        // Full sway: the hand is not swinging, it is carrying
        return ApplySway(LerpPose(entry, rest, slide), 1.0f, idlePhase);
    }

    if (blend <= 0.0f) return ApplySway(rest, 1.0f, idlePhase);

    // This attack's nudge lands on the end pose only, so the weapon still starts
    // and finishes at rest however the roll came out
    ViewModelPose end = PoseFor(hand, PoseSlot::End);
    end.right += variations[h].right;
    end.up += variations[h].up;
    end.forward += variations[h].forward;
    end.pitch += variations[h].pitch;
    end.yaw += variations[h].yaw;
    end.roll += variations[h].roll;

    return ApplySway(LerpPose(rest, end, blend), 1.0f - blend, idlePhase);
}

// Pose the weapon in camera space. Lifting the result into world space with the
// camera's own transform means no yaw/pitch bookkeeping anywhere.
Matrix ViewModel::PoseMatrix(const ViewModelPose &pose)
{
    const Matrix rotation = MatrixRotateXYZ({ DEG2RAD*pose.pitch, DEG2RAD*pose.yaw, DEG2RAD*pose.roll });
    const Matrix scale = MatrixScale(pose.scale, pose.scale, pose.scale);
    const Matrix offset = MatrixTranslate(pose.right, pose.up, -pose.forward);   // Camera looks down -Z

    return MatrixMultiply(MatrixMultiply(scale, rotation), offset);
}

void ViewModel::DrawWeapon(int i, const ViewModelPose &pose, const Camera3D &camera, Color tint)
{
    if ((i < 0) || (i >= (int)weapons.size())) return;

    Model *model = weapons[i].model;

    const Matrix local = PoseMatrix(pose);
    const Matrix cameraToWorld = MatrixInvert(GetCameraMatrix(camera));

    model->transform = MatrixMultiply(local, cameraToWorld);
    DrawModel(*model, Vector3Zero(), 1.0f, tint);
    model->transform = MatrixIdentity();    // Leave the shared asset as we found it
}

// The camera already bobs, and the weapon rides along with it - this is the
// extra sway on top, which is what actually reads as the weapon moving.
ViewModelPose ViewModel::ApplySway(const ViewModelPose &pose, float walkFade, float idlePhase) const
{
    ViewModelPose swayed = pose;

    // Walking. Both hands stay in step with each other and with the head, since
    // all three are driven by the same footfalls. Fades out during an attack,
    // where it is big enough to fight the swing.
    const float walk = bobAmount*walkFade;
    if (walk > 0.0f)
    {
        const float sway = sinf(bobPhase*PI);
        const float lift = fabsf(cosf(bobPhase*PI));

        swayed.right += sway*Config::WeaponBobSide*walk;
        swayed.up -= lift*Config::WeaponBobUp*walk;     // Dips as the head rises
        swayed.roll += sway*Config::WeaponBobRoll*walk;
    }

    // Breathing. Runs at full strength through an attack too - it is far too
    // small to disturb a swing, and without it a held block sits dead still.
    const float idle = 1.0f - bobAmount;
    if (idle > 0.0f)
    {
        const float t = idleTimer*Config::WeaponIdleSpeed + idlePhase;

        swayed.right += sinf(t*1.1f)*Config::WeaponIdleSide*idle;
        swayed.up += sinf(t*1.6f)*Config::WeaponIdleUp*idle;
        swayed.roll += sinf(t*0.9f)*Config::WeaponIdleRoll*idle;
    }

    return swayed;
}

Vector3 ViewModel::AnchorPoint(Hand hand, PoseSlot slot, const Camera3D &camera) const
{
    const ViewModelPose &pose = PoseFor(hand, slot);

    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    const Vector3 up = Vector3CrossProduct(right, forward);

    Vector3 anchor = camera.position;
    anchor = Vector3Add(anchor, Vector3Scale(right, pose.right));
    anchor = Vector3Add(anchor, Vector3Scale(up, pose.up));
    anchor = Vector3Add(anchor, Vector3Scale(forward, pose.forward));

    return anchor;
}

//----------------------------------------------------------------------------------
// The drawn pose, magnified about the eye until the tip is `reach` away.
//
// Uniform scaling is what keeps this honest. Scaling only the tip would stretch
// the blade off its own grip and break the arc; scaling the whole camera-space
// offset moves hilt and tip together, so the magnified weapon is the drawn weapon
// seen from closer in. A sword drawn with its tip a unit from the eye and its
// grip at 0.4 comes out, at reach 2.1, with the grip at roughly arm's length and
// a blade of sensible length hanging off it.
//----------------------------------------------------------------------------------
Capsule ViewModel::BladeFor(Hand hand, const Camera3D &camera, float reach, float bladeRadius) const
{
    const int i = SlotIndex(hand);
    if ((i < 0) || (i >= (int)weapons.size())) return Capsule{};

    const Matrix local = PoseMatrix(LivePose(hand));

    // Camera space: the eye is the origin, so a point's distance from the eye is
    // just its length and the magnification is a plain scale
    const Vector3 hiltCam = Vector3Transform({ 0.0f, weapons[i].bladeMin, 0.0f }, local);
    const Vector3 tipCam  = Vector3Transform({ 0.0f, weapons[i].bladeMax, 0.0f }, local);

    const float drawnReach = Vector3Length(tipCam);
    const float magnify = (drawnReach > 1e-4f) ? (reach/drawnReach) : 1.0f;

    const Matrix cameraToWorld = MatrixInvert(GetCameraMatrix(camera));

    Capsule blade;
    blade.a = Vector3Transform(Vector3Scale(hiltCam, magnify), cameraToWorld);
    blade.b = Vector3Transform(Vector3Scale(tipCam, magnify), cameraToWorld);
    blade.radius = bladeRadius;

    return blade;
}

void ViewModel::NoteThrown(Hand hand)
{
    throwing[(int)hand] = true;
    throwTimer[(int)hand] = 0.0f;
}

// The empty-handed window: the weapon that was here is in the air, and the next
// one has not been drawn yet
bool ViewModel::ThrowHidden(Hand hand) const
{
    const int h = (int)hand;

    return throwing[h] && (throwTimer[h] < Config::ThrowHideTime);
}

Matrix ViewModel::WorldOrientation(Hand hand, const Camera3D &camera) const
{
    const ViewModelPose pose = LivePose(hand);

    // Rotation only. The pose's own scale and offset describe a miniature held
    // half a metre from the eye, and neither belongs on an object that is about to
    // be a real size somewhere else in the world.
    const Matrix local = MatrixRotateXYZ({ DEG2RAD*pose.pitch, DEG2RAD*pose.yaw, DEG2RAD*pose.roll });

    Matrix world = MatrixMultiply(local, MatrixInvert(GetCameraMatrix(camera)));

    // Drop the camera's position, which the inverse view matrix carries along
    world.m12 = 0.0f;
    world.m13 = 0.0f;
    world.m14 = 0.0f;

    return world;
}

Model *ViewModel::ModelFor(Hand hand) const
{
    const int i = SlotIndex(hand);
    if ((i < 0) || (i >= (int)weapons.size())) return nullptr;

    return weapons[i].model;
}

AttackStyle ViewModel::StyleFor(Hand hand) const
{
    return StyleAt(SlotIndex(hand));
}

AttackStyle ViewModel::StyleAt(int i) const
{
    if ((i < 0) || (i >= (int)weapons.size())) return AttackStyle::Swing;

    return weapons[i].style;
}

const ViewModelPose &ViewModel::PoseFor(Hand hand, PoseSlot slot) const
{
    return PoseAt(SlotIndex(hand), hand, slot);
}

void ViewModel::SetPoseFor(Hand hand, PoseSlot slot, const ViewModelPose &pose)
{
    const int i = SlotIndex(hand);
    if ((i < 0) || (i >= (int)weapons.size())) return;

    // Only this hand and this slot: the other three are separate poses
    weapons[i].poses[(int)hand][(int)slot] = pose;
}

const char *ViewModel::NameAt(int i) const
{
    if ((i < 0) || (i >= (int)weapons.size())) return "empty";

    return weapons[i].name.c_str();
}

float ViewModel::HeightAt(int i) const
{
    if ((i < 0) || (i >= (int)weapons.size())) return 0.0f;

    return weapons[i].height;
}

const ViewModelPose &ViewModel::PoseAt(int i, Hand hand, PoseSlot slot) const
{
    if ((i < 0) || (i >= (int)weapons.size())) return fallbackPose;

    return weapons[i].poses[(int)hand][(int)slot];
}
