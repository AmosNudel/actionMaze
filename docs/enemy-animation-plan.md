# Enemy animation — state, traps, and the plan

Working document for whoever picks this up next, agent or human. It records what
is built, the things that cost real time to discover, and the remaining work
broken into phases that can each be finished and judged on their own.

Two kinds of checking appear throughout, and they are not interchangeable:

- **Agent test** — automated, headless, run from the scratchpad. Proves the data
  and the maths. An agent must run these and quote the numbers before saying a
  phase is done.
- **Player check** — the repo owner runs the game and judges it. Proves it reads
  right. Nothing here is finished until these pass, because every bug in this
  system so far looked fine in the numbers and wrong on screen.

---

## 1. What is already built

| Piece | Where | What it does |
| --- | --- | --- |
| GPU skinning raylib | `lib/libraylib.a` | raylib 6.0 built with `SUPPORT_GPU_SKINNING`. Linked ahead of `C:/raylib`, which stays stock. |
| Skinning shaders | `assets/shaders/skinning.vs/.fs` | raylib's own example pair, by Daniel Holden (zlib). |
| `AnimatedModel` | `src/render/AnimatedModel.*` | Loads a rigged glTF plus clips from several files, remaps bone order, trims the bad last frame, poses and cross-fades. |
| `Ragdoll` | `src/render/Ragdoll.*` | Verlet corpse, takes over when the death clip ends. |
| Enemy anim state | `src/entities/Enemy.h` | `anim`, `previousAnim`, `animBlend`, `animVariant`, `blocking`, `shotPending`, per-enemy `bones`, `ragdoll`. |
| State machine | `EnemyManager::UpdateAnimation` | Nine states, with cross-fade, an attack hold and per-hit clip alternates. |
| Guard | `Enemy::IsBlocking` / `TakeDamageFrom` | An AI decision that also gates damage, arc-tested like the player's shield. |
| Two prop slots | `Config::EnemyArchetype` | A weapon hand and an off hand — or any bone, so a shield and a quiver are the same mechanism. |
| Projectiles | `src/combat/Projectile.*` | Substepped arrows, owned by `Game` so a shot outlives its archer. |

Working states today: **Idle, Walk, Spawn, Attack, Shoot, Block, Hit, BlockHit,
Death → ragdoll.** Five archetypes, four of them armed, one of them ranged — see
the tables in Phase 2 and Phase 3.

---

## 2. Traps — read before touching anything

Every one of these was found the hard way. They are not hypothetical.

### 2.1 Bone order differs between character and clip files

`Skeleton_Minion.glb` and `Rig_Medium_*.glb` contain the **same 23 bones in a
different order**. The character's slot 2 is `upperleg.r`; the clip file's is
`upperleg.l`. `UpdateModelAnimation` indexes `keyframePoses[frame][i]` by the
*model's* bone index, so unremapped clips skin every limb with another limb's
motion.

**raylib 6.0 cannot detect this.** `ModelAnimation` no longer carries bone names,
so `IsModelAnimationValid` compares bone *counts* only — 23 == 23 passes. A green
result from it means nothing.

`RemapPosesToModel()` loads each clip file as a `Model` purely to read its
skeleton, builds a name → index permutation, and reorders every keyframe once at
load. A file naming a bone the character lacks is rejected whole.

**Characters differ from each other too, not just from the clip files.**
`handslot.r` is index **22** on `Skeleton_Minion` and index **8** on
`Skeleton_Warrior`. Never store or hardcode a bone index across models — resolve
by name with `AnimatedModel::FindBone()` against the model you are actually using.
The remap is computed per character, so it already handles this; anything else
that touches bones must too.

### 2.2 The last keyframe of every clip is garbage

raylib resamples glTF at a fixed 60fps and takes its final sample at exactly the
clip duration. That lookup lands a hair past the last key, the sampler reports
failure, and those bones keep their **default transform**. The final frame of
`Death_A` is the skeleton standing back up.

`TrimFailedLastFrame()` drops it at load. Trim, do not skip at playback: raylib's
own wrapping blends toward `keyframeCount`, so leaving it in bleeds it into the
end of every loop. The dropped pose is freed exactly as `UnloadModelAnimations`
would have.

### 2.3 Forward is +Z, and measuring the wrong side lies to you

`Config::EnemyModelYaw` is `180` because the skeletons are authored facing **+Z**
while body forward at yaw 0 is −Z. Any measurement of "how far does this clip
reach at the player" must use **+Z in model space**.

Measuring −Z reads the skeleton's back and understates badly: `Punch_A` measures
0.45 that way against a true **1.47**. An entire round of standoff tuning was
done on the wrong axis before this was caught.

### 2.4 Clips are in place — there is no root motion

Root drift is `0.00` on every clip measured. The body never carries itself
forward; `Body` does all movement. If an enemy appears to lunge, it is the
standoff, not the animation.

### 2.5 The skull is 45% of the character's height

`0.80 × 0.90 × 0.84` world units at `EnemyHeight` 2.0. Framing, not reach, is what
decides whether an attack reads as a monster in front of you or a face in the
lens. The skull centres on `y = 1.35 × (EnemyHeight/1.7)` against a 1.50 eye.

**Hard floor at `EnemyHeight` ≈ 1.89.** Below it the skull centre drops under the
eye line and sits on the crosshair no matter how far the enemy stands off —
backing away shrinks it but never moves it.

