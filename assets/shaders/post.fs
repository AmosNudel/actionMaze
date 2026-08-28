#version 330

//----------------------------------------------------------------------------------
// The one full-screen pass the finished world goes through - see render/PostFx.h
// for why there is a buffer between the dungeon and the screen at all.
//
// Everything here is done in a single read of the scene plus eight taps for the
// glow. That is deliberate: a proper bloom is three or four targets and two more
// passes, and what this dungeon actually needs is a halo around torches and
// spellfire, a little more contrast in the stonework, and a frame that closes in
// when the player is nearly dead. All three fit in one pass.
//----------------------------------------------------------------------------------

in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

// One texel of the SOURCE, which is the supersampled buffer rather than the
// screen. The glow taps are measured in these so the halo keeps its width in
// finished pixels whatever Config::PostRenderScale is set to.
uniform vec2 texelSize;

// How much trouble the player is in: 0 healthy, 1 on the floor. Drives the red at
// the edges and nothing else - the grade below is the same at full health.
uniform float hurt;

// A slow breath, 0 to 1. A flat red overlay reads as a broken renderer; one that
// moves reads as a pulse, which is the thing it is standing in for.
uniform float pulse;

//----------------------------------------------------------------------------------
// The grade. Small numbers on purpose: this is a correction, not a filter, and
// anything strong enough to notice on its own is strong enough to fight the art.
//----------------------------------------------------------------------------------
const float Contrast   = 1.07;      // Around mid grey, so nothing clips
const float Saturation = 1.12;
const float Lift       = 0.006;     // Keeps the deepest shade off pure black

const float GlowThreshold = 0.62;   // Luma a pixel has to beat to bleed at all
const float GlowStrength  = 0.55;
const float GlowRadius    = 3.0;    // In source texels

const float VignetteDepth = 0.40;   // How dark the corners get
const float VignetteCurve = 1.15;   // Higher keeps the middle clear for longer

const vec3 Luma = vec3(0.2126, 0.7152, 0.0722);

// Only what is already bright is allowed to bleed. Without the threshold this is
// a blur over the whole picture, which is fog rather than glow.
vec3 BrightPass(vec2 uv)
{
    vec3 c = texture(texture0, uv).rgb;

    return c*smoothstep(GlowThreshold, 1.0, dot(c, Luma));
}

void main()
{
    vec2 uv = fragTexCoord;

    // The one straight read. At a render scale of exactly 2 this single bilinear
    // sample lands dead between four source texels and averages them, which is
    // the supersample resolve - see Config::PostRenderScale.
    vec3 col = texture(texture0, uv).rgb;

    //------------------------------------------------------------------------------
    // The glow: a ring of eight taps rather than a separable blur.
    //
    // What has to bleed here is fire, the portal and whatever is in the player's
    // hand - all small, all very bright, all against dark stone. A ring a few
    // texels out catches exactly that, and the diagonals are pushed further than
    // the axes so the eight of them cover a disc rather than a plus sign.
    //------------------------------------------------------------------------------
    vec2 r = texelSize*GlowRadius;
    vec2 d = r*1.6*0.7071;

    vec3 glow = BrightPass(uv + vec2( r.x, 0.0))
              + BrightPass(uv + vec2(-r.x, 0.0))
              + BrightPass(uv + vec2( 0.0,  r.y))
              + BrightPass(uv + vec2( 0.0, -r.y))
              + BrightPass(uv + vec2( d.x,  d.y))
              + BrightPass(uv + vec2(-d.x,  d.y))
              + BrightPass(uv + vec2( d.x, -d.y))
              + BrightPass(uv + vec2(-d.x, -d.y));

    col += (glow/8.0)*GlowStrength;

    //------------------------------------------------------------------------------
    // Contrast and saturation, after the glow so the halo is graded with
    // everything else rather than sitting on top of the result
    //------------------------------------------------------------------------------
    col = mix(vec3(dot(col, Luma)), col, Saturation);
    col = (col - 0.5)*Contrast + 0.5 + Lift;

    //------------------------------------------------------------------------------
    // The vignette. Measured in screen fractions rather than corrected for aspect,
    // so it follows the shape of the window - a circular falloff on a wide monitor
    // darkens the top and bottom of the picture, which is the middle of the view.
    //------------------------------------------------------------------------------
    vec2 fromCentre = uv - 0.5;
    float radius = dot(fromCentre, fromCentre)*2.0;   // 0 centre, 1 corner

    col *= 1.0 - VignetteDepth*pow(clamp(radius, 0.0, 1.0), VignetteCurve);

    //------------------------------------------------------------------------------
    // The red at low health.
    //
    // Along the same falloff as the vignette, and for the reason the vignette has
    // one: this arrives as the frame closing in, not as a sheet of red over the
    // middle of the screen. The player is at their most desperate when this is up
    // and that is the worst possible moment to take their view away.
    //
    // The whole-frame tint underneath it is what makes it readable at a glance.
    // The edges alone are easy to miss when the fight is happening in the middle.
    //------------------------------------------------------------------------------
    float amount = hurt*(0.55 + 0.45*pulse);
    float edge = smoothstep(0.08, 0.70, radius);

    col = mix(col, vec3(0.58, 0.03, 0.03), edge*amount*0.80);
    col = mix(col, col*vec3(1.0, 0.70, 0.70), amount*0.35);

    finalColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}
