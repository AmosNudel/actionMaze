#pragma once

#include "combat/Magic.h"
#include "combat/Projectile.h"
#include "combat/Weapon.h"
#include "core/Fader.h"
#include "core/Input.h"
#include "debug/CombatDebug.h"
#include "debug/ViewModelEditor.h"
#include "entities/EnemyManager.h"
#include "entities/Player.h"
#include "progress/Arsenal.h"
#include "progress/Spellbook.h"
#include "progress/Traits.h"
#include "render/AssetManager.h"
#include "render/Sky.h"
#include "render/FpsCamera.h"
#include "render/ViewModel.h"
#include "render/Portal.h"
#include "render/Vfx.h"
#include "render/WeaponPreview.h"
#include "ui/CharacterSheet.h"
#include "ui/CreditsScreen.h"
#include "ui/Hud.h"
#include "ui/LoadingScreen.h"
#include "ui/MainMenu.h"
#include "ui/OptionsScreen.h"
#include "ui/PauseMenu.h"
#include "ui/RunEndScreen.h"
#include "ui/ShopScreen.h"
#include "world/Chaos.h"
#include "world/Event.h"
#include "world/Level.h"
#include "world/Skyline.h"
#include "world/Loot.h"
#include "world/Pickup.h"
#include "world/Treasure.h"
#include "world/Vendors.h"

//----------------------------------------------------------------------------------
// Owns every subsystem and drives the frame: poll -> update -> draw.
//
// This is the composition root, and the only place that sees both sides: it reads
// which weapon the view model is showing, looks up what that weapon does, and
// hands the result to the player. Neither gameplay nor rendering has to include
// the other.
//
// New systems (projectiles, pickups) become members here and get their slot in
// Update()/Draw(); nothing else in the codebase has to know about them.
//----------------------------------------------------------------------------------
class Game
{
public:
    void Run();

private:
    void Init();
    void Update(float delta);
    void Draw();
    void Shutdown();

    // Everything Draw() did before there was a front end for it to sit behind -
    // see the note on UpdateInGame. Only reached while appState == InGame.
    void DrawInGame();

    //------------------------------------------------------------------------------
    // Which screen owns the frame, before there is even a run to be in.
    //
    // BootLoading and RunLoading both exist for the same reason: the gameplay art
    // is a second of blocking, GL-bound loading, and a loading screen has to
    // actually be on screen - drawn and presented - before any of it runs.
    // RunLoading walks that work one step per frame so the page is drawn between
    // every pair of them; see UpdateRunLoading. MainMenu, Options and Credits are
    // the pages reachable from the front door; InGame is everything Update/Draw
    // did before any of this existed, moved into UpdateInGame without changing
    // shape.
    //------------------------------------------------------------------------------
    enum class AppState { BootLoading, MainMenu, Options, Credits, RunLoading, InGame };

    // What RunLoading is doing right now - see the note on UpdateRunLoading for
    // why loading is split across states rather than happening the instant
    // RunLoading is entered.
    enum class RunLoadStage { Enter, Loading, Done };

    // Sets `pendingState` and starts the fader; the actual switch happens once the
    // fade reaches black - see EnterState and the note on core/Fader.h.
    void RequestAppState(AppState next);

    // Called the one frame the fader is fully black, with `next` about to become
    // the visible state. Where each state's arrival work lives - showing a page,
    // taking or freeing the cursor, resetting a stage - so Update's dispatch
    // doesn't have to know any of it.
    void EnterState(AppState next);

    void UpdateBootLoading(float delta);
    void UpdateMainMenu(float delta);
    void UpdateOptions(float delta);

    // The same page over a paused run - see the note on `pauseOptions`. Returns
    // whether it OWNED this frame - not whether it is still up - which is what
    // stops the pause menu underneath reading the same click. See the definition.
    bool UpdatePauseOptions();

    // Everything the options page can report except Back, which the two callers
    // answer differently. Returns whether Back was what happened.
    bool HandleOptionsChoice(OptionsScreen::Choice choice);

    // What the page draws, filled in from the settings Game owns on its behalf.
    // `backLabel` is the caller's own word for its way out - see OptionsView.
    OptionsView OptionsShown(const char *backLabel) const;
    void UpdateCredits(float delta);
    void UpdateRunLoading(float delta);