| `EnemyHeight` | elevation vs crosshair |
| --- | --- |
| 1.9 | +0.3° (on the crosshair) |
| 2.0 (current) | +2.5° |
| 2.1 | +4.8° |
| 2.3 | +8.4° |

### 2.6 The distance constants are an ordered chain

`EnemyPersonalSpace` (2.0) < `EnemyStopDistance` (2.2) < `EnemyAttackRange` (2.4).

Below the first, an enemy walks into a shove it cannot win and jitters. Above the
second, it stops outside its own swing. **All three must move together with
`EnemyHeight`**, because they are sized off the model.

`Config::MeleeMinReach` (2.1) is the matching floor on the player's side, so every
weapon can answer an enemy that is already hitting it. Without it, 14 of 21 held
weapons could not reach — including both starting swords.

### 2.7 Separation ordering, and why `ClearOfPlayer` exists

`PushOffPlayer` moves the player a share per enemy, one enemy at a time, so with a
crowd the last one processed shoves the player back inside an earlier one. It
cannot promise clearance. `ClearOfPlayer` runs last, moves **only** enemies, and
is therefore exact in one pass. Wall resolution runs before it on purpose: a body
briefly inside a wall is invisible, a skull in the lens is not.

`EnemyPushShare` is 0.15 (enemies feel heavy) — only safe because `ClearOfPlayer`
backstops it.

### 2.8 Cross-fade direction

`UpdateModelAnimationEx(model, animA, frameA, animB, frameB, blend)` — blend 0 is
**all A**, 1 is **all B**. `AnimatedModel::PoseBlended` wraps it as from → to.

### 2.9 The feet and the clip must agree, and stopping is not free

Every attack in the pack is animated **in place** (§2.4), so anything that lets the
body keep walking through one drags the model across the floor. `ClipOwnsBody()`
is the single answer to "is a one-shot clip running this body", asked by both the
animation and the feet. Do not let those two decisions diverge.

Clearing the movement input is **not** enough to stand still. `Body::Update`
smooths `dir` toward the input rather than setting it, and then accelerates along
`dir` regardless, so a body that stops asking to move still drifts. Measured over
one 1.167s swing at `EnemySpeed` 5.0:

| Case | Drift |
| --- | --- |
| Kept walking (the bug) | 5.833 units |
| Input released only | 0.923 units — still 2.3× a body radius |
| Input released + `Body::Halt()` | 0.000 units |

`Halt()` zeroes horizontal velocity and `dir`, leaving gravity alone. It does not
interfere with separation, because the push passes move `position` directly.

### 2.10 Two clocks that nearly agree

The swing clip runs 1.167s against `EnemyAttackCooldown` 1.2s. That 2-frame gap
showed as the idle pose flashing between every pair of swings.
`EnemyAnimHoldSlack` (0.3) holds the attack when the next is that close. Any new
attack clip changes this relationship — re-check it.

### 2.11 A state needs somewhere to live, and the guard nearly had nowhere

The same two clocks decide whether **Block can happen at all**. A guard only
occupies the gap between a swing's clip ending and the next swing starting:

```
held guard = cooldown − attack clip length − EnemyBlockDropTime − EnemyAnimBlendTime
```

Written and shipped with `blockChance` 0.55 on the Warrior and 0.25 on the Rogue,
that came to **0.06s and −0.39s**. Block appeared for **zero frames** in a
simulated minute of fighting, and nothing in the code was wrong: the numbers just
left no room. Measured across the table as first written:

| Type | Attack clip | Length | Cooldown | Held guard |
| --- | --- | --- | --- | --- |
| Minion | `Punch_A` | 1.17 | 1.10 | −0.31 |
| Warrior | `1H_Chop` | 1.07 | 1.50 | **0.06** |
| Rogue | `Slice_Diagonal` | 1.00 | 0.85 | −0.39 |
| Mage | `1H_Stab` | 1.60 | 1.70 | −0.14 |

Three of four archetypes have a cooldown **shorter than their own swing clip**,
so their stated cooldown is not what they actually fire at — the clip is. The
Warrior's cooldown is now 1.90 for 0.59s of held guard, and it is the only row
whose cooldown is set by the guard rather than by the swing. The others block 0.

The general lesson: **adding a state is not enough to make it reachable.** Before
believing a new state works, count the frames it was actually in during a soak.
`phase3sim` fails if any state is never reached, for exactly this reason.

### 2.12 A missing clip must not be able to pin the body

`ClipFor` used to fall back to `idleClip` for any state it had no clip for. That
made "this pack has no hit reaction" and "the hit reaction has finished"
indistinguishable to `ClipOwnsBody`, which asks `animTime < ClipDuration(clip)` —
and a clip that owns the body until a duration it does not have owns it forever.
With five states it was survivable; with eight it is a deadlock waiting.

`ClipFor` is now strict and returns −1. The idle fallback lives in `ClipOrIdle`,
called only from the one place that has to draw something no matter what.

### 2.13 The death clips throw the weapon, and the ragdoll used to snatch it back

`handslot.r` is a child of `hand.r` and sits a rigid **0.112 model units** from it in
every clip — except three, and the exceptions are all authored:

| Clip | Gap to `hand.r` | What is happening |
| --- | --- | --- |
| everything normal | 0.112 → 0.112, spread **0.000000** | rigidly parented |
| `Spawn_Ground` | 0.000 → 0.151 | the whole rig scales from 0, **non-uniformly** |
| `Ranged_2H_Shoot` | 0.112 → 0.121 | the crossbow works in the hand, 7mm |
| `Ranged_2H_Reload` | 0.121 → 0.212 | weapon shifted right out of the hand, 7cm |
| `Death_A` | 0.112 → **0.652** | the skeleton throws its weapon as it dies |
| `Death_B` | 0.112 → 0.385 | same, less far |

The death throw starts at t=0.32 of `Death_A` and accelerates smoothly to the end.
It is a feature: the corpse drops what it was holding.

**The ragdoll then took it straight back.** `Ragdoll::Begin` measured every link's
rest length from the *bind pose*, so the thrown link arrived 5.8× too long and the
solver corrected it — **0.42 world units in a single frame**, a hard teleport of the
blade back into the dead man's fist. Fixed: a link that arrives more than
`ThrownStretch` (1.5×) past its bind length keeps the length it arrived with. Every
other link arrives at exactly 1.0×, so the threshold sits in a wide gap rather than
needing tuning, and nothing is special-cased by name.

Prop movement across the handover went from **0.417 → 0.0001** world units.

**Two tests missed this**, and both for instructive reasons:

- The seam check compared the pose after `Ragdoll::Begin` against the clip's final
  pose. `Begin` only copies the pose in, so that comparison reads 0.0000 by
  construction. The snap happens on the first `Update`. **Measure across the first
  simulated frame, not at the handover call.**
- The Phase 1 grip check measured only the chop clip, where the answer is 0.000000.

Beware what "no jump" means for the *body*: seeding a verlet solver from a pose that
was never at equilibrium legitimately moves it. `Idle_A` handed to the ragdoll
settles **0.1986** world units on frame one — more than either death does, because a
standing body is further from rest than a fallen one. So the body's first-frame
settle is reported, not asserted; the assertion is on the props, which are rigid
objects with no weight to take up.

### 2.14 Props are not all authored the same way round

The grip transform was one shared set of constants, which is only correct while
every prop is authored on the same axis. Measured, from each prop's own bounding
box, they are not:

| Prop | Size | Authored along |
| --- | --- | --- |
| `Skeleton_Blade` | 0.573 × 1.497 × 0.222 | +Y |
| `Skeleton_Axe` | 0.989 × 1.252 × 0.282 | +Y |
| `Skeleton_Staff` | 0.598 × 2.102 × 0.753 | +Y |
| `Skeleton_Shield_Small_A` | 0.832 × 0.832 × 0.156 | +Y |
| `Skeleton_Arrow` | 0.117 × 0.749 × 0.102 | +Y |
| `Skeleton_Crossbow` | 1.111 × 0.508 × **1.419** | **+Z** |

`handslot`'s local **+Y** is the out-of-the-fist direction, so identity is right for
the five authored along +Y — and wrong for the crossbow, whose barrel then follows
`handslot`'s local +Z. That axis points across the body: **the crossbow aimed
sideways.** In the aiming pose the axis pointing where the archer aims is
`handslot`'s local **+X**, and yaw 90 turns the barrel onto it — **dot +0.994**
against model forward, where uncorrected reads **+0.018**.

The grip is therefore **per prop** (`Config::EnemyPropGrips`), resolved once at load
into the prop slot. Not per slot, which was the guess written down here before this
was measured: the crossbow and the blade are both slot 0 on different archetypes, so
per-slot would not have helped.

A yaw-only correction is safe for the others by construction — they are authored
along +Y and rotating about Y leaves that axis where it is. Verified: all five still
read 1.00 along local +Y.

**This is the third time an authored axis has been assumed and been wrong**, after
`EnemyModelYaw` (§2.3) and the arrow (+X, actually +Y). Measure the bounding box.

Testing it needs care: **do not ask a pose which way a weapon points.** Sampling the
middle of `Melee_1H_Attack_Slice_Diagonal` finds the axe legitimately sweeping across
the body, so any threshold on it is arbitrary — the first version of this check
failed the Rogue for swinging correctly. The convention is pose-independent, so test
it in the bone's own frame, and separately tie it to the world in the one pose where
the answer is unambiguous: a barrel at the moment the arrow leaves.

### 2.15 `FindClip` matches substrings, and the pack ships decoys

`Death_A_Pose`, `Death_B_Pose` and a `T-Pose` in every file are single-frame
poses sitting next to the clips they are named after. `"death_b"` matches
`Death_B_Pose` too, and only wins because `Death_B` happens to come first in the
file — luck, not design. `Config::EnemyMinClipDuration` (0.10s) rejects anything
too short to be an animation, loudly. It does not fire today; it is a tripwire.

---

## 3. Measured reference data

Do not re-probe this; it is stable for the shipped assets.

### Skeleton rig — 23 bones

**Indices below are `Skeleton_Minion` only.** Every character has the same 23 bone
*names* in its own order. Use the table to know what exists, never to index.

```
 0 root          11 chest
 1 hips          12 head
 2 upperleg.r    13 upperarm.l     18 upperarm.r
 3 lowerleg.r    14 lowerarm.l     19 lowerarm.r
 4 foot.r        15 wrist.l        20 wrist.r
 5 toes.r        16 hand.l         21 hand.r
 6 upperleg.l    17 handslot.l     22 handslot.r
 7 lowerleg.l
 8 foot.l        10 spine
 9 toes.l
```

`handslot.l` (17) and `handslot.r` (22) are purpose-built weapon attachment
points. This is what Phase 1 hangs props from.

