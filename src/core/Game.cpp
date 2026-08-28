
#include "core/Game.h"

#include "combat/Attack.h"
#include "combat/Equip.h"
#include "combat/Stats.h"
#include "core/Config.h"
#include "raylib.h"
#include "raymath.h"
#include "ui/UiText.h"
#include "ui/UiTheme.h"

#include <cmath>
#include <ctime>
#include <vector>

void Game::Run()
{
    Init();

    // The window's own close button, or the pause menu. Not Escape: SetExitKey has
    // taken that away from raylib, and Escape now opens the menu that offers to quit
    // rather than quitting.
    while (!WindowShouldClose() && !quitting)
    {
        // Capped: one hitch at full speed would otherwise carry a body straight
        // through a wall before collision ever got a look at it
        const float delta = fminf(GetFrameTime(), Config::MaxFrameTime);

        Update(delta);
        Draw();
    }

    Shutdown();
}

void Game::Init()
{
    // Before InitWindow - it is a hint on the framebuffer being created, and after
    // the context exists there is nothing left to hint at.
    //
    // The dungeon pack's brickwork is modelled, not textured: one wall piece is
    // 5,492 triangles of small bevelled courses. At any distance those edges fall
    // under a pixel, and without multisampling each one snaps between covered and
    // not from frame to frame - which reads as a shimmering hatch crawling over
    // the stone. It is not depth fighting, though it looks exactly like it: a
    // single wall piece alone in an empty room does it just as readily as two
    // overlapping ones, which is what finally told them apart.
    if (Config::AntiAliasing) SetConfigFlags(FLAG_MSAA_4X_HINT);

    InitWindow(Config::ScreenWidth, Config::ScreenHeight, Config::WindowTitle);

    // Escape stops being a quit key and becomes an ordinary input. There is a
    // character and a part-cleared floor to lose now, and the pause menu is what
    // answers Escape instead - see PauseMenu.h.
    SetExitKey(KEY_NULL);

    // After InitWindow, because it resizes the window it is given to the monitor
    if (Config::Fullscreen) ToggleBorderlessWindowed();

    InitAudioDevice();
    SetTargetFPS(Config::TargetFps);
    ApplyVolume();

    // Before anything that measures a string. Every layout in the game is laid out
    // against whatever UiFont() returns, and the default font is a narrower face -
    // a panel fitted before the load and drawn after it is a panel that overflows.
    // It is also the one asset every front-end page needs before it can draw a
    // word, so it is the only load boot actually waits on - see LoadRunAssets for
    // everything else.
    LoadUiFont();

    // Nothing else here: the gameplay assets and the level are loaded behind the
    // run-loading screen instead, the first time Start Game is pressed - see
    // LoadRunAssets and UpdateRunLoading. The Main Menu is on screen as soon as
    // the boot loading screen's minimum time is up.
    appState = AppState::BootLoading;
    pendingState = AppState::BootLoading;
}

//----------------------------------------------------------------------------------
// The once-ever gameplay assets - see the note on the declaration in Game.h.
//
// What Init used to do outright before there was a front end for a loading screen
// to stand in front of it. StartNewRun (the level, the starting kit, the player
// placed on it) is deliberately not here: that runs every time a fresh run
// starts, not just the first.
//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
// The run load, as a list of steps rather than one blocking call.
//
// It used to be one function, and the loading screen was a lie: the fader reached
// black, the whole 1.3 seconds of glTF parsing ran inside a single frame, and the
// page the player was supposed to be looking at while it happened got its first
// draw AFTER the work was already done. What that looks like from a chair is a
// blank screen followed by a loading screen that flashes past - which is exactly
// what it was.
//
// One step per frame instead. Each entry is drawn before it runs, so the label
// under the bar names the thing being loaded RIGHT NOW rather than the thing that
// just finished, and the bar has somewhere real to come from.
//
// The labels and the switch below are one list in two halves and have to stay in
// step - same arrangement as PauseMenu's entry table against its Choice enum, and
// the two sitting next to each other in this file is what keeps that honest.
//----------------------------------------------------------------------------------
namespace
{
    //--------------------------------------------------------------------------
    // The steps before the dungeon art, one label each, in the order they run.
    // Index i here is case i in RunLoadStep below, and the two have to stay in
    // step - same arrangement as PauseMenu's entry table against its Choice
    // enum, and the two sitting next to each other is what keeps that honest.
    //--------------------------------------------------------------------------
    constexpr const char *RunLoadLabels[] =
    {
        "the sky",
        "your hands",
        "the armoury",
        "the dead",             // See AssetManager::GetAnimations
        "the interface",
        "arrows and motes",
        "fire and blood",
        "the way down",
        "what hunts you",
        "the vendors",
        "spoils",
        "pickups",
        "chests",
        "the horizon",          // The town outside the walls - see world/Skyline.h
    };

    constexpr int RunLoadFixed = (int)(sizeof(RunLoadLabels)/sizeof(RunLoadLabels[0]));

    //--------------------------------------------------------------------------
    // Then the dungeon kit, which is far too long to be one step - see
    // Level::LoadArtStep - and then the floor built out of it.
    //
    // The art range is sized by Level rather than written down here, so a room
    // kind gained or lost changes the bar and nothing else.
    //--------------------------------------------------------------------------
    int RunLoadArtSteps() { return Level::ArtStepCount(); }

    int RunLoadTotal() { return RunLoadFixed + RunLoadArtSteps() + 1; }
}

void Game::RunLoadStep(int index)
{
    switch (index)
    {
        case 0:  sky.Load(assets, Config::SkyCubemap); break;
        case 1:  viewModel.Load(assets);     break;
        case 2:  weaponPreview.Load(assets); break;
        case 3:  enemies.Load(assets);       break;
        case 4:  hud.Load(assets);           break;
        case 5:  projectiles.Load(assets);   break;
        case 6:  vfx.Load(assets);           break;
        case 7:  portal.Load(assets);        break;
        case 8:  events.Load(assets);        break;
        case 9:  vendors.Load(assets);       break;
        case 10: loot.Load(assets);          break;
        case 11: pickups.Load(assets);       break;
        case 12: treasure.Load(assets);      break;
        case 13: skyline.Load(assets);       break;

        default:
            if (index < RunLoadFixed + RunLoadArtSteps())
            {
                level.LoadArtStep(assets, index - RunLoadFixed);
                break;
            }

            // The last step, and the only one that runs on a second run too - see
            // the note on `runAssetsLoaded`. Building the floor is per RUN; loading
            // the art it is built out of is per LAUNCH.
            StartNewRun();
            break;
    }
}

//----------------------------------------------------------------------------------
// A whole fresh run: a new character at a fresh depth 1.
//
// What Init calls once at startup and what Restart, on the run-end screen, calls
// again - the two ARE the same operation, and keeping them as one function is what
// stops them drifting apart the next time a system is added here.
//
// Unlike Descend, this also throws away the character (Player::ResetCharacter) and
// the run's progression (ResetProgression): a floor change is a place changing
// under the same character, but a new run is a new character. Level::ResetDepth is
// the one call that makes that safe to do more than once - Level::Depth otherwise
// only ever counts up, because every OTHER caller (F6, the portal) wants exactly
// that.
//----------------------------------------------------------------------------------
void Game::StartNewRun()
{
    player.ResetCharacter();

    // After ResetCharacter, since the arsenal is sized to the view model's weapon
    // list but the starting kit it hands back has to land on a level 1 character
    ResetProgression();

    level.ResetDepth();
    level.Load(assets);

    player.Spawn(level.SpawnPoint());
    camera.SnapTo(player.Position());

    projectiles.Clear();
    vfx.Clear();
    loot.Clear();
    pickups.Clear();
    treasure.Clear();

    ResetChaos(chaos, level.Depth());
    portal.Reset();
    portal.PlaceAt(level.PortalPoint());

    // Off the map's own seed, so this floor's horizon is this floor's - see
    // Skyline::Place. Placed on every floor for the same reason the props are.
    skyline.Place(level);

    events.Place(level, level.Depth());

    // After the events, because the two share the pool of rooms and the map chose
    // the event rooms first
    vendors.Place(level);
    RerollVendorStock();

    enemies.PopulateCamps(level, player.level);
    SeedRoomLoot();
    SeedPickups();

    hud.ResetMap(level);

    runPhase = RunPhase::Playing;

    TraceLog(LOG_INFO, "RUN: depth %i, %i chaos to quell, ranks about %i, %i events, %i vendors",
             level.Depth(), chaos.max, RankCentreForDepth(level.Depth()),
             events.Outstanding(), vendors.Count());

    FreeCursor(false);  // Limit cursor to relative movement inside the window
}

