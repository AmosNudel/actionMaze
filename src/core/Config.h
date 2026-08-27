#pragma once

// The one include, and it is a header of plain enums and tables with no
// dependencies of its own - so this still costs any module that pulls it in
// nothing. The spawn camps name the kinds of room they would rather hold, and a
// preference expressed as a bare integer would be a preference nobody could read.
#include "combat/MagicKind.h"
#include "combat/StatBlock.h"
#include "world/RoomKind.h"

//----------------------------------------------------------------------------------
// Central place for tunable values. Nothing here includes raylib, so any module
// can pull it in cheaply. Group new constants by system.
//----------------------------------------------------------------------------------
namespace Config
{
    // Window ------------------------------------------------------------------
    // Size of the window when not fullscreen, and the size the window is created
    // at either way - fullscreen resizes it to the monitor immediately after
    constexpr int   ScreenWidth   = 800;
    constexpr int   ScreenHeight  = 450;
    constexpr int   TargetFps     = 60;
    constexpr char  WindowTitle[] = "ActionMaze";

    // Borderless at the desktop resolution rather than a real display mode
    // change: it alt-tabs cleanly and leaves the monitor alone if the game dies.
    //
    // Nothing needs to know the resolution. Every draw measures GetScreenWidth or
    // GetScreenHeight when it runs, and the viewmodel's render target notices its
    // own size is stale and rebuilds, so this is a one line switch either way.
    constexpr bool  Fullscreen    = true;

    // 4x multisampling. Not a nicety here: the dungeon art carries its detail as
    // geometry rather than texture - 5,492 triangles in one wall piece, all small
    // bevelled brick courses - and sub-pixel edges with no multisampling crawl.
    // Requested as a hint before InitWindow; a driver that refuses it costs
    // nothing but the aliasing.
    constexpr bool  AntiAliasing  = true;

    // Longest step any system is allowed to see. One hitch at full speed would
    // otherwise carry a body straight through a wall.
    constexpr float MaxFrameTime  = 1.0f/30.0f;

    // Where runtime data lives, relative to the working directory
    constexpr char  AssetDir[]    = "assets/";

    // Body movement -----------------------------------------------------------
    // Speeds are indoor scale: the template's 20 units/s is ~72 km/h, which turns
    // a three unit corridor into a pinball table. Gravity and jump are unchanged,
    // so the 2.25 unit apex still clears nothing at WallHeight - as intended.
    constexpr float Gravity       = 32.0f;
    constexpr float MaxSpeed      = 9.0f;
    constexpr float CrouchSpeed   = 4.0f;
    constexpr float JumpForce     = 12.0f;
    constexpr float MaxAccel      = 70.0f;
    // Grounded drag
    constexpr float Friction      = 0.86f;
    // Increasing air drag increases strafing speed
    constexpr float AirDrag       = 0.98f;
    // Responsiveness for turning movement direction to looked direction
    constexpr float Control       = 15.0f;
    // Slow down diagonal movement to the same speed as a straight line
    constexpr bool  NormalizeDiagonalInput = true;

    // View / camera -----------------------------------------------------------
    constexpr float MouseSensitivityX = 0.001f;
    constexpr float MouseSensitivityY = 0.001f;
    constexpr float CrouchHeight      = 0.0f;
    constexpr float StandHeight       = 1.0f;
    constexpr float BottomHeight      = 0.5f;
    constexpr float CrouchLerpSpeed   = 20.0f;
    constexpr float HeadBobSpeed      = 3.0f;
    constexpr float WalkLerpSpeed     = 10.0f;
    constexpr float FovDefault        = 60.0f;
    constexpr float FovWalk           = 55.0f;
    constexpr float FovLerpSpeed      = 5.0f;
    constexpr float LeanStrafe        = 0.02f;
    constexpr float LeanForward       = 0.015f;
    constexpr float LeanLerpSpeed     = 10.0f;
    constexpr float BobSide           = 0.1f;
    constexpr float BobUp             = 0.15f;
    constexpr float StepRotation      = 0.01f;

    //--------------------------------------------------------------------------
    // Camera shake.
    //
    // A "trauma" value rather than a fixed kick: shakes from two blows landing a
    // frame apart ADD rather than one replacing the other, so a flurry actually
    // reads as heavier than one hit, and it decays back to zero on its own so
    // nothing here has to remember to turn it off.
    //
    // Squared against the offset and the angle - see FpsCamera::Update - so a
    // little trauma is barely felt and a lot of it is genuinely rough. A linear
    // response would make every hit shake the same amount, which is the one
    // thing a shake should not do.
    //--------------------------------------------------------------------------
    constexpr float CameraShakeDecay      = 2.2f;    // Trauma lost per second
    constexpr float CameraShakeMaxOffset  = 0.05f;   // World units, at trauma 1
    constexpr float CameraShakeOnHit      = 0.45f;   // Trauma added when the player is struck
    constexpr float CameraShakeOnCrit     = 0.30f;   // Trauma added when the player's own blow crits

    // Held weapon -------------------------------------------------------------
    // Sway added on top of the camera's own bob, in the weapon's camera space.
    // Small numbers: the weapon sits about half a unit from the eye.
    constexpr float WeaponBobSide     = 0.018f;
    constexpr float WeaponBobUp       = 0.014f;
    constexpr float WeaponBobRoll     = 2.0f;   // Degrees

    // Breathing, felt only when standing still - it fades in exactly as the walk
    // bob lets go. The three axes run at different rates so the motion never
    // settles into an obvious loop.
    constexpr float WeaponIdleSpeed     = 1.0f;     // Radians per second
    constexpr float WeaponIdleSide      = 0.004f;
    constexpr float WeaponIdleUp        = 0.005f;
    constexpr float WeaponIdleRoll      = 0.4f;     // Degrees
    constexpr float WeaponIdleHandPhase = 1.7f;     // Left hand offset, so the two do not march in lockstep

    // Rolled once per attack and added to the end pose, so no two swings land in
    // exactly the same place. Meant to be felt rather than seen.
    constexpr float AttackVariationOffset = 0.015f; // World units, each axis
    constexpr float AttackVariationAngle  = 3.0f;   // Degrees, each axis

    // World -------------------------------------------------------------------
    // Body radius must stay under half a cell or a body can wedge in a corner.
    // Set from the KayKit dungeon pack rather than chosen: its floor tiles are
    // 4.000 square and its wall pieces 4.000 long to the millimetre, so a cell of
    // 4 lets every model sit at native scale with no correction factor. A grid
    // that disagreed with the art would mean a magic multiplier on every draw,
    // and a permanent question about which of the two was authoritative.
    constexpr float MapCellSize       = 4.0f;
    // How tall a wall stands, matching the pack's pieces, which are all exactly
    // 4.000 from base to cap. Not a ceiling: these levels are open topped and
    // nothing is rendered overhead, so nothing stops at this height either - a
    // projectile that used to die on this plane was hitting a lid the player could
    // neither see nor have guessed at.
    constexpr float WallHeight        = 4.0f;

    //--------------------------------------------------------------------------
    // Dungeon dressing.
    //
    // Walls live on the EDGES between cells, not in cells of their own.
    //
    // Pack 1.1 is modular on a 4-unit grid with 2-unit half steps: `wall` spans a
    // whole edge, `wall_half` exactly half of one, and every junction piece -
    // corner, T-split, crossing - reaches exactly 2.000 down each arm it joins.
    // Lay the centrelines ON the grid lines and the set closes: no piece is ever
    // scaled, no seam is ever left to cover. The old corner posts existed only to
    // hide the seam where a full piece butted a trimmed one, and that seam is
    // gone, so they are gone with it.
    //
    // The cost is that a wall straddles its grid line rather than sitting behind
    // it, so half of it stands in what used to be walkable floor. Collision is
    // built from the same figure, which is what keeps the two honest.
    //--------------------------------------------------------------------------

    // Native depth of every wall piece in the pack, half of it either side of the
    // grid line. A 4-unit corridor is this much narrower than its cells.
    constexpr float WallThickness     = 1.0f;
    constexpr float WallHalfThickness = WallThickness*0.5f;

    // How far a junction piece reaches down each arm it joins. Not a free
    // parameter - it is the pack's own geometry, and the half pieces cut to fill
    // the rest of an edge are the same length by construction.
    constexpr float WallJunctionReach = MapCellSize*0.5f;

    // Flat stone for the fallback boxes drawn when the pack is missing. Close to
    // the pack's own stone once the lit shader has had it.
    //
    // Channels rather than a Color: nothing in this header includes raylib, which
    // is what keeps it cheap for every module to pull in.
    constexpr unsigned char WallStone[3] = { 118, 132, 142 };

    // floor_tile_large is a shallow slab, not a plane: 4.000 square, spanning
    // -0.100 to +0.050 in Y. Its walkable top is the 0.050, so it is drawn this
    // far BELOW floorHeight to put that surface where ResolveBody stands bodies.
    constexpr float FloorTileTop      = 0.05f;

    //--------------------------------------------------------------------------
    // Sky.
    //
    // A cubemap: six square faces laid out as a 4x3 cross, each covering ninety
    // degrees. Nothing here needs a radius or a wrap count - a cubemap has no
    // seam to hide and no pole to pinch, which is the whole reason it replaced
    // the painted dome that came before it.
    //--------------------------------------------------------------------------
    constexpr const char *SkyCubemap  = "textures/skybox/sbs/Cubemap_Red_02-512x512.png";

    // Behind everything, for the frame before the skybox draws and for whatever
    // is left showing if the image is missing. Night, not white.
    constexpr unsigned char Background[3] = { 9, 14, 28 };

    // The one light in the level. Angled rather than straight down, so a wall's
    // face and its cap catch different amounts and modelled relief has something
    // to read against - straight down lights every floor evenly and every wall
    // identically, which is the flat look with extra steps.
    constexpr float SunDirection[3]   = { -0.45f, -0.80f, -0.40f };
    constexpr float SunAmbient        = 0.42f;  // Floor under an unlit face

    // Coincident surfaces flicker: the depth test has no way to order two things
    // at the same depth, so it picks differently per pixel and per frame. Nudged
    // apart by a hair - far too small to see, far too large to confuse.
    constexpr float SurfaceEpsilon    = 0.004f;

    //--------------------------------------------------------------------------
    // Level generation.
    //
    // Rooms are carved as rectangles into solid rock and joined by corridors one
    // cell wide. Walls are never authored: Map derives them from the boundary
    // between carved and uncarved, so a room and the corridor leaving it cannot
    // disagree about where their shared wall is.
    //--------------------------------------------------------------------------
    // The hard cap the DMG's own advice asks for: establish the limit before you
    // start drawing and curtail anything that would exceed it.
    //
    // Sized to the rooms rather than chosen. Grown from 26 because rooms now vary
    // in size and there are loops and dead ends to fit between them, but held well
    // under the 40 a first pass used: at 40 the dozen rooms a run produces sit too
    // far apart, and what joins them is a twenty-cell corridor with nothing in it.
    // The grid should be about as large as the rooms need and no larger.
    constexpr int   MapWidth          = 34;
    constexpr int   MapDepth          = 34;

    // A FORCING value, not the seed itself: zero rolls a fresh dungeon from the
    // clock at startup, anything else is used verbatim. Development pins a map it
    // wants to walk twice - a bug you cannot get back to is a bug you cannot fix -
    // and a normal run gets a map it has not seen.
    constexpr unsigned int LevelSeed  = 0u;

    // Rooms are placed by rejection - propose, reject on overlap, repeat - so this
    // is an upper bound and a busy map lands under it.
    //
    // Tuned DOWN from the first pass at this, which used sixty and produced
    // twenty-six rooms on a 40 grid. That is not a dungeon, it is a warren: every
    // room shares a wall with three others, corridors run through rooms rather
    // than between them, and the count of ways into a 5x4 chamber came out at
    // fourteen. Fewer, larger rooms with rock between them is the whole difference.
    constexpr int   RoomAttempts      = 32;

    // Rooms are kept this many cells apart so two of them never share a wall. A
    // shared wall would have carved floor on both sides, which by Map's rule is
    // no wall at all - the rooms would silently merge into one.
    //
    // Two rather than the minimum one, because one only stops the merge. Two is
    // what leaves enough rock for a corridor to pass BETWEEN two rooms instead of
    // being forced through one of them.
    constexpr int   RoomSpacing       = 2;

