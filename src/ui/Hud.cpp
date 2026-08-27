#include "ui/Hud.h"

#include "core/Config.h"
#include "entities/Player.h"
#include "raymath.h"
#include "raylib.h"
#include "progress/Spellbook.h"
#include "render/AssetManager.h"
#include "render/ViewModel.h"
#include "ui/UiText.h"
#include "ui/UiTheme.h"
#include "world/Level.h"
#include "world/Loot.h"

namespace
{
    //------------------------------------------------------------------------------
    // Everything here is in DESIGN pixels - see the note at the top of Hud.h. A
    // figure written here is multiplied by UiScale() before it reaches the screen,
    // so these are proportions of a 720-tall reference window rather than sizes.
    //------------------------------------------------------------------------------
    constexpr float Margin  = 10.0f;    // The same gutter the minimap sits in
    constexpr float Gap     = 5.0f;     // Between two things in the same stack

    //------------------------------------------------------------------------------
    // How large the bar art is drawn, on top of the UI scale.
    //
    // The frame is 118x13 in its own pixels - drawn for a phone, and tiny. 1.7 is
    // where the ornamented end caps stop being a smudge and start being metalwork,
    // and is the multiple the mobile game draws them at.
    //------------------------------------------------------------------------------
    constexpr float BarArtScale = 1.7f;

    // What a bar falls back to when the art is missing, in design pixels. Close to
    // the frame's own height, so a build without assets/ui lays out the same.
    constexpr float PlainBarHeight = 20.0f;

    // The health bar's own width. Fixed rather than fitted, because it is the one
    // bar read under pressure and a readout that changes width with the window is
    // one the eye has to re-find. 118 is the art's natural width; at 1.7 it comes
    // out just wider than the minimap above it, which is what ties the column
    // together.
    constexpr float HealthBarWidth = 118.0f*BarArtScale;

    //------------------------------------------------------------------------------
    // How wide the floor's two bars may get.
    //
    // They start at the map's right edge and take what is left, which on a 16:9
    // window is about right and on a 21:9 one is a bar three thousand pixels long -
    // a rule across the top of the screen rather than a readout. In the mobile game
    // the same bar is bounded by the buttons in the top-right corner; there are no
    // buttons here, so the bound is written down instead.
    //------------------------------------------------------------------------------
    constexpr float FloorBarMaxWidth = 620.0f;

    constexpr float LabelSize = 14.0f;
    constexpr float BriefSize = 20.0f;
    constexpr float TitleSize = 38.0f;

    constexpr Color Ink   = UiInk;
    constexpr Color Dim   = UiDim;
    constexpr Color Ready = UiReady;

    // One hue per bar, and none of them shared with anything else on screen. A bar
    // whose colour appears twice is a bar that can be misread as the other thing.
    // These are the FALLBACK fills - the art carries its own.
    constexpr Color HealthFill = { 200,  60,  70, 255 };
    constexpr Color ExpFill    = { 120, 170, 245, 255 };
    constexpr Color ChaosFill  = { 170, 110, 220, 255 };

    // Mana. Cold blue rather than the magic palette's violet, because violet is the
    // chaos bar's and a pool that shared a hue with the floor's own readout could be
    // misread as it - which is the one rule the bar colours follow.
    constexpr Color ManaFill   = { 110, 165, 240, 255 };

    // Objectives outstanding. Warm and pale rather than any one event colour - it is
    // a count of several things that may be four different kinds, and borrowing one
    // kind hue for the total would be a lie about which.
    constexpr Color EventPending = { 255, 195, 110, 255 };
}

void Hud::Load(AssetManager &assets)
{
    bars.Load(assets);
}

void Hud::ResetMap(const Level &level)
{
    minimap.Reset(level);
}

void Hud::Update(const Level &level, const Player &player)
{
    minimap.Update(level, player.Position());
}

float Hud::BarPixels() const
{
    const float ui = UiScale();

    return bars.Ready() ? UiBars::Height(BarArtScale*ui) : PlainBarHeight*ui;
}

