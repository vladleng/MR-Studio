#pragma once
#include "LivePlaybackState.h"
#include "LiveShowState.h"
#include <vector>

namespace ui {
// Show-owned metadata/media references; current Playback is projected into shared Core.
// Studio-project references below are retained only for legacy compatibility.
struct LiveDocument {
    juce::String title{"MR Live"};
    std::vector<juce::File> projects;
    int active{-1};
    juce::var interfaceState;
    std::map<juce::String, int> itemProjects;

    static juce::String projectLabel(const juce::File &file) {
        const auto label = file.getFileNameWithoutExtension().trim().substring(0, 120);
        return label.isEmpty() ? juce::String("Studio project") : label;
    }
    void ensureInterface(const juce::var &defaults = {}) {
        if (interfaceState.isObject())
            return;
        interfaceState = live::empty(defaults);
        for (std::size_t i = 0; i < projects.size(); ++i) {
            const auto id = live::addItem(interfaceState, projectLabel(projects[i]));
            live::insertEntry(interfaceState, id);
            itemProjects[id] = static_cast<int>(i);
            if (static_cast<int>(i) == active)
                live::put(interfaceState, "currentItem", id);
        }
    }
    LiveDocument clone() const {
        auto result = *this;
        result.interfaceState = interfaceState.clone();
        return result;
    }
    int projectFor(const juce::String &id) const {
        const auto found = itemProjects.find(id);
        return found == itemProjects.end() ? -1 : found->second;
    }
    bool currentLinked() const {
        return !interfaceState.isObject() ? active >= 0 : projectFor(interfaceState["currentItem"].toString()) >= 0;
    }
    void validate() const {
        if (projects.size() > 1024 || active < -1 || active >= static_cast<int>(projects.size()))
            throw std::runtime_error("Invalid MR Live project selection");
        for (const auto &file : projects)
            if (!file.hasFileExtension("mrsproject"))
                throw std::runtime_error("MR Live songs must reference .mrsproject");
        if (interfaceState.isObject()) {
            live::validate(interfaceState);
            live::validatePlayback(interfaceState);
            for (const auto &[id, index] : itemProjects)
                if (!live::find(interfaceState, "items", id).isObject() || index < 0 ||
                    index >= static_cast<int>(projects.size()))
                    throw std::runtime_error("Invalid MR Live item project binding");
            if (projectFor(interfaceState["currentItem"].toString()) != active)
                throw std::runtime_error("Show selection and project binding differ");
        }
    }
    juce::var json(const juce::File &file, bool absolute = false) const {
        validate();
        auto value = live::object();
        live::put(value, "schema", "mr-live-show");
        live::put(value, "version", interfaceState.isObject() ? 2 : 1);
        live::put(value, "title", title);
        live::put(value, "active", active);
        juce::Array<juce::var> refs;
        for (const auto &project : projects)
            refs.add(absolute ? project.getFullPathName() : project.getRelativePathFrom(file.getParentDirectory()));
        live::put(value, "projects", refs);
        if (interfaceState.isObject()) {
            auto savedInterface = interfaceState.clone();
            if (!absolute)
                live::playbackPaths(savedInterface, file, false);
            live::put(value, "interface", savedInterface);
            auto links = live::object();
            for (const auto &[id, index] : itemProjects)
                live::put(links, id, index);
            live::put(value, "itemProjects", links);
        }
        return value;
    }
    juce::String signature() const { return live::canonical(json({}, true)); }
    static LiveDocument read(const juce::File &file) {
        if (!file.existsAsFile() || file.getSize() > 8 * 1024 * 1024)
            throw std::runtime_error("Missing or oversized MR Live show");
        juce::var value;
        const auto parsed = juce::JSON::parse(file.loadFileAsString(), value);
        const int version = static_cast<int>(value["version"]);
        if (parsed.failed() || !value.isObject() || value["schema"].toString() != "mr-live-show" ||
            !value["version"].isInt() || (version != 1 && version != 2) || !value["projects"].isArray() ||
            !value["title"].isString() || !value["active"].isInt())
            throw std::runtime_error("Unsupported MR Live show document");
        LiveDocument result;
        result.title = value["title"].toString();
        result.active = static_cast<int>(value["active"]);
        if (value["projects"].size() > 1024)
            throw std::runtime_error("Too many MR Live project references");
        for (const auto &ref : *value["projects"].getArray()) {
            if (!ref.isString() || ref.toString().isEmpty())
                throw std::runtime_error("Invalid project reference");
            result.projects.push_back(file.getParentDirectory().getChildFile(ref.toString()));
        }
        if (version == 1) {
            if (!result.projects.empty() && result.active < 0)
                throw std::runtime_error("Invalid active MR Live song");
            result.ensureInterface();
        } else {
            if (!value["interface"].isObject() || !value["itemProjects"].isObject())
                throw std::runtime_error("Missing MR Live interface metadata");
            result.interfaceState = value["interface"].clone();
            live::validatePlayback(result.interfaceState);
            live::playbackPaths(result.interfaceState, file, true);
            for (const auto &pair : value["itemProjects"].getDynamicObject()->getProperties()) {
                if (!pair.value.isInt())
                    throw std::runtime_error("Invalid item project index");
                result.itemProjects.emplace(pair.name.toString(), static_cast<int>(pair.value));
            }
        }
        result.validate();
        return result;
    }
    void write(const juce::File &file) const {
        const auto bytes = juce::JSON::toString(json(file), false, 17);
        if (bytes.getNumBytesAsUTF8() > 8 * 1024 * 1024)
            throw std::runtime_error("MR Live show exceeds 8 MiB");
        if (file.getParentDirectory().createDirectory().failed())
            throw std::runtime_error("Cannot create MR Live folder");
        juce::TemporaryFile temporary(file);
        if (!temporary.getFile().replaceWithText(bytes) || !temporary.overwriteTargetFileWithTemporary())
            throw std::runtime_error("Cannot save MR Live show");
    }
};
} // namespace ui