    //--------------------------------------------------------------------------
    // Multiple pathways.
    //
    // The chain that joins each room to the one before it is what makes the map
    // connected, and it makes it a CORRIDOR - one route, walked forwards and then
    // backwards. Extra edges between rooms that are already near each other are
    // what turn it into a place: somewhere to circle back through, and somewhere
    // an enemy can come from that is not the way you came in.
    //--------------------------------------------------------------------------
    constexpr float LoopChance        = 0.35f;
    constexpr int   LoopMaxDistance   = 14;   // Cells between centres, else too far
    constexpr int   LoopMaxCount      = 6;

    // Share of corridors carved two cells wide instead of one. A wide mouth into a
    // room gets no door - FindDoorways only recognises a one-cell opening, and
    // wall_doorway spans exactly one cell - which is correct: what stands there is
    // an open arch, and the map wants some of those.
    constexpr float CorridorWideChance = 0.2f;

    // Short stubs off a corridor that stop in solid rock. Cheap content: somewhere
    // to search, somewhere for the dressing pass to put wreckage, and somewhere a
    // chase can go wrong.
    constexpr int   DeadEndCount      = 5;
    constexpr int   DeadEndMin        = 2;    // Cells long
    constexpr int   DeadEndMax        = 4;

    //--------------------------------------------------------------------------
    // Doors.
    //
    // A door hangs in a one-cell opening, inside a `wall_doorway` frame that fills
    // the 4.000 gap between the corner pieces already standing at its ends. Shut,
    // it is solid and opaque; struck, it swings and stays swung.
    //--------------------------------------------------------------------------

    // The leaf is 2.000 wide and hinged on one edge, so the pivot is this far from
    // the middle of the opening. Straight off the model, not chosen.
    constexpr float DoorLeafHalfWidth = 1.0f;

    // And how tall it stands, which is what a swing has to reach to connect. Off
    // the model: the leaf tops out at 2.750 under a 4.000 lintel.
    constexpr float DoorLeafHeight    = 2.75f;

    // How far a struck door swings. Not a right angle: a leaf stopped square to the
    // wall reads as a wall, and past ninety it starts to close again from the far
    // side. This leaves it clearly open and clearly still a door.
    constexpr float DoorOpenAngle     = 100.0f;   // Degrees

    // Fast enough to be a reaction to the blow rather than a scripted animation,
    // slow enough that you see it move
    constexpr float DoorSwingSpeed    = 420.0f;   // Degrees per second

    // Past this it no longer blocks. Well before fully open, because a door at the
    // edge of its swing has long since stopped being in the way.
    constexpr float DoorPassableAngle = 35.0f;    // Degrees

    //--------------------------------------------------------------------------
    // Dressing the walls.
    //
    // Two independent passes, and they cost very different things. Swapping a
    // plain wall for a cracked or broken one is free - same slot, same transform,
    // same draw. Hanging a banner on it is another model and another draw.
    //
    // Both are driven by a hash of the wall's own grid position, so the level
    // dresses itself identically every frame without storing a thing.
    //--------------------------------------------------------------------------

    // Share of full wall pieces that become something other than plain stone.
    // Every variant is solid - see the note in Level::Load - because a pierced one
    // shows the player the nothing outside the maze.
    constexpr float WallVariantChance = 0.45f;

    // Share of room-facing wall pieces that get something hung on them. Short of
    // half on purpose: a banner on every wall is wallpaper, and reads as less
    // decorated rather than more.
    constexpr float WallPropChance    = 0.22f;

    // Banners sink into the wall - their backs sit at 0.38 against a wall face at
    // 0.50 - so they mount at the wall's own centreline and lean out of it.
    // Mounted torches are the other convention: their backs are at 0.00, so they
    // mount ON the face, and they are authored around Y=0 rather than standing on
    // the floor, so they need lifting to somewhere a torch would actually be.
    constexpr float WallPropBannerOut = 0.0f;
    constexpr float WallPropTorchOut  = 0.5f;
    constexpr float WallPropTorchHigh = 2.6f;

    //--------------------------------------------------------------------------
    // Furnishing the rooms.
    //
    // Unlike the wall dressing above, this pass STORES what it placed. It has to:
    // the large props collide, and a footprint re-derived from a hash every frame
    // is a footprint that can disagree with the one collision resolved against.
    //
    // What each kind of room may contain is in world/RoomKind.h. These are the
    // dials over the top of it.
    //--------------------------------------------------------------------------

    // One multiplier over every kind's min/max counts. The single knob to turn
    // when the rooms are too bare or too full to walk through.
    constexpr float PropDensityScale  = 1.0f;

    // Clear space kept between one placed prop and the next.
    //
    // Doorways and the spawn are NOT a distance here. They are whole cells struck
    // out of the room before placement starts, which is the only version of the
    // test that also covers the two-cell arches - those have no Doorway record at
    // all, so a radius around every door would sail straight past them.
    constexpr float PropClearance     = 0.25f;

    //--------------------------------------------------------------------------
    // Which props are solid.
    //
    // Measured, not authored: a prop blocks if its own bounding box says it is
    // big enough to be in the way. That one rule separates a table from a plate
    // across the whole pack without a per-prop flag to keep in step with it - and
    // the day a new prop is added to a palette it classifies itself.
    //--------------------------------------------------------------------------
    constexpr float PropBlockMinSize   = 0.6f;  // Larger horizontal extent
    constexpr float PropBlockMinHeight = 0.4f;  // Off the floor

    // How high a wall-mounted prop hangs. The pack's shelves are authored around
    // their fixing rather than their base - shelf_large runs from -0.35 to +0.10 -
    // so they carry no height of their own to stand at, and set on the floor they
    // sink through it. Chest height on a two-unit body: high enough to read as
    // fixed to the wall, low enough to still be furniture.
    constexpr float PropMountHeight    = 1.5f;

    // Above this a prop is treated as a full wall rather than something to step
    // onto. Shelves and stacked crates; the player is 2.0 tall and a step up onto
    // something chest-high would be a climb, not a step.
    constexpr float PropStepMaxHeight  = 0.9f;

    //--------------------------------------------------------------------------
    // Pathfinding.
    //
    // A* over the cell grid, four-connected. Never eight: walls stand on the
    // lines BETWEEN cells, so a diagonal step cuts the corner of a wall slab and
    // walks a body through stone that both cells agree is solid.
    //--------------------------------------------------------------------------

    // Safety valve, not a budget. A full search of the grid is about a thousand
    // cells and costs nothing; this exists so a pathological request cannot stall
    // a frame.
    constexpr int   PathMaxNodes       = 2000;

    // How much room a cell has to have for a body to be routed through it. A shade
    // over the body radius of 0.4, so a route never threads a gap the body then
    // grinds its way along.
    constexpr float PathClearRadius    = 0.5f;

    // How often a body may ask again for a goal that has not moved. A path
    // recomputed every frame is the same path every frame.
    constexpr float PathRepathInterval = 0.75f;

    // How close counts as having reached a waypoint. Comfortably inside a cell:
    // demanding the centre exactly makes a body circle it.
    constexpr float PathWaypointReached = 1.2f;

    // Longest a body will keep walking a path it is making no progress along,
    // before throwing it away and asking again. Wedged on a prop, shoved off
    // route by a fight - either way the answer is a new path, not more pushing.
    constexpr float PathStuckTime      = 1.5f;

    //--------------------------------------------------------------------------
    // The reference height every screen-space size in the game is written against.
    //
    // Nothing on the HUD is in raw pixels any more. Each size below is a DESIGN
    // pixel, and UiScale() multiplies it by screenHeight/UiDesignHeight - which is
    // the same trick the mobile game plays, and for the same reason: a bar written
    // as 236 raw pixels is a readable bar on a 720p window and a hairline on a
    // 1440p one, and there is no single raw number that is right on both.
    //
    // 720 rather than the mobile game's 450. Its figure is for a phone held a foot
    // from the face; this is a monitor at arm's length, and lifting the reference
    // height shrinks everything drawn against it by the same ratio. At 1440 tall
    // that puts the scale at 2.0, which is where the bar art's ornaments read
    // without the health bar taking a fifth of the screen.
    //
    // Turn this DOWN to make the whole HUD bigger. That is the right dial for it:
    // one number moves every size on screen together, which is what stops a HUD
    // scaled by hand from drifting into a dozen sizes that agree with nothing.
    //--------------------------------------------------------------------------
    constexpr float UiDesignHeight     = 720.0f;

    //--------------------------------------------------------------------------
    // Minimap.
    //
    // The whole grid on screen at once, north up, at a fixed number of pixels per
    // cell. A plan rather than a radar: one you can hold against the room you are
    // standing in is worth more than one that spins under you, and at this grid
    // size the whole thing fits in a corner without scaling anything.
    //
    // In DESIGN pixels - see UiDesignHeight. The cell figure is a float because
    // scaling it lands between whole pixels, and rounding each cell independently
    // is what makes a grid drift out of square across its width.
    //--------------------------------------------------------------------------
    constexpr float MinimapCellPixels  = 5.0f;  // A 34 grid lands at 170 design px
    constexpr float MinimapPadding     = 4.0f;  // Inside the panel, around the plan
    constexpr float MinimapMargin      = 10.0f; // From the corner of the screen

    // How far the fog lifts around the player OUTSIDE a room, in cells. A room is
    // revealed whole on entry and ignores this - you do not come to know a chamber
    // one flagstone at a time - so this is really the corridor figure.
    constexpr int   MinimapRevealCells = 5;

    // Melee -------------------------------------------------------------------
    // Reach is how far from the eye the tip of the blade lands at full extension.
    // The view model draws the weapon as a miniature half a unit from the eye,
    // well inside the player's own collision radius; ViewModel::BladeFor magnifies
    // that drawn pose about the eye until its tip sits at this distance, and the
    // result is the capsule combat actually tests. Measured model height drives
    // the default so a dagger does not reach as far as a greatsword.
    constexpr float MeleeBaseReach    = 0.9f;
    constexpr float MeleeReachPerUnit = 0.5f;

    // Floor under every melee weapon's reach: if a thing can hit you, you can hit
    // it back. The standoff distances are sized off how the enemy model frames in
    // the camera, not off the fight, and they overtook the short weapons when the
    // model grew - a dagger derives 0.9 + 1.3*0.5 = 1.55, which does not span the
    // gap to an enemy standing at EnemyStopDistance.
    //
    // Sized from the worst case rather than the resting one: the tip has to arrive
    // on the far side of an enemy's capsule at EnemyAttackRange across half a unit
    // of height difference, needing sqrt(2.4^2 + 0.5^2) - 0.4 = 2.05. This clears
    // that - though under a swept blade it is a weaker guarantee than it was under
    // the cone, because the tip now has to actually pass through the body rather
    // than merely be pointed near it.
    //
    // It is a floor, not a flat value: everything longer than a shortsword still
    // out-reaches everything shorter, so the pack's proportions still matter.
    constexpr float MeleeMinReach     = 2.1f;
    constexpr float ThrustReachBonus  = 1.25f;  // Multiplier: a lunge extends
    constexpr int   SwingDamage       = 20;
    constexpr int   ThrustDamage      = 18;

    // How fat the blade is as a hit volume, before the per-weapon scaling in
    // StatsFor. This is the game's forgiveness dial, and it is the only one left:
    // the old cone forgave by ignoring where the weapon was pointing, which is
    // precisely the thing that made swings connect from across the room. A swept
    // capsule forgives by being thicker, which stays honest - the blade still has
    // to travel through the body.
    constexpr float MeleeBladeRadius  = 0.14f;

    // Interpolated positions tested between last frame's blade and this frame's.
    // At the fast part of a swing the tip crosses well over its own width in a
    // frame, so a single test per frame passes straight through a standing enemy.
    constexpr int   MeleeSweepSteps   = 4;

    // The stroke's live window in attack blend. Below MeleeLiveFrom the weapon is
    // still coming up out of rest and would be landing hits with its grip.
    constexpr float MeleeLiveFrom     = 0.25f;
    constexpr float MeleeLiveTo       = 1.0f;

    // How many distinct enemies one swing can be credited against. A cap rather
    // than a vector keeps AttackState trivially copyable and costs nothing: a
    // stroke that catches eight bodies has already earned whatever it was going
    // to earn.
    constexpr int   MaxHitsPerSwing   = 8;

