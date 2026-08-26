# Porting ActionMaze to Unity

## Read this first

The codebase is not messy. It is ~5,800 lines with a one-way dependency graph, a
README that explains every constant, and a design document for the one system
that is still moving. That is not the problem.

The problem is the **ratio**. About 1,500 of those lines — a quarter of the
project — are things an engine gives you, written by hand because raylib does not:

| File | Lines | What it actually is |
| --- | --- | --- |
| `render/AnimatedModel.*` | 469 | Bone-order permutation, clip retargeting, keyframe repair |
| `debug/ViewModelEditor.*` | 502 | Translate/rotate gizmos with screen-space ring picking |
| `render/Ragdoll.*` | 303 | A verlet solver with distance constraints and a floor plane |
| `render/AssetManager.*` | 95 | A load-once cache keyed by path |
| `entities/Body.*` | 87 | Kinematic capsule integration |

Plus a custom `libraylib.a` built with `SUPPORT_GPU_SKINNING` that has to be
rebuilt and re-copied by hand, a makefile, and a `.vscode` launch config.

In Unity every row of that table is a checkbox, a built-in wizard, or the Scene
view you already have open. **That is the case for moving, and it is a good one.**

The case against: you will throw away the tuned pose tables (`TunedPoses`,
`TunedEndPoses`) and re-author them, because they live in a hand-rolled
camera-space frame that will not survive the handedness change. Everything else
transfers.

### What survives, what dies, what needs a decision

**Ports nearly verbatim** — this is the game, and it is engine-independent:

- `core/Config.h` — every number. Two sign flips, noted below; the rest is magnitudes.
- `combat/Attack.cpp` — `InCone`, the hit-at-blend rule, the phase machine
- `combat/AttackStyle.*`, `combat/Weapon.*` — timings, reach derivation, the override table
- `entities/EnemyManager.cpp` — the AI, separation, the three ordered standoffs
- `combat/Projectile.cpp` — substepped travel (Unity's `SphereCast` is an option, but substeps are already correct and already tuned)
- `world/Map.*` — the grid, and the maze generator that is going to fill it

**Dies entirely** — delete, do not port:

- `render/AnimatedModel.*` → Unity's avatar retargeting (see §3)
- `render/Ragdoll.*` → `Rigidbody` + `CharacterJoint`, via the Ragdoll Wizard
- `render/AssetManager.*` → the asset pipeline; prefab references or Addressables
- `debug/ViewModelEditor.*` → the Scene view's own move/rotate handles
- `entities/Body.*` → `CharacterController.Move` (sliding falls out of it, same as `ResolveBody`)
- `Makefile`, `lib/libraylib.a`, `.vscode/*` → the Unity project itself

**Needs a decision:**

- `Level::LineOfSight` — keep the grid DDA. It is exact, it costs one iteration
  per cell, and it does not care whether colliders exist. `Physics.Linecast`
  would be slower and less predictable. Keep it.
- `Level::ResolveBody` — replace with `CharacterController`. Note the semantics
  differ: yours resolves *after* the move, Unity's sweeps *during* it. Yours
  needs `MaxFrameTime` to stop tunnelling; a swept controller does not, but keep
  the cap anyway — the enemies and projectiles still rely on it.
- `render/ViewModel.cpp`'s render-target trick — becomes two cameras. §5.

---

## 1. Project setup

**Unity 6 LTS**, **Universal Render Pipeline**. Create with the *Universal 3D*
template — do not start from Built-in and migrate later, the material conversion
is a chore you can skip by never incurring it.

URP over HDRP: your art is flat-shaded CC0 chibi models on a single 512px atlas.
HDRP would cost you build time and frame time to render an aesthetic that does
not want it.

### Packages (Window ▸ Package Manager)

| Package | Why |
| --- | --- |
| **Input System** | Replaces `core/Input.*`. Do install this; the legacy `Input.GetKey` path is deprecated and mouse-look deltas are better in the new one. |
| **Cinemachine** | *Optional.* Your `FpsCamera` bob/lean/FOV logic is 105 lines you understand. Cinemachine is a large system to learn for that. Skip it initially; port `FpsCamera` by hand. |
| **glTFast** (`com.unity.cloud.gltfast`) | Only if you cannot find the weapon pack's FBX set. See §2. |

When Input System is installed Unity will ask to switch the active input backend
and restart. Say yes, and set **Project Settings ▸ Player ▸ Active Input Handling**
to *Input System Package (New)*.

### Project settings to change immediately

- **Physics ▸ Gravity** → `(0, -32, 0)`. Your `Config::Gravity` is 32, and jump
  12 gives the 2.25-unit apex the level is built around. If you apply gravity
  manually inside the controller (recommended — `CharacterController` does not
  apply it for you), this setting only matters for the ragdolls, and you want
  those falling at the same rate as everything else.
- **Time ▸ Maximum Allowed Timestep** → `0.0333`. This is `Config::MaxFrameTime`,
  and it exists for the same reason.
- **Time ▸ Fixed Timestep** → leave at `0.02`. Ragdolls and projectiles run there.
- **Player ▸ Resolution** → *Fullscreen Mode: Borderless Window*. Matches
  `Config::Fullscreen` and the reasoning in its comment block.
- **Quality ▸ Anti-aliasing** — the art has hard silhouettes and no texture
  detail; MSAA 4x reads much better here than it would on a detailed scene.

### Folder layout

Mirror what you have, so the mapping stays obvious:

```
Assets/
  Scripts/
    Core/         Config.cs, GameLoop.cs, InputReader.cs
    Combat/       AttackState.cs, AttackStyle.cs, WeaponStats.cs, Melee.cs, Projectile.cs
    Entities/     PlayerController.cs, Enemy.cs, EnemyManager.cs
    World/        Map.cs, MapGenerator.cs, Level.cs
    View/         FpsCamera.cs, ViewModelRig.cs
    UI/           Hud.cs
  Art/
    Characters/   the four skeletons + their avatars
    Animations/   the Rig_Medium clip files
    Props/        blade, axe, staff, shield, crossbow, arrow
    Weapons/      the 21 held weapons
    Textures/
  Prefabs/
  Scenes/
```

---

## 2. Assets — the import pass

**This inverts one decision in your README.** You wrote:

> Only the glTF set was taken; the FBX and `fbx(unity)` copies are redundant here.

In Unity they are the opposite of redundant. Unity imports FBX natively and does
**not** import glTF without a package. And the `fbx(unity)` folders exist
precisely because Kay Lousberg pre-corrected the axes and scale for Unity's
importer. Use them.

Everything you need is already on disk:

| What | Where | Notes |
| --- | --- | --- |
| 4 skeleton characters | `KayKit_Skeletons_1.1_FREE/characters/fbx/` | `Skeleton_{Mage,Minion,Rogue,Warrior}.fbx` |
| Props | `KayKit_Skeletons_1.1_FREE/assets/fbx(unity)/` | Blade, Axe, Staff, Shield_Small_A, Crossbow, Arrow — the exact set `EnemyTypes` names |
| Character clips | `KayKit_Character_Animations_1.1/Animations/fbx/Rig_Medium/` | The four category files `EnemyAnimPaths` lists |
| Skeleton texture | `KayKit_Skeletons_1.1_FREE/texture/skeleton_texture.png` | One atlas, shared |

**The 23 held weapons are the gap.** `assets/models/weapons/` has only the glTF
set, because that is all you kept. Two options:

1. Re-download *Fantasy Weapons Bits* from itch and take its FBX folder. Free,
   CC0, five minutes. **Do this.**
2. Install glTFast and import the `.gltf`+`.bin`+atlas triples you already have.
   Works, but adds a dependency and a runtime import path for no benefit.

### Import settings — characters

Select all four `Skeleton_*.fbx`, then:

- **Model** tab: *Scale Factor* `1`. Check the bounds after import — the FBX set
  should land at roughly 1.7–2.0 units. Your `FitScaleFor()` measured the bind
  pose and fitted it to `EnemyHeight`; in Unity you set the prefab's scale once
  and it is done. Keep `EnemyHeight = 2.0` as the capsule height and scale the
  model to match it.
- **Rig** tab: *Animation Type* → **Generic**. *Avatar Definition* → **Create From
  This Model**. *Root node* → the hip/root bone.

**Generic, not Humanoid — and this is the important call.** Humanoid is the
obvious choice and it is the wrong one here. Humanoid retargets through a
normalised muscle space, which approximates: it will subtly change every pose,
and your entire combat design is measured against exact poses. The reach numbers
in `Config.h` (`0.38` unarmed, `0.59` 1H, `0.74` kick, `1.48` jumping chop) and
the three ordered standoffs are calibrated to clips as authored.

You do not need retargeting anyway. All four characters are **the same rig** —
that is stated in your README and it is why swapping is a one-line change today.
Generic with a shared avatar plays the clips exactly as authored, bit for bit.

### Import settings — animation clip files

Select the four `Rig_Medium_*.fbx` files:

- **Rig** tab: *Animation Type* → **Generic**. *Avatar Definition* → **Copy From
  Other Avatar** → pick `Skeleton_Minion`'s avatar (any one of the four; they
  share a rig).

