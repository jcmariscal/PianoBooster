#ifndef __PRACTICE_FEEDBACK_H__
#define __PRACTICE_FEEDBACK_H__

enum PracticeFeedbackKind
{
    PracticeFeedbackPending,
    PracticeFeedbackGood,
    PracticeFeedbackBad,
    PracticeFeedbackStopped,
    PracticeFeedbackMissed
};

inline bool practiceFeedbackActive(PracticeFeedbackKind kind)
{
    return kind != PracticeFeedbackPending;
}

#endif