    // Player ranged -----------------------------------------------------------
    // Staffs cast and daggers are thrown. The two are deliberately not the same
    // weapon at different numbers: a bolt is slower and hits harder, a knife is
    // fast and light, so one is a committed shot at something across the room and
    // the other is what you throw at whatever is already coming.
    constexpr int   CastDamage        = 26;
    constexpr float CastSpeed         = 20.0f;
    constexpr float CastReleaseAt     = 0.70f;  // Blend: late, the staff settles first

    constexpr int   ThrowDamage       = 15;
    constexpr float ThrowSpeed        = 30.0f;
    constexpr float ThrowReleaseAt    = 0.55f;  // Earlier - it leaves on the flick

    // How far down the crosshair a shot is aimed. The weapon fires from its own
    // tip, off to one side of the eye, so a shot sent straight along the weapon's
    // axis lands beside whatever the crosshair is on. Converging on a point down
    // the view ray fixes that at the range it matters, leaving a parallax error
    // that shrinks with distance.
    constexpr float AimDistance       = 30.0f;

    // How long a thrown weapon is once it is in the air, in world units. A length
    // rather than a scale factor, so dagger_A and dagger_B fly the same size
    // however differently the two are modelled - the scale is worked out from the
    // measured model height the view model already knows.
    constexpr float ThrownLength      = 0.55f;

    // Throwing -----------------------------------------------------------------
    // A thrown weapon leaves the hand, so the hand has to be empty for a moment
    // and then produce another one. Easing the old one back from full extension -
    // which is what every other style does - reads as the throw never happening,
    // because the thing you just watched fly away is visibly still being held.
    //
    // So the hand goes empty, then the next one slides up into place from off the
    // bottom of the screen and out to that hand's side, which reads as reaching
    // for another knife.
    constexpr float ThrowHideTime     = 0.10f;  // Hand empty after the release
    constexpr float ThrowReturnTime   = 0.24f;  // Sliding the next one into place
    constexpr float ThrowReturnDrop   = 0.55f;  // How far below the rest pose it starts
    constexpr float ThrowReturnSide   = 0.30f;  // ...and how far out toward that hand

    // Blocking ----------------------------------------------------------------
    constexpr float BlockArc          = 140.0f; // Degrees the shield actually covers
    constexpr float BlockDamageScale  = 0.25f;  // What gets through a good block

    // Player ------------------------------------------------------------------
    constexpr int   PlayerMaxHealth   = 100;
    constexpr float PlayerEyeHeight   = BottomHeight + StandHeight;

    //--------------------------------------------------------------------------
    // Stats
    //--------------------------------------------------------------------------
    // The four numbers everything that fights carries - see combat/Stats.h for
    // what each one does and why every bonus is a fraction of a base rather than
    // a multiplier on a climbing figure.
    //
    // StatBase - the neutral line, and a PIVOT: at 10 a stat contributes exactly
    // nothing, above 10 it adds and below 10 it subtracts. That last half is how
    // an enemy kind gets a weakness written into it. Declared in StatBlock.h
    // beside the struct whose defaults it sets.

    // Constitution and arms are priced against each other so that a point in
    // either is worth roughly the same in a fight. At the player's 100 base
    // health and a sword's 20 base damage, one point is +2 health or +0.8
    // damage - and against an enemy hitting for 11 a point of health buys about
    // a fifth of a swing survived, where a point of damage takes about the same
    // off the time to kill. Neither is the obvious answer, which is the point.
    constexpr float StatHealthPerPoint  = 0.02f;    // of base health, per point of con
    constexpr float StatDamagePerPoint  = 0.04f;    // of the weapon's own damage, per arms
    constexpr float StatSpellPerPoint   = 0.04f;    // of base spell power, per arcane

    // What magic is multiplied against, before any school's own multiplier. Set
    // level with SwingDamage on purpose: at StatBase arcane a middling school
    // hits for about what a sword does, so the two openings are comparable and
    // the divergence comes entirely from where the points went.
    constexpr int   BaseSpellPower      = 20;

    // Crits -------------------------------------------------------------------
    // Crit multiplies damage, so SKILL alone has nothing to multiply and must
    // lose to ARMS alone. The rates are set so that it loses EARLY and wins
    // LATE, which is the shape that makes it a decision rather than a fixed
    // answer: chance CAPS but crit damage does not, so late points keep
    // compounding against a damage figure arms has already grown.
    //
    // The chance cap costs 175 points - deliberately more than a third of what
    // seventeen levels pay out. A cap reached by level 8 collapses the choice
    // into "fill skill, then arms" before the player has met the third enemy.
    constexpr float StatCritBaseChance  = 0.05f;    // at StatBase skill
    constexpr float StatCritBaseDamage  = 1.50f;    // multiplier, at StatBase skill
    constexpr float StatCritPerPoint    = 0.004f;   // added chance per point of skill
    constexpr float StatCritDamagePerPoint = 0.01f; // added multiplier per point - UNCAPPED
    constexpr float StatCritChanceCap   = 0.75f;    // however many points go in

    // Levelling ---------------------------------------------------------------
    // A level gives NOTHING directly - no automatic health, no automatic damage.
    // It grants points to spend, and those points are the only source of either.
    // Levelling that happens TO a character is not progression, it is a number
    // going up while the player watches.
    //
    // Ten points and not five. The budget has to be able to buy a rounded
    // character - some health AND some offence - or the only build the game is
    // actually calibrated against is one the system cannot make. At ten a level,
    // roughly 40% into constitution and the rest into offence is a balanced
    // fighter, and going all in on one stat is about twice that on one axis and
    // nothing at all on the other. Both are meant to be real.
    constexpr int   PlayerStatPointsPerLevel = 10;
    constexpr int   PlayerExpFirstLevel = 120;      // exp from level 1 to level 2
    constexpr float PlayerExpGrowth     = 1.15f;    // each level costs this multiple of the last

    // The run ---------------------------------------------------------------
    // How many floors clear the dungeon. Reaching the portal on this depth, with
    // it cleared, ends the run in victory instead of opening onto a sixth floor -
    // a run needs a top as well as a bottom, or "how far can you get" never
    // actually resolves into an answer.
    constexpr int   VictoryDepth = 5;

    //--------------------------------------------------------------------------
    // Mana
    //--------------------------------------------------------------------------
    // Casting costs, and the pool refills by KILLING - mostly by killing with a
    // weapon. See the long note in progress/Spellbook.h for why, and for the one
    // invariant a new school must not break: a spell can never fund itself.
    //
    // Twenty is about four of the heaviest schools or seven of the lightest, and
    // that difference is the decision the cost is there to create. Arcane raises
    // the pool rather than the refill rate: a caster banks more casts by fighting
    // for them, which keeps the sword worth swinging on a build that never wanted
    // one.
    //--------------------------------------------------------------------------
    constexpr int   ManaMax             = 20;   // At neutral arcane; ARCANE adds
    constexpr float ManaPerArcane       = 0.9f; // Extra pool per point over StatBase

    constexpr int   ManaPerKill         = 1;    // A body dropped by a weapon

    // A body dropped by a MOTE, as a fraction. Kills are counted and paid in whole
    // mana at the rate below, so a half-rate refill is one mana per two spell
    // kills - which is what keeps the closest school in the table (a mote kills
    // one body) paying back strictly less than it cost to cast.
    constexpr int   SpellKillsPerMana   = 2;

    // What a school costs to cast, before its own damage multiplier and before any
    // trait. Six at the table's midpoint, against a pool of twenty.
    constexpr int   SpellBaseCost       = 5;

    // What the mystic asks for a school, before its damage multiplier. Gems are
    // rare - a school is a few elite kills, not a floor of them.
    constexpr int   SpellBasePrice      = 4;

    //--------------------------------------------------------------------------
    // Magic effects
    //--------------------------------------------------------------------------
    // What each school does instead of an element - see the note on Stats.h. All
    // first-guess numbers, sized to read clearly in a fight rather than measured
    // against a full balance pass; expect these to move once they have been played.
    //
    // FLAME and REND share one tick rate rather than each carrying their own,
    // because the two are the same MECHANISM (a DOT applied through
    // Enemy::dotTime) wearing different numbers - a burn that ticks on a different
    // clock to a bleed would be two systems for the one idea.
    //--------------------------------------------------------------------------
    constexpr float MagicDotTickInterval   = 0.5f;   // Seconds between ticks, either DOT

    // FLAME: a wide, patient burn - the longest DOT on the table, and the only one
    // that jumps. The radius is deliberately short: a chain that reached across a
    // whole room would make every pack fight the same fight regardless of formation.
    constexpr float FlameBurnDuration      = 4.0f;
    constexpr int   FlameBurnDamagePerTick = 3;
    constexpr float FlameSpreadRadius      = 3.0f;

    // REND: the same mechanism as FLAME, short and sharp instead of wide and patient
    // - a blade wound closes faster than a fire burns out, and hurts more while it
    // is open.
    constexpr float RendBleedDuration      = 2.0f;
    constexpr int   RendBleedDamagePerTick = 5;

    // TOXIN: stacks rather than ticking on its own clock, so repeated hits are what
    // grows it. At the cap the target panics rather than piling higher forever - a
    // poison with no ceiling would need no cap AND no flee, which is a different
    // spell.
    constexpr int   ToxinMaxStacks    = 5;
    constexpr int   ToxinDamagePerStack = 2;
    constexpr float ToxinTickInterval = 1.0f;
    constexpr float ToxinFleeDuration = 3.0f;

    // BLAST: a hard shove along the bolt's own line of travel, applied where
    // Enemy::Shove already lands a hammer's stagger - see Projectile.cpp.
    constexpr float BlastKnockbackSpeed = 9.0f;

    // SPLASH: a plain multiplier on the move input, applied at the one place every
    // movement branch of the AI already funnels through - see EnemyManager.cpp.
    constexpr float SplashSlowFactor   = 0.5f;
    constexpr float SplashSlowDuration = 3.0f;

    // FLASH: drains detection to zero and holds it there rather than damaging
    // anything - the one school that is entirely an interrupt.
    constexpr float FlashBlindDuration = 2.0f;

    // NOVA: the one school with an actual area of effect - every other living
    // enemy within this of the impact point takes the same blow the mote's real
    // target did. Set against the same two-unit body NOVA's own impact art is
    // sized for, wide enough to catch whatever was standing next to the target.
    constexpr float NovaRadius = 3.5f;

    //--------------------------------------------------------------------------
    // Vendors
    //--------------------------------------------------------------------------
    // How many a floor may have ABOVE the guaranteed first one - so the roll runs
    // one to three. Flat rather than weighted: a three-vendor floor is meant to be
    // ordinary texture, not a rarity the player learns to hope for.
    //
    // Never more than there are vendors: two merchants on a floor is one merchant
    // and a wasted room, so the placement loop caps itself at NpcKind::Count
    // whatever this says.
    constexpr int   VendorsPerFloorMax = 3;

    // Cells of floor a room needs before a vendor will stand in it. Smaller than an
    // event room - a shop is one figure to walk up to rather than a fight - but not
    // so small that the counter fills the room it is in.
    constexpr int   VendorRoomArea     = 9;

    // How close the player has to be to open the counter, and how tall the column
    // of light standing in for the art is.
    constexpr float VendorReach        = 2.0f;
    constexpr float VendorMarkerRadius = 0.7f;
    constexpr float VendorMarkerHeight = 2.2f;

    // How far away a vendor's name is still drawn. Far enough to spot one across a
    // room and to know which it is; short enough that the names are not a layer of
    // text over the whole map.
    constexpr float VendorLabelRange   = 22.0f;

    // A currency drop's coin/coin-stack prop, against the dungeon pack's own scale
    // - see world/Loot.cpp. One knob rather than a per-model guess, since all four
    // props come off the same sheet and should sit at the same size relative to
    // the floor tiles around them.
    constexpr float LootPropScale      = 1.0f;

    //--------------------------------------------------------------------------
    // The treasure room.
    //
    // Game::SeedRoomLoot already drops a gem in every otherwise-empty room and a
    // bigger one in a Vault or a Library; a Vault ADDITIONALLY scatters a few
    // separate piles of coins, so it reads as a chest someone tipped over rather
    // than as the same one-drop consolation every other empty room gets.
    //--------------------------------------------------------------------------
    constexpr int VaultCoinPilesMin  = 3;
    constexpr int VaultCoinPilesMax  = 5;
    constexpr int VaultCoinAmountMin = 4;
    constexpr int VaultCoinAmountMax = 12;