void Hud::Draw(const Player &player, const ViewModel &viewModel, const Level &level,
               Magic magic, const ChaosState &chaos, const EventManager &events,
               const Spellbook &spells, const VendorManager &vendors,
               const EnemyManager &enemies, const Camera3D &camera, bool inPortal) const
{
    (void)viewModel;    // Its tuning readout lives in the ViewModelEditor's own box

    minimap.Draw(level, events, vendors, player.Position(), player.Yaw());

    DrawCrosshair();
    DrawHurtIndicator(player, camera);

    DrawLeftColumn(player, level, magic, spells);
    DrawProgress(player);

    DrawChaos(chaos, events);
    DrawEvent(events);

    DrawVendorPrompt(player, vendors);

    // Over the world and under the pages. A champion's bar is about something the
    // player is looking at, so it belongs with the crosshair rather than in a corner.
    DrawChampionBars(enemies, camera);

    // Last of the ordinary HUD, because it is the only part that is ever allowed to
    // take the middle of the screen
    DrawFloorState(chaos, events, inPortal);
}

void Hud::DrawBar(BarHue hue, float x, float y, float width, float fill, Color colour) const
{
    if (fill < 0.0f) fill = 0.0f;
    if (fill > 1.0f) fill = 1.0f;

    const float ui = UiScale();

    if (bars.Ready())
    {
        // Only the event bar is tinted. The other three carry their hue in the art
        // itself, and a tint on top of that would be a second opinion about what
        // colour health is.
        const Color tint = (hue == BarHue::Event) ? colour : WHITE;

        bars.Draw(hue, fill, x, y, width, BarArtScale*ui, tint);

        return;
    }

    // No art. A track, a fill, and a hairline rather than a border - the bar has to
    // read as an edge against both a lit room and a dark one, and anything thicker
    // starts competing with the fill it is meant to contain.
    const float height = PlainBarHeight*ui;

    DrawRectangleRec({ x, y, width, height }, Fade(BLACK, 0.45f));
    DrawRectangleRec({ x, y, width*fill, height }, Fade(colour, 0.92f));
    DrawRectangleLinesEx({ x, y, width, height }, ui, Fade(RAYWHITE, 0.55f));
}

//----------------------------------------------------------------------------------
// Where the floor's bars start and how wide they run.
//
// Two functions rather than two copies of the sum, because the chaos bar and the
// event bar under it have to agree to the pixel: two bars stacked at slightly
// different widths is not a stack, it is a mistake the player can see.
//----------------------------------------------------------------------------------
float Hud::FloorBarLeft() const
{
    return minimap.Right() + Margin*2.0f*UiScale();
}

float Hud::FloorBarWidth() const
{
    const float ui = UiScale();

    // What is left of the screen, then capped - see FloorBarMaxWidth
    const float room = GetScreenWidth() - FloorBarLeft() - Margin*2.0f*ui;
    const float most = FloorBarMaxWidth*ui;

    return (room > most) ? most : room;
}

void Hud::DrawCrosshair() const
{
    const float ui = UiScale();

    const float cx = GetScreenWidth()*0.5f;
    const float cy = GetScreenHeight()*0.5f;

    const float gap = 4.0f*ui;
    const float len = 6.0f*ui;
    const float weight = (ui < 1.5f) ? 1.0f : 2.0f;

    DrawLineEx({ cx - gap - len, cy }, { cx - gap, cy }, weight, RAYWHITE);
    DrawLineEx({ cx + gap, cy }, { cx + gap + len, cy }, weight, RAYWHITE);
    DrawLineEx({ cx, cy - gap - len }, { cx, cy - gap }, weight, RAYWHITE);
    DrawLineEx({ cx, cy + gap }, { cx, cy + gap + len }, weight, RAYWHITE);
}

