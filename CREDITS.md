# Credits

Third-party assets used in Dungeon Foray, and what each one's licence asks of us.

Most of what is here is **CC0**, and every pack that ships its own licence text
has that text kept beside the assets rather than only summarised here.

**One entry requires attribution**: the menu sky is CC BY 4.0, where credit is a
condition of use. It is on the in-game credits screen and has to stay reachable.
Everything else asks for nothing, and it is worth keeping the ratio that way: an
asset with no stated licence is not public domain, it is all rights reserved, and
it is not worth the uncertainty when CC0 alternatives are a search away.

Several entries below are **not** CC0 — the menu sky, the HUD bar art, the UI font
and both audio packs. None is unlicensed; all are free to use on their own terms,
and each is flagged so the difference is not lost.

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

### KayKit Dungeon Pack (1.0) characters — Kay Lousberg

- Source: <https://kaylousberg.itch.io/kaykit-dungeon>
- Licence: **CC0**
- Used for: the three vendors. `character_rogue` is the merchant, `character_mage`
  the mystic, `character_knight` the captain — see `world/Npc.cpp`.
- In repo: `assets/models/npcs/`, licence text alongside
- Attribution: **not required**, requested, as above.

Note these are the **1.0** pack's characters, not 1.1's. They are STATIC — no rig
and no clips, six loose body-part meshes — which is why the vendors' idle is
procedural rather than an animation. See the note in `world/Vendors.h`.

### KayKit Medieval Hexagon Pack (1.0 FREE) — Kay Lousberg

- Source: <https://kaylousberg.itch.io/medieval-hexagon>
- Licence: **CC0**
- Used for: the town outside the maze walls — the red buildings only. See
  `world/Skyline.h`.
- In repo: `assets/models/skyline/`, licence text alongside
- Attribution: **not required**, requested, as above.

Only the `red` colourway is copied in; the pack ships blue, green, neutral and
yellow as well. Swapping to another is a path change in `Skyline.cpp`'s `Dir`.

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

### Free 2D VFX Pack — bigdamnhero6

- Source: <https://bigdamnhero6.itch.io/free-2d-vfx-pack>
- Licence: **CC0 1.0 Universal**
- Author: bigdamnhero6
- Used for: the impact effect a magic mote bursts into. Eight horizontal strips
  of 128px square frames: blood splatter, both muzzle flashes, particle splash,
  explosion, poison smoke, flame, and lightning spark.
- In repo: `assets/textures/vfx/`

The pack has ten effects; its two dust sheets are not used. CC0 does not require
attribution, but the source is recorded here for provenance.

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

### Sky background — Unicorn Creates ⚠️ attribution REQUIRED

- Source: <https://unicorncreates.itch.io/sky-backgrounds>
- Licence: **CC BY 4.0**
- Used for: the picture behind the main menu, the options page and the credits,
  tinted red to sit in the same register as the dungeon's own sky. See
  `ui/MenuBackdrop.h`.
- In repo: `assets/textures/menu/sky_night.png`
- Attribution: **REQUIRED**. This is the one asset in the project whose licence
  makes credit a condition of use rather than a courtesy, and it is why the
  in-game credits screen shows a licence column at all.

The same file the mobile game uses for its menu, brought over with the rest of the
shared face.

## Audio

### Action Music Pack 1 — ansimuz (Luis Zuno)

- Source: <https://ansimuz.itch.io/action-music-pack-1>
- Licence: **free for personal and commercial use, modification and
  redistribution** — the pack's own terms, kept beside it
- Used for: every music track. One on the front end, three in the dungeon, and the
  front-end track again for a finished run — see `audio/Music.cpp`.
- In repo: `assets/bg_music/`, terms in
  `assets/bg_music/ACTION PACK 1 OGG/public-license.txt`
- Attribution: **not required**, "appreciated" in the pack's own words. Credit
  ansimuz.

Four of the pack's eleven tracks are used; the rest were deleted rather than
shipped unread. Adding one back is the file plus a row in `Music.cpp`'s table.

### RPG Essentials SFX (FREE) — Leohpaz

- Source: <https://leohpaz.itch.io/rpg-essentials-sfx-free>
- Licence: **free — see pack**
- Used for: every sound effect. 38 clips across UI, movement, melee, magic and the
  world — see the table in `audio/Sfx.cpp`.
- In repo: `assets/sfx/`
- Attribution: not established as required. Credit Leohpaz anyway.

31 of the pack's 48 files are used; the rest were deleted. The pack ships no
licence file, which is why the terms above are the itch.io page's and not a
document in the repo — read that page before shipping commercially.

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
- [ ] Read brullov's and Hewett Tsoi's actual terms before shipping commercially.
      Both are free to use and neither is CC0, which is a different thing, and
      "free" on itch.io and dafont covers a range that includes conditions.
- [ ] **Unicorn Creates' sky is CC BY 4.0.** Credit is a CONDITION. It is on the
      in-game credits screen and must stay there; if that screen is ever cut, the
      attribution has to go somewhere else the player can reach.
- [ ] Read ansimuz's and Leohpaz's actual terms before shipping commercially. The
      music pack ships its terms; the SFX pack does not.
- [ ] Re-check this file whenever an asset is added. The rule that has already
      caught one pack: if the download has no licence file, find out why before
      building anything on top of it.