    //--------------------------------------------------------------------------
    // Limited stock.
    //
    // Each vendor's UNOWNED list is rerolled to a small offered subset every
    // floor (Game::StartNewRun / Game::Descend), rather than showing everything
    // the player does not yet own. An item wanted but not offered is a reason to
    // come back down a floor and check again, not a permanent lockout - nothing
    // here ever gates an ALREADY OWNED item's Upgrade or Sell row.
    //
    // Sized against each table: the merchant's weapon list is the longest, so it
    // gets the widest window; the mystic's eight schools and the captain's
    // thirteen traits get correspondingly smaller ones.
    //--------------------------------------------------------------------------
    constexpr int   MerchantStockPerFloor = 5;
    constexpr int   MysticStockPerFloor   = 3;
    constexpr int   CaptainStockPerFloor  = 3;

    // Of the price PriceFor(...) would otherwise ask (Arsenal.cpp). One knob for
    // "the merchant costs too much" rather than reworking that formula's three
    // constants by feel every time it needs adjusting.
    constexpr float WeaponPriceScale      = 0.7f;

    //--------------------------------------------------------------------------
    // The starting kit
    //--------------------------------------------------------------------------
    // One weapon and one school; everything else is bought. Matched as a
    // case-insensitive SUBSTRING of the model's file name, so the constant does
    // not have to know whether the asset is called "sword" or "sword_1h".
    //
    // A sword and FLAME on purpose: the two most ordinary rows in their tables.
    // The starting kit should be the thing every other purchase is measured
    // against, which means it must not be interesting.
    constexpr char  StartingWeapon[]    = "sword";
    constexpr int   StartingMagic       = 0;        // Magic::Flame

    //--------------------------------------------------------------------------
    // What the dead pay, in the three currencies
    //--------------------------------------------------------------------------
    // Coins are a fraction of the exp the same body pays, so anything that is
    // worth more to kill is worth more to loot without a second table to keep in
    // step with the first. They credit STRAIGHT to the purse with a number
    // floating off the body: a physical coin per kill would flood the drop pool
    // the moment a swept blade took a whole pack, and coins the player walked past
    // would be coins lost to a pool overflow rather than to a decision.
    constexpr float CoinsPerExp         = 0.25f;

    // Gems ARE physical, for the opposite reason: a gem that appeared as a number
    // among a dozen other numbers is a gem the player never noticed earning. Per
    // thousand kills, and multiplied for anything above the player's own tier -
    // which is what makes an elite worth walking towards.
    constexpr int   GemChancePerMille   = 18;
    constexpr int   GemChanceEliteMult  = 4;

    // Contracts come only from EVENTS. Not from kills at any rate, however small:
    // the whole point of the third currency is that it is the reward for choosing
    // to walk into an objective, and a trickle from ordinary bodies would make
    // that choice optional.
    constexpr int   EventContractsMin   = 1;
    constexpr int   EventContractsMax   = 2;

    //--------------------------------------------------------------------------
    // The captain's early stock.
    //
    // Two events a floor at 1-2 contracts each means a floor's WORST case is two
    // contracts, not three - so a first-floor counter that happened to roll
    // nothing but its pricier rows would be a shop the player cannot use yet.
    // Below this depth the reroll guarantees one offer priced at or under the
    // cap, the same idiom Arsenal::RerollOffers already uses to guarantee a
    // castable weapon on floor one - see Game::RerollVendorStock.
    //--------------------------------------------------------------------------
    constexpr int   CaptainCheapGuaranteeDepth = 3;
    constexpr int   CaptainCheapGuaranteePrice = 2;

    // ...and an event pays gems as well, because a resolved objective should move
    // more than one vendor's counter.
    constexpr int   EventGemsMin        = 1;
    constexpr int   EventGemsMax        = 3;

    // What a body belonging to an OBJECTIVE pays on top. Every enemy an event
    // spawns carries its tag, not just a bounty's champion, so this multiplies a
    // whole hunt's fifteen bodies as well as the one duel - which is why it is 3
    // and not the mobile game's 10. An event is worth walking into; it is not
    // worth three floors of ordinary income.
    constexpr int   EventCoinMult       = 3;

    //--------------------------------------------------------------------------
    // Enemy ranks and tiers
    //--------------------------------------------------------------------------
    // See entities/EnemyRank.h for what a rank is and why it is anchored on the
    // level's DEPTH rather than on the player. These are the rates.
    //
    // Growth is a fraction of the kind's own base, added, never compounded - the
    // same rule stat bonuses follow, and for the same reason. Compounding would
    // run away from the player's linear climb, and a flat step would converge the
    // Minion and the Warrior within a dozen ranks and take their identities with
    // them.
    //
    // Health grows faster than damage on purpose. Health is what makes a fight
    // LAST; damage is what makes it lethal, and damage is by a distance the
    // sharpest difficulty dial there is. A floor of bodies that hit 30% harder is
    // a different game from one whose bodies take 30% longer to cut down.
    constexpr float EnemyRankHealthGrowth = 0.30f;  // of base health, per rank past the first
    constexpr float EnemyRankDamageGrowth = 0.17f;  // of base damage, likewise
    // The one that compounds. See the note on RankedExp.
    constexpr float EnemyRankExpGrowth    = 1.14f;

    // Ceiling, so the arithmetic and the roll's window stay bounded however deep
    // a run goes. Nothing is expected to reach it.
    constexpr int   EnemyMaxRank        = 99;

    // Rank centre = BASE + (depth - 1)*STEP, so the floors run 1 / 4 / 7 / 10.
    // Three a floor against ten stat points a level is roughly parity if the
    // player levels twice a floor, and the floor pulls ahead if they do not -
    // which is what makes going down a decision rather than a corridor.
    constexpr int   EnemyDepthRankBase  = 1;
    constexpr int   EnemyDepthRankStep  = 3;

    // The roll's window about that centre, and how sharply each side falls away.
    // Asymmetric on purpose: see RollRankForDepth.
    constexpr int   EnemyRankSpanBelow  = 8;
    constexpr int   EnemyRankSpanAbove  = 2;
    constexpr float EnemyRankBelowSpread  = 0.32f;  // weight d below = 1/(1 + this*d*d)
    constexpr float EnemyRankAboveFalloff = 0.22f;  // weight d above = this^d

    //--------------------------------------------------------------------------
    // Chaos, and the portal down
    //--------------------------------------------------------------------------
    // What a floor starts full of and every kill drains. See world/Chaos.h for
    // what the pool is for; these are its numbers.
    //
    // The base is set against what a floor actually holds. Five camps of three is
    // fifteen bodies at roughly 12-28 exp each, so a floor's opening garrison is
    // worth about 250 - which means the first floor takes rather more than one
    // sweep of the map and the camps have to refill at least once. That is the
    // intent: a floor is a quantity to work through, and one that ends with the
    // first garrison would never have needed the pool at all.
    //
    // It grows geometrically because the reward for a kill does too - enemy exp
    // compounds at EnemyRankExpGrowth per rank, and ranks climb three a floor. A
    // pool that grew linearly against that would mean each floor took fewer kills
    // than the last, so the game would get shorter as it got harder.
    constexpr int   ChaosBase           = 320;
    constexpr float ChaosGrowth         = 1.22f;

    // Seconds standing in the portal before the next floor is built.
    //
    // A dwell rather than a touch, and long enough to be a decision: the portal
    // stands at the far end of the map in a room the player has every reason to
    // walk through, and a floor that ended the moment they crossed a threshold
    // would end floors by accident. Long enough to step back out of, too, which
    // is the whole reason the player walks INTO it here rather than waiting in
    // front of it - going down is something they do, not something that happens.
    constexpr float PortalDwell         = 2.2f;
    // The last of that spent fading the screen out, so the new floor is not a cut
    constexpr float PortalFade          = 1.0f;

    // The portal itself, built out of light rather than out of a model - see
    // render/Portal.h for why.
    //
    // The radius is both what is drawn and what the dwell tests against, which is
    // the whole reason it is one number: a portal whose light and whose trigger
    // disagreed would be one the player has to learn the real edge of. 1.2 is a
    // little wider than a body, so standing in it is unambiguous and walking past
    // it at arm's length is not.
    constexpr float PortalRadius        = 1.2f;
    constexpr float PortalHeight        = 3.4f;     // A shade over wall height
    constexpr float PortalRaise         = 1.2f;     // Seconds to fade up
    constexpr float PortalSpinRate      = 1.4f;     // Radians a second

    // How strongly the column is added. The step count is render/Beam.cpp's, since
    // it is a property of how a beam is drawn rather than of this beam.
    constexpr int   PortalColumnAlpha   = 120;

    constexpr int   PortalMotes         = 7;
    constexpr float PortalMoteSize      = 0.30f;

    // Cold, and nothing else in the game is. Every other light source here is warm
    // - torches, fire magic, the hurt flash - so a cold one at the far end of a
    // corridor is legible as the way out from a long way off, which is exactly the
    // job it has to do.
    //
    // As three channels rather than a Color, because nothing in this header
    // includes raylib and that is what keeps it free for every module to pull in.
    // Same shape as Background above.
    constexpr unsigned char PortalColour[3] = { 120, 190, 255 };

    //--------------------------------------------------------------------------
    // Events
    //--------------------------------------------------------------------------
    // The rooms that are not just rooms. See world/Event.h for what one is and
    // why the portal waits on them.
    //
    // Two a floor. One is a thing that happens; three is a floor made of them,
    // and the ordinary business of working through a garrison stops being what
    // the floor is about. Two also means a player who fails one still has a
    // reason to find the other.
    constexpr int   EventCount          = 2;
    // Floor cells a room needs before it may hold one. Below this a hunt is a
    // scrum against a wall, which is not a fight, it is a queue.
    //
    // It is also the number that decides whether a floor gets its full quota:
    // events are placed furthest-first over the rooms that clear this bar, so
    // raising it makes for better rooms and fewer of them. The placement pass
    // warns when it comes up short.
    constexpr int   EventRoomArea       = 12;

    // What clearing one is worth, off the floor's pool and into the player. Flat
    // rather than a fraction of the pool: the fight is the same shape on every
    // floor, kill exp already scales with rank, and a percentage would make a
    // deep floor pay more for the same work.
    constexpr int   EventChaosReward    = 70;
    constexpr int   EventExpReward      = 60;

    // How many ranks above the floor an event's bodies are rolled. It is the one
    // dial that makes an event's pack harder than the trash in the corridor
    // outside, and it is small on purpose - two ranks is about +60% health and
    // +34% damage, which is a step up rather than a wall.
    constexpr int   EventRankBonus      = 2;

    // How long the "what to do" line stays up after an event starts. Long enough
    // to read once; after that the bar and its banner are the readout, and a
    // standing instruction beside them is clutter.
    constexpr float EventBriefTime      = 5.0f;

    // The marker a Pending event stands under: a column of light in its own
    // colour, and what the player walks into to begin. Smaller than the portal,
    // because it is a thing on the floor rather than the way off it.
    constexpr float EventMarkerRadius   = 0.9f;
    constexpr float EventMarkerHeight   = 2.6f;

    // Hunt --------------------------------------------------------------------
    // Three waves, and it is the shape of the fight rather than a number that got
    // tuned up. One pack dropped on the player at once is a single decision -
    // stand and swing, or leave. Three packs on a clock is a fight with a middle,
    // where the ground taken in the first is what has to be held through the
    // third.
    //
    // The next wave also comes EARLY if the current one is already down, so a
    // player who is winning is not left standing in an empty room waiting.
    constexpr int   EventWaves          = 3;
    constexpr int   HuntWaveSize        = 5;
    constexpr float HuntWaveGap         = 14.0f;
    constexpr float HuntTimeLimit       = 130.0f;
    // How far from the room's centre a wave's bodies come up. They climb out of
    // the floor - the pack already ships the animation - so this is a scatter and
    // not a spawn point: close enough to read as one arrival, far enough that
    // five bodies are not standing inside each other.
    constexpr float HuntSpawnSpread     = 3.0f;

    // Defend ------------------------------------------------------------------
    // The relic is priced in swings that get through rather than in bodies that
    // arrive: a raider that is ignored keeps hitting, at roughly one swing every
    // two seconds, so 90 is a minute and a half of being left alone. The
    // player's job is to stop them being left alone.
    //
    // Softened from the first pass (60 HP, a wave of 4 every 9s) once it played
    // as a pure DPS-against-a-clock race: every spawn was a raider, so there was
    // never any personal threat and the only lever was how fast the player could
    // clear a wave before the next one landed on top of it.
    constexpr float DefendTimeLimit     = 75.0f;
    constexpr int   DefendRelicHealth   = 90;
    constexpr int   DefendHitDamage     = 4;
    constexpr float DefendWaveGap       = 12.0f;
    constexpr int   DefendWaveSize      = 3;