//----------------------------------------------------------------------------------
// The starting kit, and the tables the shops sell out of.
//
// Once, at the top of a run. NOT on a floor change: what the player has bought is the
// run's progression, and a weapon that vanished at the portal would make every
// purchase a rental.
//
// The arsenal is sized to the view model's weapon list, which is why this cannot run
// before ViewModel::Load - and why it takes a listing rather than the view model
// itself. See the note on WeaponListing.
//----------------------------------------------------------------------------------
void Game::ResetProgression()
{
    std::vector<WeaponListing> listing;

    listing.reserve((size_t)viewModel.Count());

    for (int i = 0; i < viewModel.Count(); ++i)
    {
        const WeaponStats table = StatsFor(viewModel.NameAt(i), viewModel.StyleAt(i),
                                           viewModel.HeightAt(i));

        WeaponListing entry;

        entry.name = viewModel.NameAt(i);
        entry.damage = table.damage;
        entry.reach = table.reach;
        entry.tags = table.tags;

        listing.push_back(entry);
    }

    arsenal.Reset(listing, Config::StartingWeapon);

    // The shield is equipped as well as owned - see below. The staff is owned
    // only: a run does not have to find the merchant to learn it can cast, but
    // starting with both hands already spoken for by a sword and a shield is
    // enough of a kit handed to the player without a third choice made for
    // them too.
    arsenal.GiveByName(Config::StartingShield);
    arsenal.GiveByName(Config::StartingStaff);

    // Rolled fresh each run rather than always Flame - the schools are already
    // balanced against each other (see MagicDef::damageMult's note), so there is
    // no "safe" school a new character needs to be steered toward, and always
    // opening on the same one made every run's first floor look the same.
    const Magic startingMagic = (Magic)GetRandomValue(0, (int)Magic::Count - 1);

    spells.Reset(startingMagic);
    traits.Clear();

    player.purse.Clear();
    player.mana = player.MaxMana();

    RefreshModifiers();

    //------------------------------------------------------------------------------
    // Sword and board: the ordinary opening kit the rest of the run is measured
    // against - see the note on Config::StartingWeapon.
    //
    // Both found BY NAME rather than with arsenal.NextOwned(-1, 1) - that finds
    // the first OWNED weapon in table order, which is fine while the sword is
    // the only thing owned and wrong the moment a second starting item is: the
    // shield sorts before the sword alphabetically, so "the first one owned"
    // silently became the shield in both hands the moment GiveByName above
    // started granting it too.
    //------------------------------------------------------------------------------
    EquipWeapon(viewModel, arsenal, Hand::Right, arsenal.IndexOfName(Config::StartingWeapon));
    EquipWeapon(viewModel, arsenal, Hand::Left, arsenal.IndexOfName(Config::StartingShield));

    magic = startingMagic;
}

//----------------------------------------------------------------------------------
// A fresh, small handful of unowned offers at every counter.
//
// One call rather than three scattered across StartNewRun and Descend, so a
// fourth vendor added later needs one new line here instead of one in each of two
// call sites that would otherwise have every reason to fall out of step.
//----------------------------------------------------------------------------------
void Game::RerollVendorStock()
{
    // The first floor guarantees a castable weapon among what is offered - a run
    // that wants to try a school from the first room down should not have to
    // hope the merchant's random five happened to include one. Every floor after
    // it is an ordinary random reroll.
    const unsigned guarantee = (level.Depth() == 1) ? (unsigned)TagCasting : 0u;

    // Same reasoning as the merchant's castable guarantee above, for the
    // captain's cheapest rows - see Config::CaptainCheapGuaranteeDepth.
    const int cheapCap = (level.Depth() <= Config::CaptainCheapGuaranteeDepth)
                        ? Config::CaptainCheapGuaranteePrice : 0;

    arsenal.RerollOffers(Config::MerchantStockPerFloor, guarantee);
    spells.RerollOffers(Config::MysticStockPerFloor);
    traits.RerollOffers(Config::CaptainStockPerFloor, cheapCap);
}

//----------------------------------------------------------------------------------
// No room left with nothing to do in it.
//
// Several room kinds are not `garrisoned` (Storage, Kitchen, Vault, Library - see
// world/RoomKind.h) so that a camp cannot claim them, and most rooms hold neither a
// vendor nor an event either - there are only ever a handful of each on a floor.
// Without this, a room that rolled none of the three is furniture and nothing
// else: a box the player walks through once and never has a reason to enter again.
//
// Run once every floor, after camps, vendors and events have all placed
// themselves, since this is the one pass that can actually see what is left
// unclaimed. Gems rather than coins, because coins have nowhere to be dropped AS -
// see the note on Loot.h for why this system carries only the two rare currencies.
//----------------------------------------------------------------------------------
void Game::SeedRoomLoot()
{
    const std::vector<Room> &rooms = level.Grid().Rooms();

    const std::vector<int> &vendorRooms = level.Grid().VendorRooms();
    const std::vector<int> &eventRooms = level.Grid().EventRooms();
    const std::vector<int> &campRooms = enemies.CampRooms();

    auto listed = [](const std::vector<int> &list, int room)
    {
        for (int i : list) { if (i == room) return true; }

        return false;
    };

    for (int i = 0; i < (int)rooms.size(); ++i)
    {
        const Room &room = rooms[(size_t)i];

        // Kept clear on purpose - see RoomKind.h. Neither is ever garrisoned,
        // vendored or made an event either, so both would otherwise get a drop
        // every floor for no reason.
        if ((room.kind == RoomKind::Entrance) || (room.kind == RoomKind::Portal)) continue;

        if (listed(vendorRooms, i) || listed(eventRooms, i) || listed(campRooms, i)) continue;

        // A little more somewhere already worth a special trip. Both figures are
        // small - this is a consolation for an empty room, not a reason to skip
        // the rooms that hold something else.
        const bool special = (room.kind == RoomKind::Vault) || (room.kind == RoomKind::Library);

        const Vector3 at = level.FindOpenSpotIn(room, 0.5f);

        loot.Spawn(Currency::Gems, special ? 2 : 1, at);

        //--------------------------------------------------------------------------
        // The Vault ADDITIONALLY gets a scatter of coin piles, each its own drop at
        // its own spot - see Config::VaultCoinPiles* - so it reads as a chest
        // someone tipped over rather than as the one-gem consolation every other
        // empty room gets. Library keeps just the gem: it is a room of books, not
        // a room of money.
        //--------------------------------------------------------------------------
        if (room.kind == RoomKind::Vault)
        {
            const int piles = GetRandomValue(Config::VaultCoinPilesMin, Config::VaultCoinPilesMax);

            for (int p = 0; p < piles; ++p)
            {
                const Vector3 pileAt = level.FindOpenSpotIn(room, 0.4f);
                const int amount = GetRandomValue(Config::VaultCoinAmountMin,
                                                  Config::VaultCoinAmountMax);

                loot.Spawn(Currency::Coins, amount, pileAt);
            }

            //----------------------------------------------------------------------
            // A weapon, rarer than the coins - see the class note on
            // TreasureManager for why this is a coin flip on top of the Vault
            // already being the rarest room kind on the table, rather than
            // something every Vault carries.
            //----------------------------------------------------------------------
            if ((GetRandomValue(0, 999)/1000.0f) < Config::TreasureChestChance)
            {
                treasure.Spawn(level.FindOpenSpotIn(room, 0.5f));

                TraceLog(LOG_INFO, "TREASURE: a chest waits in the Vault at (%i, %i)",
                         room.CenterX(), room.CenterZ());
            }
        }
    }
}