> **This single dropdown is what replaces `AnimatedModel.cpp`.** Your 367-line
> file exists because raylib indexes `keyframePoses[frame][i]` by the *model's*
> bone slot, KayKit's clip files order bones differently, and raylib 6.0 dropped
> bone names so `IsModelAnimationValid` compares counts and 23 == 23 passes. You
> built a name→index permutation and reordered every keyframe at load. Unity
> binds by **path and name**, always has. The trap does not exist here.

- **Animation** tab: each file contains many clips. Unity lists them all. For
  each one you actually use:
  - Rename to match what `EnemyManager::Load` looks for (`Idle_A`, `Running_A`,
    `Melee_1H_Attack_Chop`, `Ranged_2H_Shoot`, `Melee_Blocking`,
    `Melee_Block_Hit`, `Hit_A`/`Hit_B`, `Death_A`/`Death_B`, `Spawn_Ground`).
  - **Loop Time** on: `Idle_A`, `Running_A`, `Melee_Blocking`. Off on everything
    else — especially the death clips, which must hold their final pose.
  - Delete or ignore the single-frame decoys (`Death_A_Pose`, `T-Pose`). Your
    `EnemyMinClipDuration` guard existed to dodge these during substring
    matching; here you just don't drag them into the controller.

**Check the last frame of `Death_A`.** Your `TrimFailedLastFrame()` exists
because raylib resamples glTF at a fixed 60fps and its final sample lands a hair
past the last key, so those bones snap to default — the corpse stands back up
(2.15 tall → 0.98 lying → 2.14 again at frame 48). That is a raylib sampler bug,
not a data problem, so Unity should be clean. Verify anyway: scrub the clip to
its end in the Animation window and confirm the skeleton is still lying down. If
it is, delete the trim logic and never think about it again.

### Import settings — props and weapons

- **Model** tab: *Scale Factor* `1`, *Read/Write* off, *Mesh Compression* off
  (these are tiny meshes).
- **Materials**: *Material Creation Mode* → *Standard*, *Location* → *Use
  External Materials*. Then point every one at a single shared material using
  the atlas, so you get one draw call's worth of batching across 23 weapons
  instead of 23 materials.
- Set the atlas texture's *Filter Mode* → **Point (no filter)**. KayKit atlases
  are flat colour blocks; bilinear filtering bleeds neighbouring swatches into
  each other at glancing angles.

---

## 3. Enemies — the animation system

This is where the largest amount of your code disappears.

**Prefab per archetype.** One per row of `Config::EnemyTypes`:

```
Skeleton_Warrior (prefab)
├─ CharacterController        radius 0.4, height 2.0  (Body::radius, EnemyHeight)
├─ Animator                   controller: EnemyAnimator
├─ Enemy.cs                   the archetype's stats, from Config
└─ Skeleton_Warrior (model)
   └─ …rig…
      ├─ handslot.r
      │  └─ Skeleton_Blade    ← just parent it here
      └─ handslot.l
         └─ Skeleton_Shield_Small_A
```

**The prop system evaporates.** Today you resolve a bone by name against
`Model::boneMatrices`, apply a per-prop grip correction from the
`EnemyPropGrips` table, and multiply that by the bone transform each frame. In
Unity the bones are real GameObjects in the hierarchy — drag the prop in as a
child of `handslot.r` and it follows. Done.

And `Config::EnemyPropGrips`, the whole table with its measured-axes comment
block, becomes **the prop child's local rotation in the prefab**. The crossbow's
`yaw 90` — the one entry, because the crossbow is authored along +Z while
everything else is along +Y — is now a number you nudge in the Inspector while
looking at it. Keep the comment block somewhere; the reasoning is still true and
still worth knowing. Delete the code.

### Animator controller

An Animator Controller with your nine states and transitions on parameters:

| Parameter | Type | Drives |
| --- | --- | --- |
| `Speed` | float | Idle ↔ Running |
| `Attack` | trigger | → the archetype's attack clip |
| `Hit` | trigger | → Hit_A/Hit_B |
| `Die` | trigger | → Death_A/Death_B |
| `Blocking` | bool | → Melee_Blocking |
| `BlockHit` | trigger | → Melee_Block_Hit |