### Forward reach, `EnemyHeight` 2.0, forward = +Z

| Clip | reach | note |
| --- | --- | --- |
| `Melee_1H_Attack_Jump_Chop` | 1.48 | longest |
| `Melee_Unarmed_Attack_Punch_A` | 1.47 | **current attack** |
| `Melee_Block_Attack` | 1.07 | |
| `Melee_1H_Attack_Stab` | 0.96 | |
| `Melee_1H_Attack_Chop` | 0.86 | |
| `Melee_1H_Attack_Slice_Diagonal` / `_Horizontal` | 0.82 | |
| `Melee_Unarmed_Attack_Kick` | 0.74 | shortest attack |
| `Running_A` | 0.78 | |
| `Idle_A` | 0.44 | |

At `EnemyPersonalSpace` 2.0 the current attack leaves **0.53** of daylight. That is
the tightest number in the system; anything that lengthens the attack or shortens
the standoff eats it directly.

### Phase 3 clips — reach at `EnemyHeight` 2.0, forward = +Z

None of the states added in Phase 3 comes close to the camera; the widest is the
Mage's guard at 0.94, still 1.06 clear of `EnemyPersonalSpace`. Reach is the max
over all four characters, which is what matters — a clip is shared, the bodies
are not.

| Clip | Length | Worst reach | Worst clearance | Held by |
| --- | --- | --- | --- | --- |
| `Spawn_Ground` | 1.30 | 0.89 | 1.11 | Warrior |
| `Melee_Blocking` | 1.07 | 0.94 | 1.06 | Mage |
| `Melee_Block_Hit` | 1.07 | 0.94 | 1.06 | Mage |
| `Hit_B` | 0.87 | 0.68 | 1.32 | Mage |
| `Hit_A` | 0.67 | 0.79 | 1.21 | Mage |
| `Death_B` | 2.63 | — | — | on the floor |

`Death_B` is **2.63s against `Death_A`'s 0.80** — 1.8 seconds apart. That is why
the ragdoll handover and `RemoveDead` both ask for the clip *this corpse* is
playing rather than the type's: getting it wrong either cuts the fall short or
holds a pose for two seconds. Corpse life is now 6.63s for a `Death_B`.

`Spawn_Ground` starts with the **entire skinned mesh at or below y = 0** — it is a
genuine climb out of the floor, not a crouch. It ends within 0.17 of where
`Idle_A` frame 0 sits, so the fade out of it does not pop.

### Clip inventory

`Rig_Medium_General.glb` (copied): `Death_A` 0.82, `Death_B` 2.65, `Hit_A` 0.68,
`Hit_B` 0.88, `Idle_A` 1.08, `Idle_B` 2.15, `Interact`, `PickUp`, `Spawn_Air`,
`Spawn_Ground`, `Throw`, `Use_Item`, `T-Pose`, plus `Death_A/B_Pose`.

`Rig_Medium_MovementBasic.glb` (copied): `Running_A/B` 0.82, `Walking_A/B` 1.08,
`Walking_C` 1.62, `Jump_*` 5 clips, `T-Pose`.

`Rig_Medium_CombatMelee.glb` (copied, **21 armed clips unused**):

| Group | Clips |
| --- | --- |
| 1H | `Chop`, `Jump_Chop`, `Slice_Diagonal`, `Slice_Horizontal`, `Stab` |
| 2H | `Chop`, `Slice`, `Spin`, `Spinning`, `Stab` |
| Dual wield | `Chop`, `Slice`, `Stab` |
| Block | `Melee_Block`, `Block_Attack`, `Block_Hit`, `Blocking` |
| Idles | `Melee_2H_Idle`, `Melee_Unarmed_Idle` |

`Rig_Medium_CombatRanged.glb` (**downloaded, not copied**): 20 clips —
`Ranged_1H/2H_Shoot|Reload|Aiming|Shooting`, `Ranged_Bow_Draw|Release|Idle|
Aiming_Idle` (+`_Up` variants), `Ranged_Magic_Raise|Shoot|Spellcasting|
Spellcasting_Long|Summon`.

### Props available (not yet copied)

`KayKit_Skeletons_1.1_FREE/assets/gltf/`: `Skeleton_Axe`, `Skeleton_Blade`,
`Skeleton_Crossbow`, `Skeleton_Staff`, `Skeleton_Quiver`, `Skeleton_Shield_Large_A/B`,
`Skeleton_Shield_Small_A/B`, `Skeleton_Arrow` ×4. All share `skeleton_texture`.

Characters already copied: `Skeleton_Minion`, `_Warrior`, `_Rogue`, `_Mage` — same
rig, so any of them takes any clip.

---

## 4. How to test this system

The scratchpad harness pattern that works: a small C/C++ program linking
`lib/libraylib.a`, run headless with `SetTraceLogLevel(LOG_ERROR)` and a tiny
`InitWindow` for the GL context.

**Measure the skinned mesh, not the bone data.** This is the single most important
lesson here. Two separate bugs passed bone-level checks and were wrong on screen:

- A "no pop at the seam" check compared bone *positions* and never orientations —
  the ragdoll was reproducing positions correctly while skinning the mesh wrong.
- A death-pose check compared the held pose against "the clip's final frame" when
  both sides were the *same corrupted frame*, so it passed and the bug shipped.

The reliable probe reproduces `skinning.vs` on the CPU — weight each vertex by its
four bone matrices — and reports the bounding box. A body lying down is wide and
low; a body standing is tall and narrow. That silhouette test caught what every
bone-level test missed.