//----------------------------------------------------------------------------------
// Health, mana and a buff, scattered a few to a floor - see world/Pickup.h.
//
// Unlike SeedRoomLoot this does not avoid a vendor's, an event's or a camp's own
// room: a gem is a consolation for a room with nothing else going on, and a
// pickup is the opposite of that - something to grab mid-fight is worth more IN
// a camp's room than in an empty one. Entrance and Portal are still excluded,
// the same reasoning SeedRoomLoot uses: neither is a room the player fights or
// lingers in.
//
// Each pickup claims a different room rather than several landing on top of
// each other, so a floor's handful of them reads as spread through the dungeon
// rather than piled in whichever room happened to be rolled first.
//----------------------------------------------------------------------------------
void Game::SeedPickups()
{
    const std::vector<Room> &rooms = level.Grid().Rooms();

    std::vector<int> eligible;

    for (int i = 0; i < (int)rooms.size(); ++i)
    {
        const RoomKind kind = rooms[(size_t)i].kind;

        if ((kind == RoomKind::Entrance) || (kind == RoomKind::Portal)) continue;

        eligible.push_back(i);
    }

    if (eligible.empty()) return;

    // Picks and removes a random room from what is left, so the next pickup
    // cannot land in the same one - fewer eligible rooms than requested simply
    // places fewer pickups rather than doubling any of them up.
    auto placeSome = [&](int count, auto spawnOne)
    {
        for (int n = 0; (n < count) && !eligible.empty(); ++n)
        {
            const int pick = GetRandomValue(0, (int)eligible.size() - 1);
            const int roomIndex = eligible[(size_t)pick];

            eligible.erase(eligible.begin() + pick);

            spawnOne(level.FindOpenSpotIn(rooms[(size_t)roomIndex], 0.4f));
        }
    };

    placeSome(Config::PickupHealthPerFloor, [this](Vector3 at) { pickups.Spawn(PickupKind::Health, at); });
    placeSome(Config::PickupManaPerFloor,   [this](Vector3 at) { pickups.Spawn(PickupKind::Mana, at); });
    placeSome(Config::PickupBuffPerFloor,   [this](Vector3 at) { pickups.SpawnBuff(at); });
}

// The view model knows which weapon is in each hand; combat needs to know what
// that weapon does. This is the seam between the two, and it lives here so
// neither side has to reach across.
void Game::RefreshLoadout()
{
    for (int h = 0; h < HandCount; h++)
    {
        const Hand hand = (Hand)h;

        styles[h] = viewModel.StyleFor(hand);
        stats[h] = StatsFor(viewModel.NameFor(hand), styles[h], viewModel.HeightFor(hand));

        //--------------------------------------------------------------------------
        // What the forge added, on top of the weapon's own bonus - both Modifiers,
        // both restricted to the flat and fraction columns (see combat/Weapon.h
        // and combat/Modifiers.h), so summing them here is just ModifiersAdd and
        // never a StatBlock field added by hand.
        //
        // Here rather than inside StatsFor, because StatsFor answers "what is this
        // kind of weapon" and the forge level answers "what has this player done to
        // it" - and the first of those is a table lookup that has no business
        // knowing a run exists.
        //--------------------------------------------------------------------------
        const int slot = viewModel.SlotIndex(hand);

        if (arsenal.Owns(slot))
        {
            stats[h].damage = (int)(stats[h].damage*arsenal.DamageMult(slot) + 0.5f);

            stats[h].bonus = ModifiersAdd(stats[h].bonus, arsenal.HeldBonus(slot));
        }

        // An empty hand swings nothing, fires nothing, and - the part that is easy
        // to forget - is worth nothing. A bonus left on a hand that is no longer
        // holding the weapon it came from is a bonus the player keeps by putting
        // the weapon away, which is the wrong way round.
        if (!viewModel.HasWeapon(hand))
        {
            stats[h].melee = false;
            stats[h].ranged = false;
            stats[h].bonus = Modifiers{};
            stats[h].lifesteal = 0.0f;
            stats[h].critBonus = 0.0f;
            stats[h].stun = 0.0f;
            stats[h].knockback = 0.0f;
        }
    }

    //------------------------------------------------------------------------------
    // Both hands, summed, handed down as one Modifiers.
    //
    // Summed here rather than inside the Player because this is the only place that
    // sees the loadout at all - and because it is where the rule that two weapons
    // stack lives. Two of the same weapon is twice the bonus, which is deliberate:
    // fighting with two daggers should be the crit build, not a cosmetic choice.
    //------------------------------------------------------------------------------
    player.SetGearMods(ModifiersAdd(stats[0].bonus, stats[1].bonus));
}

Vector3 Game::AimDirectionFrom(Vector3 muzzle) const
{
    // The camera's ray, not Player::Forward(). The player's facing is deliberately
    // flattened - it is the body's heading, and melee never cared about pitch -
    // so aiming along it sends every shot out horizontally however far up or down
    // the crosshair is pointing. This is the ray the crosshair actually draws on.
    const Ray aim = camera.AimRay();

    // Converge on a point well down that ray rather than firing parallel to it.
    // Parallel would point the right way and still never cross the crosshair,
    // because the muzzle starts off to one side of the eye - a miss that stays
    // the same width however far away the target is.
    //
    // Aiming at a point instead makes the shot true at AimDistance and only
    // slightly off either side of it, which errs in the forgiving direction: the
    // error shrinks with distance, and distance is where aim has to be good.
    const Vector3 aimPoint = Vector3Add(aim.position, Vector3Scale(aim.direction, Config::AimDistance));

    return Vector3Subtract(aimPoint, muzzle);
}

//----------------------------------------------------------------------------------
// What finishing a floor means: one deeper, or the end of the run.
//
// Called from the one place a floor can be finished - the portal's dwell
// completing. It was two: the pause menu carried a DESCEND shortcut past the walk
// to the portal, which is gone (see the entry table in PauseMenu.cpp for why).
// Kept as its own function regardless, because the victory check on
// Config::VictoryDepth belongs to "a floor was finished" and not to the portal.
//----------------------------------------------------------------------------------
void Game::AdvanceFloor()
{
    if (level.Depth() >= Config::VictoryDepth)
    {
        runPhase = RunPhase::Victorious;
        runEnd.Open(true, level.Depth(), player.level);
        FreeCursor(true);
    }
    else
    {
        Descend();
    }
}

//----------------------------------------------------------------------------------
// One floor down: throw this one away and build the next.
//
// Everything downstream of the map has to be rebuilt with it, and in this order:
// the level first, then the player onto the new spawn, then the camps into the new
// rooms. An enemy left standing from the old map would be standing in the new
// map's rock, and a camp still holding room 7 would be holding a room that no
// longer exists.
//
// The character is NOT rebuilt. Level, experience, points spent and points owed all
// carry - a floor is a place, and descending is walking out of it, not starting
// again. That is the whole difference between this and a restart, and it is why
// Player::Spawn is a placement rather than a reset.
//
// Cheap, because the AssetManager caches by path - the second floor draws on
// exactly the models the first one loaded and reloads nothing.
//----------------------------------------------------------------------------------
void Game::Descend()
{
    // Counts itself one floor deeper, which everything below reads back
    level.Load(assets, (unsigned int)GetTime() ^ (unsigned int)time(nullptr));

    player.Spawn(level.SpawnPoint());
    camera.SnapTo(player.Position());

    projectiles.Clear();

    // And whatever they set off. An explosion left playing would finish in the new
    // level, at the coordinates of a room that is now solid rock.
    vfx.Clear();

    // A fresh pool for a deeper floor, and the way out of it shut again until this
    // one is emptied too
    ResetChaos(chaos, level.Depth());

    portal.Reset();
    portal.PlaceAt(level.PortalPoint());

    // A new map is a new place, and the horizon has to say so
    skyline.Place(level);

    loot.Clear();           // Last floor's uncollected gems belong to last floor
    pickups.Clear();        // Likewise its uncollected pickups
    treasure.Clear();       // And an unopened chest, if this floor even had one

    // New rooms, new objectives, new vendors. Both Places clear whatever the old
    // floor had, so nothing survives into a map where its room no longer exists.
    events.Place(level, level.Depth());
    vendors.Place(level);
    RerollVendorStock();

    // PopulateCamps empties the enemy list itself, so the old garrison goes with
    // the old map. Tiered against the level the player is on NOW - Level::Load has
    // already counted this as one floor deeper, so the new garrison rolls its ranks
    // about a higher centre than the one that just died.
    enemies.PopulateCamps(level, player.level);
    SeedRoomLoot();
    SeedPickups();

    // Whatever the player had explored was a map of a dungeon that no longer exists
    hud.ResetMap(level);

    TraceLog(LOG_INFO, "RUN: depth %i, %i chaos to quell, ranks about %i, %i events, %i vendors",
             level.Depth(), chaos.max, RankCentreForDepth(level.Depth()),
             events.Outstanding(), vendors.Count());
}