Set transition duration to `0.12` — that is `Config::EnemyAnimBlendTime`, and the
reasoning in its comment (long enough to hide the cut, short enough that the hit
still lands with the swing) holds unchanged.

Use **sub-state machines** or an **override controller** for the per-archetype
attack clip. Override controller is cleaner: one base controller, four overrides
that swap only the attack slot. That is the direct equivalent of
`EnemyArchetype::attackClip`.

`Config::EnemyAnimHoldSlack` — the 0.3s that stops the idle pose flashing between
swings when the clip is 1.17s against a 1.2s cooldown — is handled by exit-time
transitions instead. Set the attack state's *Has Exit Time* on, exit time `0.9`.

### Animation events replace two clocks

- **The arrow release.** `Config::EnemyShootRelease = 0.45` is "45% into the
  shoot clip, because the crossbow comes up over the first half." In Unity that
  is an **Animation Event** placed on the frame you can see the crossbow reach
  aim, calling `OnShootRelease()`. Better than a fraction: you place it by eye
  against the actual pose, and it stays correct if you ever swap the clip.
- **The melee hit.** `Config::MeleeHitAt = 0.55` — keep this one as a number.
  Your design deliberately drives the hit off the *attack blend*, not the clip,
  so the swing you see and the hit you deal read the same value. That reasoning
  is engine-independent and still right. Do not move it to an animation event.

### Ragdoll

Delete `render/Ragdoll.*`. Use **GameObject ▸ 3D Object ▸ Ragdoll…** on a
skeleton prefab, assign the bones it asks for, and it builds `Rigidbody` +
`CharacterJoint` + capsule colliders for you.

The handover stays exactly as you designed it: on death, play `Death_A`; when it
finishes, disable the `Animator` and set the rigidbodies non-kinematic. The bones
are already in the pose the clip ended in, so the seam is seamless for the same
reason it is today (your measured snap was 0.0000 model units).

`Config::RagdollGravityScale = 0.5` — the clip already put the body on the floor,
so full gravity reads as a slap — is now per-rigidbody *Mass* / *Drag*, or a
`Physics.gravity` override on those bodies. Same intent, and you can tune it
while watching it fall.

`Config::EnemyCorpseLinger = 4.0` stays as-is: a coroutine that destroys the
corpse four seconds after the death clip ends.

---

## 4. Player and movement

`PlayerController.cs` replaces `Body`, `Player` and `Level::ResolveBody`.

Use **`CharacterController`**, not a `Rigidbody`. Your movement is
deliberately kinematic — acceleration toward a smoothed `dir`, a friction
multiplier, `MaxAccel` clamped per frame, and the strafe-speed exploit that falls
out of it (the Quake trick your comment links a video for). A `Rigidbody` fights
all of that. `CharacterController.Move` gives you swept collision with wall
sliding, which is what `ResolveBody` produces by cancelling only the
into-the-wall velocity component.

Port `Body::Update` almost line for line. Set:

```
CharacterController.radius     = 0.4      // Body::radius
CharacterController.height     = 2.0
CharacterController.skinWidth  = 0.08     // radius * 0.2, Unity's rule of thumb
CharacterController.slopeLimit = 45
CharacterController.stepOffset = 0.3
```

`Body::radius` under half a cell (`MapCellSize / 2 = 1.5`) still matters, for the
same reason. 0.4 is fine.

`isGrounded` comes from `CharacterController.isGrounded`, but it is famously
flaky on the frame you land. Apply a small constant downward velocity when
grounded (`-2` is standard) so the controller stays in contact, rather than
letting gravity accumulate to zero.

### Camera

Port `FpsCamera` by hand — 105 lines, and you understand every one. The bob,
lean, crouch lerp and FOV shift are all `Mathf.Lerp` against `Time.deltaTime`,
identical to what is there.

`BobPhase()` and `WalkAmount()` are consumed by the viewmodel sway; keep them
public, keep them driven by the same footfall clock. The whole point of that
coupling (`the weapon bob is in step with the head, since both are driven by the
same footfalls`) survives unchanged.

---

## 5. The viewmodel

Your isolated render pass — own render target, own depth buffer, composited over
the finished world — is a **URP camera stack**, and it is three checkboxes.

1. Create a layer called `Viewmodel`.
2. **Main Camera**: *Culling Mask* → uncheck `Viewmodel`.
3. Child camera `ViewmodelCamera`:
   - *Render Type* → **Overlay**
   - *Culling Mask* → **only** `Viewmodel`
   - *Field of View* → lower than the main camera's (50 against your 60 is a good
     start; it makes held weapons read larger without moving them)