//----------------------------------------------------------------------------------
// The hurt indicator: a red bar on the ring around the crosshair, at the point
// closest to where the last blow actually came from.
//
// Built from the player's own facing rather than the world: "the right edge of
// the screen" is the only vocabulary a directional indicator can use, and that is
// a question about the camera, not about compass directions. A blow from a body
// standing due north means nothing on its own - it is a bar on the LEFT if the
// player is facing east and a bar behind them if they are facing north.
//
// A hit with no real source - the seal's bolts, which land from directly
// overhead - draws as a ring around the whole crosshair instead of a bar at one
// point on it, because inventing a side would tell the player which way to turn
// and there is nowhere better to turn to.
//----------------------------------------------------------------------------------
void Hud::DrawHurtIndicator(const Player &player, const Camera3D &camera) const
{
    if (player.lastHitAge >= Config::HurtIndicatorTime) return;

    const float ui = UiScale();
    const float fade = 1.0f - player.lastHitAge/Config::HurtIndicatorTime;

    const float screenW = (float)GetScreenWidth();
    const float screenH = (float)GetScreenHeight();

    const Vector2 center = { screenW*0.5f, screenH*0.5f };
    const float radius = fminf(screenW, screenH)*Config::HurtIndicatorRadius;

    if (!player.lastHitDirectional)
    {
        DrawRing(center, radius - 3.0f*ui, radius + 3.0f*ui, 0.0f, 360.0f, 48,
                 Fade(HealthFill, fade*0.55f));

        return;
    }

    Vector3 toSource = Vector3Subtract(player.lastHitFrom, player.Position());
    toSource.y = 0.0f;

    // Close enough to standing where the blow landed that a direction would be
    // noise rather than information - leave it at the top rather than divide by
    // whatever is left of a near-zero vector.
    float angle = 0.0f;

    if (Vector3LengthSqr(toSource) > 1e-4f)
    {
        const Vector3 forward = Vector3Normalize({ camera.target.x - camera.position.x, 0.0f,
                                                    camera.target.z - camera.position.z });
        const Vector3 right = Vector3CrossProduct(forward, { 0.0f, 1.0f, 0.0f });

        toSource = Vector3Normalize(toSource);

        // 0 dead ahead, +-pi/2 either side, pi behind - see the note above on why
        // this is relative to the camera rather than to the world.
        angle = atan2f(Vector3DotProduct(toSource, right), Vector3DotProduct(toSource, forward));
    }

    const Vector2 pos = { center.x + sinf(angle)*radius, center.y - cosf(angle)*radius };

    const Rectangle bar = { pos.x, pos.y, Config::HurtIndicatorLength*ui,
                            Config::HurtIndicatorWidth*ui };
    const Vector2 origin = { bar.width*0.5f, bar.height*0.5f };

    // Tangent to the ring - see the note above - so it reads as a mark on the
    // circle rather than an arrow, which would claim a precision ("exactly this
    // many degrees") the indicator was never meant to promise.
    DrawRectanglePro(bar, origin, angle*RAD2DEG, Fade(HealthFill, fade*0.85f));
}

