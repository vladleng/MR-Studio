#pragma once
#include <mrs/core.hpp>
#include <span>
#include <filesystem>
namespace mrs {
struct MidiPart { std::string name; int channel{}; MidiClip midi; };
// Non-RT, bounded SMF 0/1 parser. Tempo/meter metadata does not alter the project.
std::vector<MidiPart> parse_midi_file(std::span<const std::byte> bytes);
// Worker validation accepts a structurally valid, empty transcription file.
// Ordinary import retains its existing nonempty requirement.
std::vector<MidiPart> validate_transcription_midi(std::span<const std::byte> bytes);
std::vector<MidiPart> load_midi_file(const std::filesystem::path&);
}