//----------------------------------------------------------------------------------
// Whatever screen is up, and whether one is.
//
// Returning true means the world does not tick this frame: no AI, no attacks, no
// doors, no projectiles, no chaos. That is what a pause IS here, and doing it as one
// early return rather than as a flag each system checks is what makes it impossible
// for a system added later to quietly keep running.
//
// The cursor follows the same rule from one place. A screen with buttons on it and a
// captured mouse is a screen the player can see and cannot touch, and a world with a
// visible cursor over it is one where looking around stops working - so every path
// that opens or closes a screen goes through ShowCursor rather than remembering.
//----------------------------------------------------------------------------------
bool Game::UpdateScreens()
{
    //------------------------------------------------------------------------------
    // Escape, and what it no longer does.
    //
    // raylib closes the window on Escape by default and Init has turned that off -
    // there is a character and a part-cleared floor to lose now, and a key that
    // throws both away without asking is not one anyone should hit by accident.
    //
    // It also backs OUT of the sheet rather than opening the menu on top of it,
    // because that is what every player will expect it to do and a menu stacked on a
    // menu is two Escapes to get back to the game.
    //
    // The settings overlay is skipped here entirely: it reads Escape itself, and one
    // key that closed both it and the menu under it would take the player two layers
    // out when they asked to go one.
    //------------------------------------------------------------------------------
    if (IsKeyPressed(KEY_ESCAPE) && !pauseOptions)
    {
        if (shop.IsOpen()) shop.Close();
        else if (sheet.IsOpen()) sheet.Close();
        else pause.Toggle();
    }

    // Tab opens the sheet from the game and from the menu alike, so the points the
    // HUD is nagging about are always one key away from being spent
    if (input.characterSheet)
    {
        if (pause.IsOpen()) pause.Close();
        if (shop.IsOpen()) shop.Close();

        // Goes with the menu it was opened over. A settings page left up over a
        // closed pause menu would be a page with nothing underneath it and no way
        // back to what opened it.
        pauseOptions = false;

        sheet.Toggle();
    }

    //------------------------------------------------------------------------------
    // The counter, opened and closed by the same key that opens a chest.
    //
    // One key for both directions rather than Escape to leave, because a player who
    // walked up to a vendor and pressed E is holding E in their head as the thing
    // that deals with the vendor. Escape still works - it works everywhere - but it
    // is the second answer rather than the only one.
    //
    // Closed by walking away as well. A shop that stayed open while the player was
    // no longer standing at it would be a page over a world they cannot see, and the
    // way out would be a key they are not looking for.
    //------------------------------------------------------------------------------
    if (shop.IsOpen())
    {
        if (input.interact || (vendors.At(player.Position()) != shop.Vendor())) shop.Close();
    }
    else if (input.interact && !sheet.IsOpen() && !pause.IsOpen())
    {
        const NpcKind here = vendors.At(player.Position());

        if (here != NpcKind::Count) shop.Open(here);
        else if (treasure.At(player.Position())) treasure.Open(player.Position(), arsenal);
    }

    //------------------------------------------------------------------------------
    // The pause menu, and the settings page it can put over itself.
    //
    // The options overlay is checked FIRST and swallows the frame when it is up -
    // see the note on Game::pauseOptions. Without that, one Escape would close the
    // settings and the menu underneath in the same frame, and every click meant for
    // a settings row would land on whatever pause entry happened to be behind it.
    //------------------------------------------------------------------------------
    if (!UpdatePauseOptions() && pause.IsOpen())
    {
        switch (pause.Update())
        {
            case PauseMenu::Choice::Resume:    pause.Close(); break;
            case PauseMenu::Choice::Character: pause.Close(); sheet.Toggle(); break;


            // Over the menu rather than instead of it - the menu is where Back
            // comes back to, so it stays open underneath
            case PauseMenu::Choice::Options:   pauseOptions = true; options.Show(); break;

            // The one place the game ends. Straight out - there is nothing to save
            // yet, and a confirmation on a menu entry that already says "to
            // desktop" is a second question about the same decision.
            case PauseMenu::Choice::Quit:      quitting = true; break;

            default: break;
        }
    }

    if (sheet.IsOpen()) sheet.Update(player, arsenal, spells, traits, viewModel);
    if (shop.IsOpen()) shop.Update(player, arsenal, spells, traits);

    // Both pages can change what the traits are granting - the sheet by equipping
    // one, the counter by selling one out from under a slot - so the sum is rebuilt
    // while either is up. Once a frame while a page is open is nothing; doing it
    // only on the frames that actually changed something would mean two classes
    // reporting when they had, and one of them forgetting.
    if (sheet.IsOpen() || shop.IsOpen()) RefreshModifiers();

    const bool paused = sheet.IsOpen() || pause.IsOpen() || shop.IsOpen();

    FreeCursor(paused);

    return paused;
}

//----------------------------------------------------------------------------------
// The trait loadout's own bonus, refreshed onto the character.
//
// Traits are the one source this rebuilds - Player::Combined() is where the real
// sum lives, adding this to whatever a running buff and the held gear are worth
// (see Player::ApplyBuff and RefreshLoadout). A weapon's bonus, and its forge
// levels, do NOT come through here: those are part of what the HAND is carrying,
// and they arrive through SetGearMods every frame from RefreshLoadout, which is
// where a bonus that goes away when you put the weapon down belongs.
//----------------------------------------------------------------------------------
void Game::RefreshModifiers()
{
    player.SetModifiers(traits.Bonus(player.level));
}

void Game::FreeCursor(bool free)
{
    if (free == cursorFree) return;

    cursorFree = free;

    if (free) EnableCursor();
    else DisableCursor();
}

//----------------------------------------------------------------------------------
// One frame, whichever screen owns it.
//
// The fader runs regardless of state - it is what is CARRYING the player from one
// state to the next - and EnterState is what actually performs that switch, on
// the one frame the screen is fully black behind it. See core/Fader.h and the
// AppState note in Game.h.
//----------------------------------------------------------------------------------
void Game::Update(float delta)
{
    input = InputState::Poll();
    fader.Update(delta);

    switch (appState)
    {
        case AppState::BootLoading: UpdateBootLoading(delta); break;
        case AppState::MainMenu:    UpdateMainMenu(delta);    break;
        case AppState::Options:     UpdateOptions(delta);     break;
        case AppState::Credits:     UpdateCredits(delta);     break;
        case AppState::RunLoading:  UpdateRunLoading(delta);  break;
        case AppState::InGame:      UpdateInGame(delta);      break;
    }

    if (fader.AtBlack() && (appState != pendingState)) EnterState(pendingState);
}

void Game::RequestAppState(AppState next)
{
    pendingState = next;
    fader.Start();
}

void Game::EnterState(AppState next)
{
    appState = next;

    switch (next)
    {
        case AppState::BootLoading:
        case AppState::RunLoading:
            loadingElapsed = 0.0f;
            runLoadStage = RunLoadStage::Enter;
            runLoadStep = 0;
            break;

        case AppState::MainMenu:
            mainMenu.Show();
            FreeCursor(true);
            break;

        case AppState::Options:
            options.Show();
            break;

        case AppState::Credits:
            credits.Show();
            break;

        case AppState::InGame:
            FreeCursor(false);
            break;
    }
}

