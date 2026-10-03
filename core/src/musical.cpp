#include <mrs/musical.hpp>
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace mrs {
SetMusicalData::SetMusicalData(TimeMap time, std::vector<Chord> chords,
    std::vector<ArrangerSection> sections, std::vector<Marker> markers)
    : time_(std::move(time)), chords_(std::move(chords)),
      sections_(std::move(sections)), markers_(std::move(markers)) {}
void SetMusicalData::apply(Project& p) const {
    p.time = time_; p.chords = chords_; p.sections = sections_; p.markers = markers_;
}
namespace {
template<class T> void context(const std::vector<T>& lane, Tick tick,
    std::optional<T>& current, std::optional<T>& next) {
    const auto it = std::upper_bound(lane.begin(), lane.end(), tick,
        [](Tick t, const T& p) { return t < p.start; });
    if (it != lane.end()) next = *it;
    if (it != lane.begin() && tick < std::prev(it)->end) current = *std::prev(it);
}
template<class T> const T& by_id(const std::vector<T>& lane, const Id& id) {
    const auto it = std::find_if(lane.begin(), lane.end(), [&id](const T& p) { return p.id == id; });
    if (it == lane.end()) throw std::invalid_argument("unknown musical entity ID");
    return *it;
}
}
MusicalTimeline::MusicalTimeline(Services services) : services_(std::move(services)) {
    if (!services_.projects || !services_.transport) throw std::invalid_argument("missing shared services");
    refresh();
    project_connection_ = services_.projects->subscribe([this](const ProjectState&) { refresh(); });
    transport_connection_ = services_.transport->subscribe([this](const TransportState&) { refresh(); });
}
Connection MusicalTimeline::subscribe(std::function<void(const MusicalContext&)> callback) {
    return changes_.subscribe(std::move(callback));
}
void MusicalTimeline::refresh() {
    if (refreshing_) return; // rebind can synchronously emit a transport event
    refreshing_ = true;
    try {
        const auto project = services_.projects->state();
        if (!project.project) throw std::invalid_argument("missing project snapshot");
        const bool changed = project.project != state_.project;
        if (!timeline_ || project.project->time != state_.project->time ||
            project.project->sample_rate != state_.project->sample_rate) {
            Timeline candidate{project.project->time, project.project->sample_rate};
            services_.transport->rebind_timeline(candidate);
            timeline_ = std::move(candidate);
        }
        if (changed) {
            marker_order_.resize(project.project->markers.size());
            std::iota(marker_order_.begin(), marker_order_.end(), std::size_t{0});
            std::stable_sort(marker_order_.begin(), marker_order_.end(), [&](auto a, auto b) {
                return project.project->markers[a].tick < project.project->markers[b].tick;
            });
        }
        MusicalContext next;
        next.project = project.project;
        next.revision = project.revision;
        next.transport = services_.transport->state();
        next.tick = timeline_->to_ticks(next.transport.sample);
        next.transport.musical = timeline_->musical_position(next.tick);
        context(next.project->chords, next.tick, next.current_chord, next.next_chord);
        context(next.project->sections, next.tick, next.current_section, next.next_section);
        const auto marker = std::upper_bound(marker_order_.begin(), marker_order_.end(), next.tick,
            [&](Tick t, auto index) { return t < next.project->markers[index].tick; });
        if (marker != marker_order_.end()) next.next_marker = next.project->markers[*marker];
        if (marker != marker_order_.begin()) next.current_marker = next.project->markers[*std::prev(marker)];
        const bool notify = next != state_;
        state_ = std::move(next);
        refreshing_ = false;
        if (notify) changes_.publish(state_);
    } catch (...) {
        refreshing_ = false;
        throw;
    }
}
void MusicalTimeline::seek_section(const Id& id) {
    refresh();
    services_.transport->seek(timeline_->to_samples(by_id(state_.project->sections, id).start));
}
void MusicalTimeline::seek_marker(const Id& id) {
    refresh();
    services_.transport->seek(timeline_->to_samples(by_id(state_.project->markers, id).tick));
}
bool MusicalTimeline::navigate(bool sections, bool forward) {
    refresh();
    std::optional<Tick> target;
    const auto consider = [&](Tick t) {
        if (forward ? t > state_.tick : t < state_.tick) {
            if (!target || (forward ? t < *target : t > *target)) target = t;
        }
    };
    if (sections) for (const auto& s : state_.project->sections) consider(s.start);
    else for (const auto& m : state_.project->markers) consider(m.tick);
    if (!target) return false; // no wrap
    services_.transport->seek(timeline_->to_samples(*target));
    return true;
}
bool MusicalTimeline::next_section() { return navigate(true, true); }
bool MusicalTimeline::previous_section() { return navigate(true, false); }
bool MusicalTimeline::next_marker() { return navigate(false, true); }
bool MusicalTimeline::previous_marker() { return navigate(false, false); }
void MusicalTimeline::loop_section(const Id& id) {
    refresh();
    const auto& s = by_id(state_.project->sections, id);
    const LoopRange range{timeline_->to_samples(s.start), timeline_->to_samples(s.end)};
    services_.transport->set_loop(range); // sample rounding may reject sub-sample ranges
}
void MusicalTimeline::clear_loop() { services_.transport->set_loop(std::nullopt); }
Project musical_demo_project() {
    auto p = demo_project();
    p.time.tempos = {{0, 72}, {8 * 4 * ppq, 96}};
    p.time.meters = {{1, 4, 4}, {13, 3, 4}};
    p.chords = {
        {{"chord-dm7"}, "Dm7", 0, 4 * ppq},
        {{"chord-g7"}, "G7", 4 * ppq, 8 * ppq},
        {{"chord-gmaj7"}, "Gmaj7", 8 * ppq, 16 * ppq},
        {{"chord-em7"}, "Em7", 16 * ppq, 20 * ppq}};
    p.sections = {
        {{"section-intro"}, "Intro", 0, 8 * ppq, 0x505B70},
        {{"section-verse"}, "Verse", 8 * ppq, 32 * ppq, 0x4056D6},
        {{"section-chorus"}, "Chorus", 32 * ppq, 48 * ppq, 0x7050A0}};
    p.markers = {
        {{"cue-vocal"}, "Vocal entry", 8 * ppq, MarkerKind::cue},
        {{"cue-chorus"}, "Chorus", 32 * ppq, MarkerKind::navigation}};
    p.validate();
    return p;
}
} // namespace mrs