//----------------------------------------------------------------------------------
// The player's own column, down the left under the map.
//
// Depth, health, then the schools, each starting where the last one ended. The order
// is by how often it is READ rather than by how important it is: the schools are
// switched between constantly and the depth is looked at once a floor, so the depth
// goes furthest from the eye's resting place and the schools nearest.
//----------------------------------------------------------------------------------
void Hud::DrawLeftColumn(const Player &player, const Level &level, Magic magic,
                         const Spellbook &spells) const
{
    const float ui = UiScale();

    const float left = Margin*ui;
    const float bar = BarPixels();
    const float label = LabelSize*ui;

    // Under the minimap, which owns the top of this corner
    float y = Margin*ui + minimap.Height() + Gap*ui;

    UiLabel(TextFormat("DEPTH %d", level.Depth()), left, y, label, Dim);

    y += label + Gap*ui;

    //------------------------------------------------------------------------------
    // Health, and the count beside it.
    //
    // The number sits to the RIGHT of the bar rather than inside it. The frame's
    // window is seven source pixels tall and a figure dropped into it sits on the
    // fill it is describing - survivable on a drawn rectangle, and not on ornamented
    // metalwork.
    //------------------------------------------------------------------------------
    const float ratio = (player.maxHealth > 0)
                      ? (player.health/(float)player.maxHealth) : 0.0f;

    DrawBar(BarHue::Health, left, y, HealthBarWidth*ui, ratio, HealthFill);

    UiLabel(TextFormat("%d / %d", player.health, player.maxHealth),
            left + HealthBarWidth*ui + 8.0f*ui, y + (bar - label)*0.5f, label, Ink);

    y += bar + Gap*ui;

    //------------------------------------------------------------------------------
    // Mana, stacked directly under health.
    //
    // The same width, so the two read as one block rather than as two readouts that
    // happen to be near each other. It is the mobile game's own arrangement and for
    // the same reason: these are the two pools the player spends out of, and they
    // are the two things a glance has to take in together.
    //
    // The count is beside it because a mana bar has to be COUNTED rather than
    // estimated - the pool is small and a cast is priced in single figures, so "13"
    // is a number the player does arithmetic with before pressing the button.
    //------------------------------------------------------------------------------
    const int maxMana = player.MaxMana();
    const float manaRatio = (maxMana > 0) ? (player.mana/(float)maxMana) : 0.0f;

    DrawBar(BarHue::Event, left, y, HealthBarWidth*ui, manaRatio, ManaFill);

    UiLabel(TextFormat("%d / %d", player.mana, maxMana),
            left + HealthBarWidth*ui + 8.0f*ui, y + (bar - label)*0.5f, label, ManaFill);

    y += bar + Gap*ui;

    //------------------------------------------------------------------------------
    // The schools, as a row of discs under the health bar.
    //
    // Colour is the whole readout. The name is there to be read once; what is
    // actually used in play is that the third disc is green and the mote coming out
    // of the staff is green, and those two facts have to be the same value - which
    // they are, because both come out of MagicAt.
    //
    // The selected one is bigger and ringed rather than merely brighter: eight
    // saturated colours side by side have no spare brightness to signal with, and a
    // ring is legible against every one of them.
    //------------------------------------------------------------------------------
    const int count = (int)Magic::Count;

    const float radius = 6.0f*ui;
    const float step = radius*2.0f + 6.0f*ui;
    const float cy = y + radius + 2.0f*ui;

    for (int i = 0; i < count; ++i)
    {
        const MagicDef &def = MagicAt((Magic)i);
        const float cx = left + radius + i*step;
        const bool selected = (i == (int)magic);

        //--------------------------------------------------------------------------
        // A school not yet bought is drawn as an empty socket rather than left out.
        //
        // The row is how the player learns there are eight, and a list that grew as
        // they were bought would never say how many are still to come. An outline
        // says "there is one here you do not have" in a way a gap cannot.
        //--------------------------------------------------------------------------
        if (!spells.Owns((Magic)i))
        {
            DrawCircleLinesV({ cx, cy }, radius, Fade(def.colour, 0.30f));

            continue;
        }

        DrawCircleV({ cx, cy }, radius, Fade(def.colour, selected ? 1.0f : 0.45f));

        if (!selected) continue;

        DrawCircleLinesV({ cx, cy }, radius + 3.0f*ui, RAYWHITE);

        //--------------------------------------------------------------------------
        // Named beside the row rather than under its disc: eight labels at this size
        // is an unreadable smear, and only one of them is ever the answer.
        //
        // The cost goes with it, and greys out when the pool cannot cover it. That is
        // the one thing on this HUD that answers "why did nothing happen when I
        // pressed the button" before the player has to ask it.
        //--------------------------------------------------------------------------
        const char *named = TextFormat("%d %s", i + 1, def.name);

        UiLabel(named, left + count*step + 6.0f*ui, cy - label*0.5f, label, RAYWHITE);

        const int cost = spells.CostOf((Magic)i, player.Mods());

        UiLabel(TextFormat("%d mana", cost),
                left + count*step + 16.0f*ui + UiTextWidth(named, label),
                cy - label*0.5f, label, (player.mana >= cost) ? ManaFill : Dim);
    }
}