void Game::UpdateBootLoading(float delta)
{
    loadingElapsed += delta;

    if (loadingElapsed >= Config::BootLoadingMinTime) RequestAppState(AppState::MainMenu);
}

void Game::UpdateMainMenu(float delta)
{
    (void)delta;

    switch (mainMenu.Update())
    {
        case MainMenu::Choice::Start:   RequestAppState(AppState::RunLoading); break;
        case MainMenu::Choice::Options: RequestAppState(AppState::Options);    break;
        case MainMenu::Choice::Credits: RequestAppState(AppState::Credits);    break;

        // Straight out, the same as every other Quit on these pages - there is
        // nothing to save from the front end.
        case MainMenu::Choice::Exit:    quitting = true; break;

        default: break;
    }
}

//----------------------------------------------------------------------------------
// What raylib is actually told, from the two things the player set.
//
// Mute wins and the volume is left alone underneath it - see the note on
// Game::muted. One function rather than the same two lines at each of the three
// controls, because "muted, but the slider still moved" is exactly the state a
// forgotten branch leaves behind.
//----------------------------------------------------------------------------------
void Game::ApplyVolume() const
{
    SetMasterVolume(muted ? 0.0f : masterVolume);
}

//----------------------------------------------------------------------------------
// The settings themselves, shared by the front-end page and the paused one.
//
// Everything except Back, which is the ONLY thing the two callers answer
// differently: one fades out to the main menu, the other just closes an overlay
// over a run that never stopped. Returns whether Back was what happened, so the
// caller decides its own way out rather than this having to know which page it is
// serving.
//----------------------------------------------------------------------------------
bool Game::HandleOptionsChoice(OptionsScreen::Choice choice)
{
    switch (choice)
    {
        case OptionsScreen::Choice::Back: return true;

        case OptionsScreen::Choice::ToggleFullscreen:
            ToggleBorderlessWindowed();
            fullscreenOn = !fullscreenOn;
            break;

        // Toggled rather than being the volume going to zero and back - the volume
        // is what the player chose and muting must not spend it
        case OptionsScreen::Choice::ToggleMute:
            muted = !muted;
            ApplyVolume();
            break;

        case OptionsScreen::Choice::VolumeDown:
            masterVolume = fmaxf(0.0f, masterVolume - 0.1f);
            ApplyVolume();
            break;

        case OptionsScreen::Choice::VolumeUp:
            masterVolume = fminf(1.0f, masterVolume + 0.1f);
            ApplyVolume();
            break;

        default: break;
    }

    return false;
}

// What the page is showing, from the settings it does not own - see OptionsView.
OptionsView Game::OptionsShown(const char *backLabel) const
{
    OptionsView view;

    view.fullscreenOn = fullscreenOn;
    view.muted = muted;
    view.volume = masterVolume;
    view.backLabel = backLabel;

    return view;
}

void Game::UpdateOptions(float delta)
{
    (void)delta;

    if (HandleOptionsChoice(options.Update())) RequestAppState(AppState::MainMenu);
}

//----------------------------------------------------------------------------------
// The same page over a paused run - see the note on Game::pauseOptions.
//
// Back closes this and LEAVES THE MENU STANDING rather than resuming: the player
// paused to do something, went to settings on the way, and dropping them back into
// the dungeon would be the game deciding they were finished.
//
// Returns whether this page OWNED the frame, which is not the same question as
// whether it is still up - and the difference was a real bug. The click that
// closes this page closes it on the same frame it is read, so reporting "no
// longer up" let the pause menu underneath read the very same click and fire
// whatever entry happened to be under the cursor: BACK sits exactly where QUIT
// does, so backing out of settings quit the game.
//
// One click is one control's, and the control that got it is the one that was on
// top when it happened.
//----------------------------------------------------------------------------------
bool Game::UpdatePauseOptions()
{
    if (!pauseOptions) return false;

    if (HandleOptionsChoice(options.Update())) pauseOptions = false;

    return true;
}

void Game::UpdateCredits(float delta)
{
    (void)delta;

    if (credits.Update() == CreditsScreen::Choice::Back) RequestAppState(AppState::MainMenu);
}

//----------------------------------------------------------------------------------
// The run loading screen, walked one step at a time.
//
// Every load here is GL-bound and so has to happen on this thread - there is no
// worker to hand it to. What there IS is more than one frame: the work is a list
// (see RunLoadStep) and taking one entry per Update leaves an ordinary Draw
// between each pair, which is what turns a frozen screen into a moving bar.
//
// Enter waits for the fade-in, Loading walks the list, Done holds the full bar for
// its own minimum time - the same reason BootLoading has one.
//----------------------------------------------------------------------------------
void Game::UpdateRunLoading(float delta)
{
    loadingElapsed += delta;

    switch (runLoadStage)
    {
        //--------------------------------------------------------------------------
        // Waiting for the page to actually be on screen.
        //
        // EnterState runs on the frame the fader is fully BLACK, so the loading
        // screen spends the whole fade-in invisible underneath it. Starting work
        // there would put the first and biggest step behind the black again, which
        // is the bug this whole arrangement exists to fix - so nothing loads until
        // the cut has finished and the player can see what they are waiting for.
        //--------------------------------------------------------------------------
        case RunLoadStage::Enter:
            if (fader.IsIdle()) runLoadStage = RunLoadStage::Loading;
            break;

        //--------------------------------------------------------------------------
        // One step a frame, so the page is drawn between every pair of them.
        //
        // Skipping the art on a second run is what `runAssetsLoaded` is for, and
        // the LAST step is exempt: the art is per launch, the floor is per run.
        //--------------------------------------------------------------------------
        case RunLoadStage::Loading:
            if (runAssetsLoaded && (runLoadStep < RunLoadTotal() - 1)) runLoadStep = RunLoadTotal() - 1;

            RunLoadStep(runLoadStep);

            runLoadStep++;

            if (runLoadStep >= RunLoadTotal())
            {
                runAssetsLoaded = true;
                runLoadStage = RunLoadStage::Done;
                loadingElapsed = 0.0f;
            }
            break;

        // A beat at a full bar, so the last step is something the player saw finish
        // rather than the frame the screen vanished on
        case RunLoadStage::Done:
            if (loadingElapsed >= Config::RunLoadingMinTime) RequestAppState(AppState::InGame);
            break;
    }
}

//----------------------------------------------------------------------------------
// How far through the run load we are, and what is happening right now.
//
// `runLoadStep` is the step ABOUT to run, which is why it doubles as the count of
// those already done - and why the label is the honest answer to "what is it doing"
// rather than to "what did it just finish".
//----------------------------------------------------------------------------------
float Game::RunLoadProgress() const
{
    return (float)runLoadStep/(float)RunLoadTotal();
}

const char *Game::RunLoadLabel() const
{
    if (runLoadStep < RunLoadFixed) return RunLoadLabels[runLoadStep];

    // Every slice of the dungeon kit says the same thing. What the player wants
    // from this line is which PART of the load is running, and "the stonework"
    // answers that for the whole range - naming the room kind being furnished
    // would be a line that changed six times a second and told them nothing.
    if (runLoadStep < RunLoadFixed + RunLoadArtSteps()) return "the stonework";
    if (runLoadStep < RunLoadTotal()) return "the floor itself";

    return "ready";
}

//----------------------------------------------------------------------------------
// Everything Update() did before there was a front end for it to sit behind - see
// the note on the declaration in Game.h.
//----------------------------------------------------------------------------------
void Game::UpdateInGame(float delta)
{
    // The camera is still falling. Nothing below this matters until the beat
    // runs out - see UpdateDying.
    if (runPhase == RunPhase::Dying)
    {
        UpdateDying(delta);
        return;
    }

    // The run is over. Nothing below this matters until Restart puts a fresh one
    // in play - see UpdateRunEnd.
    if (runPhase != RunPhase::Playing)
    {
        UpdateRunEnd();
        return;
    }

    if (UpdateScreens()) return;

    UpdateWorld(delta);

    // Checked after the world has moved, so a blow that emptied the pool this very
    // frame is what actually starts the fall - not a frame behind it. The cursor
    // stays captured through the fall - see UpdateDying for where it is freed.
    if (!player.IsAlive())
    {
        runPhase = RunPhase::Dying;
        deathTimer = 0.0f;
        camera.BeginDeathFall();
    }
}

