#pragma once

#include "raylib.h"

//----------------------------------------------------------------------------------
// Who made what.
//
// A flat table of attributions and a screen that scrolls it, in the same shape the
// mobile game's credits.cpp uses - and the shape matters for the same reason it does
// there. It is DATA compiled into the binary rather than a text file read at runtime:
// a missing path would make the credits look empty while the game still shipped the
// art, which is a licence breach that looks like a working feature.
//
// ONE ROW PER PACK, not per file. Four columns, because that is what a licence
// actually asks for: what was used, who made it, where it came from, and the terms.
// CREDITS.md at the repo root is the long form - this is the reading a player gets,
// and between them the second is the one that ships.
//
// --- Not every row is optional ---------------------------------------------------
// Most of what is here is CC0 and asks for nothing. The menu sky does not: it is
// CC BY 4.0, where attribution is a CONDITION of use rather than a courtesy. That is
// the row this screen exists to guarantee, and it is why the licence column is shown
// at all rather than summarised as "free".
//----------------------------------------------------------------------------------
enum class CreditSection
{
    Game = 0,   // The game itself - design, code, assembly
    Art,
    Audio,
    Font,
    Engine,

    Count
};

struct CreditEntry
{
    CreditSection section;

    const char *what;       // "Dungeon art", "Sound effects"
    const char *who;        // The author, named as the licence wants it shown
    const char *where;      // Source, as a page a person could actually type
    const char *licence;    // Short tag: "CC0", "CC BY 4.0", "free - see pack"
};

int CreditCount();
const CreditEntry &CreditAt(int index);
const char *CreditSectionName(CreditSection section);

class CreditsScreen
{
public:
    enum class Choice { None, Back };

    void Show();

    Choice Update();
    void Draw() const;

private:
    struct Layout
    {
        float ls = 1.0f;

        Rectangle page{};
        Rectangle list{};       // The scrolling area
        Rectangle back{};

        float titleY = 0.0f;
        float rowHeight = 0.0f;
        int visible = 0;        // Rows that fit in `list`

        Rectangle RowAt(int slot) const;
    };

    static Layout Measure();

    //------------------------------------------------------------------------------
    // The list is section HEADINGS and entries interleaved, so what scrolls is not
    // the table itself - a heading is a row too. Resolved once here rather than at
    // both the measure and the paint, which is the same rule ShopScreen's own
    // BuildRows follows and for the same reason.
    //
    // `entry` is -1 on a heading row, in which case `section` names it.
    //------------------------------------------------------------------------------
    struct Line
    {
        int entry = -1;
        CreditSection section = CreditSection::Game;
    };

    static int BuildLines(Line *out, int max);

    // Clamped against the line count every frame rather than when it changes - see
    // ShopScreen::ClampScroll, which is the same problem.
    int ClampScroll(int lineCount, int visible) const;

    bool justShown = false;
    int scroll = 0;
};