    // One step of the run load - see the label table beside the definition. Steps
    // run one a frame so the loading screen is drawn between them.
    void RunLoadStep(int index);

    // What the loading screen shows: how much is done, and what is being loaded
    // right now. Both derived from `runLoadStep` alone.
    float RunLoadProgress() const;
    const char *RunLoadLabel() const;

    // Everything Update() did before the front end existed: the run-phase switch,
    // UpdateScreens, UpdateWorld, the death check. Only reached while
    // appState == InGame.
    void UpdateInGame(float delta);

    // The beat between death and the run-end screen - see UpdateInGame's death
    // check and Config::DeathFallToFadeDelay.
    void UpdateDying(float delta);

    //------------------------------------------------------------------------------
    // What the equipped weapons do this frame, read back from the view model
    void RefreshLoadout();

    //------------------------------------------------------------------------------
    // A whole fresh run: a new character at depth 1, starting kit only.
    //
    // What Init calls once at startup, and what a Restart choice on the run-end
    // screen calls again - the two are the same operation, which is what lets this
    // be the only place either of them has to be written. See the note on the
    // definition for how it differs from Descend.
    //------------------------------------------------------------------------------
    void StartNewRun();

    // A new floor from a new seed, and everything that stands on it rebuilt to
    // match. What the portal calls, and what F6 calls. See the note on the
    // definition.
    void Descend();

    //------------------------------------------------------------------------------
    // What finishing a floor actually does: one floor deeper, unless this was the
    // last one, in which case the run ends in victory instead.
    //
    // Reached by walking the portal's dwell out, which is now the only way down -
    // see the note on the definition.
    //------------------------------------------------------------------------------
    void AdvanceFloor();

    // The world, one frame. Split out of Update so the pause paths can skip it
    // wholesale rather than each system having to be told to hold still.
    void UpdateWorld(float delta);

    // Whatever screen is up, one frame. Returns whether anything is up at all,
    // which is the same question as "is the world paused".
    bool UpdateScreens();

    // The run-end page, while the run is over. Reads its choice and acts on it -
    // Restart calls StartNewRun, Quit sets the same flag the pause menu's Quit does.
    void UpdateRunEnd();

    // Rebuilds Player::mods from everything granting one. Called whenever a source
    // changes - which today means whenever the character page or the captain's
    // counter has been open, since neither is worth tracking more finely than that.
    void RefreshModifiers();

    // The starting kit, and the tables the shops sell out of. Once, after the view
    // model has loaded - the arsenal is sized to its weapon list.
    void ResetProgression();

    // Rerolls every vendor's limited stock. Called once per floor, from
    // StartNewRun and from Descend - see Config::MerchantStockPerFloor and its
    // neighbours.
    void RerollVendorStock();

    //------------------------------------------------------------------------------
    // A guaranteed find in every room a camp, a vendor and an event all passed
    // over.
    //
    // Called once per floor, after all three have placed themselves, so this is
    // the one pass that actually knows what is left unclaimed. See the note on the
    // definition for why a room can otherwise end up holding nothing but its own
    // furniture.
    //------------------------------------------------------------------------------
    void SeedRoomLoot();

    // Health, mana and a buff, scattered a few to a floor - see world/Pickup.h
    // and Config::PickupHealthPerFloor and its neighbours. Called alongside
    // SeedRoomLoot rather than folded into it: a gem is a consolation for a
    // room with nothing else in it, and a pickup is seeded independently of
    // whether the room already holds a vendor, an event or a camp.
    void SeedPickups();

    // Free the mouse, or take it back. Tracked rather than set every frame: raylib
    // talks to the window manager on every call, and one place deciding it is also
    // what stops a screen from forgetting to hand the cursor back.
    void FreeCursor(bool free);

    //------------------------------------------------------------------------------
    // Which way a shot leaving `muzzle` should travel.
    //
    // The muzzle is the tip of the held weapon - off to one side of the eye and
    // below it - while the crosshair is the camera's own forward axis. Those are
    // two different rays, so "forward" from the muzzle and "forward" from the eye
    // do not point at the same thing, and the gap is what makes a shot land
    // beside whatever you aimed at.
    //
    // Need not be normalised: ProjectileManager::Spawn scales whatever it is given.
    //------------------------------------------------------------------------------
    Vector3 AimDirectionFrom(Vector3 muzzle) const;

