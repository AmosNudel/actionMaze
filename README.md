# Dungeon Foray

First person action game built on [raylib](https://www.raylib.com/), grown from the
`[core] 3d camera fps` example. C++14, no dependencies beyond raylib.

## Build

```sh
make                    # release build -> DungeonForay.exe
make BUILD_MODE=DEBUG   # debug build (-g -O0)
make run                # build and launch
make clean              # remove obj/ and the executable
```

On Windows the makefile defaults to the raylib installer layout
(`C:/raylib/raylib` + `C:/raylib/w64devkit`); override with
`make RAYLIB_PATH=... COMPILER_PATH=...`. On Linux/macOS point `DESTDIR` at the
prefix raylib is installed under (default `/usr/local`).

**`lib/libraylib.a` is linked ahead of that install**, and is a raylib 6.0 built
with `SUPPORT_GPU_SKINNING` — the animated enemies need it, see [Animated
enemies](#animated-enemies). The install itself is stock and untouched, so other
projects sharing it are unaffected. To rebuild:

```sh
make -C $RAYLIB_PATH/src CUSTOM_CFLAGS="-DSUPPORT_GPU_SKINNING=1"
cp $RAYLIB_PATH/src/libraylib.a lib/           # then restore that tree's own copy
```

Delete `lib/libraylib.a` and the build falls back to the shared install, where
enemies would need CPU skinning — a full `Model` per enemy.

In VS Code: <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>B</kbd> builds, F5 debugs.
Both work from any open file now — the build no longer keys off the filename.

Every `.cpp` under `src/` is compiled automatically, so **new files need no
makefile edit**. Header dependencies are tracked (`-MMD`), so touching a `.h`
rebuilds only what includes it.

## Controls

| Input | Action |
| --- | --- |
| W A S D | Move |
| Mouse | Look |
| Space | Jump |
| Left Ctrl | Crouch |
| Mouse wheel | Cycle the right hand weapon (wraps through "empty") |
| Shift + wheel | Cycle the left hand weapon |
| Left mouse | Attack with the right hand |
| Right mouse | Attack with the left hand (hold to keep a shield up) |
| E | Trade, when standing at a vendor |
| 1-8 | Select a school of magic (only the ones you own) |
| Tab | Character page |
| Esc | Pause page |

## World and collision

The level is a tile grid ([Map.h](src/world/Map.h)) at `MapCellSize` 4.0 — set by the KayKit dungeon pack, whose floor tiles are
4.000 square and wall pieces 4.000 long, so every model sits at native scale — and it is
generated ([Map::Generate](src/world/Map.cpp)). The hand-authored `ArenaRows` survives only
as a fallback for the degenerate case where no room could be placed at all. Cell (0,0) sits at
the world origin, so converting between the two is a multiply and a floor. **Anything
outside the grid reads as `Wall`**, which means collision, line of sight and the AI all
treat the map edge as solid with no special case.

`Level::ResolveBody()` resolves a circle against the wall cells it overlaps, *after* the
body has moved, rather than sweeping — no previous position needed, and wall sliding
falls out of it: only the component of velocity heading into the wall is cancelled, so
speed along the wall survives. That matters because `Body::Update` accelerates from
whatever velocity it finds. Two passes settle an inside corner.

`Level::LineOfSight()` is a grid DDA, stepping cell to cell rather than sampling points —
exact, and it costs one iteration per cell crossed.

Two constraints hold it together: `Body::radius` (0.4) must stay under half a cell or a
body can wedge in a corner, and `Game::Run` caps `delta` at `MaxFrameTime` (1/30) so one
hitch cannot carry a body through a wall before collision sees it.

Movement was slowed for indoor scale — `MaxSpeed` 20 → 9. Twenty units/s is about
72 km/h, which turns a three unit corridor into a pinball table. Gravity and jump are
unchanged, so the 2.25 unit apex still clears nothing at `WallHeight`.

## Hitting things

**Reach is an authored gameplay number and is deliberately unrelated to the drawn
weapon.** The view models are miniatures about half a unit from the eye at 0.45 scale —
`sword_A` renders as an 0.8-unit object sitting *inside* the player's own 0.4 collision
radius. Its world transform is cosmetically perfect and physically meaningless. Hit-test
the rendered blade and a sword that fills the screen would reach about a metre.

So melee magnifies the drawn pose instead. `ViewModel::BladeFor` takes the very same
pose matrix the weapon is rendered with — rest lerped toward the end pose, per-swing
variation, sway and all — and scales it about the eye until the tip lands at `reach`.
What comes back is a **capsule down the blade in world space**: same direction, same arc,
same timing as the picture, with only the length being a gameplay number.

[Attack.cpp](src/combat/Attack.cpp) then **sweeps** that capsule from where it was last
frame to where it is now (`Config::MeleeSweepSteps` samples, because a tip crosses more
than its own width in a frame) against each enemy's `BodyCapsule`, then `LineOfSight` so
nothing is hit through a wall. A swing that visibly misses now misses — which is the
whole reason this is not a cone any more. A cone ignored where the weapon was pointing,
and what that bought in forgiveness it paid for in hits landing a metre wide of the
target. Forgiveness now comes from `MeleeBladeRadius`, i.e. blade thickness, which stays
honest: the blade still has to travel through the body.

Reach defaults come off the model's measured height (`MeleeBaseReach + height*0.5`), so a
dagger does not reach as far as a greatsword without anyone authoring 21 numbers; a small
override table in [Weapon.cpp](src/combat/Weapon.cpp) fixes the ones that get.

Because a swept blade sits inside a body for several frames rather than resolving on one,
`AttackState` carries a **hit list** (`hitIds`) instead of a one-shot flag — enemy ids,
not indices, since `RemoveDead` compacts the vector mid-swing. The stroke is live between
`liveFrom` and `liveTo` (0.25 to 1.0), so a weapon still climbing out of rest does not
land blows with its own grip.

Cast and Throw put a projectile in the air instead of cutting, released at `releaseAt`
from that same capsule's tip — so a bolt leaves the end of the staff and a thrown dagger
leaves the point, carrying the hand's exact orientation into flight.

Enemies keep out of `EnemyPersonalSpace` (1.15) of the player, which is deliberately
wider than the two bodies are. Radius alone would let a capsule's surface sit almost
against the lens; stopping at attack range is not enough either, because the enemy
carries momentum past it and nothing stops the player walking into an enemy. The player
absorbs a fifth of the shove rather than none, so an enemy caught between the player and
a wall pushes free instead of jittering.

Enemies are vertical capsules — rotation-free, so every distance test stays cheap and
melee never needs to know which way one is facing. They are a `Body`, so they inherit
gravity, acceleration and `Level::ResolveBody` for free.

Sight is what decides a fight: walk at the player when it can see them, swing when close.
**Nothing routes while it can see where it is going** — a body that walks an A* path
across an open room takes the corners of the grid rather than the line, and reads as a
machine rather than as a swordsman. It walks straight and lets `Level::ResolveBody` slide
it along whatever it meets.

Routing ([PathFinder.h](src/world/PathFinder.h)) is for everything else. Losing sight used
to end the thought entirely — the enemy simply stopped, and any corner in the level beat
it. Now it paths to where the player was last seen, searches there, gives up when
`alertTime` runs out, and walks back to its post; patrollers walk a beat between two rooms
for as long as they live. A shut door is passable to the search and gets struck open by
whatever walks into it, since treating it as a wall would mean a patroller could never
leave a room that has one.

Two things keep a routed body moving. A shut door is passable to the search and gets
**shoved open** by whatever leans on it for `EnemyDoorShoveTime` — the other half of the
bargain that lets a patroller leave a room with a door in it. And in a fight the straight
line stays a straight line until it stops working: closing on the player without getting
closer for `EnemyShoveTime` means something is in the way that `ResolveBody` will not slide
you round — a table in open floor, where a wall would have carried you along it — so the
body routes for a moment and then goes back to walking at them.

`SpreadCries` carries only as far as somebody being **hurt**, so `SpreadAlarm` covers the
rest: a fight in your room, or within `EnemyCombatAlarmRange`, fills your detection meter
whether or not anyone in it has bled yet. Otherwise a guard could duel the player in the
middle of a room while the two beside it went on facing the wall.

The search is **four-connected, never eight**. Walls stand on the lines *between* cells, so
two diagonal neighbours can both be open floor with solid stone on the corner they share,
and a diagonal step walks straight through it. It is the one mistake this grid
representation makes easy, and the symptom is not a body in a wall so much as a body that
occasionally teleports past one.

## The minimap

[Minimap.h](src/ui/Minimap.h) draws the whole grid in the top-left corner, north up, at a
fixed pixels-per-cell — a floor plan rather than a radar, because a plan you can hold
against the room you are standing in is worth more than one that spins under you.

Fog of war only ever lifts, and two rules decide what counts as seen. A **room is learned
whole** the moment you set foot in it: you do not come to know a chamber one flagstone at a
time. A **corridor is learned as you walk it**, out to `MinimapRevealCells` and only where
`Level::LineOfSight` agrees, so standing beside a wall does not reveal the passage on the
other side of it.

Only floor is remembered. Walls are drawn from the grid at the point of drawing, for any
line with a remembered cell on either side of it — which is exactly the set of walls you
would have been able to see from the floor you have walked, and avoids keeping a second
copy of the level in step with the first. Doorways are drawn thicker and in their own
colour across the gap the wall pass deliberately left, since where the doors are is the one
thing a player wants off a dungeon map. The player is an arrowhead, wound deliberately
rather than hopefully: raylib culls back faces on filled triangles, so an arrow built from
a facing direction comes out wound one way for half the compass and the other way for the
rest — and the half that comes out backwards is not drawn wrong, it is not drawn at all.
Forcing the sign of the signed area makes the marker independent of which way you look.

`Hud` owns it, which is why the depth label, the health bar and the school row lay themselves
out *below* `Minimap::Height()` and the chaos bar starts at `Minimap::Right()`.

Everything on the HUD is written in **design pixels** and multiplied by `UiScale()` — see
`src/ui/UiTheme.h`. That is `screenHeight / Config::UiDesignHeight`, so one number moves the
whole overlay together; turn `UiDesignHeight` *down* to make the HUD bigger. Height and not
width, deliberately: a 21:9 monitor is not a screen wanting a bigger HUD, it is a screen with
more room beside the same one.

## Champions

Two or more ranks ahead of the player. They carry 1.5x health and 1.33x damage on top of the
linear rank curve, which by floor 5 is 40-45 swings with a plain sword — so two things exist
purely to make that fight legible and winnable-by-skill rather than by clicking fast:

**A health bar, and only on champions.** A bar over every body is a screen of bars with the
important one lost among them. A bar over exactly the thing that takes forty swings says "this
one is different" before it says anything about numbers — and it answers the question the game
could not answer before, which is whether a champion is taking damage at all.

**Poise.** The Hit clip is a stun the *player* controls: a body playing it is not swinging, so
against anything with a large pool the winning move was to stand still and attack as fast as
possible. A champion now absorbs an eighth of its pool (`EnemyTierDef::poise`) before it gives
ground, and the meter drains when it is not being hit. Every other tier has a poise of zero and
flinches at every blow exactly as before.

## The economy

Three currencies, three vendors, and the split is the point: each one sells a different
KIND of power for a different currency, so they cannot be substituted for each other and
a run is shaped by which of them it happened to earn.

| Vendor | Sells | Currency | Where it comes from |
| --- | --- | --- | --- |
| Merchant | Weapons, and forge levels on the ones you own | Coins | Every kill pays a fraction of the exp it was worth |
| Mystic | Schools of magic, and empower levels | Gems | A rare drop, four times likelier off anything above your tier |
| Captain | Traits, and a free respec | Contracts | Resolving an event, and nothing else |

Coins credit straight to the purse — a physical coin per kill would flood the drop pool the
moment a swept blade took a pack. Gems and contracts are dropped as objects and walked over,
for the opposite reason: they are rare, and one that arrived as a number among a dozen other
numbers is one nobody noticed earning.

A run starts owning **one weapon and one school** (`Config::StartingWeapon`,
`Config::StartingMagic`). The mouse wheel cycles only what you own, and the number keys reach
only the schools you have bought. What has been bought survives the portal — it is the run's
progression, and a weapon that vanished on the way down would make every purchase a rental.

Vendors stand one to three per floor, each in its own room, and **which** rooms is decided by
the room kinds themselves: every `RoomKindSpec` carries a short list of the vendors that make
sense in it (`vendors[]`). A mystic belongs in a library or a shrine, a captain in a guardroom
or a barracks, and nobody at all in a lair — something already lives there.

## Mana

Casting costs, and the pool refills by **killing** — mostly by killing with a weapon. Without
a cost, standing at the back of a room cycling motes is strictly better than closing with
anything, and the schools stop being a decision.

The invariant, and the thing a new school can quietly break: **a spell can never fund itself.**
A mote kills at most one body, and a spell kill pays half what a weapon kill does, so the most
any cast can return is less than it cost. Any change to `Config::SpellKillsPerMana` or to a
school's cost has to be checked against that.

Spell kills pay *at all* — rather than nothing — because schools key off ARCANE alone. A
character who spent everything on arcane has strong spells and a base-10 sword, so under a rule
of "only weapons pay" that build starves itself and arcane is a trap wearing the costume of a
choice.

ARCANE does **not** raise the pool. It did, and the two effects compounded: the same points
bought harder casts *and* more of them, which is what let a pure caster outrun every weapon
build in playtesting. Arcane is spell power alone now and the pool is a flat
`Config::ManaMax` for everyone, so the cost of a cast means the same thing to every build.
Gear and traits can still add to it through `Modifiers::flatMana` — that is a slot being
spent, not a bonus riding along on a stat the build was buying anyway.

## Modifiers

Traits, and anything else that ever grants a bonus, fill in one `Modifiers` struct and `Player`
adds them up (`src/combat/Modifiers.h`). Written as separate mechanisms they would need
separate hooks in combat, separate places to remember when a new bonus is invented, and
separate chances to get the sum wrong.

The one split that is load-bearing: permanent sources grant stat **points**, temporary ones
grant **absolute** amounts. `Stats.h` is explicit that a permanent bonus multiplying an
already-climbing figure makes the player quadratic against linear enemy health — the exact
failure the rank system exists to prevent.

## When an enemy blow lands

Partway through the clip, not on the frame the swing starts. A melee attack used to resolve its
damage the instant it began — the player was hit before the axe had begun to move, which read as
being struck by an intention rather than by a weapon, and it made the wind-up decoration.

`Config::EnemyMeleeLand` puts the blow at 55% of the clip, the same way `EnemyShootRelease`
already put an arrow at 45% of its own. Reach and facing are **re-tested at that moment** rather
than trusted from the frame the swing began, and the enemy's aim is committed for the wind-up
(it tracks again during the recovery) — so stepping back or stepping around a swing both make it
miss. A swing that misses is spent; it does not keep trying for the rest of its clip.

The blow is still committed once started, like a loosed arrow: breaking line of sight makes an
enemy *waste* a swing rather than cancel one.

## Attacks

An attack is a trip from the weapon's **rest** pose to its **end** pose and back.
Both are ordinary poses, so the gizmos tune them the same way — press G to put the
gizmo on the end pose, and drag the translucent ghost to say where the swing
finishes. The crosshair is the camera's forward axis, so "swings at the
crosshair" just means an end pose centred on screen; a long weapon needs less
travel to get there than a short one, which falls out of this for free.

The style only decides timing, easing, and whether the weapon waits at full
extension. Assigned by name in `StyleRules`:

| Style | Weapons | Motion |
| --- | --- | --- |
| Swing | axes, swords, hammers, halberd | Arc through the crosshair, 0.13s out / 0.22s back |
| Thrust | `spear_A`, `sword_D` (the rapier) | Straight down the view axis, quickest in and out |
| Block | shields | Raise, **hold while the button is down**, lower |
| Cast | `wand_A`, `staff_A`, `staff_B` | Push forward, settle back slowly |
| Throw | daggers | Sharp release, slow recovery |

`TunedEndPoses` lists the right hand only. A hand with no entry of its own takes
the other hand's rest → end **stroke**, mirrors it, and applies it to its own
rest pose. That is deliberately not the same as mirroring the end pose: a weapon
whose left grip was hand tuned (`axe_C`, `halberd`, `hammer_B`, `hammer_C`,
`sword_C`) keeps that grip and still swings the same stroke. Retune a right hand
end pose and the left follows automatically.

Failing both, an end pose is generated from the rest pose and the style, so
nothing is ever without somewhere to swing.

Angles interpolate **raw**, not by the shortest route. The editor accumulates
them without wrapping, so the stored numbers already describe the arc that was
dragged — a 200° swing stays a 200° swing rather than being "corrected" into a
160° one going the other way. Shortest-path interpolation could only ever agree
with the authored numbers or overrule them, and it flipped direction at a hair's
breadth either side of 180°.

Each attack rolls a small offset that is added to the end pose only, so repeated
swings do not trace an identical arc while the weapon still starts and finishes
at rest. It is deliberately at the edge of perception —
`AttackVariationOffset` ±0.015 units and `AttackVariationAngle` ±3° per axis, one
roll per attack so the whole stroke stays coherent. The editor and the dump never
see it.

## Fog

Distance fog, in the two fragment shaders the world is drawn with — `assets/shaders/lit.fs`
and `assets/shaders/skinning.fs`. Between them they cover the stonework, the props, the
vendors, the loot, the skyline and every animated body, because `AssetManager` caches a
shader per path pair and hands all eight attach sites the same two programs.

Without it every wall was drawn at the same clarity whether it was one cell away or thirty,
so a long corridor and a short one looked alike and the town on the horizon looked like a
*model* of a town rather than a distant one.

The amount is `1 - exp(-(distance*FogDensity)²)` off the fragment's world position and the
eye — exponential squared, so there is nothing at all for the first few paces, then a
shoulder, then a long tail. Linear fog has a start line you can see; a plain exponential
greys the wall you are standing against.

It also **thins with height**, `exp(-(y - FogFloor)/FogHeight)` scaled into `FogTop`. That is
what puts the haze *under* the distant buildings: their bases sit in it and their roofs stand
clear of it, and that gap is most of what makes them read as far away rather than as small.
The height is the fragment's own rather than an integral along the ray — the eye never leaves
the floor by more than a jump here, so the cheap version draws the same picture.

`FogColour` is **sampled off the horizon band of `SkyCubemap`**, not picked. Distant walls
have to fade into the sky they are seen against, and a grey haze in front of a red sky reads
as a bug in the renderer.

The **skybox is exempt** — fog is what geometry dissolves *into*, and fogging the thing it
dissolves into would leave nothing for the horizon to be. So are the held weapons and the
weapon preview, which draw with raylib's own shader and are inches from the eye either way,
and the additive effects (impacts, the portal, spell motes), which are light rather than
surface: haze in front of a lamp does not dim the lamp, it spreads it.

[`src/render/Fog.h`](src/render/Fog.h) feeds the two programs — everything from `Config`
once at load, and the eye position once a frame. It is a class rather than four
`SetShaderValue` calls at each attach site because the failure mode of the alternative is one
model in the game that never fogs, which looks like a rendering fault rather than a missed
call.

## Post-processing

The world and the held weapons go into a buffer rather than straight at the window, and one
full-screen pass puts them on screen graded ([`src/render/PostFx.h`](src/render/PostFx.h),
`assets/shaders/post.fs`): a ring of eight bright-pass taps for a glow around fire and
spellfire, a small contrast and saturation lift, a vignette, and the red that closes in on
the frame below `Config::HurtVignetteAt` of the health pool.

The buffer is `Config::PostRenderScale` times the window because a raylib render texture
cannot carry the window's 4x multisampling, and losing that brings the brickwork shimmer
straight back. At exactly 2 the bilinear resolve is a clean 2×2 box downsample —
supersampling, which beats multisampling here because it smooths shaded detail as well as
silhouettes, at four times the fragment work. The HUD, the labels and the pages are drawn
after the pass, at the window's own resolution, ungraded.

## The viewmodel pass

The held weapons are drawn into **their own render target**, with their own depth
buffer, and composited over the finished world ([Game::Draw](src/core/Game.cpp)). They
sit about half a unit from the eye — well inside anything the world can legitimately put
there, like a wall you are pressed against or an enemy's chest — so sharing a depth
buffer with the world means being sliced by it.

Isolating the pass makes clipping structurally impossible rather than merely unlikely.
The two hands still occlude each other correctly, because they share that buffer with
each other; nothing outside can reach in. It costs one screen-sized target and one blit.
The target is rebuilt if the window size changes.

Everything held goes in this pass, the editor's gizmos and ghost included, so they stay
registered with the weapon they are attached to.

## Sway

The weapons carry two motions, and they cross-fade on `walkAmount` so exactly one
of them is ever at full strength:

- **Walk bob** — on top of the camera's own bob, in step with the head, since
  both are driven by the same footfalls. It fades out as an attack commits,
  being large enough to fight a swing.
- **Idle breathing** — takes over as the walk bob lets go. Three sines at
  unrelated rates (×1.1, ×1.6, ×0.9) so it never settles into a visible loop, and
  the left hand runs a phase behind so the two hands do not march in lockstep.
  It keeps running through an attack: it is far too small to disturb a swing, and
  without it a held block sits dead still.

Two things are deliberately not done yet: `Throw` animates the motion but the
dagger stays in hand — it needs the projectile system — and no attack deals
damage, since that waits on enemies and `camera.AimRay()`.

## Hands

Every weapon stores **one pose per hand**, and the two are independent: dragging
a gizmo in the left hand never disturbs the right. `TunedPoses` lists an entry
per hand, so a weapon appears there once or twice depending on how much tuning it
needed.

A hand with no entry of its own starts as the mirror of the hand that has one:

```
right -> -right      pitch -> pitch      yaw -> -yaw      roll -> -roll
```

Mirroring across the camera's YZ plane reverses the two rotations whose axes lie
in that plane and leaves the one about the mirror axis alone. That is a decent
starting point but not an answer — a mirrored grip often points the weapon the
wrong way round, which is why `axe_C`, `halberd`, `hammer_B`, `hammer_C` and
`sword_C` carry a hand-tuned left pose of their own.

The mesh itself is never mirrored — negative scale would invert its normals — so
a single edged weapon keeps its own handedness in both hands.

## Layout

```
src/
  main.cpp              Entry point: constructs Game, runs it
  core/
    Config.h            Every tunable constant, grouped by system
    Hand.h              Right/Left - shared by gameplay and rendering
    Input.h/.cpp        InputState - one frame of player intent
    Game.h/.cpp         Owns all subsystems, drives poll -> update -> draw
  combat/
    AttackStyle.h/.cpp  Swing/Thrust/Block/Cast/Throw + their timings
    Weapon.h/.cpp       WeaponStats: reach, arc, damage, hit point
    Attack.h/.cpp       Attack state machine + swept-blade hit resolution
    Collider.h/.cpp     Capsules: the one hit volume, and the segment maths under it
  entities/
    Body.h/.cpp         Kinematic movement (gravity, accel, strafing)
    Player.h/.cpp       Player body, health, one attack state per hand
    Enemy.h/.cpp        A capsule with a Body, hit points and its own bone pose
    EnemyManager.*      The enemy list, thin AI, separation, animation states
  world/
    Map.h/.cpp          The tile grid; the maze generator fills this later
    Level.h/.cpp        Owns the grid: drawing, collision, line of sight
  render/
    FpsCamera.h/.cpp    Mouse look, crouch height, head bob, lean, aim ray
    AssetManager.h/.cpp Load-once cache for textures/models/sounds/shaders
    AnimatedModel.*     One rigged glTF drawn in many poses (GPU skinning)
    Ragdoll.h/.cpp      Verlet corpse: takes the bones over after the death clip
    ViewModel.h/.cpp    The held weapon: per-weapon pose, drawn in camera space
  ui/
    Hud.h/.cpp          Crosshair, health bar, minimap, and status displays
assets/
  models/weapons/       23 KayKit glTF models + shared atlas
  models/enemies/       4 rigged KayKit skeletons (.glb, texture embedded)
    animations/         The 3 KayKit clip files those five states need
  shaders/              skinning.vs/.fs - the GPU skinning pair
  textures/ sounds/
lib/
  libraylib.a           raylib 6.0 built with SUPPORT_GPU_SKINNING; see Build
```

The dependency direction is one way: `Game` knows every subsystem, subsystems
know only `core/`. `Body` has no idea the world exists — `Level::ResolveBody()`
pushes it out of geometry after it moves. `FpsCamera` never moves anything, it
only derives a view (and an aim ray) from a body position.

Gameplay never includes rendering. `Player` owns the attack clock — when a hit lands is
a gameplay decision — and `ViewModel` is handed the resulting blend to interpolate its
poses along, so the swing you see and the hit you deal read the same number and cannot
drift apart. Which weapon is in which hand is still `ViewModel`'s business, and `Game`,
as the composition root, is the one place that sees both: it reads the equipped weapon,
looks up what it does, and feeds `Player`.

## Assets

`assets/models/weapons/` holds 23 models from
[KayKit: Fantasy Weapons Bits](https://kaylousberg.itch.io/fantasy-weapons-bits)
by Kay Lousberg — CC0, no attribution required (`LICENSE-KayKit.txt` kept for
provenance). Only the glTF set was taken; raylib cannot load FBX, and the OBJ set
is redundant. Each `.gltf` needs its matching `.bin` and the shared
`weapons_bits_texture.png` sitting beside it, so keep the folder intact.
`_contents_reference.png` is the pack's contact sheet showing which name is which
model — reference only, safe to delete.

Verified loading: every model is a single mesh with the atlas bound.

21 of the 23 are held weapons. Dropped along the way: the four `fistweapon_*`
models (worn on the hands, and this is a no-arms viewmodel) and the four `bow_*`
models (a bow needs two hands and a draw animation to read as one — the plan is
melee plus ranged magic, which `staff_A`, `staff_B` and `wand_A` cover). The two
`arrow_*` models stay on disk but are excluded from the held weapon list by
`IsHeldWeapon()` — they are projectile art, kept for whatever the magic throws.

```cpp
Model &sword = assets.GetModel("models/weapons/sword_A.gltf");
```

**They are not authored at player scale.** Eye height here is 1.5 units
(`BottomHeight + StandHeight`), while `sword_A` is 1.77 units long and `sword_E`
is 3.25 — swords taller than the player. Every pose therefore carries its own
`scale`; the hand-tuned set in `TunedPoses` currently sits at 0.45 across the
board. When weapons gain damage and reach, keep scale next to them rather than
promoting it to a global constant — the pack's proportions vary a lot (daggers
~1.3, halberd 2.75, spear 3.14).

### Animated enemies

> Ongoing work, the traps found so far and the phased plan for the rest live in
> [`docs/enemy-animation-plan.md`](docs/enemy-animation-plan.md). Read it before
> changing anything under `render/AnimatedModel`, `render/Ragdoll` or the enemy
> animation state machine.

`assets/models/enemies/` holds four rigged characters from
[KayKit: Skeletons](https://kaylousberg.itch.io/kaykit-skeletons) — `Skeleton_Minion`,
`_Warrior`, `_Rogue` and `_Mage`, one per row of `Config::EnemyTypes`, all on the
same rig, so swapping is a one-line change. Unlike the weapons these are
`.glb` with the texture embedded, so each file stands alone. Only the glTF set was
taken; the FBX and `fbx(unity)` copies are redundant here.

**The characters ship with no animations at all** — the pack's `Animations/` folder
is a link, not clips. They come from
[KayKit: Character Animations](https://kaylousberg.itch.io/kaykit-character-animations)
instead, in `animations/`, which is split by category, so the nine states an enemy
needs span four files:

| State | Clip | File |
| --- | --- | --- |
| Idle | `Idle_A` | `Rig_Medium_General.glb` |
| Spawn | `Spawn_Ground` | `Rig_Medium_General.glb` |
| Hit | `Hit_A` or `Hit_B`, per hit | `Rig_Medium_General.glb` |
| Death | `Death_A` or `Death_B`, per death | `Rig_Medium_General.glb` |
| Walk | `Running_A` | `Rig_Medium_MovementBasic.glb` |
| Attack | per archetype, e.g. `Melee_1H_Attack_Chop` | `Rig_Medium_CombatMelee.glb` |
| Block | `Melee_Blocking` | `Rig_Medium_CombatMelee.glb` |
| BlockHit | `Melee_Block_Hit` | `Rig_Medium_CombatMelee.glb` |
| Shoot | `Ranged_2H_Shoot` | `Rig_Medium_CombatRanged.glb` |

Hence `AnimatedModel::Load()` taking a *list* of clip files and indexing them as one
flat set. The other three Rig_Medium categories, all of Rig_Large and the Mannequin
are not copied in; `Config::EnemyAnimPaths` is where to add one.

Each archetype names its own attack clip and carries up to two props, each hung off
a bone resolved by name (`Model::boneMatrices` exposes the pose): a weapon in
`handslot.r`, and a shield in `handslot.l` for the one archetype that blocks. Only
the Minion fights unarmed. Blocking is not just a pose — `Enemy::TakeDamageFrom`
arc-tests a raised guard and scales the damage through it, the same shape as the
player's shield, and the Warrior carries a shield because a guard animation played
with an empty hand reads as a mistake.

The Archer shoots rather than swings: `src/combat/Projectile.*` flies real arrows
that a wall can stop, spawned from the crossbow bone partway through the shoot clip.
They move in substeps because a whole frame's travel is wider than the player is.

Clips are matched by name, not index: `EnemyManager::Load()` lists candidates per
state and takes the first case-insensitive hit, so a pack that names things
differently needs a name added rather than code changed. Matching is by *substring*,
and the pack ships single-frame decoys — `Death_A_Pose` sits right next to
`Death_A` — so anything shorter than `Config::EnemyMinClipDuration` is rejected.

`docs/enemy-animation-plan.md` is the working document for this system: what is
built, the traps that cost real time, and the measurements behind every constant.

**Bone order is the trap.** `UpdateModelAnimation` reads
`anim.keyframePoses[frame][i]` for bone `i` of the *model*, so the clip file's bone
order has to match the character's. KayKit's do not — same 23 bones, different
order (the character's slot 2 is `upperleg.r`, the library's is `upperleg.l`), which
skins every limb with another limb's motion. raylib 6.0 cannot catch this:
`ModelAnimation` no longer carries bone names, so `IsModelAnimationValid` compares
bone *counts* and nothing else, and 23 == 23 passes. So `AnimatedModel` loads each
clip file as a `Model` purely to read its skeleton, builds a name → index
permutation against the character's order, and reorders every keyframe pose once at
load; a file naming a bone the character lacks is rejected whole rather than
skinning one joint from whatever shared its slot. Verified against the `T-Pose`
clip, which must reproduce the bind pose exactly: worst bone-matrix error 2.0
before, 0.0 after.

**The last keyframe of every clip is thrown away**, in `TrimFailedLastFrame()`.
raylib resamples glTF clips at a fixed 60fps and takes its final sample at exactly
the clip's duration; that lookup lands a hair past the last key, the sampler
reports failure, and those bones keep their default transform. The final frame of
`Death_A` is therefore the skeleton standing back up — measured on the skinned mesh,
the silhouette goes 2.15 units tall at frame 0, down to 0.98 (lying) by frame 42,
then back to 2.14 at frame 48. A looping clip hides this as a one frame hitch, but a
clip that holds its end shows the wrong pose for as long as the corpse is there.
Trimming at load rather than skipping it at playback matters, because raylib's own
frame wrapping blends toward `keyframeCount` — leaving the bad frame in place would
still bleed it into the end of every loop. The dropped pose is freed exactly as
`UnloadModelAnimations` would have freed it, so nothing leaks and nothing
double-frees.

Scale is not hand-tuned the way the weapon poses are — `FitScaleFor()` measures the
bind pose and fits it to `Config::EnemyHeight`, so swapping characters needs no
retuning. To make enemies bigger, raise `EnemyHeight` and the hit capsule follows;
`EnemyModelScale` is an art-only multiplier on top, for when the model wants a nudge
and the fight does not. `Config::EnemyModelYaw` covers the one thing that cannot be
measured: which way the model faces.

**The standoff is sized against the model, not the capsule.** Once enemies are meshes
rather than capsules, `EnemyPersonalSpace` is what keeps a skull out of the lens, so
it is set from measured reach: the reach of `Melee_Unarmed_Attack_Punch_A` is 0.38
world units ahead of the enemy's centre, against 1.45 of standoff. The 1H clips reach
0.59 and `Melee_Unarmed_Attack_Kick` 0.74, so a longer attack wants a longer standoff.
Every KayKit clip is in place — root drift is 0.00 on all of them — so the animation
never carries the body forward; only the standoff decides this.

The three distances are ordered, and the order is the point: `EnemyPersonalSpace`
(1.45) < `EnemyStopDistance` (1.6) < `EnemyAttackRange` (1.8). Below the first an
enemy walks into a shove it cannot win and jitters; above the second it stops outside
its own swing. `PushOffPlayer()` also runs *after* wall resolution rather than before
— an enemy pushed out of a wall is pushed straight back toward the player, and with
nothing after it to separate them the standoff collapsed exactly when the player was
pinned against geometry. A body a few centimetres inside a wall for one frame is
invisible; a skull in the lens is not.

Missing files are not an error. `EnemyManager` falls back to the debug capsules it
always drew, and every clip found is logged at load with its length — read that log
first when an enemy stands still.

Dying is the one place this reaches into gameplay: a corpse has to outlive its own
death, so `RemoveDead()` keeps it until the death clip plus
`Config::EnemyCorpseLinger` runs out. Everything that matters — aggro, melee,
`AliveCount()` — already gates on `IsAlive()`, which goes false the moment health
does, so a lingering corpse is scenery and nothing else.

**The corpse ragdolls once `Death_A` has played out.** raylib ships no physics, so
`Ragdoll` is a small verlet solver: one particle per bone, distance links along the
skeleton to hold bone lengths, and a floor plane. Bones are simulated, not bodies —
there are no collision volumes and it never talks to the level, which it does not
need to, because `IsAlive()` is already false. Angles are held only by the extra
grandparent links `Begin()` builds; without them a chain of distance constraints is
a noodle and the body folds flat.

It works in model space, which is where bone transforms already live — raylib
flattens glTF bind and keyframe poses with `cgltf_node_transform_world`, so a bone's
global transform needs no hierarchy walk, and the feet sit at `y = 0` because that
is where `body.position` draws from. Gravity is therefore passed in model units
(world gravity over the model scale) and trimmed by `Config::RagdollGravityScale`,
which is at `0.5` because the clip already put the body on the floor — full gravity
reads as a slap. Bone rotations come back from the particles as swing only, no
twist, which a corpse does not need.

The handover is seeded from the pose the clip ended in, which is what keeps the
authored fall readable — measured against `Death_A`'s real final frame, the snap at
the seam is 0.0000 model units, so nothing pops. After four seconds of settling:
worst bone stretch 0.4%, nothing below the floor, motion down to 0.0002
units/frame. On the skinned mesh the body goes into the ragdoll lying (1.01 units
tall) and stays lying (0.92) — worth measuring that way rather than by bone
positions alone, since bone positions can be right while the orientations that
actually skin the mesh are wrong.

```cpp
enemies.Load(assets);   // Game::Init, before PopulateCamps
```

## Generating a level

`Map::Generate(seed)` carves rectangular rooms into solid rock by rejection sampling,
sized from a weighted table after the DMG's Chamber table (p291) where one cell is about
ten feet. Each new room is joined by an L-bend corridor to the **nearest room already
placed** — nearest rather than previous, since rooms are scattered over the whole grid and
joining consecutive placements produced twenty-cell corridors with nothing in them. Either
way the map is connected by construction, so no flood fill is needed to prove it.

On top of that connected map come loop corridors between rooms already near each other
(multiple pathways), dead-end stubs, and corridors two cells wide. A two-cell mouth gets no
door and should not: `wall_doorway` spans exactly one cell, so what stands there is an open
arch.

`AssignKinds` then gives every room a purpose and a condition, both read off the finished
layout — how far it is from the entrance puts the vault and the crypt at the far end, and
how many ways in it has is what makes a room worth guarding. The vocabulary is twelve kinds
in [RoomKind.h](src/world/RoomKind.h), each with its own prop palette.

`Level::DressRooms()` furnishes them: an anchor in the middle, edge props with their backs
to a wall, and scatter on the floor. Unlike the wall dressing, which is re-derived from a
hash every frame and stores nothing, this **stores what it placed** — a prop the player can
walk into has a footprint, and a footprint recomputed per frame is one that can disagree
with the one collision resolved against. Whether a prop is solid is measured from its own
bounding box rather than authored, so the pack classifies itself.

Two rules keep a furnished room walkable, and the second is the one that bites:

- doorway cells and the spawn are struck out of the room before placement starts — as
  whole cells, not as a radius, because that is the only version that also covers the
  two-cell arches, which have no `Doorway` record at all;
- no prop may leave part of its own room unreachable from its doorways. Every other
  placement test is local, and being cut in half is not a property of any single cell. It
  bit hardest on a 2×2 room that a corridor ran through, where filling two of the four
  cells severed the map — with both doorways still present and both doorway cells still
  clear.

Ten models in the pack are authored around something other than their base — `sword_shield`
hangs 0.82 below its origin, the wall shelves 0.30 — because they were made to be held,
dropped or mounted. Set down as though they stood on their feet they sink through the
flags, which is what they did. The fix reads the art rather than listing names: a model
whose lowest point is not its origin was not authored standing, and its **placement role**
says what to do about it — an edge prop that is not standing is a shelf and gets mounted at
`PropMountHeight`, a scatter prop that is not is litter and gets laid on its side. A prop
already flattest through Y (a coin) is left the right way up, and nothing lying where it
fell blocks: furniture is what you walk around, a dropped weapon is what you walk over.

`Level::AuditReachability()` checks the result at load and warns about any room the
furniture has sealed. `Config::LevelSeed` is a forcing value: zero rolls from the clock,
anything else pins a map worth walking twice.

**Melee and ranged combat** — add `combat/Weapon.h/.cpp` (damage, range,
cooldown, melee vs. hitscan vs. projectile) and `combat/Projectile.h/.cpp`.
`Player` holds the equipped weapon and its cooldown; `Game::Update()` resolves an
attack because it is the one place that has both `camera.AimRay()` and the enemy
list. `InputState` already carries `attack`, `attackHeld` and `altAttack`.

**Collectables** — add `entities/Pickup.h/.cpp`; a position, a kind, and a
pickup radius tested against the player each frame. `Player::Heal()` and the
weapon/ammo state are the effects it applies. `InputState::interact` is there for
pickups that need a keypress.

**Assets and shaders** — drop files under `assets/` and ask `AssetManager` for
them by relative path (`assets.GetTexture("textures/wall.png")`,
`assets.GetShader("shaders/lighting.vs", "shaders/lighting.fs")`). It caches by
path, so asking twice costs nothing. `Level::Load()` already receives the manager.
`Game::Shutdown()` unloads everything while the GL context is still alive — that
ordering matters.

## Credits

Movement, camera feel and the placeholder level come from the raylib
`core_3d_camera_fps` example by Agnis Aldins (@nezvers), reviewed by Ramon
Santamaria (@raysan5), used under the zlib/libpng license.

Weapon models, skeleton characters and character animations by Kay Lousberg,
www.kaylousberg.com (CC0) — from Fantasy Weapons Bits, KayKit: Skeletons and
KayKit: Character Animations. Each pack's own licence text is kept beside its
files under `assets/models/`.

The GPU skinning shader pair in `assets/shaders/` is raylib's own
`models_animation_gpu_skinning` example shader by Daniel Holden (@orangeduck),
used under the zlib/libpng license.
