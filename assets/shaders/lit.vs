#version 330

// Input vertex attributes
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
in vec3 vertexNormal;

// Input uniform values
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

// Output vertex attributes (to fragment shader)
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;

// Where this vertex actually is in the world. Only the fog wants it - see the
// block in lit.fs - and it is worked out here rather than there because matModel
// is a per-draw constant and this runs per vertex instead of per pixel.
out vec3 fragPosition;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragPosition = vec3(matModel*vec4(vertexPosition, 1.0));

    // Through matNormal rather than matModel: a normal is a direction, and it
    // survives a non-uniform scale only under the inverse transpose, which is
    // what raylib puts in matNormal for us.
    fragNormal = normalize(vec3(matNormal*vec4(vertexNormal, 0.0)));

    gl_Position = mvp*vec4(vertexPosition, 1.0);
}