//----------------------------------------------------------------------------------
// The camera keeling over, for a beat, before the run-end screen takes it - see
// Config::DeathFallToFadeDelay and its neighbours, and FpsCamera::UpdateDeathFall.
//----------------------------------------------------------------------------------
void Game::UpdateDying(float delta)
{
    deathTimer += delta;
    camera.UpdateDeathFall(deathTimer);

    if (deathTimer >= Config::DeathFallToFadeDelay + Config::DeathFadeDuration)
    {
        runPhase = RunPhase::Defeated;
        runEnd.Open(false, level.Depth(), player.level);
        FreeCursor(true);
    }
}

//----------------------------------------------------------------------------------
// The run-end page, while there is no run to update.
//
// The same shape as the pause/sheet/shop handling in UpdateScreens: read the
// choice, act on it, hand the cursor back or take it, once. Kept separate rather
// than folded into UpdateScreens because a run that has ended is not a pause - the
// world underneath is not merely stopped, there is no floor state left worth
// resuming into.
//----------------------------------------------------------------------------------
void Game::UpdateRunEnd()
{
    switch (runEnd.Update())
    {
        case RunEndScreen::Choice::Restart:
            runEnd.Close();
            StartNewRun();
            break;

        // Straight out, the same as the pause menu's Quit - there is nothing to
        // save, and a confirmation on a choice already reached by dying or winning
        // is a second question about the same decision.
        case RunEndScreen::Choice::Quit:
            quitting = true;
            break;

        default:
            break;
    }
}