4. On the Main Camera, *Stack* → **+** → add `ViewmodelCamera`.

URP overlay cameras clear depth before rendering and composite over the base
camera's result. That is precisely your `BeginPass` / `EndPass` / `Composite`,
including the property you cared about most: the two hands still occlude each
other correctly because they share the overlay's depth buffer, and nothing in the
world can reach into it. The render target rebuild on window resize is handled.

Put the weapon models, and anything else held, on the `Viewmodel` layer.

### The poses

**This is the one place you lose work.** `TunedPoses` and `TunedEndPoses` are
authored in a hand-built camera-space frame with its own Euler order, and raylib
is right-handed where Unity is left-handed. The numbers will not survive.

What replaces them is better, though:

- A `Rest` and an `End` empty GameObject parented under `ViewmodelCamera`, per
  hand, per weapon. Drag them in the Scene view with the standard move/rotate
  handles.
- `LerpPose` becomes `Vector3.Lerp` + `Quaternion.Slerp` between the two
  transforms, driven by `AttackState.blend` exactly as now.
- Saving is Ctrl+S on the prefab. `viewmodel_poses.txt`, the `U` key that appends
  to it, and the paste-over-`TunedPoses` ritual all go away.

So: 502 lines of gizmo editor deleted, and the pose table it produced deleted
with it. Re-tuning 21 weapons in the Scene view is an evening's work, and it is
the *last* evening you spend on it.

**Two things to carry over deliberately, because Unity will not give them to you:**

1. **Raw angle interpolation.** Your README is emphatic: angles interpolate raw,
   not by the shortest route, because a 200° swing must stay a 200° swing.
   `Quaternion.Slerp` takes the short way and will silently "correct" exactly the
   swings you authored longest. If a weapon's stroke exceeds 180°, either
   interpolate Euler angles unwrapped (as you do now) or split the arc into two
   Slerps through a midpoint. Do not let this one slip in unnoticed — it will
   present as "some weapons swing backwards", which is the same bug you already
   solved once.
2. **Per-attack variation.** `AttackVariationOffset` ±0.015 and
   `AttackVariationAngle` ±3°, rolled once per attack and added to the end pose
   only. Ten lines, and it is deliberately at the edge of perception. Port it.

`MirrorPose` still applies: mirroring across the camera's YZ plane flips the
sideways offset and reverses the two rotations whose axes lie in that plane.
Left-handed coordinates do not change that geometry. And keep the rule that the
mesh itself is never mirrored — negative scale still inverts normals in Unity.

---

## 6. The level

Keep `Map` as a plain C# class. It is not a `MonoBehaviour`, it has no Unity
dependency, and the maze generator that is going to fill it does not want one.
`Tile`, `IsWall`, `WorldToCell`, `CellCenter` port unchanged.

`Level` becomes a `MonoBehaviour` that, at load:

1. Builds or loads the grid.
2. **Generates collision from it.** One `BoxCollider` per wall cell is the simple
   version and is fine for a test arena. For a real maze, greedy-merge runs of
   adjacent wall cells into longer boxes first — a 40×40 maze is ~800 wall cells
   and ~80 merged boxes, and `CharacterController` sweeps against every collider
   it might touch.
3. **Generates visuals from it.** Instantiate a wall prefab per cell, or build
   one combined mesh. Prefer the combined mesh with `StaticBatchingUtility` —
   these are untextured cubes on a shared material and they should cost one draw
   call, not eight hundred.

The grid stays authoritative for everything that asks questions: `LineOfSight`,
aggro, the AI. The colliders exist only so `CharacterController` and the
projectiles have something to hit. **Do not let the two diverge** — generate the
colliders from the grid, never author them by hand, so there is exactly one
source of truth about where a wall is.

`Anything outside the grid reads as Wall` — keep that. It is the reason
collision, LOS and AI need no map-edge special case, and it is free.

---

## 7. Traps

**Handedness — expect exactly two sign flips.** raylib is right-handed with
movement-forward at `-Z`; Unity is left-handed with `transform.forward` at `+Z`.

- `Config::EnemyModelYaw = 180.0f` → **`0`**. It exists because "body forward at
  yaw 0 is -Z, and the KayKit skeletons are authored facing +Z, so they need
  turning around." In Unity forward *is* +Z and the models already face it. Delete
  the correction.