    AssetManager assets;
    Level level;
    Player player;
    EnemyManager enemies;
    // Owned here rather than by EnemyManager: a shot outlives the enemy that fired
    // it, and RemoveDead would otherwise take the arrow with the archer
    ProjectileManager projectiles;
    // Shared, not owned by whatever set an effect off: an impact outlives the shot
    // that caused it and a death outlives the body, so a pool held by either would
    // be torn down mid-animation
    VfxManager vfx;
    // The way down. Placed by the level and raised by the chaos pool emptying, so
    // it is drawn here rather than by Level: the level knows where it goes and
    // nothing about whether it is open.
    Portal portal;
    // How much of this floor is left. Owned here because it is run state rather
    // than map state - the map is thrown away and rebuilt, and this is reset
    // alongside it rather than by it.
    ChaosState chaos;
    // The rooms that are not just rooms. Owned here rather than by the Level for the
    // same reason the chaos pool is: the level is geometry, and an objective is
    // something happening on it.
    EventManager events;

    //------------------------------------------------------------------------------
    // The three vendors on this floor, and the loot lying on it.
    //
    // Both are floor state and both are rebuilt by Descend, for the same reason the
    // events are: a vendor is standing in a room that no longer exists, and a gem
    // nobody picked up belongs to the floor it was dropped on.
    //------------------------------------------------------------------------------
    VendorManager vendors;
    LootManager loot;

    // Health, mana and buff pickups - floor state for the same reason the loot
    // is: seeded fresh by SeedPickups every time Descend rebuilds the map, and
    // thrown away with everything else the old floor was holding.
    PickupManager pickups;

    // A rare chest, holding a weapon rather than a currency - see the class note
    // on TreasureManager. Floor state for the same reason as the two above:
    // seeded by SeedRoomLoot alongside the Vault's own coin piles, and gone with
    // the rest of the floor at the next Descend.
    TreasureManager treasure;

    //------------------------------------------------------------------------------
    // What the player has bought, across the whole run.
    //
    // NOT floor state. These survive the portal - they are the run's progression, and
    // a weapon bought on floor two that vanished on floor three would make every
    // purchase a rental.
    //
    // They live here rather than on the Player because the Player is the body and the
    // stat line, and these are three tables the shop writes and the loadout reads.
    // Keeping them apart is what lets Player stay ignorant of the view model's weapon
    // indices, which is the one thing an Arsenal is made of.
    //------------------------------------------------------------------------------
    Arsenal arsenal;
    Spellbook spells;
    TraitLoadout traits;

    Sky sky;

    // The town outside the walls - see world/Skyline.h. Beside the sky rather than
    // beside the level, because that is what it is: something on the horizon that
    // the floor is seen against, and nothing the floor itself can touch.
    Skyline skyline;
    FpsCamera camera;
    Hud hud;
    CharacterSheet sheet;
    PauseMenu pause;
    ShopScreen shop;
    RunEndScreen runEnd;
    ViewModel viewModel;
    ViewModelEditor viewModelEditor;

    // The front end - see the AppState note above
    Fader fader;
    MainMenu mainMenu;
    OptionsScreen options;
    CreditsScreen credits;
    LoadingScreen loadingScreen;

    // What the shop and character pages draw instead of a weapon's name - see
    // render/WeaponPreview.h. Owned here rather than by either page: both read
    // from it, and a render target per page would be twice the GPU memory for a
    // picture that is never on screen twice at once.
    WeaponPreview weaponPreview;
    CombatDebug combatDebug;

    InputState input;
    AttackStyle styles[HandCount] = { AttackStyle::Swing, AttackStyle::Swing };
    WeaponStats stats[HandCount];

    // Where each hand's blade was last frame. The sweep needs both ends of the
    // frame, and only the composition root sees the view model that builds them
    // and the combat that consumes them.
    Capsule blades[HandCount];
    bool hadBlades = false;     // False on the first frame, where there is no "last"

    // Which school the player's casts are made of. One selection for both hands -
    // a staff in each hand casts the same magic, because the alternative is a
    // per-hand debug binding nobody asked for.
    //
    // Not on the Player: a school is a property of the LOADOUT, and the loadout is
    // read back from the view model here every frame. When there is a real spellbook
    // this is what it will replace.
    Magic magic = Magic::Flame;

