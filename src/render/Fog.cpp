#include "render/Fog.h"

#include "core/Config.h"
#include "render/AssetManager.h"

void Fog::Load(AssetManager &assets)
{
    count = 0;

    // The two the world is drawn with: the stonework and everything standing on
    // it, and the bodies walking about on top of that
    Shader *programs[2] =
    {
        &assets.GetShader("shaders/lit.vs", "shaders/lit.fs"),
        &assets.GetShader("shaders/skinning.vs", "shaders/skinning.fs"),
    };

    for (Shader *shader : programs)
    {
        const int viewLoc = GetShaderLocation(*shader, "viewPos");

        // A program that has no viewPos has no fog block in it, and pushing the
        // rest at it would be four warnings a launch for a shader that is working
        // exactly as written
        if (viewLoc == -1)
        {
            TraceLog(LOG_WARNING, "FOG: shader %i carries no fog block - it will draw unhazed",
                     shader->id);
            continue;
        }

        // Everything that never moves, once. These are uniforms on the PROGRAM, so
        // they outlast the call and every model sharing it inherits them.
        const float density = Config::FogDensity;
        const float floorY = Config::FogFloor;
        const float height = Config::FogHeight;
        const float top = Config::FogTop;

        SetShaderValue(*shader, GetShaderLocation(*shader, "fogColour"),
                       Config::FogColour, SHADER_UNIFORM_VEC3);

        SetShaderValue(*shader, GetShaderLocation(*shader, "fogDensity"), &density, SHADER_UNIFORM_FLOAT);
        SetShaderValue(*shader, GetShaderLocation(*shader, "fogFloor"), &floorY, SHADER_UNIFORM_FLOAT);
        SetShaderValue(*shader, GetShaderLocation(*shader, "fogHeight"), &height, SHADER_UNIFORM_FLOAT);
        SetShaderValue(*shader, GetShaderLocation(*shader, "fogTop"), &top, SHADER_UNIFORM_FLOAT);

        bound[count].shader = shader;
        bound[count].viewLoc = viewLoc;
        count++;
    }

    TraceLog(LOG_INFO, "FOG: %i of 2 world shaders hazed", count);
}

void Fog::SetView(Vector3 eye) const
{
    for (int i = 0; i < count; ++i)
    {
        SetShaderValue(*bound[i].shader, bound[i].viewLoc, &eye, SHADER_UNIFORM_VEC3);
    }
}
