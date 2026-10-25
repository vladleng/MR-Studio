#pragma once
#include "Controls.h"

namespace ui {
// Show metadata only. Audio, plugins and song edits remain in .mrsproject.
struct LiveDocument {
    juce::String title{"MR Live"};
    std::vector<juce::File> projects;
    int active{-1};

    static LiveDocument read(const juce::File& file) {
        if (!file.existsAsFile() || file.getSize()>1024*1024)
            throw std::runtime_error("Missing or oversized MR Live show");
        juce::var json;const auto result=juce::JSON::parse(file.loadFileAsString(),json);
        if(result.failed() || !json.isObject() || json["schema"].toString()!="mr-live-show" ||
           !json["version"].isInt() || int(json["version"])!=1 || !json["projects"].isArray() ||
           !json["title"].isString() || !json["active"].isInt())
            throw std::runtime_error("Unsupported MR Live show document");
        LiveDocument show;show.title=json["title"].toString();show.active=int(json["active"]);
        const auto* refs=json["projects"].getArray();
        if(refs->size()>1024)throw std::runtime_error("MR Live show has too many project references");
        for(const auto& ref:*refs){
            if(!ref.isString() || ref.toString().isEmpty())throw std::runtime_error("Invalid MR Live project reference");
            auto project=file.getParentDirectory().getChildFile(ref.toString());
            if(!project.hasFileExtension("mrsproject"))throw std::runtime_error("MR Live songs must reference .mrsproject files");
            show.projects.push_back(project);
        }
        if(show.active < -1 || show.active>=static_cast<int>(show.projects.size()) || (!show.projects.empty()&&show.active<0))
            throw std::runtime_error("Invalid active MR Live song");
        return show;
    }
    void write(const juce::File& file) const {
        auto object=std::make_unique<juce::DynamicObject>();object->setProperty("schema","mr-live-show");
        object->setProperty("version",1);object->setProperty("title",title);object->setProperty("active",active);
        juce::Array<juce::var> refs;for(const auto& project:projects)refs.add(project.getRelativePathFrom(file.getParentDirectory()));
        object->setProperty("projects",refs);
        const auto bytes=juce::JSON::toString(juce::var(object.release()));
        if(file.getParentDirectory().createDirectory().failed())throw std::runtime_error("Cannot create MR Live folder");
        juce::TemporaryFile temp(file);
        if(!temp.getFile().replaceWithText(bytes) || !temp.overwriteTargetFileWithTemporary())
            throw std::runtime_error("Cannot save MR Live show");
    }
};
}
