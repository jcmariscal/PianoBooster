#ifndef __TRANSPORT_STATE_H__
#define __TRANSPORT_STATE_H__

#include <QtGlobal>

constexpr float TransportMinSpeed = 0.1f;
constexpr float TransportMaxSpeed = 2.0f;

struct TransportState
{
    qint64 currentTick = 0;
    bool playing = false;
    float speed = 1.0f;
    qint64 loopStartTick = 0;
    qint64 loopEndTick = 0;
    bool followPlayback = true;
};

inline qint64 validTransportTick(qint64 tick)
{
    return tick > 0 ? tick : 0;
}

inline void setTransportCurrentTick(TransportState& state, qint64 tick)
{
    state.currentTick = validTransportTick(tick);
}

inline void advanceTransportTick(TransportState& state, qint64 delta)
{
    setTransportCurrentTick(state, state.currentTick + delta);
}

inline void setTransportPlaying(TransportState& state, bool playing)
{
    state.playing = playing;
}

inline void setTransportSpeed(TransportState& state, float speed)
{
    if (speed < TransportMinSpeed)
        speed = TransportMinSpeed;
    if (speed > TransportMaxSpeed)
        speed = TransportMaxSpeed;
    state.speed = speed;
}

inline void setTransportLoop(TransportState& state, qint64 startTick, qint64 endTick)
{
    state.loopStartTick = validTransportTick(startTick);
    state.loopEndTick = validTransportTick(endTick);
    if (state.loopEndTick <= state.loopStartTick)
        state.loopEndTick = state.loopStartTick;
}

inline bool transportLoopEnabled(const TransportState& state)
{
    return state.loopEndTick > state.loopStartTick;
}

inline bool transportLoopReached(const TransportState& state)
{
    return transportLoopEnabled(state) && state.currentTick > state.loopEndTick;
}

#endif