void Game::UpdateWorld(float delta)
{
    if (input.toggleCombatDebug) combatDebug.Toggle();
    if (input.regenerateLevel) Descend();

    //------------------------------------------------------------------------------
    // The number keys, over the schools the player has actually bought.
    //
    // Not a debug binding any more - it is the spellbook, and the mystic is what
    // fills it. A key for a school the player has not bought does nothing at all
    // rather than selecting something that cannot be cast, and it does it silently:
    // the HUD already draws that school as an empty socket, which says why better
    // than a message would.
    //
    // Still guarded on the enum's length rather than trusted. Poll bounds it against
    // the same enum, and two places agreeing is not the same as one place deciding.
    //------------------------------------------------------------------------------
    if ((input.magic >= 0) && (input.magic < (int)Magic::Count) && spells.Owns((Magic)input.magic))
    {
        magic = (Magic)input.magic;
    }

    // Look first: the body turns toward where the player is looking this frame.
    // Suspended while a gizmo is up, so dragging it does not spin the view.
    if (!viewModelEditor.BlocksLook()) camera.ApplyLookInput(input.look);

    // Doors finish their swings before anything is resolved against them, so a
    // body never gets pushed by a slab that has already moved out of its way
    level.Update(delta);

    player.Update(delta, input, camera.Yaw());
    level.ResolveBody(player.body);

    camera.Update(delta, player.Position(), input.move, input.crouch, player.IsGrounded());

    // The camps stop sending anyone once the pool is empty - there is nothing left
    // for a new body to be worth
    enemies.Update(delta, level, player, projectiles, vfx, !chaos.quelled);
    level.ResolveBody(player.body);     // Enemies can shove; walls still win

    // Left button swings the right hand, right button the left. While a gizmo is
    // up the mouse belongs to the editor, so clicks must not also attack.
    RefreshLoadout();

    bool pressed[HandCount] = { false, false };
    bool held[HandCount] = { false, false };

    if (!viewModelEditor.IsActive())
    {
        pressed[(int)Hand::Right] = input.attack;
        held[(int)Hand::Right] = input.attackHeld;
        pressed[(int)Hand::Left] = input.altAttack;
        held[(int)Hand::Left] = input.altAttackHeld;
    }

    player.UpdateAttacks(delta, pressed, held, styles);

    // Feed the view model the blend the attacks produced, so what is drawn and
    // what was hit agree by construction. Ahead of the melee sweep, not behind it:
    // the sweep reads the pose the view model just settled on, and running it
    // afterwards would test last frame's picture against this frame's world.
    //------------------------------------------------------------------------------
    // The wheel, over the OWNED weapons.
    //
    // Decided here rather than inside the view model, because which weapons the
    // player has paid for is a fact about the run and the view model is a renderer.
    // It is handed the answer instead of the input - see ViewModel::SetSlot.
    //
    // An empty OFF hand is a slot in the cycle rather than an absence: a shield
    // wants a free hand and so does a spell, so stepping off the last owned
    // weapon lands on nothing held before coming round again. The main hand has
    // no such slot - see below.
    //------------------------------------------------------------------------------
    if (input.wheel != 0.0f)
    {
        const Hand hand = input.offhand ? Hand::Left : Hand::Right;
        const int step = (input.wheel > 0.0f) ? 1 : -1;

        // The main hand's cycle skips shields outright - see the note on
        // Arsenal::NextOwned's excludeTags and combat/Equip.h's EquipWeapon,
        // which enforces the same rule for the Inventory tab's buttons.
        // Skipping it here rather than just refusing it in EquipWeapon is what
        // keeps the wheel moving instead of sticking on a shield it will not
        // equip.
        const unsigned excludeTags = (hand == Hand::Right) ? TagBlocking : 0;

        int newIndex = arsenal.NextOwned(viewModel.SlotIndex(hand), step, excludeTags);

        // ...and never lands on empty either - see the note on EquipWeapon.
        // Landing on -1 means the walk stopped exactly at that slot in the
        // ring, so continuing from there carries on to whatever owned weapon
        // comes next instead of just refusing and leaving the wheel stuck.
        if ((hand == Hand::Right) && (newIndex < 0))
        {
            newIndex = arsenal.NextOwned(arsenal.Count(), step, excludeTags);
        }

        EquipWeapon(viewModel, arsenal, hand, newIndex);
    }

    ViewModelInput weaponInput;
    weaponInput.delta = delta;
    weaponInput.wheel = 0.0f;       // See above - the cycle is decided here now
    weaponInput.offhand = input.offhand;
    weaponInput.bobPhase = camera.BobPhase();
    weaponInput.walkAmount = camera.WalkAmount();

    for (int h = 0; h < HandCount; h++)
    {
        weaponInput.blend[h] = player.Attack((Hand)h).blend;
        weaponInput.attackStarted[h] = player.Attack((Hand)h).startedThisFrame;
    }

    viewModel.Update(weaponInput);

    // Whether anything the player did this frame - a sweeping blade or a shot
    // that lands - rolled a critical, for the camera shake at the end of this
    // function. One flag across both hands and every shot: a frame that crits
    // twice should not shake any harder than a frame that crits once.
    bool critLanded = false;

    // The blade sweeps from where it was to where it is, and damage lands wherever
    // along that path it passed through a body - so a swing that visibly misses
    // does miss. Every frame of the live window, not once at a fixed blend.
    for (int h = 0; h < HandCount; h++)
    {
        const Capsule blade = viewModel.BladeFor((Hand)h, camera.Get(), stats[h].reach,
                                                 stats[h].bladeRadius);

        // On the very first frame, and on the frame a swing begins, there is no
        // meaningful previous position: sweeping from a stale one would drag the
        // blade across half the room. Start the stroke where the weapon is.
        const bool fresh = !hadBlades || player.Attack((Hand)h).startedThisFrame;
        const Capsule from = fresh ? blade : blades[h];

        const MeleeResult melee = SweepMelee(from, blade, player.EyePosition(), stats[h],
                                             player.Fighting(), level, enemies.All(),
                                             player.Attack((Hand)h));

        combatDebug.NoteHit(melee.hits);

        if (melee.crit) critLanded = true;

        // What the weapon drank. Paid on what actually came off the bodies rather
        // than on what was swung for, so a stroke a guard ate returns nothing - and
        // rounded UP, because a weapon whose whole identity is sustain returning
        // zero off a small hit reads as the behaviour being broken rather than as
        // arithmetic.
        if ((stats[h].lifesteal > 0.0f) && (melee.damage > 0))
        {
            player.Heal((int)(melee.damage*stats[h].lifesteal + 0.999f));
        }

        // A door is not a body, so it is not in the list SweepMelee tests - but a
        // swing that passes through one should still knock it open.
        //
        // Gated on the same live window SweepMelee uses. Without that it fired on
        // every frame, and since the blade capsule is simply where the weapon is,
        // walking up to a door was enough to knock it open.
        if (MeleeIsLive(stats[h], player.Attack((Hand)h)))
        {
            level.StrikeDoors(from, blade);
        }

        // Cast and Throw put something in the air instead of cutting. The muzzle
        // is the same blade capsule's tip: a bolt leaves the end of the staff and
        // a knife leaves the point, because that is where the weapon is drawn
        // pointing - the same magnified pose the melee sweep tests with.
        AttackState &attack = player.Attack((Hand)h);

        if (stats[h].ranged && attack.shotPending &&
            (attack.phase == AttackState::Phase::Out) && (attack.blend >= stats[h].releaseAt))
        {
            attack.shotPending = false;     // A threshold, not an event: fire once

            //------------------------------------------------------------------
            // A cast is paid for. A throw is not.
            //
            // Checked HERE, at the release, rather than when the button went down:
            // the animation is already most of the way through by this point, and
            // refusing the swing at the start would mean a staff that sometimes
            // does not move at all. This way the staff always swings and the mote
            // simply does not leave it, which reads as fizzling rather than as a
            // dropped input.
            //
            // The empty-handed case cannot get here - an empty hand is neither
            // melee nor ranged (see RefreshLoadout) - so there is no school
            // without a staff to pay for.
            //------------------------------------------------------------------
            if (styles[h] == AttackStyle::Cast)
            {
                if (!spells.Owns(magic)) continue;
                if (!player.SpendMana(spells.CostOf(magic, player.Mods()))) continue;
            }

            // What it looks like in the air. A thrown weapon IS the weapon that
            // left the hand - a dagger that flies as an arrow is the wrong object
            // entirely - sized off its measured height so dagger_A and dagger_B
            // come out the same length however differently the two are modelled,
            // and keeping the exact orientation the hand was holding it in so the
            // handover is a continuation rather than a snap.
            //
            // A cast is a mote: no model at all, a ball of light in the school's
            // colour that becomes that school's impact effect where it lands. It
            // used to borrow the arrow, which was a placeholder and looked like one
            // - a wooden shaft leaving the end of a staff.
            ProjectileLook look;

            if (styles[h] == AttackStyle::Cast)
            {
                look.magic = &MagicAt(magic);

                // What the mystic's levels bought. On the SCHOOL's own figures and
                // never on the caster's, which is the rule that keeps the damage
                // curve linear against enemy health - see progress/Spellbook.h.
                //
                // Through impactScale, which already exists for the enemy casters
                // and does exactly this job. An empowered school bursting wider is
                // feedback that the purchase landed; it is not an area of effect,
                // because nothing in this game has one yet.
                look.impactScale = spells.SizeMult(magic);
            }

            if (styles[h] == AttackStyle::Throw)
            {
                const float height = viewModel.HeightFor((Hand)h);

                look.model = viewModel.ModelFor((Hand)h);
                look.scale = (height > 1e-3f) ? (Config::ThrownLength/height) : 1.0f;
                look.fixed = true;
                look.orientation = viewModel.WorldOrientation((Hand)h, camera.Get());

                // The hand is empty now, and has to visibly produce another one
                viewModel.NoteThrown((Hand)h);
            }

            // The weapon tip is up to two units from the eye and can be pointed
            // at a wall you are standing against or the floor under your feet, so
            // it is not automatically a legal place to put anything. Pulled back
            // along the line from the eye until it is - and the eye itself always
            // is, because ResolveBody keeps the body out of walls.
            const Vector3 muzzle = level.ClipSpawn(player.EyePosition(), blade.b);

            // A cast takes its speed and its damage from the school rather than
            // from the staff. The staff is how it is thrown; the magic is what is
            // thrown, and a lightning bolt does not slow down because it left a
            // heavier stick.
            //
            // The two halves are scaled by different stats and that is the point:
            // a thrown dagger grows with ARMS, and a mote grows with ARCANE, so
            // the same hand holding the same staff hits for wildly different
            // numbers depending on where the character's points went.
            const StatBlock fighting = player.Fighting();

            const float speed = look.magic ? look.magic->speed : stats[h].projectileSpeed;

            const int raw = look.magic
                          ? (int)(player.SpellPower()*look.magic->damageMult
                                  *spells.DamageMult(magic) + 0.5f)
                          : player.WeaponDamage(stats[h].damage);

            // Rolled as it leaves, not on impact. The shot carries a number rather
            // than a stat block, and one shot has to be one roll - a mote that
            // re-rolled every substep would crit eventually, always.
            //
            // SPARK never rolls at all - its signature effect (see combat/Magic.cpp)
            // is that every hit crits, which is what pays for it being the one
            // school with no burst at all: SKILL still decides how HARD it crits,
            // only whether it does is taken out of the player's hands.
            const bool crit = ((styles[h] == AttackStyle::Cast) && (magic == Magic::Spark))
                            || RollWeaponCrit(fighting, stats[h].critBonus);

            projectiles.Spawn(muzzle, AimDirectionFrom(muzzle), speed,
                              ResolveDamage(raw, fighting, crit),
                              ProjectileSide::AtEnemies, look, crit, stats[h].poiseScale);
        }

        blades[h] = blade;
    }

    hadBlades = true;
    combatDebug.NoteBlades(blades);

    // After both sides have fired, so anything released this frame takes its first
    // step now rather than hanging in the weapon until the next one
    if (projectiles.Update(delta, level, player, enemies.All(), vfx)) critLanded = true;

    // After the shots, so an impact set off this frame shows its first frame now
    // rather than a frame behind whatever caused it
    vfx.Update(delta);

    //------------------------------------------------------------------------------
    // Pay for the dead, and take the same figure off the floor.
    //
    // Before RemoveDead and after everything that can deal damage: this is the one
    // point in the frame where every kill has happened and no corpse has been swept
    // up yet.
    //------------------------------------------------------------------------------
    //------------------------------------------------------------------------------
    // Coins, gems and mana, then experience and chaos.
    //
    // In that order, and it matters: both passes read the same `expPending` flag and
    // CollectExp is what clears it, so a payout that ran second would find every
    // body already settled and pay nothing.
    //------------------------------------------------------------------------------
    enemies.PayLoot(player, loot);

    if (Quell(chaos, enemies.CollectExp(player)))
    {
        TraceLog(LOG_INFO, "RUN: depth %i quelled, %i events outstanding",
                 level.Depth(), events.Outstanding());
    }

    //------------------------------------------------------------------------------
    // The objectives.
    //
    // After the kills are collected, so a hunt sees the body that died this frame as
    // dead - and before RemoveDead, so the bodies it is counting are all still in
    // the list it is counting them in.
    //------------------------------------------------------------------------------
    events.Update(delta, level, player, enemies, vfx, loot, chaos, level.Depth());

    vendors.Update(delta);
    loot.Update(delta);
    pickups.Update(delta);
    treasure.Update(delta);

    //------------------------------------------------------------------------------
    // Anything the player is standing on goes into the purse.
    //
    // Walked over rather than picked up with a key. A currency the player has to
    // press a button for is a currency they walk past, and there is nothing to decide
    // about a gem - taking it is never the wrong answer.
    //------------------------------------------------------------------------------
    int taken[(int)Currency::Count] = { 0 };

    if (loot.Collect(player.Position(), player.purse, taken))
    {
        for (int i = 0; i < (int)Currency::Count; ++i)
        {
            if (taken[i] <= 0) continue;

            TraceLog(LOG_INFO, "LOOT: +%i %s", taken[i], CurrencyName((Currency)i));
        }
    }

    // Health, mana and a buff, taken the same way - walked over, nothing to
    // decide. See PickupManager::Collect for why this pays the player directly
    // rather than handing anything back for the log line above to report.
    pickups.Collect(player.Position(), player);

    // Whichever of the pool and the events finished second is what clears the floor,
    // so this is asked every frame rather than at the moment either one changes
    if (TryClear(chaos, events.BlockingClear()))
    {
        TraceLog(LOG_INFO, "RUN: depth %i cleared - the way down is open", level.Depth());
    }

    enemies.RemoveDead();
    combatDebug.Update(delta);

    //------------------------------------------------------------------------------
    // The camera shake, once everything this frame that could have landed a blow
    // either way has had its say - the player's own melee and shots above, and
    // whatever enemies.Update and events.Update did to the player before this
    // point in the frame.
    //------------------------------------------------------------------------------
    if (critLanded) camera.Shake(Config::CameraShakeOnCrit);

    if (player.hitPending)
    {
        camera.Shake(Config::CameraShakeOnHit);
        player.hitPending = false;
    }

    //------------------------------------------------------------------------------
    // The way down.
    //
    // Raised every frame the floor is cleared rather than once on the frame it
    // clears: Portal::Open is idempotent, and asking it repeatedly means there is
    // no single frame that has to fire correctly for the floor to have an exit.
    //
    // The player walks INTO it and stands there. That is the difference between
    // this and waiting in front of a door, and it is worth the dwell being long
    // enough to step back out of - going down should be something they do, not
    // something that happens to them for standing in the wrong room.
    //------------------------------------------------------------------------------
    if (chaos.cleared) portal.Open();

    portal.Update(delta);

    inPortal = portal.Contains(player.Position());

    if (UpdateChaos(chaos, delta, inPortal))
    {
        AdvanceFloor();

        // Everything below this read the floor that no longer exists, or there is
        // no floor at all any more - either way, nothing below this frame's return
        // has anything left to act on
        return;
    }

    // Last, so the fog lifts around where the player actually ended the frame -
    // after the walls and the enemies have both had their say about that
    hud.Update(level, player);

    viewModelEditor.Update(viewModel, camera.Get());

    // TODO: pickups.Update(delta, player);
}

