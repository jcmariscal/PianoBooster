#ifndef __SCORE_VIEWPORT_H__
#define __SCORE_VIEWPORT_H__

#include <cmath>

#include <QtGlobal>

#include "Cfg.h"
#include "Chord.h"

struct ScoreViewport
{
    qint64 originTick = 0;
    qint64 visibleTicks = 1;
    float pixelsPerTick = 1.0f;
    whichPart_t selectedHand = PB_PART_both;
    viewMode_t viewMode = PB_VIEW_MODE_score;
};

inline whichPart_t validViewportHand(whichPart_t hand)
{
    if (hand == PB_PART_right || hand == PB_PART_left)
        return hand;
    return PB_PART_both;
}

inline viewMode_t validViewportMode(int mode)
{
    if (mode == PB_VIEW_MODE_synthesia)
        return PB_VIEW_MODE_synthesia;
    return PB_VIEW_MODE_score;
}

inline ScoreViewport makeScoreViewport(qint64 originTick, qint64 visibleTicks,
                                       float pixelsPerTick, whichPart_t hand,
                                       int viewMode)
{
    ScoreViewport viewport;
    viewport.originTick = originTick < 0 ? 0 : originTick;
    viewport.visibleTicks = visibleTicks < 1 ? 1 : visibleTicks;
    viewport.pixelsPerTick = pixelsPerTick > 0.0f ? pixelsPerTick : 1.0f;
    viewport.selectedHand = validViewportHand(hand);
    viewport.viewMode = validViewportMode(viewMode);
    return viewport;
}

inline qint64 viewportEndTick(const ScoreViewport& viewport)
{
    return viewport.originTick + viewport.visibleTicks;
}

inline bool viewportContainsTick(const ScoreViewport& viewport, qint64 tick)
{
    return tick >= viewport.originTick && tick < viewportEndTick(viewport);
}

inline float viewportTickOffsetPixels(const ScoreViewport& viewport, qint64 tick)
{
    return static_cast<float>(tick - viewport.originTick) * viewport.pixelsPerTick;
}

inline qint64 viewportVisibleTicksForPixels(float pixels, float pixelsPerTick)
{
    if (pixels <= 0.0f || pixelsPerTick <= 0.0f)
        return 1;
    return static_cast<qint64>(std::ceil(pixels / pixelsPerTick));
}

#endif
