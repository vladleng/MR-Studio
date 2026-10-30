#pragma once
#include "LiveShowContextImport.h"
#include "LiveStudioOneChordDecoder.h"
#include <juce_core/juce_core.h>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

// Read-only source adapter for the observed Studio One Show/show.xml schema.
// No .song paths, media, VSTs or second project/audio engine are opened.
namespace ui::live {
struct StudioOneShowScan {
    std::vector<ShowContextSong> items; // original SetList order; titles need not be unique
};

inline std::string normalizeStudioOneXml(const std::string& src) {
    if (src.find("<!DOCTYPE") != std::string::npos || src.find("<!ENTITY") != std::string::npos ||
        src.find("<!doctype") != std::string::npos || src.find("<!entity") != std::string::npos)
        throw std::runtime_error("Studio One XML DTD/entities are unsupported");
    std::string out;
    out.reserve(src.size());
    bool inTag = false, inName = false;
    char quoted = 0;
    for (std::size_t i = 0; i < src.size();) {
        const char c = src[i];
        if (!inTag) {
            out += c;
            if (c == '<') { inTag = true; inName = true; }
            ++i;
        } else if (quoted) {
            out += c;
            if (c == quoted) quoted = 0;
            ++i;
        } else if (c == '"' || c == '\'') {
            out += c; quoted = c; ++i;
        } else if (c == '>') {
            out += c; inTag = inName = false; ++i;
        } else if (inName && c == ':' && i + 1 < src.size() && src[i + 1] == ':') {
            out += "__"; i += 2; // MixerSnapshot::ChannelState is not a legal XML name
        } else if (!inName && (i == 0 || std::isspace(static_cast<unsigned char>(src[i-1]))) &&
                   src.compare(i, 4, "x:id") == 0 && i + 4 < src.size() &&
                   (src[i + 4] == '=' || std::isspace(static_cast<unsigned char>(src[i + 4])))) {
            out += "x_id"; i += 4; // Studio One leaves prefix x unbound
        } else {
            if (inName && std::isspace(static_cast<unsigned char>(c))) inName = false;
            out += c; ++i;
        }
    }
    return out;
}

inline void guardStudioOneXmlComplexity(const std::string& xml) {
    std::size_t nodes = 0;
    int depth = 0;
    for (std::size_t i = 0; i < xml.size();) {
        if (xml[i] != '<') { ++i; continue; }
        if (xml.compare(i, 4, "<!--") == 0) {
            const auto end = xml.find("-->", i + 4);
            if (end == std::string::npos) throw std::runtime_error("Unterminated Studio One XML comment");
            i = end + 3; continue;
        }
        if (xml.compare(i, 9, "<![CDATA[") == 0) {
            const auto end = xml.find("]]>", i + 9);
            if (end == std::string::npos) throw std::runtime_error("Unterminated Studio One XML CDATA");
            i = end + 3; continue;
        }
        const bool endTag = i + 1 < xml.size() && xml[i + 1] == '/';
        const bool declaration = i + 1 < xml.size() && (xml[i + 1] == '?' || xml[i + 1] == '!');
        char quote = 0;
        std::size_t j = i + 1;
        for (; j < xml.size(); ++j) {
            if (quote) { if (xml[j] == quote) quote = 0; }
            else if (xml[j] == '"' || xml[j] == '\'') quote = xml[j];
            else if (xml[j] == '>') break;
        }
        if (j == xml.size()) throw std::runtime_error("Unterminated Studio One XML tag");
        if (!declaration) {
            if (++nodes > 100000) throw std::runtime_error("Studio One XML node limit exceeded");
            const bool selfClosing = j > i && xml[j - 1] == '/';
            if (endTag) --depth;
            else if (!selfClosing) ++depth;
            if (depth < 0 || depth > 64) throw std::runtime_error("Studio One XML nesting limit exceeded");
        }
        i = j + 1;
    }
    if (depth != 0) throw std::runtime_error("Unbalanced Studio One XML elements");
}

inline const juce::XmlElement* studioOneChild(const juce::XmlElement* parent,
                                              const juce::String& tag, const juce::String& sourceId = {}) {
    if (!parent) return nullptr;
    for (auto* node = parent->getFirstChildElement(); node; node = node->getNextElement())
        if (node->hasTagName(tag) &&
            (sourceId.isEmpty() || node->getStringAttribute("x_id") == sourceId))
            return node;
    return nullptr;
}
inline double studioOneNumber(const juce::XmlElement& node, const char* attribute) {
    if (!node.hasAttribute(attribute)) throw std::runtime_error("Studio One numeric attribute missing");
    const auto bytes = node.getStringAttribute(attribute).trim().toStdString();
    std::size_t used = 0;
    double number = 0;
    try { number = std::stod(bytes, &used); }
    catch (const std::exception&) { throw std::runtime_error("Studio One numeric attribute invalid"); }
    if (used != bytes.size() || !std::isfinite(number))
        throw std::runtime_error("Studio One numeric attribute invalid");
    return number;
}

inline StudioOneShowScan readStudioOneShowXml(const std::string& sourceXml) {
    if (sourceXml.size() > 8 * 1024 * 1024)
        throw std::runtime_error("Studio One Show XML exceeds 8 MiB");
    auto normalized = normalizeStudioOneXml(sourceXml);
    guardStudioOneXmlComplexity(normalized);
    // Studio One writes a UTF-8 BOM before the XML declaration.
    if (normalized.size() >= 3 &&
        static_cast<unsigned char>(normalized[0]) == 0xef &&
        static_cast<unsigned char>(normalized[1]) == 0xbb &&
        static_cast<unsigned char>(normalized[2]) == 0xbf)
        normalized.erase(0, 3);
    juce::XmlDocument parser(juce::String::fromUTF8(normalized.data(), static_cast<int>(normalized.size())));
    auto root = parser.getDocumentElement();
    if (!root || !root->hasTagName("Show")) throw std::runtime_error("Unsupported Studio One XML");
    const auto* model = studioOneChild(root.get(), "Attributes", "Model");
    const auto* setList = studioOneChild(model, "Attributes", "setList");
    const auto* items = studioOneChild(setList, "List", "Items");
    if (!items) throw std::runtime_error("Studio One SetList Items not found");

    StudioOneShowScan result;
    int ordinal = 0;
    for (auto* item = items->getFirstChildElement(); item; item = item->getNextElement()) {
        if (!item->hasTagName("SetListItem")) continue;
        if (++ordinal > 500) throw std::runtime_error("Studio One Show item limit exceeded");
        ShowContextSong source;
        source.sourceItemOrdinal = ordinal - 1;
        if (const auto* meta = studioOneChild(item, "PersistentAttributes", "metaInfo"))
            for (auto* attr = meta->getFirstChildElement(); attr; attr = attr->getNextElement())
                if (attr->hasTagName("Attribute") && attr->getStringAttribute("id") == "Media:Title") {
                    source.sourceItemTitle = attr->getStringAttribute("value").trim().substring(0,120);
                    break;
                }
        if (source.sourceItemTitle.isEmpty()) source.sourceItemTitle = "Untitled item " + juce::String(ordinal);

        try {
            const double secQuarter = studioOneNumber(*item, "tempo");
            const double num = studioOneNumber(*item, "timeSignature.numerator");
            const double den = studioOneNumber(*item, "timeSignature.denominator");
            if (secQuarter <= 0. || num < 1. || num > 32. || den < 1. || den > 32. ||
                std::floor(num) != num || std::floor(den) != den)
                throw std::runtime_error("Invalid source tempo/meter");
            source.sourceMeter = juce::String(static_cast<int>(num)) + "/" +
                                 juce::String(static_cast<int>(den));
            const double secBar = secQuarter * num * 4. / den;
            const double bpm = 60. / secQuarter;
            if (bpm >= 20. && bpm <= 400.) source.bpm = bpm;
            else source.warnings.add("Tempo outside MR Live 20..400 BPM range");
            const double bars = studioOneNumber(*item, "length") / secBar;
            if (bars >= 1. && bars <= 4096.) source.lengthBars = bars;
            else source.warnings.add("Item length outside MR Live 1..4096 bars range");

            for (const auto& laneName : std::array<const char*,2>{"sections","chords"}) {
                const bool isChords = juce::String(laneName) == "chords";
                const auto* lane = studioOneChild(item, "List", laneName);
                if (!lane) continue; // unavailable, never an empty replacement
                juce::Array<juce::var> parsed, raw;
                bool compatible = true;
                int count = 0;
                for (auto* span = lane->getFirstChildElement(); span; span = span->getNextElement()) {
                    if (!span->hasTagName(isChords ? "SetListItemChord" : "SetListItemSection")) continue;
                    if (++count > (isChords ? 256 : 128)) { compatible = false; break; }
                    try {
                        if (span->getStringAttribute("timeFormat") != "0")
                            throw std::runtime_error("Unknown source musical time format");
                        const double start = studioOneNumber(*span,"localOffset") / secBar;
                        const double length = studioOneNumber(*span,"length") / secBar;
                        if (start < 0. || length < .25 - 1.e-7 || start + length > 4096. + 1.e-7)
                            throw std::runtime_error("Musical span outside MR Live limits");
                        auto row = object();
                        put(row,"id",freshId()); put(row,"start",start); put(row,"length",length);
                        if (isChords) {
                            const auto* encoded = studioOneChild(span,"Attributes","chord");
                            if (!encoded) throw std::runtime_error("Missing encoded chord data");
                            for (const auto* key : {"root","intervals","type","bass"})
                                put(row,key,encoded->getStringAttribute(key));
                            raw.add(row);
                            auto normalized = object();
                            put(normalized, "id", row["id"]);
                            put(normalized, "start", start);
                            put(normalized, "length", length);
                            put(normalized, "symbol", decodeStudioOneChord(*encoded));
                            parsed.add(normalized);
                        } else {
                            const auto name = span->getStringAttribute("name").trim().substring(0,120);
                            if (name.isEmpty()) throw std::runtime_error("Unnamed source section");
                            put(row,"name",name);
                            // Source ARGB has no verified one-to-one MR UDS mapping.
                            parsed.add(row);
                        }
                    } catch (const std::exception&) {
                        compatible = false; // category remains unavailable; no partial replacement
                    }
                }
                if (isChords) {
                    source.sourceChordCount = count;
                    source.rawChords = raw;
                    if (count && compatible) {
                        try { validateMusicalRows(juce::var(parsed), MusicalLane::chords); }
                        catch (const std::exception&) { compatible = false; }
                    }
                    if (count && compatible) source.chords = juce::var(parsed);
                    if (count && !compatible)
                        source.warnings.add("Chords contain unsupported symbols or invalid/overlapping spans");
                } else {
                    source.sourceSectionCount = count;
                    if (count && compatible) {
                        try { validateMusicalRows(juce::var(parsed), MusicalLane::sections); }
                        catch (const std::exception&) { compatible = false; }
                    }
                    if (count && compatible) source.sections = juce::var(parsed);
                    if (count && !compatible)
                        source.warnings.add("Sections include missing/short/overlapping spans");
                }
            }
        } catch (const std::exception&) {
            source.warnings.add("Could not decode source item tempo, meter or length");
        }
        result.items.push_back(std::move(source));
    }
    return result;
}

inline StudioOneShowScan readStudioOneShow(const juce::File& file) {
    if (!file.existsAsFile() || !file.hasFileExtension("show") || file.getSize() < 1 ||
        file.getSize() > 512ll * 1024 * 1024)
        throw std::runtime_error("Missing, oversized or unsupported Studio One .show");
    juce::ZipFile zip(file);
    const int count = zip.getNumEntries();
    if (count < 1 || count > 4096) throw std::runtime_error("Invalid Studio One ZIP");
    int xmlEntry = -1;
    std::uint64_t totalExpanded = 0;
    for (int i=0; i<count; ++i) {
        const auto* entry = zip.getEntry(i);
        if (!entry) throw std::runtime_error("Invalid Studio One archive member");
        const auto path = entry->filename.replaceCharacter('\\','/');
        if (path.startsWithChar('/') || path.contains("../") || path == ".." || path.contains(":/"))
            throw std::runtime_error("Unsafe archive path");
        if (entry->uncompressedSize < 0 ||
            static_cast<std::uint64_t>(entry->uncompressedSize) > 1024ull * 1024 * 1024 - totalExpanded)
            throw std::runtime_error("Excessive total Show archive size");
        totalExpanded += static_cast<std::uint64_t>(entry->uncompressedSize);
        if (entry->filename == "Show/show.xml") {
            if (xmlEntry >= 0) throw std::runtime_error("Duplicate Show/show.xml");
            if (entry->uncompressedSize < 1 || entry->uncompressedSize > 8 * 1024 * 1024)
                throw std::runtime_error("Oversized Studio One Show XML");
            xmlEntry = i;
        }
    }
    if (xmlEntry < 0) throw std::runtime_error("Missing Show/show.xml");
    auto stream = zip.createStreamForEntry(xmlEntry);
    if (!stream) throw std::runtime_error("Unreadable Show/show.xml");
    std::string xml;
    std::array<char,16*1024> buffer{};
    for (;;) {
        const int amount = stream->read(buffer.data(), static_cast<int>(buffer.size()));
        if (amount <= 0) break;
        if (xml.size() + static_cast<std::size_t>(amount) > 8 * 1024 * 1024)
            throw std::runtime_error("Studio One XML expands beyond limit");
        xml.append(buffer.data(), static_cast<std::size_t>(amount));
    }
    return readStudioOneShowXml(xml);
}
} // namespace ui::live
