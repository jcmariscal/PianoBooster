#include <iostream>

#include "ScoreSlot.h"

namespace {
int failures = 0;

void expectBool(const char *name, bool actual, bool expected)
{
    if (actual == expected)
        return;
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    failures++;
}

void expectInt(const char *name, int actual, int expected)
{
    if (actual == expected)
        return;
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    failures++;
}

void testDefaults()
{
    ScoreSlot slot;
    ScoreSlotSymbol symbol;

    expectInt("slot id", slot.id, -1);
    expectInt("slot tick", static_cast<int>(slot.absoluteTick), 0);
    expectBool("symbols empty", slot.symbols.isEmpty(), true);
    expectInt("symbol type", symbol.type, PB_SYMBOL_none);
    expectInt("symbol note id", symbol.noteId, -1);
}

void testPlainNoteSlot()
{
    ScoreSlot slot;
    ScoreSlotSymbol symbol;

    slot.id = 3;
    slot.absoluteTick = 96;
    slot.durationTicks = 48;
    slot.leftEdgeTicks = -12;
    slot.av8Left = MIDI_OCTAVE;
    symbol.type = PB_SYMBOL_crotchet;
    symbol.hand = PB_PART_right;
    symbol.midiNote = 60;
    symbol.noteId = 7;
    slot.symbols.append(symbol);

    expectInt("note slot id", slot.id, 3);
    expectInt("note slot tick", static_cast<int>(slot.absoluteTick), 96);
    expectInt("note slot left edge", static_cast<int>(slot.leftEdgeTicks), -12);
    expectInt("note slot av8 left", slot.av8Left, MIDI_OCTAVE);
    expectInt("note symbol count", slot.symbols.size(), 1);
    expectInt("note symbol pitch", slot.symbols[0].midiNote, 60);
    expectInt("note symbol id", slot.symbols[0].noteId, 7);
}
}

int main()
{
    testDefaults();
    testPlainNoteSlot();

    if (failures == 0) {
        std::cout << "ScoreSlot tests passed\n";
        return 0;
    }
    std::cerr << failures << " score slot test(s) failed\n";
    return 1;
}