//----------------------------------------------------------------------------------
// A bar over every champion in view - see the note in Hud.h for why only champions.
//
// Culled three ways, and each one earns its place:
//
//   BEHIND    GetWorldToScreen happily returns a point for something behind the
//             camera, and the bar would appear mirrored across the screen. Tested
//             against the camera's own forward axis, which is the only reliable
//             answer.
//   DISTANT   a champion three rooms away is not a fight yet, and its bar hanging
//             through a wall is a readout for a decision nobody is making.
//   DEAD      a corpse keeps its record until the death clip finishes, and a bar
//             over a body that is falling over is a fight that looks unfinished.
//
// Deliberately NOT occlusion-tested against the level. A champion behind a pillar
// still has a bar, because the alternative is a readout that flickers as the player
// strafes - and knowing where the thing that is hunting you went is worth more than
// the realism of hiding it.
//----------------------------------------------------------------------------------
void Hud::DrawChampionBars(const EnemyManager &enemies, const Camera3D &camera) const
{
    const float ui = UiScale();

    const float width = Config::ChampionBarWidth*ui;
    const float bar = BarPixels();
    const float label = LabelSize*ui;

    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));

    const Color tint = TierAt(EnemyTier::Champion).tint;

    for (const Enemy &enemy : enemies.All())
    {
        if (!enemy.IsAlive()) continue;
        if (enemy.tier != EnemyTier::Champion) continue;

        const Vector3 head = { enemy.body.position.x,
                               enemy.body.position.y + enemy.height + Config::ChampionBarLift,
                               enemy.body.position.z };

        const Vector3 toward = Vector3Subtract(head, camera.position);

        if (Vector3DotProduct(toward, forward) <= 0.0f) continue;

        const float away = Vector3Length(toward);

        if (away > Config::ChampionBarRange) continue;

        // Fades over the last quarter of the range rather than snapping off, so a
        // bar does not blink as the player walks the boundary
        const float fadeFrom = Config::ChampionBarRange*0.75f;
        const float alpha = (away <= fadeFrom)
                          ? 1.0f
                          : 1.0f - (away - fadeFrom)/(Config::ChampionBarRange - fadeFrom);

        const Vector2 screen = GetWorldToScreen(head, camera);

        const float ratio = (enemy.maxHealth > 0)
                          ? (enemy.health/(float)enemy.maxHealth) : 0.0f;

        const float x = screen.x - width*0.5f;

        //--------------------------------------------------------------------------
        // The name above the bar, in the champion's own tint.
        //
        // "CHAMPION WARRIOR" and not a health figure. The number is what the bar is
        // for; what the words add is WHY this body has a bar when the four beside it
        // do not, which is the one thing a player meeting their first champion needs
        // told.
        //--------------------------------------------------------------------------
        UiTextCenteredOutline(TextFormat("CHAMPION %s",
                                         Config::EnemyTypes[enemy.type].name),
                              screen.x, screen.y - label - bar - 4.0f*ui, label,
                              Fade(tint, alpha));

        DrawBar(BarHue::Event, x, screen.y - bar, width, ratio, Fade(tint, alpha));
    }
}

//----------------------------------------------------------------------------------
// The prompt, under the crosshair, while standing at a vendor.
//
// Near the crosshair rather than in a corner, because it is the one line on this HUD
// that is about something the player is standing IN. A prompt in the corner for a
// thing under your feet is a prompt nobody reads.
//----------------------------------------------------------------------------------
void Hud::DrawVendorPrompt(const Player &player, const VendorManager &vendors) const
{
    const NpcKind here = vendors.At(player.Position());

    if (here == NpcKind::Count) return;

    const float ui = UiScale();

    const NpcDef &def = NpcAt(here);

    UiTextCenteredOutline(TextFormat("E    trade with the %s", def.name),
                          GetScreenWidth()*0.5f, GetScreenHeight()*0.5f + 48.0f*ui,
                          BriefSize*ui, def.colour);
}