**Drive the real system where you can.** `phase3sim` links the game minus
`main.cpp` and runs the actual `EnemyManager` against an actual `Level` and
`Player`, asserting only things a player could see. That is what caught §2.11: a
reimplementation of the state machine would have shown Block working perfectly,
because the state machine *was* working perfectly — the numbers around it left it
no window. Build it with:

```
g++ -std=c++14 -O1 -I src -I C:/raylib/raylib/src harness.cpp \
    $(ls obj/**/*.o | grep -v obj/main.o) \
    -o harness.exe -L lib -L C:/raylib/raylib/src -lraylib -lopengl32 -lgdi32 -lwinmm
```

The harness has to stage its own fight. The enemies have no pathfinding and a
harness has no navigation, so the first run of `phase3sim` spent its whole minute
with the player wedged against a wall and every enemy idle — it passed the "does
not get stuck" assertion while proving nothing at all. It now finds the largest
open, in-sight pocket of the map and holds the fight there, and **fails if any
state was never reached**. A soak that cannot show its coverage is not evidence.

Existing harnesses worth keeping (rebuild from this doc if the scratchpad is gone):
`probe` (clips + rig), `bones` (bone order diff), `seamtest` (silhouette),
`ragdolltest` (physics invariants), `reach` (forward reach), `framing` (skull vs
crosshair), `weaponreach` (player reach vs standoff), `blendtest` (cross-fade
endpoints), `lastframe` (trailing-frame corruption), `clips` (clip inventory and
what each substring candidate actually resolves to), `phase3clips` (new clips:
trailing frame, reach, spawn silhouette, both ragdoll handovers), `phase3sim`
(the real state machine, soaked).

---

## 5. The plan

### Phase 1 — Weapon attachment, one archetype end to end

**Status: built, agent tests pass, awaiting player checks.**

`Skeleton_Warrior` + `Skeleton_Blade` in `handslot.r`, attacking with
`Melee_1H_Attack_Chop`. Props live in `assets/models/enemies/props/`;
`Config::EnemyPropPath` / `EnemyPropBone` select them and the seven
`EnemyProp*` grip constants adjust the hold without touching code.

Warrior numbers: bind height 2.590 → scale 0.772 at `EnemyHeight` 2.0 (it is
taller than the Minion because of the helmet, and deeper because of the cloak).
`handslot.r` is index **8** on this rig.

Agent test results:

| Test | Result |
| --- | --- |
| Prop origin finite every frame | PASS |
| Worst frame-to-frame jump | 0.0880 (limit 0.5) |
| `handslot.r` → `hand.r` drift | 0.000% (limit 1%) |
| Forward reach, body / blade | 0.94 / 1.26 |
| Clearance at `EnemyPersonalSpace` 2.0 | 0.74 — PASS |

The armed chop reaches **less** than the unarmed punch it replaced (1.26 against
1.47), so camera clearance improved from 0.53 to 0.74 as a side effect.

**Work**
1. Copy the prop `.gltf`/`.bin` + texture into `assets/models/enemies/props/`, with
   the pack licence.
2. `AnimatedModel::BoneWorldTransform(int bone, const Matrix *bones)` returning
   `bindMatrix[bone] × bones[bone]`. `Ragdoll::Begin` already does this recovery —
   reuse the maths, do not reinvent it.
3. Compose prop transform: bone world transform → model scale → yaw
   (+`EnemyModelYaw`) → world position. Draw with `DrawModelEx` or a matrix push.
4. `EnemyManager` holds the prop `Model*` and the resolved `handslot.r` index.

**Agent tests**
- Prop origin tracks the hand: sample `handslot.r` world position across the chop
  clip and assert it moves smoothly, no NaN, no frame-to-frame jump > 0.5 units.
- Prop stays attached: distance from `handslot.r` to `hand.r` constant to within
  1% across the whole clip.
- Reach re-measured with the prop included; confirm it still clears
  `EnemyPersonalSpace`, and report the new number.

**Player checks**
- Blade sits *in* the hand, not through it or floating beside it.
- Blade points the right way along the arm — a 180° error looks plausible in a
  still and obvious in motion.
- Swing reads as the blade leading, not the fist.
- Blade does not clip the camera at melee range.

### Phase 2 — Archetypes

**Status: built, agent tests pass, awaiting player checks.**

`Config::EnemyArchetype` + the `EnemyTypes` table hold everything that varies by
kind; the constants above it are what they share. `EnemyManager::LoadedType` is one
per row — model, scale, clip indices, prop — and `Enemy::type` indexes both.
`SpawnTestEnemies` cycles the table so one of each is on screen.

Phase 3 has since changed three things in this table — the Warrior's cooldown and
shield, and a fifth row — so the current state is:

| Type | Model | Holds | Off hand | Attack | HP | Dmg | Cooldown | Speed | Block |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Minion | `Skeleton_Minion` | — | — | `Unarmed_Attack_Punch_A` | 40 | 6 | 1.10 | 5.6 | — |
| Warrior | `Skeleton_Warrior` | `Skeleton_Blade` | `Skeleton_Shield_Small_A` | `1H_Attack_Chop` | 90 | 11 | 1.90 | 4.0 | 55% |
| Rogue | `Skeleton_Rogue` | `Skeleton_Axe` | — | `1H_Attack_Slice_Diagonal` | 55 | 7 | 0.85 | 6.0 | — |
| Mage | `Skeleton_Mage` | `Skeleton_Staff` | — | `1H_Attack_Stab` | 50 | 9 | 1.70 | 4.4 | — |
| Archer | `Skeleton_Rogue` | `Skeleton_Crossbow` | — | `Ranged_2H_Shoot` | 45 | 8 | 2.20 | 4.6 | — |