    //--------------------------------------------------------------------------
    // Not every spawn raids. This fraction of each wave ignores the player and
    // beelines the relic exactly as before; the rest spawn as ORDINARY hostiles
    // - `raiding` left false - and chase and fight the player through the same
    // AI the floor's own population uses.
    //
    // Without this the event was one note played twelve times: nothing here was
    // ever a threat to the player, only to the relic, so standing still and
    // swinging was strictly correct. A body that comes for the PLAYER instead is
    // what turns "defend the relic" into a fight rather than a chore with a
    // health bar.
    //--------------------------------------------------------------------------
    constexpr float DefendRaiderFraction = 0.65f;

    // How far out raiders come in from. Well outside the ring the player will be
    // standing in, so they are visibly arriving rather than appearing on top of
    // the thing they are here for.
    constexpr float DefendSpawnRing     = 7.0f;
    constexpr float DefendRelicRadius   = 0.55f;

    // The relic itself: a piece out of the dungeon pack, off the floor and turning.
    //
    // A model rather than the column of light it used to be, because the thing the
    // waves are walking at should be an OBJECT - a light with no shape is scenery,
    // and the player is being asked to care about it. The column stays underneath
    // it and still dims as the relic is broken, so the readout is unchanged and
    // what has been added is the thing the readout is about.
    //
    // Off the ground and slowly turning for the same reason a pickup is: nothing
    // in a dungeon floats and revolves, so anything that does is not furniture.
    // Slow enough to read as suspended rather than as spinning - a fast turn on a
    // chest-shaped model is a coin flip, not a relic.
    // Under models/dungeon/. A gold chest is the one piece in the pack that reads
    // as treasure at ten metres without a label on it.
    constexpr char  DefendRelicModel[]  = "props_medium/chest_gold.gltf";

    constexpr float DefendRelicLift     = 1.05f;    // Centre height off the floor
    constexpr float DefendRelicBob      = 0.13f;    // How far it rises and falls
    constexpr float DefendRelicBobRate  = 1.5f;     // Radians a second of the bob
    constexpr float DefendRelicSpin     = 0.55f;    // Radians a second, turning
    constexpr float DefendRelicScale    = 1.15f;

    // Bounty ------------------------------------------------------------------
    // One body, and the only event that is a duel. It is worth more ranks than a
    // hunt's whole pack because it is one target: the player can bring everything
    // to bear on it, and a champion that folds to that is not a fight worth
    // gating a floor on.
    constexpr int   BountyRankBonus     = 5;
    constexpr float BountyHealthScale   = 2.2f;
    constexpr float BountyTimeLimit     = 100.0f;

    // Seal --------------------------------------------------------------------
    // The one event that is not about killing anything: runes scattered across
    // the room, a clock, and the ceiling coming down while you gather them.
    //
    // The bolts are a STORM and not a metronome. Each tick drops a small volley
    // and several are live at once, with per-hit damage low enough that volume is
    // the teeth rather than any one strike - the player is reading the room, not
    // dodging a single telegraph.
    constexpr int   SealRunes           = 8;
    constexpr float SealTimeLimit       = 30.0f;
    constexpr float SealPickupRadius    = 1.1f;
    constexpr float SealBoltGap         = 0.9f;     // Seconds between volleys
    constexpr int   SealBoltsPerVolley  = 2;
    // Seconds a bolt is a ring on the floor before it lands. Long enough to walk
    // out of and short enough that walking out of it is a decision rather than a
    // stroll - a telegraph the player can ignore is not a telegraph.
    constexpr float SealBoltWarn        = 0.75f;
    constexpr float SealBoltRadius      = 1.6f;
    // A fraction of the player's own pool, so a bolt costs the same share of a
    // deep character as of a shallow one. Flat damage would be lethal at level 1
    // and free at level 20.
    constexpr float SealBoltDamageFrac  = 0.09f;
    // How long the strike's effect and its ring linger after it lands
    constexpr float SealBoltLinger      = 0.25f;

    // The cleared banner: up, held, then eased back to a standing reminder that
    // does not go away. It has to outlive its own announcement - the portal is
    // somewhere else entirely and the walk there is most of a minute.
    constexpr float BannerFadeIn        = 0.4f;
    constexpr float BannerHold          = 4.0f;
    constexpr float BannerSettle        = 1.0f;
    constexpr float BannerIdleAlpha     = 0.45f;

    // Enemies, shared ---------------------------------------------------------
    // What every kind of skeleton has in common. What tells them apart is the
    // EnemyTypes table below.
    constexpr float EnemyAggroRange     = 18.0f;

    // What counts as the player being right on top of an enemy, for picking
    // between swings that want different room.
    //
    // The window this has to live in is narrower than it looks. ClearOfPlayer
    // guarantees nothing stands closer than its own personalSpace, and an attack
    // only starts inside attackRange - so at the moment a swing is chosen the
    // distance is always between those two. For the melee rows that is 2.0 to
    // 2.4, or 2.0 to 2.6 for the Reaver. A threshold at or below 2.0 can never be
    // met and silently disables whatever it gates.
    constexpr float EnemyCloseRange     = 2.25f;

    // Seeing ------------------------------------------------------------------
    // Total arc an enemy can see, degrees, centred on the way it is facing. Wide,
    // because this is a game about being swarmed in corridors rather than one
    // about stealth: at 120 the player can get behind a body that has not noticed
    // them, and cannot walk a slow circle round one that has.
    //
    // Flat, like every other test here - the grid has no vertical extent and
    // neither does this. An enemy sees as far up and down as it does sideways.
    constexpr float EnemyViewCone       = 120.0f;

    // Inside this the cone stops mattering. Nobody fails to notice someone
    // standing at their elbow, and an enemy that did could be walked round and
    // stabbed forever by a player who never had to do anything else.
    constexpr float EnemyViewNear       = 3.0f;

    //--------------------------------------------------------------------------
    // How fast an enemy goes from a glimpse to certain. Awareness fills a meter
    // rather than flipping a switch: an enemy that snapped to full alert on the
    // frame a pixel of the player crossed its cone leaves no window to break line
    // again, which is the whole of moving through a level unseen.
    //
    // Both rates are fractions of the meter per second, so 2.0 is half a second
    // to notice. Near and far are the ends of a straight line across aggro range:
    // someone at the far end of a hall is a shape that takes a moment to resolve,
    // someone at ten feet is not.
    //--------------------------------------------------------------------------
    constexpr float EnemyDetectNear     = 4.0f;
    constexpr float EnemyDetectFar      = 0.8f;
    // And how fast it drains once the player is out of the cone or behind
    // something. Slower than either fill: ducking out for a moment should not
    // undo being spotted, or a player could stutter-step across an open room.
    constexpr float EnemyDetectDecay    = 0.4f;

    // Hearing -----------------------------------------------------------------
    // How far a fight carries. Deliberately shorter than sight: a room away, not
    // a level away, or every camp on the map converges on the first swing and the
    // level is one fight.
    constexpr float EnemyHearingRange   = 14.0f;

    //--------------------------------------------------------------------------
    // How much of the detection meter one shout fills, measured at the noise
    // itself and falling off to nothing at the edge of earshot.
    //
    // A hit is worth less than a full meter on purpose. One thud from across the
    // room puts a body on edge - which is to say it reacts the instant it does
    // look your way - without on its own being enough to bring it over. A fight
    // that goes on, or a death, is.
    //--------------------------------------------------------------------------
    constexpr float EnemyHurtNoise      = 0.45f;
    constexpr float EnemyDeathNoise     = 1.0f;

    // Losing sight ------------------------------------------------------------
    // How long an enemy keeps hunting after the player breaks line of sight.
    // Without this a ranged enemy is beaten permanently by one corner: it stands
    // at its standoff, the player steps behind a wall, and it forgets on the same
    // frame. Long enough to cross a room, short enough that walking away works.
    constexpr float EnemyAlertMemory    = 6.0f;
    // How close counts as having reached the spot the player was last seen. Being
    // exact is pointless - the player is not there, that is the whole situation.
    constexpr float EnemyTrailReached   = 1.5f;
    // Degrees per second a hunter turns while scanning at a cold trail
    constexpr float EnemySearchTurnRate = 110.0f;
    // How much of a shove the enemy absorbs; the rest moves the player. Low, so
    // walking into one feels like walking into something heavy: the player gives
    // way and the enemy barely does. It used to be 0.8, which made them skate.
    //
    // This can be small because it is no longer the thing keeping the camera
    // clear - ClearOfPlayer runs after it and moves the enemy whatever distance is
    // still needed, so nothing gets wedged even when the enemy will not budge here.
    constexpr float EnemyPushShare      = 0.15f;
    constexpr float EnemyAttackArc      = 90.0f;
    constexpr float EnemySeparation     = 0.8f; // How hard they push each other apart

    // Blocking ----------------------------------------------------------------
    // An enemy's guard is the mirror of the player's shield: an arc it actually
    // covers, and a scale on what gets through. It is NOT cosmetic - a block that
    // plays the animation and takes full damage teaches the player to ignore the
    // animation, which is worse than not having it.
    //
    // Weaker than the player's 0.25, on purpose. The player blocks in response to
    // a swing they can see coming; the enemy blocks on a dice roll between its own
    // swings, so the same number would turn a lucky roll into a wall.
    //--------------------------------------------------------------------------
    // How fast a poise meter drains, as a fraction of the body's max health per
    // second - see EnemyTierDef::poise and Enemy::poise.
    //
    // 0.09 against a champion's 0.125 threshold means about a second and a half of
    // not being hit undoes a full meter. That is the number that decides what
    // "sustained pressure" means: fast enough that chip damage between other
    // fights never banks a flinch, slow enough that a real flurry still earns one.
    constexpr float EnemyPoiseRecovery  = 0.09f;

    //--------------------------------------------------------------------------
    // The champion's health bar.
    //
    // Only champions get one, and that is the point of it. A bar over every body
    // would be a screen of bars - twenty skeletons in a room, twenty readouts, and
    // the one that mattered lost among them. A bar that appears over exactly the
    // thing that is going to take forty swings is a bar that MEANS something: it
    // says "this one is different, and here is how far through it you are".
    //
    // The range is short for the same reason. A champion three rooms away is not a
    // fight yet, and its bar hanging through a wall is a readout for a decision
    // nobody is making.
    constexpr float ChampionBarRange    = 26.0f;
    constexpr float ChampionBarWidth    = 150.0f;   // Design px - see UiTheme.h
    constexpr float ChampionBarLift     = 0.55f;    // World units over its head

    //--------------------------------------------------------------------------
    // The hurt indicator: a red bar at the edge of the screen, on the side the
    // last blow came in from.
    //
    // It fades on a clock rather than snapping off, so a hit landing while the
    // last one is still fading READS as two hits rather than restarting the
    // same one - see Hud::DrawHurtIndicator.
    //--------------------------------------------------------------------------
    constexpr float HurtIndicatorTime   = 0.9f;     // Seconds to fade fully out
    constexpr float HurtIndicatorRadius = 0.40f;    // Fraction of the shorter screen side
    constexpr float HurtIndicatorLength = 130.0f;   // Design px, along the ring
    constexpr float HurtIndicatorWidth  = 22.0f;    // Design px, across the ring

    constexpr float EnemyBlockArc       = 140.0f;
    constexpr float EnemyBlockDamageScale = 0.35f;
    // The guard drops this long before the next swing, so lowering the arms and
    // starting the chop read as one motion rather than a guard that snaps into it.
    //
    // Exactly one cross-fade, and no more. The guard only exists in the gap
    // between the end of a swing's clip and the start of the next swing, and that
    // gap is small: this was 0.25 and, against the Warrior's 1.50s cooldown, left
    // 0.06s of held guard behind the fade - a state that never once appeared in a
    // simulated minute of fighting. Anything taken here comes straight out of the
    // only window the guard has.
    constexpr float EnemyBlockDropTime  = 0.12f;

