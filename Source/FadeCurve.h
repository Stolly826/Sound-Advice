#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

// The shape of a fade. The fade in and the fade out each pick their own.
// The numbers are saved in transitions, so new curves go on the end.
enum class FadeCurve
{
    defaultCurve = 0,  // sine/cosine (equal power): overlapping fades keep the mix's loudness steady
    linear,            // straight line
    exponential,       // fade out drops fast then tails off, fade in starts slow then rises fast
    logarithmic,       // fade out holds then drops late, fade in rises fast then levels off
    constantPower,     // no fade: full volume for the whole overlap, so both songs play at once
    numCurves
};

inline FadeCurve fadeCurveFromInt(int value)
{
    return juce::isPositiveAndBelow(value, (int)FadeCurve::numCurves) ? (FadeCurve)value
                                                                       : FadeCurve::linear;
}

inline const char* getFadeCurveName(FadeCurve curve)
{
    switch (curve)
    {
        case FadeCurve::defaultCurve:  return "Default";
        case FadeCurve::linear:        return "Linear";
        case FadeCurve::exponential:   return "Exponential";
        case FadeCurve::logarithmic:   return "Logarithmic";
        case FadeCurve::constantPower: return "Constant Power";
        default:                       return "Linear";
    }
}

// volume for a fade IN, p = 0 (start of the fade) to 1 (end of the fade)
// before the start it is silent and after the end it is full, whatever the curve
inline float fadeInLevel(float p, FadeCurve curve)
{
    if (p <= 0.0f) return 0.0f;
    if (p >= 1.0f) return 1.0f;

    switch (curve)
    {
        case FadeCurve::defaultCurve:  return std::sin(p * juce::MathConstants<float>::halfPi);
        case FadeCurve::exponential:   return p * p * p;
        case FadeCurve::logarithmic:   { const float q = 1.0f - p; return 1.0f - q * q * q; }
        case FadeCurve::constantPower: return 1.0f; // straight in at full volume
        case FadeCurve::linear:
        default:                       return p;
    }
}

// volume for a fade OUT, p = 0 (start of the fade) to 1 (end of the fade)
// the same curve run backwards: full before the start, silent after the end
// (constant power stays at full volume right up to the fade end, then cuts out)
inline float fadeOutLevel(float p, FadeCurve curve)
{
    return fadeInLevel(1.0f - p, curve);
}
