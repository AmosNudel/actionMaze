#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
in vec3 fragPosition;

// Output fragment color
out vec4 finalColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Where the light comes from, in world space. A direction, not a position: the
// level has no lamps and one steady sun is what makes a modelled brick read as
// modelled at all.
uniform vec3 lightDir;
// How dark an unlit face is allowed to get. Not zero - a wall facing away from
// the sun in a roofless dungeon is in shade, not in a cave.
uniform float ambient;

//----------------------------------------------------------------------------------
// Distance fog.
//
// This block is duplicated in skinning.fs. GLSL has no #include, and two copies of a
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
    vec4 texelColor = texture(texture0, fragTexCoord);

    // Half-lambert rather than max(dot, 0): the straight version drives every
    // face past ninety degrees to a single flat value, which throws away exactly
    // the shading that separates one course of bricks from the next on a wall
    // turned away from the light. Wrapping it keeps a gradient the whole way
    // round.
    float lambert = dot(normalize(fragNormal), -normalize(lightDir))*0.5 + 0.5;
    float light = ambient + (1.0 - ambient)*lambert*lambert;

    vec4 base = texelColor*colDiffuse*fragColor;

    // Fogged after the lighting, not before it: fog is what is BETWEEN the eye and
    // the surface, so it replaces the lit colour rather than being lit itself
    finalColor = vec4(mix(base.rgb*light, fogColour, FogAmount(fragPosition)), base.a);
}