    // How many alternates each of these states picks between. The clips are
    // Hit_A/Hit_B and Death_A/Death_B; a type missing the alternate falls back to
    // the first, so this is a ceiling and not a requirement.
    constexpr int   EnemyHitVariants    = 2;
    constexpr int   EnemyDeathVariants  = 2;
    // How many attack clips one archetype may name. Three because that is what
    // the 2H family actually offers - Chop, Slice and Spin are visibly different
    // swings, where a fourth would be splitting hairs. A row that names fewer
    // ends its list with "" and simply repeats what it has.
    constexpr int   EnemyAttackVariants = 3;
    // Idle_A and Idle_B. Rolled once per body when it spawns and then kept, so a
    // camp reads as several people standing about rather than one pose copied.
    constexpr int   EnemyIdleVariants   = 2;

    // Above this much movement input an enemy is running, below it walking. The
    // input is a magnitude as well as a direction - closing is full throttle,
    // giving ground and sidestepping are deliberately not - so the same number
    // that sets the speed picks the clip, and the feet cannot disagree with the
    // animation about how fast the body is going.
    constexpr float EnemyRunThreshold   = 0.75f;
    // What the slower movements ask for. Retreating and clearing a firing line
    // are both meant to look measured next to a charge.
    constexpr float EnemyRetreatDrive   = 0.55f;
    constexpr float EnemyStrafeDrive    = 0.60f;
    constexpr float EnemyHuntDrive      = 0.85f;    // Purposeful, but not a sprint

    // Walking a beat, or walking back to a post. Slower than a hunt: a body with
    // nothing to chase should read as bored, and its walk clip should be the walk
    // rather than the run.
    constexpr float EnemyPatrolDrive    = 0.5f;

    // How near its camp's middle counts as being at its post. Not the exact
    // centre - a garrison of three all walking to one point would spend the rest
    // of the level shoving each other off it.
    constexpr float EnemyPostRadius     = 4.0f;

    //--------------------------------------------------------------------------
    // Getting unstuck.
    //
    // Level::ResolveBody slides a body along whatever it meets, which carries it
    // neatly round a wall and does nothing at all for a table standing in open
    // floor: there the slide just holds it in place, grinding, for as long as it
    // stays interested. Two different obstacles, two different answers.
    //--------------------------------------------------------------------------

    // How long a body may close on the player without getting closer before it
    // stops walking straight at them and routes instead. Long enough that
    // ordinary jostling in a fight does not trigger it.
    constexpr float EnemyShoveTime      = 0.7f;

    // How much nearer counts as progress. A shade above the noise a fight puts
    // into the distance between two bodies from one frame to the next.
    constexpr float EnemyShoveProgress  = 0.05f;

    // A shut door is the one obstacle meant to be pushed through - the pathfinder
    // deliberately routes through doorways, since treating a shut door as a wall
    // would mean a patroller could never leave a room that has one. So a body
    // held up this long leans on whatever is in front of it, and if that is a
    // door, the door gives.
    constexpr float EnemyDoorShoveTime  = 0.4f;
    constexpr float EnemyDoorShoveReach = 0.5f;   // Past its own radius
    constexpr float EnemyDoorShoveHigh  = 1.2f;   // Chest height, where a leaf is

    //--------------------------------------------------------------------------
    // Noticing a fight nobody has bled in yet.
    //
    // SpreadCries carries only as far as somebody being HURT. This is the other
    // half: standing next to a swordfight is enough to notice one.
    //--------------------------------------------------------------------------

    // Meter fill per second for a body in the same room as a fight, or within
    // range of one. Half a second to look round, which reads as reacting rather
    // than as a switch.
    constexpr float EnemyCombatAlarmRate  = 2.0f;

    // Close enough to notice from outside the room - the corridor beyond an open
    // arch, where a room test alone would miss it
    constexpr float EnemyCombatAlarmRange = 9.0f;

    // Shortest thing FindClip is allowed to return. KayKit ships Death_A_Pose,
    // Death_B_Pose and T-Pose - single-frame poses - in the same files as the
    // clips they are named after, and a substring match will take one of those
    // instead if the pack ever reorders. One frame at 60fps is 0.017s.
    constexpr float EnemyMinClipDuration = 0.10f;

    //--------------------------------------------------------------------------
    // Enemy types
    //
    // One row per kind of skeleton. A row that names a model or prop that is not
    // on disk is skipped with a warning rather than failing the level, and a clip
    // name that does not resolve falls back to the shared candidate lists in
    // EnemyManager, so a typo costs you the right swing and not a working enemy.
    //
    // `attackClip` and `idleClip` are matched case-insensitively as substrings.
    //
    // THREE THINGS ARE NOT FREE TO CHANGE HERE:
    //
    //  - `height` drives the hit capsule and, through FitScaleFor, how large the
    //    model draws. It also decides where the skull lands relative to the
    //    crosshair, because these characters are chibi - the skull alone is 45% of
    //    the character's height, and its centre sits at y = 1.35*(height/1.7)
    //    against an eye at 1.50. Below about 1.89 the skull drops under the
    //    crosshair and sits in your face at any standoff, because backing away
    //    shrinks it without moving it. Every row is 2.00 for that reason.
    //
    //  - The standoffs are ordered: personalSpace < stopDistance < attackRange.
    //    Under the first an enemy walks into a shove it cannot win and jitters;
    //    over the second it stops outside its own swing.
    //
    //  - The standoffs are sized against the MODEL, not the capsule, so they move
    //    with `height` and with how far the attack clip reaches. Measure a new
    //    attack clip before adopting it: forward is +Z, and the reaches already
    //    measured run from 0.74 (unarmed kick) to 1.48 (jumping chop).
    //
    // docs/enemy-animation-plan.md has the measurements and the harnesses.
    //--------------------------------------------------------------------------
    struct EnemyArchetype
    {
        const char *name;
        const char *modelPath;
        // Two things it can carry. Slot 0 is the weapon hand, slot 1 the off hand -
        // but neither is really "a hand": both are just a model and the bone to
        // hang it from, so slot 1 takes a shield on handslot.l or a quiver on the
        // spine equally well. "" leaves the slot empty.
        const char *propPath;
        const char *propBone;
        const char *offhandPath;
        const char *offhandBone;
        // Up to EnemyAttackVariants swings, rolled between so the same enemy does
        // not trace the same arc every time. "" ends the list; a row naming one
        // clip behaves exactly as it did when this was a single string.
        const char *attackClips[EnemyAttackVariants];
        const char *idleClip;

        int   maxHealth;
        int   damage;
        // What killing one is worth AT RANK 1, before the rank scales it. A column
        // and not a formula off health and damage: the two do not measure the same
        // thing to a player. A Warrior is 90 health of standing still and a Mage is
        // 50 health of being shot at from across a room, and which of those was the
        // harder fight is a judgement the table gets to make.
        int   exp;
        float cooldown;
        float height;
        float speed;

        float personalSpace;
        float stopDistance;
        float attackRange;

        // How often it raises its guard between swings, 0 to 1. Rolled once per
        // cooldown, so it is the fraction of gaps it spends blocking rather than a
        // per-frame chance. 0 for anything with nothing to block with.
        //
        // A guard needs somewhere to live, and the only place it fits is the gap
        // between the end of one swing's CLIP and the start of the next swing:
        //
        //     held guard = cooldown - attack clip length - EnemyBlockDropTime
        //                                                - EnemyAnimBlendTime
        //
        // A type whose cooldown is not comfortably longer than its own swing has
        // no such gap and will never block however high this is set. Three of the
        // four rows below were in exactly that state; see the clip lengths in
        // docs/enemy-animation-plan.md §3 before raising this on a new one.
        float blockChance;

        // Shoots rather than swings. Its `attackClip` is then a shoot animation
        // and `damage` is what the arrow carries; `attackRange` becomes the range
        // it opens fire at and `stopDistance` where it prefers to stand, so the
        // same three ordered numbers do the same job at a different scale.
        bool ranged;

        //----------------------------------------------------------------------
        // Casts motes rather than throwing something solid, and which school's
        // colour they are. An index into the Magic table, or -1 for an archetype
        // whose shot is an object.
        //
        // The school decides the COLOUR and nothing else. Its speed and damage
        // stay the archetype's, because those are what the kind is balanced on -
        // borrowing a player school's numbers would make a Mage as strong as
        // whatever the player last pressed a number key for.
        //----------------------------------------------------------------------
        int magic;

        //----------------------------------------------------------------------
        // Stops shooting and channels a buff over its allies instead.
        //
        // The one archetype flag that makes a body worth killing FIRST. Everything
        // else in this table is a threat in its own right, so a pack is a set of
        // problems the player can take in any order; a supporter makes the order
        // matter, which is the whole reason it exists.
        //
        // It never buffs itself. A support that also sharpened its own teeth would
        // be the biggest threat in the room as well as the one you have to kill
        // first, and those are two different jobs - the point is that it is
        // dangerous through OTHERS, so killing it has to be about them.
        //----------------------------------------------------------------------
        bool support;

        //----------------------------------------------------------------------
        // What this kind is made of, over and above its health and damage line.
        //
        // NEVER scaled by rank. Rank moves health, damage and what a kill pays; a
        // stat that climbed with it would be a property of the number rather than
        // of the kind, and the Minion would stop being fragile the moment it was
        // rolled deep. A kind's stat line is part of what the kind IS, exactly
        // like its walk speed.
        //
        // 10 is neutral and contributes nothing, so a row that leaves this
        // default behaves exactly as it did before stats existed. The interesting
        // half is BELOW ten: that is where a weakness is written, and a weakness
        // is what makes one kind in a pack the one to open on.
        //
        // Arcane is at the line on every row and will stay there until enemies
        // cast - it is in the struct because the four travel together, not
        // because anything reads it yet.
        //----------------------------------------------------------------------
        StatBlock stats;

        //----------------------------------------------------------------------
        // How much of its own pool this KIND absorbs before it flinches, on top
        // of whatever its tier adds - see EnemyTierDef::poise in EnemyRank.h,
        // which is where the mechanism and the units (a fraction of max health)
        // are explained. Tier alone used to be the whole of it, which meant a
        // Minion and a Warrior staggered identically at the same tier despite
        // one of them being built to take a hit and the other built to run.
        // This is the kind's own floor; a Champion of either still adds the
        // tier's 0.125 on top.
        //----------------------------------------------------------------------
        float poise;
    };