//----------------------------------------------------------------------------------
// The run, across the bottom of the screen.
//
// Wide rather than tucked in a corner, which is the one bar on this HUD that earns
// it: everything else is a reading taken at a moment, and this is the only thing on
// screen measuring the run as a whole. It is also the mobile game's own arrangement,
// where it spans the bottom for the same reason.
//----------------------------------------------------------------------------------
void Hud::DrawProgress(const Player &player) const
{
    const float ui = UiScale();

    const float margin = Margin*2.0f*ui;
    const float width = GetScreenWidth() - margin*2.0f;

    if (width < 1.0f) return;

    const float label = LabelSize*ui;
    const float y = GetScreenHeight() - BarPixels() - Margin*ui;

    const float ratio = (player.expToNext > 0)
                      ? (player.exp/(float)player.expToNext) : 0.0f;

    DrawBar(BarHue::Exp, margin, y, width, ratio, ExpFill);

    // Above the bar, at either end. See the note about the frame's window in the
    // left column - nothing is dropped inside the metalwork.
    const float labelY = y - label - Gap*ui*0.5f;

    UiLabel(TextFormat("LV %d", player.level), margin, labelY, label, Ink);

    UiLabelRight(TextFormat("%d / %d exp", player.exp, player.expToNext),
                 margin + width, labelY, label, Dim);

    if (player.statPoints > 0)
    {
        UiLabelCentered(TextFormat("TAB    %d points to spend", player.statPoints),
                        margin + width*0.5f, labelY, label, Ready);
    }
}

//----------------------------------------------------------------------------------
// The floor, along the top beside the map.
//
// It DRAINS rather than fills, which is the whole point of it being here: the exp bar
// across the bottom is the player getting stronger and this is the floor running out,
// and the two moving in opposite directions off the same kills is the clearest
// possible statement of what a run is.
//
// Kept at full opacity even when empty rather than fading away. A bar that vanished
// on reaching zero would take the answer to "am I done here?" with it.
//----------------------------------------------------------------------------------
void Hud::DrawChaos(const ChaosState &chaos, const EventManager &events) const
{
    const float ui = UiScale();

    const float label = LabelSize*ui;

    const float x = FloorBarLeft();
    const float width = FloorBarWidth();

    if (width < 1.0f) return;

    const float y = Margin*ui + label + Gap*ui;

    const float ratio = (chaos.max > 0) ? (chaos.left/(float)chaos.max) : 0.0f;

    DrawBar(BarHue::Chaos, x, y, width, ratio, ChaosFill);

    //------------------------------------------------------------------------------
    // The label answers "am I done here?", and there are three answers.
    //
    // The middle one is the whole reason the events exist and is the one worth
    // getting right: the pool is empty, the floor LOOKS finished, and it is not.
    // Saying so in as many words - and saying how many are left - is what stops that
    // state reading as a bug.
    //------------------------------------------------------------------------------
    const int outstanding = events.Outstanding();

    const char *text = chaos.cleared ? "FLOOR CLEARED"
                     : (chaos.quelled ? TextFormat("%d EVENTS REMAIN", outstanding)
                                      : "CHAOS");

    const Color tint = chaos.cleared ? Ready : (chaos.quelled ? EventPending : Dim);

    UiLabel(text, x, Margin*ui, label, tint);

    if (!chaos.quelled)
    {
        UiLabelRight(TextFormat("%d left", chaos.left), x + width, Margin*ui, label, Dim);

        // How many objectives are on this floor, before any of them matters. Shown
        // from the start rather than only once the pool is empty: a player who first
        // learns there are two events at the moment the portal refuses to open has
        // learned it too late to have gone looking for them.
        if (outstanding > 0)
        {
            UiLabelCentered(TextFormat("%d events", outstanding), x + width*0.5f,
                            Margin*ui, label, EventPending);
        }
    }
}

