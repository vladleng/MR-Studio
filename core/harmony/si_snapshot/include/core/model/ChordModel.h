// Snapshot from https://github.com/vladleng/Smart-Improviser/blob/28f6bb3a4df06bb179a6a51a0415b99fc1b1a1f1/src/core/model/ChordModel.h
// Same project owner. Keep this unmodified for a compatible shared-harmony extraction.
#pragma once

#include "core/context/HarmonicContext.h"

#include <array>
#include <cstdint>
#include <string>

namespace smartimproviser::harmony
{
enum class ChordQuality : std::uint8_t
{
    undefined = 0,
    major,
    minor,
    dominant,
    diminished,
    halfDiminished,
    augmented,
    suspended2,
    suspended4,
    power,
    noThird,
    unknown
};

enum class ChordExtension : std::uint16_t
{
    none = 0,
    sixth = 1u << 0,
    minorSeventh = 1u << 1,
    majorSeventh = 1u << 2,
    ninth = 1u << 3,
    eleventh = 1u << 4,
    thirteenth = 1u << 5
};

enum class ChordAlteration : std::uint16_t
{
    none = 0,
    flatFifth = 1u << 0,
    sharpFifth = 1u << 1,
    flatNinth = 1u << 2,
    sharpNinth = 1u << 3,
    sharpEleventh = 1u << 4,
    flatThirteenth = 1u << 5
};

constexpr std::uint16_t flag(ChordExtension value) noexcept
{
    return static_cast<std::uint16_t>(value);
}

constexpr std::uint16_t flag(ChordAlteration value) noexcept
{
    return static_cast<std::uint16_t>(value);
}

struct NormalizedChord
{
    bool valid = false;

    std::int32_t rootFifths = 0;
    std::int32_t bassFifths = 0;
    int rootPitchClass = 0;
    int bassPitchClass = 0;
    bool slashBass = false;

    ChordQuality quality = ChordQuality::undefined;
    std::uint16_t extensions = 0;
    std::uint16_t alterations = 0;

    std::array<bool, kPitchClassCount> tones {};
    std::array<std::uint8_t, kPitchClassCount> degrees {};

    bool hasTone(int semitones) const noexcept;
    bool hasDegree(int degree) const noexcept;
    bool hasExtension(ChordExtension extension) const noexcept;
    bool hasAlteration(ChordAlteration alteration) const noexcept;
};

int circleOfFifthsToPitchClass(std::int32_t fifths) noexcept;
NormalizedChord normalizeChord(const ChordContext& source) noexcept;
const char* chordQualityName(ChordQuality quality) noexcept;
std::string normalizedChordSymbol(const NormalizedChord& chord);
}
