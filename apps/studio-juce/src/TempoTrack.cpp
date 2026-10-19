#include "MeterTrack.h"
#include "TempoTrack.h"
#include "Desktop.h"
#include "ChordTrack.h"
#include "SectionTrack.h"
#include "MarkerTrack.h"
#include <charconv>
#include <algorithm>
namespace ui {
namespace {
juce::String bpmText(double value){std::array<char,32> text{};const auto result=std::to_chars(text.data(),text.data()+text.size(),value);return juce::String::fromUTF8(text.data(),static_cast<int>(result.ptr-text.data()));}
juce::String position(const mrs::Timeline& time,mrs::Tick tick){const auto p=time.musical_position(tick);return juce::String(p.bar)+":"+juce::String(p.beat)+":"+juce::String(p.tick);}
mrs::Tick parsePosition(const mrs::Timeline& time,juce::String value){const auto parts=juce::StringArray::fromTokens(value.trim(),":","");if(parts.size()!=3)throw std::invalid_argument("Use bar:beat:tick");for(const auto& part:parts)if(part.isEmpty()||part.length()>12||!part.containsOnly("0123456789"))throw std::invalid_argument("Invalid bar:beat:tick");const auto beat=parts[1].getLargeIntValue();if(beat<1||beat>64)throw std::invalid_argument("Invalid beat");return time.to_ticks({parts[0].getLargeIntValue(),static_cast<int>(beat),parts[2].getLargeIntValue()});}
struct Fields{juce::String tick,bpm;};
class Editor final:public juce::PopupMenu::CustomComponent {
public:
    Editor(std::shared_ptr<Fields> values,Theme& theme,bool anchor):CustomComponent(false),fields(std::move(values)){
        setLookAndFeel(&theme);setName("Tempo point");addAndMakeVisible(tick);addAndMakeVisible(bpm);addAndMakeVisible(apply);
        tick.setName("Position");bpm.setName("BPM");tick.setFont(Theme::browserFont());bpm.setFont(Theme::browserFont());tick.setText(fields->tick,false);bpm.setText(fields->bpm,false);tick.setReadOnly(anchor);
        tick.setTooltip(anchor?"Anchor at tick 0 cannot move":"bar:beat:tick");bpm.setTooltip("1..1000 BPM; constant until the next point");
        for(auto* field:{&tick,&bpm}){field->setSelectAllWhenFocused(true);field->onReturnKey=[this]{save();};field->onEscapeKey=[]{juce::PopupMenu::dismissAllActiveMenus();};}
        apply.onClick=[this]{save();};
    }
    ~Editor()override{setLookAndFeel(nullptr);}
    void getIdealSize(int& w,int& h)override{w=300;h=116;}
    void resized()override{tick.setBounds(86,8,getWidth()-98,26);bpm.setBounds(86,44,getWidth()-98,26);apply.setBounds(86,80,getWidth()-98,26);}
    void paint(juce::Graphics& g)override{g.fillAll(juce::Colour(surface));g.setFont(Theme::browserFont());g.setColour(juce::Colour(primaryText));g.drawText("Position",12,8,72,26,juce::Justification::centredLeft);g.drawText("BPM",12,44,72,26,juce::Justification::centredLeft);}
private:
    void save(){fields->tick=tick.getText();fields->bpm=bpm.getText();triggerMenuItem();}
    std::shared_ptr<Fields> fields;juce::TextEditor tick,bpm;juce::TextButton apply{"Apply"};
};
}
TempoTrack::TempoTrack(Desktop& d,Arrangement& a):owner(d),arrangement(a){setWantsKeyboardFocus(true);setTitle("Tempo map");setDescription("Double/right click to edit; drag a point to move; Backspace removes. Anchor tick 0 stays.");addAndMakeVisible(list);addAndMakeVisible(add);list.setTextWhenNothingSelected("Tempo points");add.setTooltip("Add tempo point at playhead (Pause/Stop, loop off)");
    add.onClick=[this]{owner.run([&]{const auto p=owner.project();const mrs::Timeline time(p->time,p->sample_rate);const auto tick=time.to_ticks(owner.displayState().sample);std::optional<mrs::TempoPoint> existing;for(const auto& point:p->time.tempos)if(point.tick==tick)existing=point;edit(existing,tick);});};
    list.onChange=[this]{const auto index=list.getSelectedId()-1;const auto& points=owner.project()->time.tempos;if(index>=0&&index<static_cast<int>(points.size())){arrangement.meters->clearSelection();arrangement.sections->clearSelection();arrangement.chords->clearSelection();arrangement.markers->clearSelection();owner.selectedClip.reset();owner.selectedClips.clear();selected=points[static_cast<std::size_t>(index)].tick;grabKeyboardFocus();repaint();}};
}
void TempoTrack::resized(){list.setBounds(70,4,132,24);add.setBounds(208,4,34,24);}
void TempoTrack::cancel(){original.reset();preview.reset();store.reset();repaint();}
void TempoTrack::sync(bool replaced){const auto p=owner.project();if(replaced){selected.reset();cancel();}if(original&&(store!=owner.app.services().projects||captured!=p->time||zoom!=arrangement.pixelsPerSecond||scroll!=arrangement.horizontal))cancel();if(selected&&std::none_of(p->time.tempos.begin(),p->time.tempos.end(),[&](const auto& point){return point.tick==*selected;}))selected.reset();
    if(listed!=p->time){listed=p->time;list.clear(juce::dontSendNotification);const mrs::Timeline t(p->time,p->sample_rate);int i=0;for(const auto& point:p->time.tempos)list.addItem(position(t,point.tick)+" / "+bpmText(point.bpm)+" BPM",++i);}int choice=0;for(std::size_t i=0;i<p->time.tempos.size();++i)if(selected==p->time.tempos[i].tick)choice=static_cast<int>(i)+1;list.setSelectedId(choice,juce::dontSendNotification);add.setEnabled(!owner.app.recording());repaint();
}
float TempoTrack::xAt(mrs::Tick tick)const{const auto p=owner.project();return static_cast<float>(Arrangement::left+static_cast<double>(mrs::Timeline(p->time,p->sample_rate).to_samples(tick))/p->sample_rate*arrangement.pixelsPerSecond-arrangement.horizontal);}
std::optional<mrs::TempoPoint> TempoTrack::pointAt(float x)const{if(x<Arrangement::left)return {};std::optional<mrs::TempoPoint> result;const auto& points=owner.project()->time.tempos;for(std::size_t i=0;i<points.size();++i){const auto left=xAt(points[i].tick);const auto right=i+1<points.size()?std::min(left+104.f,xAt(points[i+1].tick)):left+104.f;if(std::abs(x-left)<=6||(x>=left&&x<right)){if(selected==points[i].tick)return points[i];if(!result)result=points[i];}}return result;}
void TempoTrack::paint(juce::Graphics& g){g.fillAll(juce::Colour(arrangementCanvas));g.setColour(juce::Colour(surface));g.fillRect(0,0,Arrangement::left,getHeight());g.setFont(Theme::browserFont());g.setColour(juce::Colour(primaryText));g.drawText("Tempo",8,0,60,getHeight(),juce::Justification::centredLeft);
    juce::Graphics::ScopedSaveState save(g);g.reduceClipRegion(Arrangement::left,0,std::max(0,getWidth()-Arrangement::left),getHeight());arrangement.paintGrid(g,0,static_cast<float>(getHeight()));g.setFont(Theme::browserFont());const auto& points=owner.project()->time.tempos;
    for(std::size_t i=0;i<points.size();++i){const auto point=preview&&original&&points[i]==*original?*preview:points[i];const auto x=xAt(point.tick);if(x>getWidth()||x<Arrangement::left-104)continue;const auto next=i+1<points.size()?xAt(points[i+1].tick):static_cast<float>(getWidth());const auto width=std::max(0.f,std::min(100.f,next-x-3));
        g.setColour(owner.selectionColour());g.drawVerticalLine(static_cast<int>(x),3,29);if(width>8){const juce::Rectangle<float> r{x+3,3,width,26};drawCard(g,r,selected==points[i].tick?owner.selectionColour():juce::Colour(raised));g.setColour(owner.assetSkin()&&selected==points[i].tick?juce::Colour(ConceptSkin::canvas):juce::Colour(primaryText));g.drawText(bpmText(point.bpm)+" BPM",r.toNearestInt().reduced(5,0),juce::Justification::centredLeft,true);}}
}
void TempoTrack::edit(std::optional<mrs::TempoPoint> expected,mrs::Tick tick,std::optional<juce::Point<int>> screen){cancel();juce::PopupMenu::dismissAllActiveMenus();const auto p=owner.project();const auto time=p->time;const auto projectStore=owner.app.services().projects;const mrs::Timeline t(time,p->sample_rate);if(expected)selected=expected->tick;double value=120;for(const auto& point:time.tempos)if(point.tick<=tick)value=point.bpm;
    auto fields=std::make_shared<Fields>();fields->tick=position(t,expected?expected->tick:tick);fields->bpm=bpmText(expected?expected->bpm:value);auto form=std::make_unique<Editor>(fields,owner.theme,expected&&expected->tick==0);editor=form.get();juce::PopupMenu menu;menu.setLookAndFeel(&owner.theme);if(expected){menu.addItem(2,"Remove tempo point (Backspace)",expected->tick!=0&&!owner.app.recording());menu.addSeparator();}menu.addCustomItem(1,std::move(form),nullptr,"Tempo position and BPM");
    auto options=juce::PopupMenu::Options().withTargetComponent(this);if(screen)options=options.withTargetScreenArea({screen->x,screen->y,1,1});juce::Component::SafePointer<TempoTrack> safe(this);
    menu.showMenuAsync(options,[safe,fields,projectStore,time,expected](int result){if(!safe||!result)return;safe->owner.run([&]{if(projectStore!=safe->owner.app.services().projects)throw std::runtime_error("Project changed; reopen the tempo editor");auto values=time.tempos;
        if(expected){const auto at=std::find(values.begin(),values.end(),*expected);if(at==values.end())throw std::runtime_error("Tempo point changed");values.erase(at);}
        if(result==2){if(!expected||expected->tick==0)throw std::invalid_argument("Anchor tick 0 cannot be removed");}
        else{std::size_t used{};const auto text=fields->bpm.trim().toStdString();const auto bpm=std::stod(text,&used);if(used!=text.size())throw std::invalid_argument("Enter BPM in 1..1000");const auto positionTick=parsePosition(mrs::Timeline(time,safe->owner.project()->sample_rate),fields->tick);if(expected&&expected->tick==0&&positionTick!=0)throw std::invalid_argument("Anchor tick 0 cannot move");values.push_back({positionTick,bpm});safe->selected=positionTick;}
        safe->owner.app.set_tempos(time,std::move(values));});safe->grabKeyboardFocus();});
}
void TempoTrack::removeSelected(){if(!selected)return;if(*selected==0)throw std::invalid_argument("Anchor tick 0 cannot be removed");const auto time=owner.project()->time;auto values=time.tempos;values.erase(std::remove_if(values.begin(),values.end(),[&](const auto& point){return point.tick==*selected;}),values.end());owner.app.set_tempos(time,std::move(values));selected.reset();}
void TempoTrack::mouseDown(const juce::MouseEvent& e){cancel();grabKeyboardFocus();arrangement.meters->clearSelection();arrangement.sections->clearSelection();arrangement.chords->clearSelection();arrangement.markers->clearSelection();owner.selectedClip.reset();owner.selectedClips.clear();if(e.x<Arrangement::left)return;const auto point=pointAt(e.position.x);if(e.mods.isPopupMenu()){owner.run([&]{edit(point,arrangement.chords->tickAt(e.position.x),e.getScreenPosition());});return;}if(!e.mods.isLeftButtonDown())return;selected=point?std::optional<mrs::Tick>{point->tick}:std::nullopt;if(!point||point->tick==0){repaint();return;}const auto head=owner.displayState();if(owner.app.recording()||head.playback==mrs::PlaybackState::playing||head.loop||head.count_remaining){owner.message="Pause/stop and turn off loop before tempo editing";owner.repaint();return;}original=preview=point;captured=owner.project()->time;store=owner.app.services().projects;zoom=arrangement.pixelsPerSecond;scroll=arrangement.horizontal;dragX=e.position.x;anchor=arrangement.chords->tickAt(dragX,false);repaint();}
void TempoTrack::mouseDrag(const juce::MouseEvent& e){sync();const auto head=owner.displayState();if(owner.app.recording()||head.playback==mrs::PlaybackState::playing||head.loop||head.count_remaining){cancel();return;}if(!original)return;auto point=*original;point.tick=std::clamp(point.tick+arrangement.chords->tickAt(e.position.x,false)-anchor,mrs::Tick{1},mrs::max_tick);if(owner.snap)point.tick=std::max(mrs::Tick{1},arrangement.snapTick(point.tick));preview=point;repaint();}
void TempoTrack::mouseUp(const juce::MouseEvent& e){sync();if(!original)return;const auto expected=*original,value=*preview;const auto time=captured;const auto projectStore=store;const bool moved=std::abs(e.position.x-dragX)>=2;cancel();if(!moved||value==expected)return;owner.run([&]{if(projectStore!=owner.app.services().projects)throw std::runtime_error("Project changed; retry tempo gesture");auto values=time.tempos;const auto at=std::find(values.begin(),values.end(),expected);if(at==values.end())throw std::runtime_error("Tempo point changed");*at=value;owner.app.set_tempos(time,std::move(values));selected=value.tick;});}
void TempoTrack::mouseDoubleClick(const juce::MouseEvent& e){if(e.mods.isLeftButtonDown()&&e.x>=Arrangement::left)owner.run([&]{edit(pointAt(e.position.x),arrangement.chords->tickAt(e.position.x),e.getScreenPosition());});}
bool TempoTrack::keyPressed(const juce::KeyPress& key){if(key.getKeyCode()==juce::KeyPress::escapeKey){cancel();return true;}if(original)return true;if(!key.getModifiers().isAnyModifierKeyDown()&&(key.getKeyCode()==juce::KeyPress::backspaceKey||key.getKeyCode()==juce::KeyPress::deleteKey)){owner.run([&]{removeSelected();});return true;}return owner.workspaceShortcut(key);}
}
