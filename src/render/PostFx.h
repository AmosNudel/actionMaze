#pragma once

#include "raylib.h"

class AssetManager;

//----------------------------------------------------------------------------------
// The buffer the world is drawn into, and the one pass that puts it on screen.
//
// Everything the dungeon is made of used to go straight at the window. It still
// could - nothing here changes what is drawn - but there is no way to grade a
// picture, glow the bright parts of it or close the frame in around the player
// once it has been presented. Those want the finished frame back as a texture,
// which means rendering into one. See assets/shaders/post.fs for what is
// actually done with it.
//
// --- Why the buffer is bigger than the window ----------------------------------
// The window was created with 4x multisampling and it earns its keep - see
// Config::AntiAliasing for what the brickwork does without it. A render texture
// cannot carry that: raylib has no multisampled target, so moving the world into
// one would hand the shimmer straight back.
//
// So the buffer is rendered at Config::PostRenderScale times the window instead
// and sampled back down with a bilinear filter. At exactly 2 that resolve is a
// clean 2x2 box - supersampling, which beats multisampling here because it
// smooths the SHADING on those bevelled courses as well as their edges. It also
// costs four times the fragment work, which is the whole of the trade.
//
// --- What goes through it and what does not ------------------------------------
// The world and the held weapons; not the HUD, the pages or the labels. A player
// reading a price off a shop row does not want it graded, glowed or dimmed at the
// corners, and the crosshair least of all. Everything drawn after PostFx::Draw is
// drawn at the window's own resolution, straight, as it always was.
//----------------------------------------------------------------------------------
class PostFx
{
public:
    // Silently does nothing if the shader is missing, in which case Draw puts the
    // buffer on screen untouched - one absent .fs should not cost the frame
    void Load(AssetManager &assets);
    void Unload();

    // The world pass goes between these. Sizes (and resizes) the buffer, so a
    // window that has just changed shape is picked up here rather than needing to
    // be told - the same arrangement ViewModel::BeginPass uses.
    void BeginPass();
    void EndPass();

    //------------------------------------------------------------------------------
    // The buffer to the screen, through the shader.
    //
    // `hurt` is 0 for a healthy player and 1 for one about to die; `time` is any
    // steadily rising clock, and only drives the pulse the red breathes on. Both
    // are the caller's, because how close to death a player is belongs to the
    // game rather than to a render target.
    //------------------------------------------------------------------------------
    void Draw(float hurt, float time) const;

private:
    void Resize();

    // The world and the weapons, at PostBufferWidth by PostBufferHeight. Carries
    // its own depth buffer, which is what makes the 3D pass inside it work.
    RenderTexture2D scene = { 0 };

    // Borrowed from the AssetManager, which owns and frees it - the same
    // arrangement Sky has with the skybox program
    Shader shader = { 0 };

    int texelLoc = -1;
    int hurtLoc = -1;
    int pulseLoc = -1;

    bool ready = false;
};

//----------------------------------------------------------------------------------
// How big that buffer is this frame.
//
// Free functions rather than members because the VIEW MODEL's target has to be
// the same size, and it is built before this one is - see the note at the top of
// Game::DrawInGame. Both answers are derived from the window and the render
// scale alone, so asking twice in a frame gives the same number twice.
//----------------------------------------------------------------------------------
int PostBufferWidth();
int PostBufferHeight();