    constexpr EnemyArchetype EnemyTypes[] =
    {
        // Fast, fragile, empty handed - what the unarmed clips were tuned against.
        // Nothing in its hands to block with, so it never does.
        { "Minion", "models/enemies/Skeleton_Minion.glb", "", "", "", "",
          { "melee_unarmed_attack_punch", "melee_unarmed_attack_kick", "" }, "idle_a",
        // 1.30 rather than 1.10: the punch is 1.167s and the kick 0.933s, so the
        // old cooldown was shorter than the swing it had to contain - the next
        // attack came due before the clip had played out, and the state machine
        // never got a gap between them. Still the fastest thing in the table.
        // Paper, and quick. Four under the line on constitution is -8% off an
        // already small pool; two over on skill is barely a crit at all, and is
        // there so a swarm occasionally stings rather than always tickling.
          40,  6,  8, 1.30f, 2.00f, 5.6f,   2.0f, 2.2f, 2.4f,   0.00f, false,
          -1, false, { 6, 10, 12, 10 }, 0.00f },

        // Slow and heavy, blade in the right hand. The one that fights defensively,
        // and the only row whose cooldown is set by the guard rather than by the
        // swing: 1.90 against a 1.07s chop leaves 0.59s of held guard, where the
        // 1.50 it used to be left 0.06s and the state never appeared at all. The
        // damage it stops is what pays for the damage it no longer deals.
        // The shield is not decoration: this is the only archetype that blocks, and
        // a guard animation played with an empty left hand reads as a mistake.
        { "Warrior", "models/enemies/Skeleton_Warrior.glb",
          "models/enemies/props/Skeleton_Blade.gltf", "handslot.r",
          "models/enemies/props/Skeleton_Shield_Small_A.gltf", "handslot.l",
        // The second swing is named short on purpose. raylib stores an animation
        // name in a char[32], and "Melee_1H_Attack_Slice_Horizontal" is exactly 32
        // characters, so what actually lands in the struct is missing its last
        // letter and the full name can never match. FindClip matches substrings,
        // which is what makes a shortened name work at all - and this is the only
        // clip in the pack long enough to hit the limit.
          { "melee_1h_attack_chop", "attack_slice_horizonta", "" }, "idle_a",
        // The wall. Six over the line on constitution on top of the highest base
        // health in the table, and under it on skill - it does not need luck, it
        // needs you to still be standing there in four seconds' time.
          90, 11, 20, 1.90f, 2.00f, 4.0f,   2.0f, 2.2f, 2.4f,   0.55f, false,
          -1, false, { 16, 12, 7, 10 }, 0.06f },

        // Quick and light, axe, and answered by swinging back rather than by
        // waiting. Still no guard - 1.15 leaves nowhere near enough gap to raise
        // one, and giving it a real guard means slowing it to about 1.35, which is
        // a different archetype.
        { "Rogue", "models/enemies/Skeleton_Rogue.glb",
          "models/enemies/props/Skeleton_Axe.gltf", "handslot.r", "", "",
        // Two swings now that the cooldown clears them: the 1.00s slice and the
        // 1.067s chop. Stab is out at 1.60s - the cooldown has to contain the clip
        // or the next attack is due before this one has finished playing.
          { "melee_1h_attack_slice_diagonal", "melee_1h_attack_chop", "" }, "idle_a",
        // 1.15 rather than 0.85, which was shorter than its own 1.00s slice. That
        // is the mismatch docs/enemy-animation-plan.md flags for Phase 4, and the
        // load-time check now catches it rather than leaving it to be noticed.
        // Still the quickest blade here, and with a real gap it could take a
        // second swing - see the note above about why it has only one.
        // The one that gets lucky. Eight over the line on skill is a 1-in-4 crit
        // at half again the damage, which against a fast axe is what makes the
        // Rogue the body in a pack that can actually take a chunk out of you -
        // and it pays for it with the second-lowest constitution here.
          55,  7, 12, 1.15f, 2.00f, 6.0f,   2.0f, 2.2f, 2.4f,   0.00f, false,
          -1, false, { 8, 10, 18, 10 }, 0.02f },

        // Staff, and a bolt off the end of it. The second ranged archetype, and
        // deliberately not a reskinned Archer: it stands at 7 where the Archer
        // stands at 9, reloads a fifth slower, and hits half again as hard for it.
        // Closing on the Archer is a walk; closing on this is a walk taken while
        // being hit harder, which is the whole difference between them.
        //
        // The bolt is an arrow model until there is a spell one - it travels, it
        // is blocked by walls, and Ranged_Magic_Shoot is 0.93s against a 2.40
        // cooldown, so the clip always finishes with room to spare.
        { "Mage", "models/enemies/Skeleton_Mage.glb",
          "models/enemies/props/Skeleton_Staff.gltf", "handslot.r", "", "",
          { "ranged_magic_shoot", "ranged_magic_spellcasting", "" }, "idle_a",
        // Hits hard from range and folds when reached: five over on arms, five
        // under on constitution. Closing on it is the whole answer to it, and the
        // stat line is what makes closing worth the walk.
        // The only caster and the only support. Its bolt is a mote of TOXIN - the
        // one school on the table nothing else uses much, so a green streak across
        // a room is unambiguously an enemy cast and not the player's own.
          50, 12, 16, 2.40f, 2.00f, 4.4f,   2.0f, 7.0f, 9.0f,   0.00f, true,
          (int)Magic::Toxin, true, { 5, 15, 10, 10 }, 0.00f },

        //----------------------------------------------------------------------
        // The one that shoots. Crossbow, and a real arrow that travels.
        //
        // Its three standoffs are the same ordered chain as everyone else's, just
        // at a different scale: it opens fire at 11, prefers to stand at 9, and
        // still refuses to be closer than 2 like every other body. The camera
        // framing rules are untouched by that - they are about what happens when
        // you DO get close, and this one is shoved out to 2.0 exactly as the rest.
        //
        // It reuses the Rogue's character because the pack has four and all four
        // are spoken for; the crossbow reads as the difference. `damage` is what
        // the arrow carries, and 8 against the Rogue's 7 is deliberate - being
        // shot from across a room should cost more than being nicked in melee,
        // because you had further to walk to stop it.
        //----------------------------------------------------------------------
        { "Archer", "models/enemies/Skeleton_Rogue.glb",
          "models/enemies/props/Skeleton_Crossbow.gltf", "handslot.r", "", "",
          { "ranged_2h_shoot", "", "" }, "idle_a",
        // Steady. Nothing above the line and nothing much below it - the Archer's
        // threat is its standoff, not its numbers, and a stat line that agreed
        // with the crossbow would make it the answer to everything at range.
          45,  8, 14, 2.20f, 2.00f, 4.6f,   2.0f, 9.0f, 11.0f,  0.00f, true,
          -1, false, { 9, 11, 10, 10 }, 0.02f },

        //----------------------------------------------------------------------
        // The two hander, and the reason attack variants exist.
        //
        // The 2H family is the widest the pack ships - Chop is an overhead, Slice
        // a horizontal sweep, Spin a full turn - and they are different enough
        // that watching one enemy cycle them reads as a fighter rather than a
        // loop. Everything else in the table gained variants from the same
        // mechanism; this row is the one that needed it.
        //
        // Both hands on the haft means no shield, which is why it does not block
        // despite being the heaviest thing here: its defence is that you have to
        // walk into a 2.60s cooldown to answer it. Spin is 2.40s on its own, so
        // that cooldown is nearly all clip - see the note on blockChance for what
        // happens to a row whose cooldown does not clear its own swing.
        //
        // Reuses the Warrior character with the axe instead of blade-and-board,
        // the same way the Archer reuses the Rogue.
        //----------------------------------------------------------------------
        { "Reaver", "models/enemies/Skeleton_Warrior.glb",
          "models/enemies/props/Skeleton_Axe.gltf", "handslot.r", "", "",
          { "melee_2h_attack_chop", "melee_2h_attack_slice", "melee_2h_attack_spin" }, "melee_2h_idle",
        // The heaviest thing in the table on every axis that matters and nothing
        // over the line anywhere. It does not need a stat to be frightening - the
        // 18 damage and the 2.6s commitment already are - and giving the biggest
        // base numbers a bonus on top is how a table stops having a top and
        // starts having an outlier.
          110, 18, 28, 2.60f, 2.10f, 3.8f,   2.0f, 2.3f, 2.6f,   0.00f, false,
          -1, false, { 13, 10, 10, 10 }, 0.08f },
    };

    constexpr int EnemyTypeCount = (int)(sizeof(EnemyTypes)/sizeof(EnemyTypes[0]));

    //--------------------------------------------------------------------------
    // Where enemies live.
    //
    // A camp is a place on the map that holds a garrison and quietly refills it.
    // That is the whole population model: there is no global wave timer and no
    // spawn queue, because a camp that maintains its own numbers gives both for
    // free - it is an interval, and it is a group of enemies in a place.
    //
    // It also bounds the fight. A wave timer that fires regardless of what is
    // still alive eventually buries the player under everything it ever spawned;
    // a garrison cannot exceed its own size however long you avoid it.
    //
    // Cells must be floor. `types` is the rows of EnemyTypes this camp draws
    // from, terminated by -1, so a camp can be one kind or a mixed band.
    //--------------------------------------------------------------------------
    // No cell coordinates: the map is generated, so there is nothing fixed to
    // author against. A camp says what it is made of and EnemyManager::ChooseCampRoom
    // says where it stands, which is the only split that survives regeneration.
    struct SpawnCamp
    {
        const char *name;
        float spread;       // World units a body may stand from the room's middle
        int garrison;       // How many living bodies it keeps
        int types[3];       // Archetype rows, -1 ends the list

        // How many of the garrison walk a beat instead of standing post. A camp
        // that is all sentries is a camp the player meets only on their own terms,
        // and one that is all patrollers is an empty room with a wandering monster
        // problem. One or two out of three is the mix that makes a corridor
        // dangerous without emptying the room it came from.
        int patrollers;

        // Which kinds of room this camp would rather hold, in order, ending at
        // RoomKind::Count. A preference and not a requirement - a map that offers
        // none of them still gets its camps, just somewhere less fitting.
        RoomKind prefer[3];
    };

    // Table order is priority: the first camp gets the pick of the rooms.
    constexpr SpawnCamp SpawnCamps[] =
    {
        // Fast and unarmoured, the ones you can fight in the open without being
        // outranged - and the Reaver, whose swings need room to be read. A two
        // hander in a corridor is just a wall of hitboxes, so this camp wants space.
        { "North watch", 4.0f, 3, { 0, 2, 5 }, 1,
          { RoomKind::Guardroom, RoomKind::Lair, RoomKind::Count } },

        // A Warrior in front of an Archer is the exact arrangement the shot-line
        // check exists for, and this camp is composed to produce it rather than
        // wait for it to happen.
        { "Pillar guard", 4.5f, 3, { 1, 4, -1 }, 1,
          { RoomKind::Guardroom, RoomKind::Prison, RoomKind::Count } },

        // The only camp with both casters. Somewhere enclosed is where holding a
        // line of fire actually means something.
        { "East hall", 4.5f, 3, { 3, 4, 1 }, 0,
          { RoomKind::Shrine, RoomKind::Workshop, RoomKind::Count } },

        // Asleep in their bunks until something wakes them. All melee, because a
        // barracks emptying into a corridor should be a wall of bodies.
        { "Barrack watch", 5.0f, 3, { 1, 0, 2 }, 2,
          { RoomKind::Barracks, RoomKind::Guardroom, RoomKind::Count } },

        // Minions among the graves. Weak individually and placed where the map is
        // furthest from the entrance, which is where the crypts end up.
        { "Grave watch", 4.0f, 2, { 0, 3, -1 }, 1,
          { RoomKind::Crypt, RoomKind::Lair, RoomKind::Count } },
    };

    constexpr int SpawnCampCount = (int)(sizeof(SpawnCamps)/sizeof(SpawnCamps[0]));

    // The gap between one body arriving and the camp being allowed to send the
    // next. Per camp, not global, so clearing one camp does not slow another's
    // recovery - and long enough that a cleared camp stays cleared for a while.
    constexpr float CampRespawnDelay   = 7.0f;

    // A camp will not reinforce while the player is this close. Bodies climbing
    // out of the floor in front of you read as the game cheating, however good
    // the spawn animation is.
    constexpr float CampSafeDistance   = 14.0f;

    // How many times to look for somewhere to stand inside a camp before giving
    // up for this frame. A camp packed into a corner can genuinely have no room,
    // and the answer to that is to try again shortly, not to search forever.
    constexpr int   CampPlacementTries = 24;

    // How many things one enemy can carry. Slot 0 is the weapon hand, slot 1 the
    // off hand or the back.
    constexpr int EnemyPropSlots = 2;

    // Enemy support ------------------------------------------------------------
    // What a channelled buff is worth, and what it costs the caster to give.
    //
    // The cost is the point. A supporter stands still for the whole channel, does
    // not shoot, does not step aside, and is drawn lit up while it does - so the
    // buff is bought with the one thing a ranged enemy is otherwise never short
    // of, which is being hard to reach. A player who reads it and closes gets a
    // free kill; one who ignores it fights a pack that hits half again as hard.
    constexpr float EnemyChannelTime    = 1.6f;
    constexpr float EnemyChannelCooldown = 9.0f;
    // World units the buff reaches. Generous - it is a room-sized effect, and a
    // radius the player has to measure is one they cannot play against.
    constexpr float EnemyBuffRadius     = 9.0f;
    // How few allies make it not worth doing. Below this the caster shoots: a
    // support that channels over one skeleton has spent nine seconds of cooldown
    // and a second and a half of standing still on almost nothing.
    constexpr int   EnemyBuffMinAllies  = 2;

    constexpr float EnemyBuffTime       = 12.0f;
    constexpr float EnemyBuffDamage     = 1.5f;
    // Multiplies how FAST a buffed body swings, so it divides the gap between
    // swings - the same way a tier's own `swing` column does
    constexpr float EnemyBuffHaste      = 1.25f;
    // The aura under a buffed body, and the caster while it channels. Green,
    // matching the Mage's own school, so what is being cast and what it did are
    // the same colour.
    constexpr unsigned char EnemyBuffColour[3] = { 120, 230, 110 };

    // How large an enemy mote's impact flash is drawn, as a fraction of the
    // school's own impact size. Small: it is there to say a hit landed, not to be
    // mistaken for a spell the player cast.
    constexpr float EnemyMoteImpactScale = 0.35f;