    // Whether the player is standing in the way down. Worked out in the update and
    // read by the draw, so the HUD's prompt and the fade are answering the same
    // frame's question rather than testing it twice.
    bool inPortal = false;

    // Whether the mouse is currently the player's or the game's - see FreeCursor.
    //
    // Starts TRUE because that is the state a window is created in: the cursor is
    // free until something takes it, and Init's FreeCursor(false) is what takes it.
    // Starting this false would make that call a no-op and leave the mouse loose
    // over a game that is trying to use it to look around.
    bool cursorFree = true;

    // The pause menu asked to quit. Read by Run, which is the only thing that can
    // actually end the loop - CloseWindow from inside Update would tear the context
    // down under the Draw that is about to run.
    bool quitting = false;

    //------------------------------------------------------------------------------
    // Whether there is a run in progress, and how it ended if not.
    //
    // Dying the frame Player::IsAlive() goes false - the camera keels over on its
    // own for a beat before the screen goes black, see UpdateDying - and only then
    // Defeated. Victorious the frame the player steps into the portal on
    // Config::VictoryDepth with it cleared, instead of the ordinary Descend.
    // Either way the world stops ticking - see UpdateRunEnd - and the only way
    // back to Playing is StartNewRun.
    //------------------------------------------------------------------------------
    enum class RunPhase { Playing, Dying, Defeated, Victorious };
    RunPhase runPhase = RunPhase::Playing;

    // How long the current Dying beat has been running - see UpdateDying.
    float deathTimer = 0.0f;

    //------------------------------------------------------------------------------
    // Which screen is up, and where the fader is taking it.
    //
    // `pendingState` is only meaningful while the fader is mid-transition: it is
    // what EnterState switches `appState` to the moment the screen goes black.
    // Equal to `appState` the rest of the time, which is also how the fade-driven
    // switch in Update tells "nothing to do" from "arrived".
    //------------------------------------------------------------------------------
    AppState appState = AppState::BootLoading;
    AppState pendingState = AppState::BootLoading;

    // Where RunLoading is in its own sequence - see UpdateRunLoading.
    RunLoadStage runLoadStage = RunLoadStage::Enter;

    // Which load step runs next, and therefore how many have finished. Reset by
    // EnterState; drives both the bar and the line under it.
    int runLoadStep = 0;

    // How long the current loading screen (boot or run) has been up - drives its
    // minimum visible time and the animated dots on it. Reset by EnterState.
    float loadingElapsed = 0.0f;

    // Whether the ART steps of the run load have run yet. False only until the
    // first Start Game press; the last step - building the floor - runs every time
    // regardless. See UpdateRunLoading.
    bool runAssetsLoaded = false;

    // The runtime settings Options controls. Session-only - nothing here is
    // written to disk, matching that nothing else in the project persists state
    // between launches either.
    bool fullscreenOn = Config::Fullscreen;
    float masterVolume = 1.0f;

    //------------------------------------------------------------------------------
    // Mute, kept apart from the volume rather than folded into it - see the note on
    // the mute row in OptionsScreen.cpp. `masterVolume` is what the player chose and
    // never moves when they mute; what raylib is actually told is the two combined,
    // which is the one line in ApplyVolume.
    //------------------------------------------------------------------------------
    bool muted = false;

    // Pushes `masterVolume` and `muted` at raylib. One place rather than at each of
    // the three controls that can change either, so a new way to change them cannot
    // forget half the answer.
    void ApplyVolume() const;

    //------------------------------------------------------------------------------
    // The options page is up OVER the paused run, rather than as an AppState.
    //
    // A state would mean leaving InGame and coming back to it, which is a fade, a
    // cursor hand-off and a run that has to be preserved across both - all to show
    // three rows over a world that is already stopped. The pause menu is drawn the
    // same way and for the same reason, and this sits one layer above it: the menu
    // opened it, so it goes on top of the menu, exactly as the character sheet does.
    //
    // Only ever true while `pause` is open. Back closes this and leaves the menu
    // standing, which is where the player was when they asked for it.
    //------------------------------------------------------------------------------
    bool pauseOptions = false;
};
