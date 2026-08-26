#pragma once

//----------------------------------------------------------------------------------
// The schools, as a plain enum with no dependencies.
//
// Split out of Magic.h for the same reason StatBlock is split out of Stats.h: the
// enemy archetype table in Config.h names a school, and Config.h is a header of
// plain tables that any module can pull in for nothing. Magic.h reaches raylib
// through the effect sheets, so naming a school from Config would drag the whole
// render layer into every translation unit in the project.
//
// What each school IS - its colour, what it bursts into, how fast it flies and what
// it multiplies - is the table in Magic.cpp.
//
// The order is the debug key order: school 0 is the 1 key, and so on. That coupling
// is deliberate and is why this enum is contiguous from zero.
//----------------------------------------------------------------------------------
enum class Magic
{
    Flame = 0,
    Spark,
    Toxin,
    Blast,
    Splash,
    Flash,
    Nova,
    Rend,

    Count
};
