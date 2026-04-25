#include <iostream>

#include "NoteIndex.h"

namespace {
int failures = 0;

void expectInt(const char *name, int actual, int expected)
{
    if (actual == expected)
        return;
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    failures++;
}

NoteEvent noteAt(int id, qint64 startTick, qint64 endTick)
{
    NoteEvent note;
    note.id = id;
    note.startTick = startTick;
    note.endTick = endTick;
    return note;
}

void testInsideWindow()
{
    QVector<NoteEvent> notes;
    notes.append(noteAt(0, 10, 15));
    notes.append(noteAt(1, 20, 25));
    notes.append(noteAt(2, 30, 35));

    const QVector<int> indexes = visibleNoteIndexes(notes, 18, 10);
    expectInt("inside count", indexes.size(), 1);
    expectInt("inside index", indexes[0], 1);
}

void testSustainedNoteBeforeWindow()
{
    QVector<NoteEvent> notes;
    notes.append(noteAt(0, 10, 40));
    notes.append(noteAt(1, 50, 60));

    const QVector<int> indexes = visibleNoteIndexes(notes, 25, 5);
    expectInt("sustained count", indexes.size(), 1);
    expectInt("sustained index", indexes[0], 0);
}

void testNegativeWindowClamps()
{
    QVector<NoteEvent> notes;
    notes.append(noteAt(0, 0, 5));
    notes.append(noteAt(1, 20, 25));

    const QVector<int> indexes = visibleNoteIndexes(notes, -10, 1);
    expectInt("negative count", indexes.size(), 1);
    expectInt("negative index", indexes[0], 0);
}
}

int main()
{
    testInsideWindow();
    testSustainedNoteBeforeWindow();
    testNegativeWindowClamps();

    if (failures == 0) {
        std::cout << "NoteIndex tests passed\n";
        return 0;
    }
    std::cerr << failures << " note index test(s) failed\n";
    return 1;
}