The Archer is ranged: it stands off at 9 and opens fire at 11, where every melee
row stops at 2.2 and swings at 2.4. All five keep `personalSpace` 2.0.

Agent test results — all four archetypes, 0 failures:

| Test | Result |
| --- | --- |
| Every named clip resolves | 4/4, no fallbacks taken |
| Shared 23-bone rig, one remap path | 4/4 |
| Ordering `personalSpace < stopDistance < attackRange` | 4/4 |
| Framing floor (skull centre above the 1.50 eye) | 4/4 at height 2.00 |

Forward reach against each type's own standoff:

| Type | Reach | Clearance at 2.00 |
| --- | --- | --- |
| Minion | 1.47 | 0.53 — tightest, it is the unarmed punch |
| Warrior | 0.94 | 1.06 |
| Rogue | 0.85 | 1.15 |
| Mage | 1.05 | 0.95 |

**`handslot.r` resolved to index 8, 18, 18 and 22** across the four characters —
four models, three different indices. §2.1 is not a hypothetical.

Memory shape verified by construction: the only asset loads in `EnemyManager` are
inside `LoadType`/`LoadProp`, which run once per row before any enemy exists. Per
enemy, the only growth is `bones` and `ragdoll`. **Clip sets cannot be shared
between types** even though all four name the same three files — the remap is
applied in place against one character's bone order, so each type needs its own
copy. Four types therefore load 4 models and 12 clip files, by design.

**Work**
1. `EnemyArchetype`: model path, prop + slot, clip name candidates per state,
   health, damage, cooldown, reach/standoff overrides.
2. `Enemy` carries an archetype index; `EnemyManager` holds one `AnimatedModel`
   *per archetype* (the shared-model/per-instance-bones design already supports
   many enemies per model).
3. Suggested set: Minion unarmed (current), Warrior 1H + shield, Rogue dual wield,
   Mage staff + magic.
4. Spawn a mix.

**Agent tests**
- Every archetype resolves every clip it names; fail loudly listing misses.
- Per-archetype forward reach vs its own standoff, tabulated.
- All archetypes share the 23-bone rig, so one remap path covers them — assert it.
- Memory: N archetypes load N models and N clip sets, not N × enemies.

**Player checks**
- Each type is recognisable at a glance in a fight.
- 2H/dual-wield clips match what is actually in the hands.
- Mixed crowds do not crowd the camera worse than one enemy does.

### Phase 3 — More states

**Status: built except ranged, agent tests pass, awaiting player checks.**

`EnemyAnim` went from five states to eight: **Spawn**, **Block** and **BlockHit**
are new, and Hit and Death each pick between two clips per event.

| State | Clip | Loops | Owns the body | Chosen by |
| --- | --- | --- | --- | --- |
| Idle | `Idle_A` | yes | no | state machine |
| Walk | `Running_A` | yes | no | state machine |
| Block | `Melee_Blocking` | yes | no, but pins the feet | state machine, from `Enemy::blocking` |
| Spawn | `Spawn_Ground` | no | yes | `Enemy::StartAnim` at creation |
| Attack | per archetype | no | yes, + hold | the AI, in `EnemyManager::Update` |
| Shoot | per archetype (same slot as Attack) | no | yes, no hold | the AI, when `ranged` |
| Hit | `Hit_A` / `Hit_B` | no | yes | `TakeDamageFrom` |
| BlockHit | `Melee_Block_Hit` | no | yes | `TakeDamageFrom`, when the guard caught it |
| Death | `Death_A` / `Death_B` | no | holds, then ragdoll | `TakeDamageFrom` |

**Blocking gates damage — that question is answered.** A guard that plays the
animation and lets full damage through teaches the player to ignore the
animation, which is worse than not having it. `Config::EnemyBlockDamageScale` is
0.35, weaker than the player's 0.25 on purpose: the player blocks in response to
a swing they can see coming, the enemy blocks on a dice roll between its own
swings, so the same number would turn a lucky roll into a wall.

The decision is thin, in keeping with the rest of the AI: one roll per cooldown
against the archetype's `blockChance`, held for that whole gap, dropped
`EnemyBlockDropTime` before the next swing. Rolling per frame flickers the arms.

Two predicates, not one, and the second is defined in terms of the first so they
cannot drift (§2.9): `ClipOwnsBody` is "a one-shot clip is running", `FeetPinned`
is that **or** a raised guard. Block loops, so it must pin the feet without
claiming ownership — claiming it would deadlock the state machine, which only
reconsiders when nothing owns the body.

Agent test results — `phase3clips` and `phase3sim`, 0 failures:

