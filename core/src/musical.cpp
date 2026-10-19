#include <mrs/musical.hpp>
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace mrs {
void UpdateMeterMap::apply(Project& p)const{if(p.time!=expected_)throw std::invalid_argument("Time map changed; reopen the meter editor");p.time.meters=value_;std::sort(p.time.meters.begin(),p.time.meters.end(),[](const auto& a,const auto& b){return a.bar<b.bar;});}
void UpdateTempoMap::apply(Project& p)const{if(p.time!=expected_)throw std::invalid_argument("Time map changed; reopen the tempo editor");p.time.tempos=value_;std::sort(p.time.tempos.begin(),p.time.tempos.end(),[](const auto& a,const auto& b){return a.tick<b.tick;});}
void EditSectionGroup::apply(Project& p)const{
    std::vector<Id> ids;
    for(const auto& expected:expected_){if(std::find(ids.begin(),ids.end(),expected.id)!=ids.end())throw std::invalid_argument("Duplicate section target");ids.push_back(expected.id);
        const auto at=std::find(p.sections.begin(),p.sections.end(),expected);if(at==p.sections.end())throw std::invalid_argument("Sections changed; retry group edit");}
    std::erase_if(p.sections,[&](const auto& s){return std::find(ids.begin(),ids.end(),s.id)!=ids.end();});
    p.sections.insert(p.sections.end(),values_.begin(),values_.end());std::sort(p.sections.begin(),p.sections.end(),[](const auto& a,const auto& b){return a.start<b.start;});
}
void EditChordGroup::apply(Project& p)const{
    std::vector<Id> ids;for(const auto& item:expected_){if(std::find(ids.begin(),ids.end(),item.id)!=ids.end())throw std::invalid_argument("Duplicate chord target");ids.push_back(item.id);if(std::find(p.chords.begin(),p.chords.end(),item)==p.chords.end())throw std::invalid_argument("Chords changed; retry gesture");}
    std::erase_if(p.chords,[&](const auto& item){return std::find(ids.begin(),ids.end(),item.id)!=ids.end();});p.chords.insert(p.chords.end(),values_.begin(),values_.end());std::sort(p.chords.begin(),p.chords.end(),[](const auto& a,const auto& b){return a.start<b.start;});
}
void EditMarkerGroup::apply(Project& p)const{
    std::vector<Id> ids;for(const auto& item:expected_){if(std::find(ids.begin(),ids.end(),item.id)!=ids.end())throw std::invalid_argument("Duplicate marker target");ids.push_back(item.id);if(std::find(p.markers.begin(),p.markers.end(),item)==p.markers.end())throw std::invalid_argument("Markers changed; retry gesture");}
    // Replace original IDs in place, preserving project order and equal-tick ties.
    for(auto it=p.markers.begin();it!=p.markers.end();){if(std::find(ids.begin(),ids.end(),it->id)==ids.end()){++it;continue;}const auto at=std::find_if(values_.begin(),values_.end(),[&](const auto& v){return v.id==it->id;});if(at==values_.end())it=p.markers.erase(it);else{*it=*at;++it;}}
    for(const auto& item:values_)if(std::find(ids.begin(),ids.end(),item.id)==ids.end())p.markers.push_back(item);
}
void CopySection::apply(Project& p)const{
    const auto at=std::find_if(p.sections.begin(),p.sections.end(),[&](const auto& s){return s.id==expected_.id;});
    if(at==p.sections.end()||*at!=expected_)throw std::invalid_argument("Section changed or was removed; retry copying");
    auto value=*at;const auto length=value.end-value.start;
    if(length<=0||start_<0||start_>max_tick-length)throw std::invalid_argument("Section copy exceeds timeline range");
    value.id=created_;value.start=start_;value.end=start_+length;
    p.sections.push_back(std::move(value));std::sort(p.sections.begin(),p.sections.end(),[](const auto& a,const auto& b){return a.start<b.start;});
}
void RemoveSection::apply(Project& p) const {
    const auto at=std::find_if(p.sections.begin(),p.sections.end(),[&](const auto& s){return s.id==expected_.id;});
    if(at==p.sections.end()||*at!=expected_)throw std::invalid_argument("Section changed or was removed; reopen the menu");
    p.sections.erase(at);
}
void AddSection::apply(Project& p) const {p.sections.push_back(value_);std::sort(p.sections.begin(),p.sections.end(),[](const auto& a,const auto& b){return a.start<b.start;});}
void UpdateSection::apply(Project& p) const {
    auto at=std::find_if(p.sections.begin(),p.sections.end(),[&](const auto& s){return s.id==expected_.id;});
    if(at==p.sections.end()||*at!=expected_)throw std::invalid_argument("Section changed or was removed; reopen the editor");
    if(value_.id!=expected_.id)throw std::invalid_argument("Section ID cannot change");
    *at=value_;std::sort(p.sections.begin(),p.sections.end(),[](const auto& a,const auto& b){return a.start<b.start;});
}
namespace {
void order_chords(Project& p) {
    std::sort(p.chords.begin(), p.chords.end(), [](const Chord& a, const Chord& b) { return a.start < b.start; });
}
auto expected_chord(Project& p, const Chord& expected) {
    auto it = std::find_if(p.chords.begin(), p.chords.end(), [&](const Chord& c) { return c.id == expected.id; });
    if (it == p.chords.end() || *it != expected)
        throw std::invalid_argument("Chord changed or was removed; reopen the editor");
    return it;
}
}
void AddChord::apply(Project& p) const {
    p.chords.push_back(value_); order_chords(p);
}
void UpdateChord::apply(Project& p) const {
    if (value_.id != expected_.id) throw std::invalid_argument("Chord ID cannot change");
    *expected_chord(p, expected_) = value_; order_chords(p);
}
void RemoveChord::apply(Project& p) const {
    p.chords.erase(expected_chord(p, expected_));
}
void CopyChord::apply(Project& p) const {
    auto value = *expected_chord(p, expected_);
    const auto length = value.end - value.start;
    if (start_ < 0 || start_ > max_tick - length)
        throw std::invalid_argument("Chord copy exceeds timeline range");
    value.id = created_; value.start = start_; value.end = start_ + length;
    p.chords.push_back(std::move(value)); order_chords(p);
}
SetMusicalData::SetMusicalData(TimeMap time, std::vector<Chord> chords,
    std::vector<ArrangerSection> sections, std::vector<Marker> markers)
    : time_(std::move(time)), chords_(std::move(chords)),
      sections_(std::move(sections)), markers_(std::move(markers)) {}
namespace {
void authored_marker(const Marker& value) {
    if(value.name.empty() || value.name.size()>256 || value.name.find_first_not_of(" \t\r\n")==std::string::npos)
        throw std::invalid_argument("Enter a marker name (1..256 UTF-8 bytes)");
}
auto expected_marker(Project& p,const Marker& expected) {
    auto it=std::find_if(p.markers.begin(),p.markers.end(),[&](const auto& m){return m.id==expected.id;});
    if(it==p.markers.end()||*it!=expected)
        throw std::invalid_argument("Marker changed or was removed; reopen the editor");
    return it;
}
}
void AddMarker::apply(Project& p) const { authored_marker(value_);p.markers.push_back(value_); }
void UpdateMarker::apply(Project& p) const {
    authored_marker(value_);
    if(value_.id!=expected_.id)throw std::invalid_argument("Marker ID cannot change");
    *expected_marker(p,expected_)=value_; // project order, including equal-tick ties, stays stable
}
void RemoveMarker::apply(Project& p) const { p.markers.erase(expected_marker(p,expected_)); }
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
MusicalTimeline::MusicalTimeline(Services services,std::function<void()> navigation_guard) : services_(std::move(services)),navigation_guard_(std::move(navigation_guard)) {
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
        if (changed && (!state_.project || project.project->markers != state_.project->markers)) {
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
    if(navigation_guard_)navigation_guard_();
    refresh();
    services_.transport->seek(timeline_->to_samples(by_id(state_.project->sections, id).start));
}
void MusicalTimeline::seek_marker(const Id& id) {
    if(navigation_guard_)navigation_guard_();
    refresh();
    services_.transport->seek(timeline_->to_samples(by_id(state_.project->markers, id).tick));
}
bool MusicalTimeline::navigate(bool sections, bool forward) {
    if(navigation_guard_)navigation_guard_();
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
    if(navigation_guard_)navigation_guard_();
    refresh();
    const auto& s = by_id(state_.project->sections, id);
    const LoopRange range{timeline_->to_samples(s.start), timeline_->to_samples(s.end)};
    services_.transport->set_loop(range); // sample rounding may reject sub-sample ranges
}
void MusicalTimeline::clear_loop() {if(navigation_guard_)navigation_guard_(); services_.transport->set_loop(std::nullopt); }
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