    // Projectiles ---------------------------------------------------------------
    // Only enemies shoot, so these are all about arrows crossing a room at a
    // standing player.
    //
    // Speed is a readability choice as much as a physics one: fast enough that
    // standing still is punished, slow enough that the arrow is visibly in the air
    // and can be walked out of. At 24 u/s a shot from the archer's 9-unit standoff
    // takes 0.37s.
    constexpr float ProjectileSpeed     = 24.0f;
    // Longest a shot lives if it hits nothing. Comfortably past aggro range.
    constexpr float ProjectileLife      = 3.0f;
    // Never advance a shot further than this in one test. Must stay well under the
    // player's body radius or a shot steps straight through someone standing still.
    constexpr float ProjectileStep      = 0.12f;
    // Radius the shot is treated as having when testing the player's capsule
    constexpr float ProjectileRadius    = 0.10f;

    // How long a spent shot stays in whatever it landed in. A shot that vanished
    // on contact gave the player nothing to read: where their own throws went,
    // where they are being shot from, whether that volley missed or was blocked.
    // Purely scenery from the moment it stops - nothing collides with it.
    constexpr float ProjectileStickLife = 12.0f;
    // How far past the surface it buries itself, on top of the half length that is
    // already inside by virtue of the model being drawn about its own middle.
    // Small: enough that the point is not visibly floating off the stone.
    constexpr float ProjectileStickBite = 0.06f;

    // Above this - measured from the floor - a shot is out of the level and is
    // simply gone. These levels are open topped, so there is nothing up there to
    // hit, and a shot that stuck to the grid at this height hung in the skybox.
    //
    // Deliberately well clear of WallHeight rather than equal to it. A ceiling at
    // wall height is an invisible lid barely over the player's head: a staff whose
    // muzzle sits 1.98 units out spawns its bolt at 3.47 when aimed up, which is
    // already most of the way to 4.000 before the shot has moved at all.
    constexpr float ProjectileSkyHeight = WallHeight*3.0f;
    // Skeleton_Arrow measures 0.117 x 0.749 x 0.102 in its own units, so it is
    // authored standing along +Y and 0.75 units long - drawn 1:1 it is a little
    // over a third of an enemy's height, which is about right for an arrow.
    //
    // Nothing here needs a yaw correction, unlike EnemyModelYaw: the draw builds
    // the rotation that takes +Y onto the velocity directly, so the authored axis
    // is named once in Projectile.cpp and never guessed at. It was guessed at
    // once, as +X, which is a 90 degree error that looks plausible in a still.
    constexpr float ProjectileDrawScale = 1.0f;

    // Magic ---------------------------------------------------------------------
    // What the player's casts look like. The per-school numbers - colour, speed,
    // damage, which impact sheet - live in the table in combat/Magic.cpp; what is
    // here is the shape of a mote, which is the same for every school so that eight
    // different spells still read as eight of the same KIND of thing.
    //
    // The mote texture: a white radial falloff generated at load, tinted per school
    // at the draw. 64 is generous - it is drawn a quarter of a unit across, so it is
    // never more than a few dozen pixels, and the falloff has no detail in it to
    // lose. Softness is the exponent on (1 - radius): 1 is a linear ramp that reads
    // as a flat disc under additive blending, and much above 3 the mote shrinks to a
    // pinprick with a haze round it.
    constexpr int   MoteTextureSize   = 64;
    constexpr float MoteSoftness      = 2.2f;

    // How much wider the coloured halo is than the core, and how strongly it is
    // added. The halo is what carries the school's colour - the core saturates to
    // near white - so it has to be clearly bigger than the core and clearly dimmer,
    // or the mote is one flat bright ball with no colour left in it.
    constexpr float MoteHaloScale     = 2.6f;
    constexpr int   MoteHaloAlpha     = 150;
    // How far the core is mixed towards white, in [0, 1]. Mixed rather than
    // brightened: see the note at DrawMotes.
    constexpr float MoteCoreWhiten    = 0.72f;

    // The tail. Samples strung back along the heading, spaced in mote radii. Four
    // at 0.9 is about one and a half mote diameters of tail - enough to say which
    // way it is going, short enough that it is not a streak the player reads as a
    // beam. Each sample costs two triangles, and they are the same texture and the
    // same blend as the mote itself, so this is free in every way that matters.
    constexpr int   MoteTrailCount    = 4;
    constexpr float MoteTrailSpacing  = 0.9f;

    // How far back along the heading the impact effect is centred from the point
    // the mote actually stopped. Left exactly there, half the billboard is inside
    // the wall or inside the chest and the depth test eats it.
    constexpr float MoteBurstBackoff  = 0.35f;

    // How many impact effects can be playing at once. One cast is one effect, and
    // shots are limited by fire rate rather than by a magazine, so this only has to
    // cover the overlap of a few casts in flight at once plus whatever the enemies
    // set off. Overflowing recycles the oldest rather than dropping the newest.
    constexpr int   MaxActiveVfx      = 32;
    // Where in the shoot clip the arrow actually leaves, as a fraction of it. The
    // crossbow comes up over the first half of Ranged_2H_Shoot, so firing on the
    // frame the state starts sends the arrow before the weapon is raised.
    constexpr float EnemyShootRelease   = 0.45f;

    //--------------------------------------------------------------------------
    // Where in a melee clip the blow actually lands.
    //
    // The same idea as EnemyShootRelease and it exists for the same reason: a
    // blow used to resolve on the frame the swing STARTED, which meant an enemy
    // damaged the player before the weapon had begun to move. It read as being
    // hit by an intention rather than by an axe, and it made the wind-up - the
    // one part of an attack a player is supposed to answer - decoration.
    //
    // 0.55 rather than the shot's 0.45: a bow is loosed early in its clip and a
    // swing connects past the middle of its arc. Both are fractions of the clip
    // rather than seconds, so a slow chop and a quick jab each land at the right
    // moment in their own animation.
    constexpr float EnemyMeleeLand      = 0.55f;

    //--------------------------------------------------------------------------
    // What the blow still has to be true of when it lands.
    //
    // Re-tested at the landing frame rather than trusted from the frame the swing
    // began, which is the whole point of the change: stepping out of a swing has
    // to be able to make it miss, or the wind-up is still decoration.
    //
    // Both are generous on purpose. The enemy is pinned in place for the whole
    // clip while the player is free to move, so a tight window would make every
    // melee archetype trivially kiteable - which is a different broken fight, not
    // a fixed one. The slack is what a weapon's reach past the body's own stop
    // distance is worth, and the arc is a swing rather than a thrust.
    constexpr float EnemyMeleeLandSlack = 0.9f;     // World units past attackRange
    constexpr float EnemyMeleeLandArc   = 150.0f;   // Degrees, centred on the facing
    // A ranged enemy backs off when the player closes inside this fraction of its
    // standoff, rather than standing there being hit
    constexpr float EnemyRangedRetreat  = 0.6f;

    // Clearing the line of fire -----------------------------------------------
    // A ranged enemy checks the line its shot will actually travel - muzzle to
    // eye, not centre to centre - and steps sideways when something is in it.
    // Awareness uses the centre-to-eye line and is unchanged; these two lines
    // differ by however far the weapon is held from the body, which is exactly
    // the gap that produces a shot into a doorframe the enemy can see past.
    //
    // How much room a shot wants beside a body it has to pass. Bigger than the
    // arrow so the enemy clears the line properly rather than shaving it and
    // firing into a shoulder anyway.
    constexpr float EnemyShotClearance  = 0.25f;
    // How long it commits to one side before trying the other. Long enough to
    // actually get somewhere - a shorter fuse makes an enemy that cannot solve
    // its line oscillate on the spot instead of walking round the problem.
    constexpr float EnemyStrafeFlipTime = 1.2f;

    //--------------------------------------------------------------------------
    // Grip adjustment, applied to a prop before the bone transform.
    //
    // Per PROP, not per slot and not global. That distinction was learned the hard
    // way: the constants here used to be one shared set, which is fine only while
    // every prop is authored the same way round. Measured, they are not:
    //
    //   Skeleton_Blade      0.573 x 1.497 x 0.222   along +Y
    //   Skeleton_Axe        0.989 x 1.252 x 0.282   along +Y
    //   Skeleton_Staff      0.598 x 2.102 x 0.753   along +Y
    //   Skeleton_Shield_*   0.832 x 0.832 x 0.156   along +Y
    //   Skeleton_Arrow      0.117 x 0.749 x 0.102   along +Y
    //   Skeleton_Crossbow   1.111 x 0.508 x 1.419   along +Z   <-- the odd one
    //
    // handslot's local +Y is the out-of-the-fist direction, so identity is right
    // for everything authored along +Y and the table below is empty of them. The
    // crossbow is authored along +Z, so with no correction its barrel follows
    // handslot's local +Z, which points across the body: it aims sideways.
    //
    // In the Archer's aiming pose handslot's local +X is the axis pointing where it
    // aims, and yaw 90 turns the crossbow's barrel onto it - measured as dot +0.994
    // against model forward, where uncorrected reads +0.018. A yaw-only correction
    // also cannot disturb the others by construction: they are authored along +Y,
    // and rotating about Y leaves that axis exactly where it was.
    //
    // Matched as a substring of the prop's path, resolved once at load. Offsets are
    // in model units, angles in degrees. Anything not listed gets identity.
    //--------------------------------------------------------------------------
    struct PropGrip
    {
        const char *match;
        float pitch, yaw, roll;
        float scale;
        float offsetX, offsetY, offsetZ;
    };

    constexpr PropGrip EnemyPropGrips[] =
    {
        { "Skeleton_Crossbow", 0.0f, 90.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f },
    };

    constexpr int EnemyPropGripCount = (int)(sizeof(EnemyPropGrips)/sizeof(EnemyPropGrips[0]));

    // What a prop gets when the table says nothing about it
    constexpr PropGrip EnemyPropGripDefault = { "", 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f };
    // The characters ship with no clips of their own - these come from KayKit
    // Character Animations, which is split by category, so the five states an
    // enemy needs are spread over three files. All are Rig_Medium, which is the
    // rig the skeletons are built on.
    constexpr const char *EnemyAnimPaths[] =
    {
        "models/enemies/animations/Rig_Medium_General.glb",          // Idle, Hit, Death
        "models/enemies/animations/Rig_Medium_MovementBasic.glb",    // Running, Walking
        "models/enemies/animations/Rig_Medium_CombatMelee.glb",      // The swing
        "models/enemies/animations/Rig_Medium_CombatRanged.glb",     // The shot
    };
    // Cross-fade between animation states, seconds. Long enough to hide the cut,
    // short enough that a swing still lands when the damage does - the hit itself
    // resolves off the cooldown, not the clip, so a slow fade would drift the two
    // apart visibly.
    constexpr float EnemyAnimBlendTime  = 0.12f;
    // How close the next swing has to be for the body to hold its attack rather
    // than return to idle. The swing clip runs 1.17s against EnemyAttackCooldown
    // of 1.2s, and without this the two frames of daylight between them show up as
    // the idle pose flashing on and off between every pair of swings.
    constexpr float EnemyAnimHoldSlack  = 0.3f;

    // Art-only multiplier on top of the fit to each type's `height`. 1.0 means the
    // model is drawn exactly as tall as its hit capsule.
    //
    // To make enemies genuinely bigger - taller, harder to miss, and framed higher
    // in the view - raise the type's `height` instead and the capsule follows. This
    // is for the case where the art wants a nudge and the fight does not, so it
    // deliberately does NOT move the capsule: push it far from 1.0 and what you see
    // stops matching what you hit.
    constexpr float EnemyModelScale     = 1.0f;
    // Added to the enemy's facing before drawing. Body forward at yaw 0 is -Z, and
    // the KayKit skeletons are authored facing +Z, so they need turning around.
    //
    // This also fixes which way "forward" is when measuring how far a clip reaches
    // at the player: +Z in model space, not -Z. Measuring the wrong side reads the
    // skeleton's back and understates an attack badly - Punch_A measures 0.45 that
    // way against a true 1.47.
    constexpr float EnemyModelYaw       = 180.0f;
    // How long a corpse stays after its death clip finishes. This is the ragdoll's
    // whole life, so it also decides how long the body has to settle.
    constexpr float EnemyCorpseLinger   = 4.0f;
    // Trim on the ragdoll's fall. The death clip already puts the body on the
    // floor, so the physics is mostly settling limbs - full gravity reads as a
    // slap. Raise it toward 1.0 for a heavier drop.
    constexpr float RagdollGravityScale = 0.5f;
}
