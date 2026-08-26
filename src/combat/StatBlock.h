#pragma once

//----------------------------------------------------------------------------------
// The four stats as a plain struct, and the neutral line they are measured from.
//
// Split out of Stats.h and kept free of every dependency for one reason: the enemy
// archetype table in Config.h writes a stat line per kind, and Config.h is a header
// of plain tables that any module can pull in for nothing. Stats.h reads the rates
// OUT of Config.h, so the type cannot live there without the two including each
// other. Same shape as RoomKind.h, and for the same reason.
//
// What each stat does, and the arithmetic every bonus follows, is in Stats.h.
//----------------------------------------------------------------------------------
namespace Config
{
    // The pivot. At 10 a stat contributes exactly nothing; above 10 adds and below
    // 10 SUBTRACTS. It lives beside the struct rather than with the other tunables
    // because it is what the struct's own defaults mean - a default-constructed
    // StatBlock is the neutral character, and that has to stay true by construction.
    constexpr int StatBase = 10;
}

struct StatBlock
{
    int con  = Config::StatBase;
    int arms = Config::StatBase;
    int skl  = Config::StatBase;
    int arc  = Config::StatBase;
};
