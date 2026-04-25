#include <iostream>

#include "ScoreViewport.h"

namespace {
int failures = 0;

void expectBool(const char *name, bool actual, bool expected)
{
    if (actual == expected)
        return;
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    failures++;
}

void expectFloat(const char *name, float actual, float expected)
{
    if (std::abs(actual - expected) < 0.0001f)
        return;
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    failures++;
}

void expectInt(const char *name, qint64 actual, qint64 expected)
{
    if (actual == expected)
        return;
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    failures++;
}

void testClamps()
{
    const ScoreViewport viewport = makeScoreViewport(-5, 0, 0.0f, PB_PART_none, 99);

    expectInt("origin clamp", viewport.originTick, 0);
    expectInt("visible clamp", viewport.visibleTicks, 1);
    expectFloat("pixels clamp", viewport.pixelsPerTick, 1.0f);
    expectInt("hand clamp", viewport.selectedHand, PB_PART_both);
    expectInt("mode clamp", viewport.viewMode, PB_VIEW_MODE_score);
}

void testWindow()
{
    const ScoreViewport viewport = makeScoreViewport(100, 40, 2.0f,
                                                     PB_PART_left,
                                                     PB_VIEW_MODE_synthesia);

    expectInt("end tick", viewportEndTick(viewport), 140);
    expectBool("contains start", viewportContainsTick(viewport, 100), true);
    expectBool("contains last", viewportContainsTick(viewport, 139), true);
    expectBool("excludes end", viewportContainsTick(viewport, 140), false);
    expectFloat("offset pixels", viewportTickOffsetPixels(viewport, 112), 24.0f);
    expectInt("visible pixels", viewportVisibleTicksForPixels(25.0f, 2.0f), 13);
}
}

int main()
{
    testClamps();
    testWindow();

    if (failures == 0) {
        std::cout << "ScoreViewport tests passed\n";
        return 0;
    }
    std::cerr << failures << " score viewport test(s) failed\n";
    return 1;
}
