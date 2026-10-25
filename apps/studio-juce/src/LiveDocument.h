#pragma once
#include "LiveShowState.h"
#include <vector>

namespace ui {
// Metadata document. Song/audio/plugin state is always owned by .mrsproject.
struct LiveDocument {
    juce::String title{"MR Live"};
    std::vector<juce::File> projects;
    int active{-1};
    juce::var interface;
    std::map<juce::String, int> itemProjects;

    static juce::String projectLabel(const juce::File &file) {
        const auto label = file.getFileNameWithoutExtension().trim().substring(0, 120);
        return label.isEmpty() ? juce::String("Studio project") : label;
    }
    void ensureInterface(const juce::var &defaults = {}) {
        if (interface.isObject())
            return;
        interface = live::empty(defaults);
        for (std::size_t i = 0; i < projects.size(); ++i) {
            const auto id = live::addItem(interface, projectLabel(projects[i]));
            live::insertEntry(interface, id);
            itemProjects[id] = static_cast<int>(i);
            if (static_cast<int>(i) == active)
                live::put(interface, "currentItem", id);
        }
    }
    LiveDocument clone() const {
        auto result = *this;
        result.interface = interface.clone();
        return result;
    }
    int projectFor(const juce::String &id) const {
        const auto found = itemProjects.find(id);
        return found == itemProjects.end() ? -1 : found->second;
    }
    bool currentLinked() const {
        return !interface.isObject() ? active >= 0 : projectFor(interface["currentItem"].toString()) >= 0;
    }
    void validate() const {
        if (projects.size() > 1024 || active < -1 || active >= static_cast<int>(projects.size()))
            throw std::runtime_error("Invalid MR Live project selection");
        for (const auto &file : projects)
            if (!file.hasFileExtension("mrsproject"))
                throw std::runtime_error("MR Live songs must reference .mrsproject");
        if (interface.isObject()) {
            live::validate(interface);
            for (const auto &[id, index] : itemProjects)
                if (!live::find(interface, "items", id).isObject() || index < 0 ||
                    index >= static_cast<int>(projects.size()))
                    throw std::runtime_error("Invalid MR Live item project binding");
            if (projectFor(interface["currentItem"].toString()) != active)
                throw std::runtime_error("Show selection and project binding differ");
        }
    }
    juce::var json(const juce::File &file, bool absolute = false) const {
        validate();
        auto value = live::object();
        live::put(value, "schema", "mr-live-show");
        live::put(value, "version", interface.isObject() ? 2 : 1);
        live::put(value, "title", title);
        live::put(value, "active", active);
        juce::Array<juce::var> refs;
        for (const auto &project : projects)
            refs.add(absolute ? project.getFullPathName() : project.getRelativePathFrom(file.getParentDirectory()));
        live::put(value, "projects", refs);
        if (interface.isObject()) {
            live::put(value, "interface", interface.clone());
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
            result.interface = value["interface"].clone();
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
