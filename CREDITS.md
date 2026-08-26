# Credits

Third-party assets used in ActionMaze, and what each one's licence asks of us.

Most of what is here is **CC0**, and every pack that ships its own licence text
has that text kept beside the assets rather than only summarised here. Nothing in
this project currently requires attribution. Keep it that way as far as possible:
an asset with no stated licence is not public domain, it is all rights reserved,
and it is not worth the uncertainty when CC0 alternatives are a search away.

Two entries below are **not** CC0 — the HUD bar art and the UI font, both brought
over from the mobile-game project. Neither is unlicensed; both are free to use on
their own terms, and both are flagged so the difference is not lost.

## Art

### KayKit Dungeon Asset Pack (1.1) — Kay Lousberg

- Source: <https://kaylousberg.itch.io/kaykit-dungeon>
- Licence: **CC0** (Creative Commons Zero)
- Used for: every wall, floor, door, prop and decoration in the level
- In repo: `assets/models/dungeon/`, full licence text in
  `assets/models/dungeon/LICENSE-KayKit-DungeonPack.txt`
- Attribution: **not required**, requested. Credit as "Kay Lousberg,
  www.kaylousberg.com".

### KayKit Skeletons (1.1) and Character Animations (1.1) — Kay Lousberg

- Source: <https://kaylousberg.itch.io/>
- Licence: **CC0**
- Used for: enemy characters, their rigs and every animation clip
- In repo: `assets/models/enemies/`, `assets/models/weapons/`
- Attribution: **not required**, requested, as above.

### SBS Planet Surface Skyboxes — Screaming Brain Studios

- Source: <https://screamingbrainstudios.itch.io/> (Planet Surface Skyboxes, Cubemap)
- Licence: **CC0 1.0 Universal** — commercial or not, no restrictions, credit optional
- Used for: the skybox. `Cubemap_Red_02` only; the pack ships 22 others.
- In repo: `assets/textures/skybox/sbs/`, licence text alongside it in
  `LICENSE-ScreamingBrainStudios.txt`
- Attribution: **not required**. The licence says so in as many words.

Format note: a 4x3 cross, 2048x1536, so 512 per face. That layout is named
explicitly in `Sky::Load` as `CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE` rather than
auto-detected, so swapping in a differently laid out skybox means changing that
constant too. Trying a different one from this pack is only a path change in
`Config::SkyCubemap` — they all share the layout and the resolution.

Replaced the VoidPix Skybox Pack, which shipped no licence of any kind.

### VFX sprite sheets — source unidentified ⚠️

- Source: **unknown**. Brought over from the mobile-game project, where the
  credits table records them as "VFX packs, TODO" and no licence file shipped
  with them.
- Licence: **not established**
- Used for: the impact effect a magic mote bursts into. Eight horizontal strips
  of 128px square frames.
- In repo: `assets/textures/vfx/`

This is the one entry in this file that does not meet the rule at the bottom of
it, and it is recorded here rather than quietly used so that it cannot be
forgotten. An asset with no stated licence is not public domain, it is all
rights reserved. Either the pack is identified and its terms written down, or
these eight sheets are replaced with CC0 equivalents before anything ships.

Nothing else depends on them being these particular files: `VfxManager` reads a
path, a frame count and a rate per sheet from one table in `Vfx.cpp`, so a
swap is eight rows and no code.

### Castle of Despair HUD art — brullov ⚠️ not CC0

- Source: <https://brullov.itch.io/2d-platformer-asset-pack-castle-of-despair>
- Licence: **free to use, see the pack's own terms** — not CC0, and the pack
  ships no licence file
- Used for: every bar on the HUD. `bar.png` is a 118x13 ornamented frame with a
  hollow window; `bar_background.png` and `health_bar.png` are the 100x7 strips
  that drop into it. The blue, violet and white fills are the red strip
  recoloured per pixel at load — see `UiBars.cpp`.
- In repo: `assets/ui/`
- Attribution: not established as required. Credit brullov anyway.

Brought over from the mobile-game project, which uses the same three files for
the same job, so the two games' HUDs are visibly the same object. `weapon_icon.png`
came with them and is not used yet.

Nothing depends on these being present: `UiBars::Ready()` is false when the files
are missing, and every bar falls back to the drawn rectangle the HUD used before
the art arrived.

### Alagard — Hewett Tsoi ⚠️ not CC0

- Source: <https://www.dafont.com/alagard.font>. The sprite-font atlas is Ramon
  Santamaria's, shipped with raylib as
  `examples/text/resources/sprite_fonts/alagard.png`.
- Licence: **freeware**
- Used for: every string the player reads. See `UiText.h`.
- In repo: `assets/fonts/alagard.png`, with the details in `CREDIT.txt` beside it
- Attribution: not required. Credit Hewett Tsoi anyway.

The same face the mobile game uses, and for the same reason the HUD art is shared:
the two projects are meant to be recognisably one thing. It is a 95-glyph image
font, ASCII 32-126 — anything outside that range draws as `?`, silently, which is
the one thing to remember when writing a user-visible string.

## Code

### raylib — Ramon Santamaria and contributors

- Source: <https://www.raylib.com>
- Licence: **zlib/libpng**
- `assets/shaders/skybox.vs` and `skybox.fs` are taken from raylib's
  `examples/models` resources unchanged.

## Before release

- [ ] Decide whether to credit these in-game. None of them requires it — KayKit
      asks nicely, Screaming Brain Studios explicitly says credit is optional —
      but between them they are doing essentially all of the visual work.
- [ ] **Resolve the VFX sheets above.** Identify the pack and record its terms,
      or replace them. This is the only unlicensed art in the project.
- [ ] Read brullov's and Hewett Tsoi's actual terms before shipping commercially.
      Both are free to use and neither is CC0, which is a different thing, and
      "free" on itch.io and dafont covers a range that includes conditions.
- [ ] Re-check this file whenever an asset is added. The rule that has already
      caught one pack: if the download has no licence file, find out why before
      building anything on top of it.
