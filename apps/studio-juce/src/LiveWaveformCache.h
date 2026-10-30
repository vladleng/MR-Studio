#pragma once
#include <future>
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <mrs/waveform.hpp>
#include <set>

namespace ui {
// Display-only cache: never publishes assets/voices or changes the active Core project.
// One cancellable worker; bounded Core peak pyramids, no retained PCM or paint-time I/O.
class LiveWaveformCache {
  public:
    struct Entry {
        std::shared_ptr<const mrs::audio::Waveform> wave;
        std::uint32_t rate{};
        juce::String error, stamp;
    };
    ~LiveWaveformCache() {
        cancel->store(true);
        if (pending.valid())
            pending.wait();
    }
    bool sync(const std::vector<juce::String> &visibleFiles) {
        std::vector<juce::String> files;
        std::set<juce::String> unique;
        for (const auto &path : visibleFiles) {
            if (unique.insert(path).second)
                files.push_back(path);
            if (files.size() == 64)
                break;
        }
        bool changed = false;
        if (pending.valid() && pending.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto result = pending.get();
            entries[pendingPath] = std::move(result);
            pendingPath.clear();
            changed = true;
        }
        const auto now = juce::Time::getMillisecondCounter();
        if (!changed && static_cast<juce::int32>(now - lastCheck) < 1000 && files == requested)
            return changed;
        lastCheck = now;
        requested = files;
        std::set<juce::String> wanted(files.begin(), files.end());
        // Keep recently viewed items until memory pressure, even when offscreen.
        while (entries.size() > 64) {
            const auto victim = std::find_if(entries.begin(), entries.end(),
                                             [&](const auto &entry) { return !wanted.contains(entry.first); });
            if (victim == entries.end())
                break;
            entries.erase(victim);
        }
        if (pending.valid())
            return changed;
        for (const auto &path : files) {
            const juce::File file(path);
            const auto stamp =
                juce::String(file.getSize()) + ":" + juce::String(file.getLastModificationTime().toMilliseconds());
            const auto found = entries.find(path);
            if (found != entries.end() && found->second.stamp == stamp)
                continue;
            if (found != entries.end()) {
                entries.erase(found);
                changed = true;
            }
            pendingPath = path;
            const auto token = cancel;
            pending = std::async(std::launch::async, [path, stamp, token] {
                Entry result;
                result.stamp = stamp;
                try {
                    const auto data = mrs::audio::open_wav(std::filesystem::path(path.toWideCharPointer()), 0);
                    result.rate = data.sample_rate;
                    result.wave = std::make_shared<mrs::audio::Waveform>(data, token);
                } catch (const std::exception &error) {
                    result.error = juce::String::fromUTF8(error.what());
                }
                return result;
            });
            break;
        }
        return changed;
    }
    const Entry *find(const juce::String &path) const {
        const auto it = entries.find(path);
        return it == entries.end() ? nullptr : &it->second;
    }

  private:
    std::map<juce::String, Entry> entries;
    std::vector<juce::String> requested;
    juce::String pendingPath;
    std::shared_ptr<std::atomic<bool>> cancel{std::make_shared<std::atomic<bool>>(false)};
    std::future<Entry> pending;
    juce::uint32 lastCheck{};
};
} // namespace ui
