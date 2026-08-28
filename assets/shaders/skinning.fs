#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragPosition;

// Output fragment color
out vec4 finalColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

//----------------------------------------------------------------------------------
// Distance fog.
//
// This block is duplicated in lit.fs. GLSL has no #include, and two copies of a
// dozen lines is a better trade than a build step that stitches shaders together -
// but they ARE a pair: change one and change the other.
//
// Everything about it comes from Config (see the Fog section there) and is pushed
// once at load by render/Fog.h. Only `viewPos` moves, once a frame.
//----------------------------------------------------------------------------------
uniform vec3 viewPos;        // The eye, in world space
uniform vec3 fogColour;
uniform float fogDensity;    // Larger closes the view in sooner
uniform float fogFloor;      // The height the ground haze is measured up from
uniform float fogHeight;     // Units of height it takes to thin out
uniform float fogTop;        // What is left of it well above the floor

float FogAmount(vec3 world)
{
    // Exponential squared: nothing at all for the first few paces, then a
    // shoulder, then a long tail. Linear fog has a start line you can see, and a
    // plain exponential greys the wall the player is standing against.
    float d = length(world - viewPos)*fogDensity;
    float fog = 1.0 - exp(-d*d);

    // Thinner the higher it sits, so a distant tower's base is buried in haze
    // while its roof is still a clean silhouette against the sky - which is what
    // makes the tower read as FAR rather than as small.
    //
    // Measured on the fragment's own height rather than integrated along the ray.
    // The eye never leaves the floor by more than a jump here, so the cheap
    // version and the correct one draw the same picture.
    float lift = exp(-max(world.y - fogFloor, 0.0)/fogHeight);

    return fog*mix(fogTop, 1.0, lift);
}

void main()
{
    vec4 base = texture(texture0, fragTexCoord)*colDiffuse*fragColor;

    // The same haze the walls behind them are in. An enemy that stayed crisp at
    // the far end of a fogged corridor would be the one thing in the picture with
    // no distance to it, which is exactly backwards - a body IS the thing the
    // player is judging the distance to.
    finalColor = vec4(mix(base.rgb, fogColour, FogAmount(fragPosition)), base.a);
}