- **Mouse-look yaw sign** flips. Don't port the trig from `Body::Update`
  (`front = {sin(yaw), 0, cos(yaw)}`, then negated via `input.y = -move.y`) — use
  `transform.forward` and `transform.right` and let Unity's rotation define the
  frame. Strafe (`+X` = right) is already consistent and needs nothing.

Everything else in `Config.h` is a magnitude and ports unchanged. Speeds,
distances, arcs in degrees, damage, cooldowns, blend fractions — all of it.

**The +Z measuring convention outlives the flip.** Your note that attack reach is
measured along model-space **+Z**, and that measuring the wrong side reads the
skeleton's back and understates badly (`Punch_A` measures 0.45 instead of 1.47),
is still true — the models are still authored facing +Z. Only the *body's*
forward changed, not the model's.

**Unity's units are metres by convention, and yours already are.** Eye at 1.5,
cell 3.0, enemies 2.0, `MaxSpeed` 9 (≈32 km/h). Nothing needs rescaling. The
`Scale Factor 1` import setting above is what keeps it that way — resist the urge
to "fix" a model that looks small by scaling the import; scale the prefab.

**`Time.deltaTime` vs your capped delta.** Everything ported from `Update()` uses
`Time.deltaTime`; anything touching a `Rigidbody` (ragdolls only) goes in
`FixedUpdate` with `Time.fixedDeltaTime`. Set Maximum Allowed Timestep as in §1
so a hitch still cannot carry a body through a wall.

**Projectiles.** `ProjectileStep = 0.12` must stay under the player's body radius
or a shot steps through someone standing still — that constraint is unchanged.
Substeps port directly. If you switch to `Physics.SphereCast` instead, use
`ProjectileRadius = 0.10` as the cast radius and you can drop the substepping —
but you lose nothing by keeping what already works and is already tuned.

**Draw order for blocking.** `Enemy::TakeDamageFrom` arc-tests a raised guard and
scales damage through it — it is not cosmetic, and your comment explains why
(a block that plays the animation and takes full damage teaches the player to
ignore the animation). Keep the arc test in code. Do not be tempted to replace it
with a shield collider; a collider tests where the shield *is*, and the design
tests where the guard *covers*, which is deliberately wider (`EnemyBlockArc` 140°).

---

## 8. Port order

Each phase should end at something you can press Play on. Do not port the whole
codebase and then debug it.

1. **Empty scene, grey box.** URP project, packages, project settings. A plane,
   a capsule, a light. Confirms the install.
2. **Player moves.** `CharacterController`, ported `Body::Update`, `FpsCamera`
   with bob and lean, Input System bindings for `core/Input.h`'s intent struct.
   No world yet — a flat plane. This is where the two sign flips get found.
3. **The level.** `Map` ported, `LoadTestArena` reproduced, collider and mesh
   generation from the grid, `LineOfSight` ported. Walk the arena, slide along
   walls, confirm you cannot leave it.
4. **One enemy, animated.** Import one skeleton and the four clip files, build
   the Animator, prefab it. Walk-at-player AI only. **This is the phase that
   proves the move was worth it** — it is where `AnimatedModel.cpp` would have
   been, and it should take an afternoon instead of a week.
5. **Combat.** `AttackState`, `InCone`, `ResolveMelee`, damage, death, the Ragdoll
   Wizard. Enemies die and fall over.
6. **The viewmodel.** Camera stack, one weapon, rest/end transforms, blend driven
   by `AttackState`. Then the other twenty.
7. **The rest.** All five archetypes, props on `handslot.r`, projectiles,
   blocking, HUD.
8. **The maze generator** — which is what you actually wanted to be building.

---

## 9. What to bring that is not code

Your README is the most valuable file in this repository and almost none of it is
about raylib. The reasoning behind `EnemyPersonalSpace` (1.45) < `EnemyStopDistance`
(1.6) < `EnemyAttackRange` (1.8), why the skull sits under the crosshair below
1.89 height, why reach is deliberately unrelated to the drawn weapon, why the
melee test is a cone and not a swept blade, why angles interpolate raw — that is
design work, it took real time, and it is engine-independent.

**Port the documentation first, before any code.** Every constant in `Config.h`
arrives in Unity with the paragraph explaining it, or you will re-learn all of it
by hand.

`docs/enemy-animation-plan.md` needs a triage pass instead: its measurements stay
valid (clip lengths, reach distances, root drift), but the sections on bone-order
permutation, `TrimFailedLastFrame` and the GPU-skinning build are obsolete the
moment you open Unity. Mark them as such rather than deleting — "this is what the
engine does for you now" is useful context in six months.
