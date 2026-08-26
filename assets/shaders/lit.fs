#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

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

    finalColor = vec4(base.rgb*light, base.a);
}
