// Snapshot from https://github.com/vladleng/Smart-Improviser/blob/28f6bb3a4df06bb179a6a51a0415b99fc1b1a1f1/src/core/model/ChordModel.cpp
// Same project owner. Keep this unmodified for a compatible shared-harmony extraction.
#include "core/model/ChordModel.h"

namespace smartimproviser::harmony
{
namespace
{
constexpr std::uint8_t genericUsedValue = 0xFFu;

bool explicitDegreeAt(const NormalizedChord& chord, int semitones, int degree) noexcept
{
    return semitones >= 0
        && semitones < kPitchClassCount
        && chord.degrees[static_cast<std::size_t>(semitones)] == degree;
}

void addExtension(NormalizedChord& chord, ChordExtension extension) noexcept
{
    chord.extensions |= flag(extension);
}

void addAlteration(NormalizedChord& chord, ChordAlteration alteration) noexcept
{
    chord.alterations |= flag(alteration);
}

std::string fifthsName(std::int32_t fifths)
{
    switch (fifths)
    {
        case -7: return "Cb";
        case -6: return "Gb";
        case -5: return "Db";
        case -4: return "Ab";
        case -3: return "Eb";
        case -2: return "Bb";
        case -1: return "F";
        case  0: return "C";
        case  1: return "G";
        case  2: return "D";
        case  3: return "A";
        case  4: return "E";
        case  5: return "B";
        case  6: return "F#";
        case  7: return "C#";
        case  8: return "G#";
        case  9: return "D#";
        case 10: return "A#";
        case 11: return "E#";
        default: break;
    }

    static constexpr const char* pitchClassNames[kPitchClassCount] =
        { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    return pitchClassNames[circleOfFifthsToPitchClass(fifths)];
}

bool hasUnalteredNinth(const NormalizedChord& chord) noexcept
{
    return chord.hasExtension(ChordExtension::ninth)
        && ! chord.hasAlteration(ChordAlteration::flatNinth)
        && ! chord.hasAlteration(ChordAlteration::sharpNinth);
}

bool hasUnalteredEleventh(const NormalizedChord& chord) noexcept
{
    return chord.hasExtension(ChordExtension::eleventh)
        && ! chord.hasAlteration(ChordAlteration::sharpEleventh);
}

bool hasUnalteredThirteenth(const NormalizedChord& chord) noexcept
{
    return chord.hasExtension(ChordExtension::thirteenth)
        && ! chord.hasAlteration(ChordAlteration::flatThirteenth);
}
}

bool NormalizedChord::hasTone(int semitones) const noexcept
{
    return semitones >= 0
        && semitones < kPitchClassCount
        && tones[static_cast<std::size_t>(semitones)];
}

bool NormalizedChord::hasDegree(int degree) const noexcept
{
    if (degree <= 0 || degree > 13)
        return false;

    for (const auto value : degrees)
        if (value == degree)
            return true;

    return false;
}

bool NormalizedChord::hasExtension(ChordExtension extension) const noexcept
{
    return (extensions & flag(extension)) != 0;
}

bool NormalizedChord::hasAlteration(ChordAlteration alteration) const noexcept
{
    return (alterations & flag(alteration)) != 0;
}

int circleOfFifthsToPitchClass(std::int32_t fifths) noexcept
{
    auto pitchClass = static_cast<int>((fifths * 7) % kPitchClassCount);
    if (pitchClass < 0)
        pitchClass += kPitchClassCount;
    return pitchClass;
}

NormalizedChord normalizeChord(const ChordContext& source) noexcept
{
    NormalizedChord result;
    result.rootFifths = source.root;
    result.bassFifths = source.bass;
    result.rootPitchClass = circleOfFifthsToPitchClass(source.root);
    result.bassPitchClass = circleOfFifthsToPitchClass(source.bass);
    result.slashBass = source.bass != source.root;

    if (! source.available || ! source.defined || ! source.intervals.any())
        return result;

    result.valid = true;

    for (int semitones = 0; semitones < kPitchClassCount; ++semitones)
    {
        const auto usage = source.intervals.values[static_cast<std::size_t>(semitones)];
        if (usage == 0)
            continue;

        result.tones[static_cast<std::size_t>(semitones)] = true;

        if (usage != genericUsedValue && usage >= 1 && usage <= 13)
            result.degrees[static_cast<std::size_t>(semitones)] = usage;
    }

    const auto minorThird = result.hasTone(3) && ! explicitDegreeAt(result, 3, 9);
    const auto majorThird = result.hasTone(4);
    const auto perfectFifth = result.hasTone(7);

    const auto explicitFlatFifth = explicitDegreeAt(result, 6, 5);
    const auto explicitSharpEleventh = explicitDegreeAt(result, 6, 11);
    const auto structuralFlatFifth = result.hasTone(6) && ! perfectFifth && ! explicitSharpEleventh;

    const auto explicitSharpFifth = explicitDegreeAt(result, 8, 5);
    const auto explicitFlatThirteenth = explicitDegreeAt(result, 8, 13);
    const auto structuralSharpFifth = result.hasTone(8)
                                   && ! perfectFifth
                                   && majorThird
                                   && ! explicitFlatThirteenth;

    const auto minorSeventh = result.hasTone(10);
    const auto majorSeventh = result.hasTone(11);
    const auto diminishedSeventh = explicitDegreeAt(result, 9, 7);

    if (minorSeventh) addExtension(result, ChordExtension::minorSeventh);
    if (majorSeventh) addExtension(result, ChordExtension::majorSeventh);
    if (result.hasDegree(6)) addExtension(result, ChordExtension::sixth);
    if (result.hasDegree(9)) addExtension(result, ChordExtension::ninth);
    if (result.hasDegree(11)) addExtension(result, ChordExtension::eleventh);
    if (result.hasDegree(13)) addExtension(result, ChordExtension::thirteenth);

    if (explicitFlatFifth || structuralFlatFifth) addAlteration(result, ChordAlteration::flatFifth);
    if (explicitSharpFifth || structuralSharpFifth) addAlteration(result, ChordAlteration::sharpFifth);
    if (explicitDegreeAt(result, 1, 9)) addAlteration(result, ChordAlteration::flatNinth);
    if (explicitDegreeAt(result, 3, 9)) addAlteration(result, ChordAlteration::sharpNinth);
    if (explicitSharpEleventh) addAlteration(result, ChordAlteration::sharpEleventh);
    if (explicitFlatThirteenth) addAlteration(result, ChordAlteration::flatThirteenth);

    const auto flatFifth = result.hasAlteration(ChordAlteration::flatFifth);
    const auto sharpFifth = result.hasAlteration(ChordAlteration::sharpFifth);

    if (minorThird && flatFifth && minorSeventh)
        result.quality = ChordQuality::halfDiminished;
    else if (minorThird && flatFifth)
        result.quality = ChordQuality::diminished;
    else if (majorThird && minorSeventh)
        result.quality = ChordQuality::dominant;
    else if (majorThird && sharpFifth)
        result.quality = ChordQuality::augmented;
    else if (majorThird)
        result.quality = ChordQuality::major;
    else if (minorThird)
        result.quality = ChordQuality::minor;
    else if (result.hasTone(2) && perfectFifth)
        result.quality = ChordQuality::suspended2;
    else if (result.hasTone(5) && perfectFifth)
        result.quality = ChordQuality::suspended4;
    else if (perfectFifth)
        result.quality = ChordQuality::power;
    else if (diminishedSeventh || result.hasTone(6) || result.hasTone(8))
        result.quality = ChordQuality::noThird;
    else
        result.quality = ChordQuality::unknown;

    return result;
}

const char* chordQualityName(ChordQuality quality) noexcept
{
    switch (quality)
    {
        case ChordQuality::undefined:      return "undefined";
        case ChordQuality::major:          return "major";
        case ChordQuality::minor:          return "minor";
        case ChordQuality::dominant:       return "dominant";
        case ChordQuality::diminished:     return "diminished";
        case ChordQuality::halfDiminished: return "half-diminished";
        case ChordQuality::augmented:      return "augmented";
        case ChordQuality::suspended2:     return "sus2";
        case ChordQuality::suspended4:     return "sus4";
        case ChordQuality::power:          return "power";
        case ChordQuality::noThird:        return "no-third";
        case ChordQuality::unknown:        return "unknown";
    }
    return "unknown";
}

std::string normalizedChordSymbol(const NormalizedChord& chord)
{
    if (! chord.valid)
        return "(no chord)";

    std::string result = fifthsName(chord.rootFifths);
    const auto b7 = chord.hasExtension(ChordExtension::minorSeventh);
    const auto maj7 = chord.hasExtension(ChordExtension::majorSeventh);
    const auto six = chord.hasExtension(ChordExtension::sixth);
    const auto nine = hasUnalteredNinth(chord);
    const auto eleven = hasUnalteredEleventh(chord);
    const auto thirteen = hasUnalteredThirteenth(chord);

    switch (chord.quality)
    {
        case ChordQuality::major:
            if (maj7)
            {
                if (thirteen) result += "maj13";
                else if (eleven) result += "maj11";
                else if (nine) result += "maj9";
                else result += "maj7";
            }
            else if (six)
                result += nine ? "6/9" : "6";
            break;
        case ChordQuality::minor:
            if (b7)
            {
                if (thirteen) result += "m13";
                else if (eleven) result += "m11";
                else if (nine) result += "m9";
                else result += "m7";
            }
            else if (maj7)
                result += "m(maj7)";
            else if (six)
                result += nine ? "m6/9" : "m6";
            else
                result += "m";
            break;
        case ChordQuality::dominant:
            if (thirteen) result += "13";
            else if (eleven) result += "11";
            else if (nine) result += "9";
            else result += "7";
            break;
        case ChordQuality::diminished:
            result += (chord.hasDegree(7) && chord.hasTone(9)) ? "dim7" : "dim";
            break;
        case ChordQuality::halfDiminished: result += "m7b5"; break;
        case ChordQuality::augmented: result += "aug"; break;
        case ChordQuality::suspended2:
            if (maj7) result += "maj7sus2";
            else if (b7) result += "7sus2";
            else result += "sus2";
            break;
        case ChordQuality::suspended4:
            if (maj7) result += "maj7sus4";
            else if (b7) result += "7sus4";
            else result += "sus4";
            break;
        case ChordQuality::power: result += "5"; break;
        case ChordQuality::noThird: result += "(no3)"; break;
        case ChordQuality::unknown: result += "?"; break;
        case ChordQuality::undefined: return "(no chord)";
    }

    if (chord.hasAlteration(ChordAlteration::flatFifth)
        && chord.quality != ChordQuality::diminished
        && chord.quality != ChordQuality::halfDiminished)
        result += "b5";
    if (chord.hasAlteration(ChordAlteration::sharpFifth)
        && chord.quality != ChordQuality::augmented)
        result += "#5";
    if (chord.hasAlteration(ChordAlteration::flatNinth)) result += "b9";
    if (chord.hasAlteration(ChordAlteration::sharpNinth)) result += "#9";
    if (chord.hasAlteration(ChordAlteration::sharpEleventh)) result += "#11";
    if (chord.hasAlteration(ChordAlteration::flatThirteenth)) result += "b13";

    if (chord.slashBass)
        result += "/" + fifthsName(chord.bassFifths);

    return result;
}
}