//----------------------------------------------------------------------------------
// One frame, whichever screen owns it - the Draw half of the dispatch in Update.
//
// Every front-end page is a plain 2D draw with nothing loaded for it to depend
// on, so each gets its own small BeginDrawing/EndDrawing pair; InGame is
// everything Draw() did before any of this existed, moved into DrawInGame
// unchanged bar the death fade. The fader draws last in every case - it is
// what is ON TOP of whichever screen this frame actually showed.
//----------------------------------------------------------------------------------
void Game::Draw()
{
    switch (appState)
    {
        case AppState::BootLoading:
            BeginDrawing();
                ClearBackground(BLACK);
                loadingScreen.Draw("LOADING", loadingElapsed);
                fader.Draw();
            EndDrawing();
            break;

        case AppState::MainMenu:
            BeginDrawing();
                ClearBackground(BLACK);
                mainMenu.Draw();
                fader.Draw();
            EndDrawing();
            break;

        case AppState::Options:
            BeginDrawing();
                ClearBackground(BLACK);
                options.Draw(OptionsShown("BACK TO MENU"));
                fader.Draw();
            EndDrawing();
            break;

        case AppState::Credits:
            BeginDrawing();
                ClearBackground(BLACK);
                credits.Draw();
                fader.Draw();
            EndDrawing();
            break;

        case AppState::RunLoading:
            BeginDrawing();
                ClearBackground(BLACK);
                loadingScreen.Draw("ENTERING THE DUNGEON", loadingElapsed,
                                   RunLoadProgress(), RunLoadLabel());
                fader.Draw();
            EndDrawing();
            break;

        case AppState::InGame:
            DrawInGame();
            break;
    }
}

void Game::DrawInGame()
{
    // The held weapons go into their own target first, with their own depth
    // buffer, so nothing in the world can clip through them
    viewModel.BeginPass(camera.Get());
        viewModel.Draw(camera.Get());
        viewModelEditor.Draw(viewModel, camera.Get());
    viewModel.EndPass();

    BeginDrawing();

        ClearBackground(Color{ Config::Background[0], Config::Background[1],
                               Config::Background[2], 255 });

        BeginMode3D(camera.Get());

            // First, and writing no depth, so everything below simply covers it
            sky.Draw();

            // After the sky, which writes no depth, and before the level - the town
            // is ordinary solid geometry and the walls have to be able to hide it
            skyline.Draw();

            level.Draw();
            enemies.Draw(camera.Get());
            projectiles.Draw(camera.Get());

            // Before the effects and after the world: both are additive like they
            // are, and both have to be behind anything that goes off in front of them
            portal.Draw(camera.Get());
            events.Draw(camera.Get());
            vendors.Draw(camera.Get());
            loot.Draw(camera.Get());
            pickups.Draw(camera.Get());
            treasure.Draw(camera.Get());

            // Last of the world, and the only part of it that is additive: an
            // impact goes over the body it went off on rather than being sorted
            // against it
            vfx.Draw(camera.Get());
            combatDebug.Draw(player, stats, level, enemies);

        EndMode3D();

        viewModel.Composite();  // Over the finished world

        viewModelEditor.DrawUi(viewModel);
        combatDebug.DrawUi(player, enemies);

        // Their names, over their columns. Screen space, so it cannot live inside
        // BeginMode3D with the columns themselves - and before the HUD, so a vendor
        // standing behind the crosshair does not draw over it.
        vendors.DrawLabels(camera.Get());

        hud.Draw(player, viewModel, level, magic, chaos, events, spells, vendors,
                 treasure, enemies, camera.Get(), inPortal);

        // The last of the dwell spent going dark, so the next floor arrives out of
        // black rather than as a cut. Over the HUD as well as the world - the whole
        // picture is leaving, not just the part of it made of walls.
        if (inPortal && (chaos.dwell > Config::PortalDwell - Config::PortalFade))
        {
            const float into = (chaos.dwell - (Config::PortalDwell - Config::PortalFade))
                             / Config::PortalFade;

            UiFadeOverlay(into);
        }

        // The camera keeling over, going dark for the back half of the beat - see
        // UpdateDying and Config::DeathFallToFadeDelay. Same idea as the portal
        // fade just above: the last of a moment spent going dark rather than
        // cutting out of it.
        if ((runPhase == RunPhase::Dying) && (deathTimer > Config::DeathFallToFadeDelay))
        {
            const float into = (deathTimer - Config::DeathFallToFadeDelay)/Config::DeathFadeDuration;

            UiFadeOverlay(fminf(into, 1.0f));
        }

        // Over everything, because both are modal - see the notes in
        // CharacterSheet.h and PauseMenu.h. The sheet last: it is what the menu
        // opens, so it has to sit on top of the menu that opened it.
        pause.Draw();

        // Over the menu that opened it, for the reason the sheet is drawn over the
        // menu too - see the note on Game::pauseOptions
        if (pauseOptions) options.Draw(OptionsShown("BACK"));

        shop.Draw(player, arsenal, spells, traits, weaponPreview);
        sheet.Draw(player, arsenal, spells, traits, weaponPreview, viewModel);

        // Over all of them: once a run has ended none of the pages above it are
        // reachable any more (UpdateRunEnd shortcuts past UpdateScreens entirely),
        // so this is the one page actually on top when it is up.
        runEnd.Draw();

        // Last of all - see the note on Game::Draw. Only visible while a
        // transition is actually in flight (RunLoading fading into InGame); zero
        // alpha the rest of the time.
        fader.Draw();

    EndDrawing();
}

void Game::Shutdown()
{
    UnloadUiFont();

    level.Unload();
    sky.Unload();
    viewModel.Unload();
    weaponPreview.Unload();
    enemies.Unload();
    assets.UnloadAll();     // Must happen while the GL context is still alive

    CloseAudioDevice();
    CloseWindow();
}