//----------------------------------------------------------------------------------
// The objective that is running, under the chaos bar.
//
// Drawn in the event's OWN colour - the same colour as the column of light the player
// walked into to start it. That is the one thing tying the readout to the thing it is
// reporting on, and it costs nothing to keep true because both come out of one table.
//
// The clock only appears once it is worth worrying about. A timer counting down from
// a hundred and thirty since the first second is a number the player stops reading
// long before it starts to matter.
//----------------------------------------------------------------------------------
void Hud::DrawEvent(const EventManager &events) const
{
    Color colour = WHITE;
    float progress = 0.0f;
    float timeLeft = 0.0f;

    if (!events.RunningState(colour, progress, timeLeft)) return;

    const float ui = UiScale();

    const float bar = BarPixels();
    const float label = LabelSize*ui;

    const float x = FloorBarLeft();
    const float width = FloorBarWidth();

    if (width < 1.0f) return;

    const float y = Margin*ui + label + Gap*ui + bar + Gap*ui;

    DrawBar(BarHue::Event, x, y, width, progress, colour);

    if (const char *banner = events.RunningBanner())
    {
        UiLabel(banner, x, y + bar + Gap*ui*0.5f, label, colour);
    }

    if ((timeLeft > 0.0f) && (timeLeft < 20.0f))
    {
        // Red under ten, because by then it is the only thing on the HUD worth
        // reacting to
        const Color urgent = (timeLeft < 10.0f) ? (Color){ 255, 110, 100, 255 } : colour;

        UiLabelRight(TextFormat("%.0f", timeLeft), x + width, y + bar + Gap*ui*0.5f,
                     label, urgent);
    }

    // What to do about it, for the first few seconds. In the middle of the screen and
    // bigger than the banner, because it is the one thing on this HUD that is ever an
    // instruction - and the one thing that has to be read without looking away from
    // the crosshair.
    if (const char *brief = events.RunningBrief())
    {
        UiTextCenteredOutline(brief, GetScreenWidth()*0.5f,
                              GetScreenHeight()*0.5f - 90.0f*ui, BriefSize*ui, colour);
    }
}

//----------------------------------------------------------------------------------
// What to do about a floor that is finished.
//
// Two messages that are never both loud at once. The banner announces that the floor
// is done and then settles to a standing reminder - it has to outlive its own
// announcement, because the portal is at the far end of the map and the walk there is
// most of a minute. The prompt replaces it entirely once the player is actually
// standing in the portal, because at that point the reminder has served its purpose
// and the only thing worth saying is how long to hold still.
//----------------------------------------------------------------------------------
void Hud::DrawFloorState(const ChaosState &chaos, const EventManager &events,
                         bool inPortal) const
{
    (void)events;

    if (!chaos.cleared) return;

    const float ui = UiScale();

    const float cx = GetScreenWidth()*0.5f;
    const float cy = GetScreenHeight()*0.5f;

    if (inPortal)
    {
        //--------------------------------------------------------------------------
        // A ring closing on the crosshair rather than a number counting down.
        //
        // The player is standing still with nothing else to read, and a shrinking
        // ring says "keep holding" in a way a digit does not - it is a progress bar
        // that does not need a label, and it is centred on the one place they are
        // already looking.
        //--------------------------------------------------------------------------
        const float held = (chaos.dwell > 0.0f) ? chaos.dwell : 0.0f;
        const float progress = held/Config::PortalDwell;

        const float radius = (46.0f - 26.0f*progress)*ui;

        DrawCircleLinesV({ cx, cy }, radius, Fade(Ink, 0.35f));
        DrawRing({ cx, cy }, radius - 2.0f*ui, radius + 2.0f*ui,
                 -90.0f, -90.0f + 360.0f*progress, 48, Fade(Ready, 0.9f));

        UiTextCenteredOutline("DESCENDING", cx, cy + 62.0f*ui, BriefSize*ui, Ready);

        return;
    }

    const float alpha = BannerAlpha(chaos.sinceCleared);

    // A third of the way down rather than centred. The middle of the screen is the
    // crosshair's, and a banner sitting on it while the player is still fighting the
    // last of a garrison is a banner in the way.
    const float y = GetScreenHeight()/3.0f;

    UiTextCenteredOutline("FLOOR CLEARED", cx, y, TitleSize*ui, Fade(Ready, alpha));
    UiTextCenteredOutline("find the portal", cx, y + TitleSize*1.2f*ui, BriefSize*ui,
                          Fade(Ink, alpha*0.8f));
}