| Test | Result |
| --- | --- |
| Trailing-frame trap, every new clip × 4 characters | 28/28, worst last-frame move 0.0027 |
| Forward reach vs each type's own standoff | 20/20, worst clearance 1.06 |
| `Spawn_Ground` starts below the floor, ends at `Idle_A` | 4/4, worst handover gap 0.167 |
| Ragdoll invariants, `Death_A` **and** `Death_B` | both — props move 0.012/0.025 across the handover, stretch <0.8%, settled, no NaN |
| One-shot states let go | worst 1.72s in Attack, limit 3.05s |
| **All nine** states reached in a simulated minute | 9/9 — Shoot 1326 frames, Block 71, BlockHit 130 |
| Both death clips picked | `Death_A` 10 / `Death_B` 3 over 13 deaths |
| Both hit clips picked | `Hit_A` 20 / `Hit_B` 13 over 33 flinches |
| No drift while the feet are pinned | 0.0000 units/frame |
| Everything settles to Idle/Walk out of sight | 0 stuck |
| Every blocking archetype has a gap to block in | Warrior 0.59s held |
| Guard gates damage | front 7, back 20, open 20, mid-fade 20, of 20 |
| A guarded Warrior still dies | 13 blocked swings |
| Standoff ordering, all five archetypes | 5/5, including the Archer's 2.0 < 9.0 < 11.0 |
| An arrow crossing 10 units hits a standing player | 9 damage |
| ...and still hits at **600 u/s**, so it cannot tunnel | one frame is 10 units against a 1.0-wide body |
| A wall stops an arrow | 0 damage |
| Spent arrows are removed | 0 left in flight |
| The Archer shot during the minute | 21 arrows |
| Both prop slots resolve and stay put | 6 props over 5 archetypes, worst grip drift 0.008 |
| Every prop clears the camera while alive | worst 0.98 clear (the crossbow) |
| Every prop sits on the right hand axis | 5/5 at 1.00 along local +Y, crossbow 1.00 along +X |
| The crossbow aims where the archer aims | forward +0.994 (uncorrected: +0.018) |

The soak drives the **real** `EnemyManager` against a real `Level` and `Player` —
it links the game minus `main.cpp` — and asserts only what a player could see.
Both times it was written against a reimplementation of the state machine it
would have passed while missing §2.11 entirely.

#### Ranged — built

`Rig_Medium_CombatRanged.glb` is copied in, and so are `Skeleton_Crossbow`,
`Skeleton_Arrow` and `Skeleton_Quiver`. `ProjectileManager`
(`src/combat/Projectile.*`) owns arrows in flight; `Game` owns it, not
`EnemyManager`, because a shot outlives the enemy that fired it and `RemoveDead`
would otherwise take the arrow with the archer.

A fifth archetype, **Archer**: crossbow, 45 HP, 8 damage, 2.20s cooldown. Its
three standoffs are the same ordered chain as everyone else's at a wider scale —
opens fire at 11, prefers to stand at 9, still refuses to be closer than 2 — so
none of the camera-framing rules change. It reuses the Rogue's character because
the pack has four and all four were spoken for; the crossbow is the difference.

Design points worth knowing:

- **`Attack` and `Shoot` share one clip slot.** An archetype names one attack
  animation either way; `ranged` decides which state plays it. They differ only in
  what happens: melee damages on the frame it starts, a shot puts an arrow in the
  air at `EnemyShootRelease` (0.45) of the clip — the crossbow comes up over the
  first half, so firing on frame one sends the arrow before it is pointing anywhere.
- **The release is outside the awareness test.** A shot already started is
  committed, so stepping behind a wall makes an archer waste a shot rather than
  cancel one. `Enemy::shotPending` makes it one arrow per state rather than one per
  frame past the threshold.
- **Arrows substep.** A frame at 24 u/s is 0.4 units against a 0.4 body radius, so
  one test per frame would let a shot pass through someone standing still.
  `ProjectileStep` caps a step at 0.12 units.
- **The arrow leaves the crossbow**, via `PropMuzzle` — the same bone-to-world
  composition `DrawProps` uses. So a shot is only as accurate as the pose.
- **The player is tested before the wall behind them**, or a shot taken with your
  back to a wall passes through you and dies in the brickwork.
- **Straight lines, no gravity.** Over 11 units at 24 u/s a ballistic arc is
  invisible and much harder to aim, so it buys nothing.
- The archer gives ground when the player closes inside `EnemyRangedRetreat` (0.6)
  of its standoff, which is the only AI in the game that walks backwards.

The arrow's authored axis was **measured, not assumed**: `Skeleton_Arrow` is
0.117 × 0.749 × 0.102, so its length runs along **+Y**. It was first written as +X
— a 90° error that looks plausible in a still and absurd in flight, exactly the
`EnemyModelYaw` trap in §2.3. The draw builds one rotation taking that axis onto
the velocity, rather than a yaw and a pitch that would each need the axis baked in
and would go degenerate on a shot travelling straight up.

Still missing: the **Mage** is melee with a staff. `Ranged_Magic_Shoot` and a spell
projectile are the obvious next archetype now that the machinery exists. The
**quiver** is copied in but unused — it wants a back bone rather than a handslot,
and KayKit authors handslots, so it needs a grip offset that the shared
`EnemyProp*` constants cannot currently express per slot.

**Player checks**
- Blocks read as blocks, and the Warrior's guard is visible for long enough to
  react to — 0.59s is the number, and it is the only archetype that blocks.
- Walking round behind a guarding enemy visibly does more damage than facing it.
- The **shield sits in the left hand** and the guard pose uses it — the numbers say
  it is rigidly attached and clears the camera, they cannot say it looks held.
- Spawn does not leave the enemy sliding, and the climb out of the floor does not
  show the model through the floor plane from a normal standing eye height.
- Both death clips settle convincingly, and `Death_B`'s 2.63s fall does not
  outstay itself before the ragdoll takes over.
