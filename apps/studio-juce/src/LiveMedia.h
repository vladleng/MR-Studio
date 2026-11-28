#pragma once
#include "LiveDocument.h"
#include <mrs/project_folders.hpp>
#include <mrs/sha256.hpp>
#include <filesystem>
#include <memory>

namespace ui::live {
namespace fs = std::filesystem;

inline fs::path mediaFsPath(const juce::File& file) {
    return fs::path(file.getFullPathName().toWideCharPointer());
}
inline juce::File mediaJuceFile(const fs::path& path) {
    return juce::File(juce::String(path.wstring().c_str()));
}

// Immutable imported media identity is its SHA-256, not an Item name or
// a display filename. Preserve the original basename for the existing UI.
// The shared MediaCopy owns publication, verification and rollback.
class ShowMediaCopy {
public:
    explicit ShowMediaCopy(const juce::File& show)
        : root_(mediaFsPath(show.getParentDirectory())), copies_(root_,false) {}
    juce::File own(const juce::File& file) {
        if (!file.existsAsFile() || !file.hasFileExtension("wav"))
            throw std::runtime_error("Missing or unsupported Show WAV: "+file.getFullPathName().toStdString());
        const auto source=mediaFsPath(file);
        const auto canonical=fs::canonical(source);
        const auto mediaRoot=fs::canonical(root_/"Media");
        if(const auto found=resolved_.find(canonical);found!=resolved_.end())
            return mediaJuceFile(found->second);
        // A Show's existing files must not be duplicated just because they
        // predate content-addressed naming (e.g. a portable exported Item).
        const auto within=canonical.lexically_relative(mediaRoot);
        if(!within.empty()&&!within.is_absolute()&&*within.begin()!="..") {
            const auto target=root_/"Media"/within;
            resolved_.emplace(canonical,target);
            return mediaJuceFile(target);
        }
        const auto digest=mrs::sha256_file(canonical);
        const auto folder=fs::path("Media")/digest;
        const auto absoluteFolder=root_/folder;
        if(fs::is_symlink(absoluteFolder))
            throw std::runtime_error("Show media identity directory must not be a symbolic link");
        auto relative=folder/source.filename();
        if(fs::exists(absoluteFolder)) {
            if(!fs::is_directory(absoluteFolder))
                throw std::runtime_error("Show media identity directory is not a folder");
            bool found=false;
            for(const auto& entry:fs::directory_iterator(absoluteFolder)) {
                if(entry.is_symlink()||!entry.is_regular_file()||
                   !mediaJuceFile(entry.path()).hasFileExtension("wav"))
                    throw std::runtime_error("Unexpected entry in Show media identity directory");
                // Exactly one content-addressed WAV per digest. A different
                // name for identical audio reuses the first published file.
                if(found)throw std::runtime_error("Ambiguous Show media identity directory");
                found=true;
                relative=folder/entry.path().filename();
            }
        }
        const auto destination=copies_.verified(canonical,relative,digest);
        resolved_.emplace(canonical,destination);
        return mediaJuceFile(destination);
    }
    void commit() noexcept { copies_.commit(); }

    // Includes saved Browser snapshots; never lose an audio source that exists
    // only in a legacy saved version. The new copy has no external WAV refs.
    void ownAll(juce::var& state, bool savedVersions=true) {
        const auto bindings=state["performance"]["nativePlayback"];
        if(bindings.isObject())
            for(const auto& pair:bindings.getDynamicObject()->getProperties())
                for(auto row:playbackRows(state,pair.name.toString()))
                    put(row,"file",own(juce::File(row["file"].toString())).getFullPathName());
        if(!savedVersions)return;
        const auto library=state["library"];
        if(library.isObject()) {
            juce::StringArray keys;
            for(const auto& pair:library.getDynamicObject()->getProperties())
                if(pair.value.isString())keys.add(pair.name.toString());
            for(const auto& key:keys) {
                auto saved=juce::JSON::parse(library[juce::Identifier(key)].toString());
                if(!saved["performance"]["nativePlayback"].isObject())continue;
                validatePlayback(saved);
                ownAll(saved,false);
                put(map(state,"library"),key,juce::JSON::toString(saved,true,17));
            }
        }
    }
private:
    fs::path root_;
    mrs::desktop::MediaCopy copies_;
    std::map<fs::path,fs::path> resolved_;
};

inline std::unique_ptr<ShowMediaCopy> beginOwnedMedia(const LiveDocument& document,const juce::File& show) {
    if(!document.managedMedia||show==juce::File())return {};
    return std::make_unique<ShowMediaCopy>(show);
}

// Save As is a new document, never an in-place migration. Reserve its folder
// exclusively and publish the JSON only after all referenced media are copied.
inline juce::File writeManagedShowCopy(const LiveDocument& source,const juce::File& requested) {
    auto document=source.clone();
    if(!document.projects.empty())throw std::runtime_error("Linked Studio projects cannot be copied as native Shows");
    const auto selected=requested.withFileExtension("mrlive");
    const auto name=selected.getFileNameWithoutExtension();
    if(name.trim().isEmpty()||name=="."||name=="..")throw std::runtime_error("Choose a Show name");
    const auto root=selected.getParentDirectory().getFileName()==name
        ? selected.getParentDirectory():selected.getParentDirectory().getChildFile(name);
    const auto file=root.getChildFile(selected.getFileName());
    if(selected.exists()||root.exists())throw std::runtime_error("Show destination exists; choose a new Show name");
    fs::create_directories(mediaFsPath(root.getParentDirectory()));
    if(!fs::create_directory(mediaFsPath(root)))throw std::runtime_error("Show destination exists");
    try {
        ShowMediaCopy media(file);
        document.title=name;
        document.managedMedia=true;
        media.ownAll(document.interfaceState);
        document.write(file);
        media.commit();
    } catch (...) {
        // MediaCopy has rolled back newly published WAVs. Remove only empty
        // directories, never files or a directory with unexpected contents.
        std::error_code ec;
        const auto mediaRoot=mediaFsPath(root.getChildFile("Media"));
        for(fs::directory_iterator it(mediaRoot,ec),end;!ec&&it!=end;it.increment(ec)) {
            std::error_code ignored;
            if(!it->is_symlink(ignored)&&it->is_directory(ignored))
                fs::remove(it->path(),ignored);
        }
        ec.clear();fs::remove(mediaRoot,ec);
        ec.clear();fs::remove(mediaFsPath(root),ec);
        throw;
    }
    return file;
}
} // namespace ui::live