- **The thrown weapon lands somewhere plausible.** It now stays where the clip put
  it instead of snapping back, but it is still attached to the corpse's arm by an
  invisible link rather than falling on its own — worth an eye.
- Hit alternates read as variation, not as two different enemies.
- **The arrow points the way it is flying** and is the right size against the
  skeletons (0.75 units, drawn 1:1).
- **The crossbow reads as a crossbow** in the hand, and the shot looks like it
  leaves the weapon rather than the chest.
- The Archer's 9-unit standoff feels like a threat to close on rather than a
  sniper, and backing it into a corner does not make it useless.
- Two archetypes now use the Rogue's body. Check a mixed crowd still reads.

### Phase 4 — Polish

Attack variety (pick among the 1H clips per swing), per-archetype cooldown matched
to its clip length, hit reactions that respect direction, footstep timing.

**Start with the cooldowns.** §2.11 found that three of four archetypes have a
cooldown *shorter* than their own swing clip, so the number in the table is not
the rate they actually attack at — the clip is. Minion −0.07, Rogue −0.15,
Mage +0.10 against a 0.30 hold slack. Nothing is visibly broken, because the
attack hold covers it, but it means `cooldown` is currently a lie for three rows
and no new state can fit in a gap that is not there. The Rogue is the row to look
at first: at ~1.35 it would have a real guard and could take its `blockChance`
back.

**Agent tests**
- For every attack clip in the rotation, assert clip length vs cooldown never
  reopens the §2.10 gap, and reach never exceeds standoff minus a stated margin.

**Player checks**
- Variety reads as variety, not as randomness.
- Damage still lands when the swing looks like it lands.

---

## 6. Open questions

- **Enemy height** is 2.0, close to the 1.89 framing floor. If crowding still
  bothers the owner, 2.1 buys most of the elevation back for 5% more size — but
  the three standoff distances must move with it.
- **`MeleeMinReach` flattens seven weapons** onto exactly 2.1. Raising
  `MeleeBaseReach` from 0.9 to ~1.3 would lift everything and restore the spread,
  at the cost of changing player reach against everything in the game.
- ~~**Blocking** — cosmetic, or does it actually gate damage?~~ **Answered: it
  gates.** `EnemyBlockDamageScale` 0.35 within a 140° arc. See Phase 3.
- **Only the Warrior blocks**, because it is the only archetype with a gap between
  its swings to block in (§2.11). Is one blocking enemy in four enough variety, or
  should the Rogue be slowed to ~1.35 to earn a guard back?
- **Warrior cooldown moved 1.50 → 1.90** to make room for the guard, which is a
  combat change made for an animation reason. Its damage per second is down about
  20% and the damage it *stops* is meant to pay for that — worth a play check.
- ~~**Ranged enemies** are blocked on projectiles existing.~~ **Built** — see
  Phase 3. What is left is the Mage: it still stabs with its staff where
  `Ranged_Magic_Shoot` and a spell projectile now have everything they need.
- **The Archer reuses the Rogue's character**, because the free pack has four and
  all four were already spoken for. Worth a look in a mixed fight, and worth
  knowing that it costs a second copy of that model and its clips (§Phase 2 — clip
  sets cannot be shared, the remap is applied in place per character).
- **A dropped weapon does not fall.** The death clip throws it clear and it now
  stays thrown, but it is still driven by `handslot` on the corpse rather than being
  a loose object with its own gravity. Making it a real drop is the next step if it
  reads badly.
- ~~**The `EnemyProp*` grip constants are shared by both slots.**~~ Replaced by
  `Config::EnemyPropGrips`, a per-prop table — see §2.14, which is why. The quiver
  still needs a back bone, but the grip mechanism can now express it.
- **The arrow leaves from the crossbow's grip, not its tip.** The bow is 1.4 units
  long, so a shot starts about a unit behind where the string is. Invisible at
  speed, and fixable with an offset in the grip table if it reads badly.
- **`EnemySeparation`** (enemy vs enemy) is still 0.8 and untouched by the
  heaviness pass. Should they feel heavy to each other too?

---

## 7. Rules for whoever works on this

1. **Measure the skinned mesh.** Bone-level checks have passed twice while the
   thing on screen was broken.
2. **Never trust `IsModelAnimationValid`.** It compares bone counts.
3. **Re-measure reach on +Z** whenever an attack clip changes.
4. **Move the three distance constants together**, and with `EnemyHeight`.
5. **Quote real numbers** when reporting a phase done. "Looks right" is the
   owner's call, not the agent's.
6. **Say what was not verified.** Several bugs here survived because a check was
   reported as stronger evidence than it was.
7. **Count the frames a new state actually ran for.** Adding a state does not make
   it reachable, and the code can be entirely correct while the numbers around it
   leave it no window (§2.11).
8. **A new clip changes the clocks.** Its length sets what the cooldown, the hold
   slack and any guard window have to be — check all three, not just the one you
   were working on.
9. **Measure the authored axis of anything you attach or throw.** Three for three
   wrong when assumed: the characters (§2.3), the arrow, the crossbow (§2.14). A
   bounding box answers it in one line.
10. **A well-attached prop can still be pointing the wrong way.** Rigidity, no
    jumps and camera clearance all pass on a crossbow aimed across its own body.
    Ask which way the business end points — and ask it somewhere the answer is not
    ambiguous, which is not the middle of a swing.
