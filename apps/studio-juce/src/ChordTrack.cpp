#include "MeterTrack.h"
#include "ChordTrack.h"
#include "Desktop.h"
#include "TempoTrack.h"
#include "SectionTrack.h"
#include "MarkerTrack.h"
#include <algorithm>

namespace ui {
namespace {
juce::String label(const std::string& text) { return juce::String::fromUTF8(text.c_str()); }
}
ChordTrack::ChordTrack(Desktop& d, Arrangement& a) : owner(d), arrangement(a) {
    setWantsKeyboardFocus(true); setTitle("Chord Track");
    setDescription("Double click empty space to create; double click a chord to edit. Alt-drag to copy; Backspace to remove.");
    addAndMakeVisible(add);
    add.setTooltip("Add a chord at the playhead or the next free position");
    add.onClick=[this] {
        owner.run([&] {
            const auto p=owner.project();
            const mrs::Timeline time(p->time,p->sample_rate);
            auto tick=time.to_ticks(owner.displayState().sample);
            for(const auto& c:p->chords) if(c.start<=tick&&tick<c.end) tick=c.end;
            create(tick);
        });
    };
}
void ChordTrack::resized() { add.setBounds(208,1,34,22); }
void ChordTrack::cancel() {
    const bool box=group.selecting;group.cancel();if(box)selected=group.primary();original.reset(); preview.reset(); gestureStore.reset(); copyDrag=false;
    setMouseCursor(juce::MouseCursor::NormalCursor); repaint();
}
void ChordTrack::sync(bool replaced) {
    add.setEnabled(!owner.app.recording());
    if(replaced){cancel();group.ids.clear();selected.reset();}
    const auto p=owner.project();group.sync(p->chords);
    if(selected&&std::none_of(p->chords.begin(),p->chords.end(),[&](const auto& c){return c.id==*selected;})) selected.reset();
    if(original&&(gestureStore!=owner.app.services().projects||p->time!=gestureTime||
       arrangement.pixelsPerSecond!=gestureZoom||arrangement.horizontal!=gestureScroll||
       std::none_of(p->chords.begin(),p->chords.end(),[&](const auto& c){return c==*original;}))) cancel();
    repaint();
}
juce::Rectangle<float> ChordTrack::chordRect(const mrs::Chord& c) const {
    const auto p=owner.project(); const mrs::Timeline time(p->time,p->sample_rate);
    const auto x=[&](mrs::Tick t){return static_cast<float>(Arrangement::left+
        static_cast<double>(time.to_samples(t))/p->sample_rate*arrangement.pixelsPerSecond-arrangement.horizontal);};
    const auto start=x(c.start);
    return {start,1.f,std::max(1.f,x(c.end)-start),22.f};
}
mrs::Tick ChordTrack::tickAt(float x,bool snapped) const {
    const auto p=owner.project(); const mrs::Timeline time(p->time,p->sample_rate);
    const auto sample=std::clamp((x-Arrangement::left+arrangement.horizontal)/
        arrangement.pixelsPerSecond*p->sample_rate,0.,static_cast<double>(mrs::max_sample));
    auto tick=time.to_ticks(static_cast<mrs::Sample>(std::llround(sample)));
    if(owner.snap&&snapped) tick=arrangement.snapTick(tick);
    return std::clamp(tick,mrs::Tick{0},mrs::max_tick);
}
std::optional<mrs::Chord> ChordTrack::chordAt(juce::Point<float> point) const {
    if(point.x<Arrangement::left) return {};
    for(const auto& c:owner.project()->chords) if(chordRect(c).contains(point)) return c;
    return {};
}
std::optional<mrs::Id> ChordTrack::currentChord() const {
    // Use the acknowledged shared context, including its half-open/gap semantics.
    const auto& context=owner.displayedMusical;
    if(context.current_chord&&context.project==owner.project()) return context.current_chord->id;
    return {};
}
int ChordTrack::edgeAt(const mrs::Chord& c,float x) const {
    const auto r=chordRect(c); const auto a=x-r.getX(),b=r.getRight()-x;
    return std::min(a,b)<std::min(6.f,r.getWidth()/3.f)?(a<=b?-1:1):0;
}
void ChordTrack::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(arrangementCanvas));
    g.setColour(juce::Colour(surface));g.fillRect(0,0,Arrangement::left,getHeight());
    g.setFont(Theme::browserFont());
    g.setColour(juce::Colours::lightgrey);
    g.drawText("Chord Track",8,0,150,24,juce::Justification::left);
    g.saveState();g.reduceClipRegion(Arrangement::left,0,std::max(0,getWidth()-Arrangement::left),getHeight());
    arrangement.paintGrid(g,0,static_cast<float>(getHeight()));
    const auto current=currentChord();
    const auto draw=[&](const mrs::Chord& c,bool ghost){auto r=chordRect(c);const bool active=current==c.id,chosen=group.ids.contains(c.id.value);const auto color=juce::Colour(0xff000000|c.color.value_or(active?(owner.selectionColour().getARGB()&0xFFFFFF):0x414c58));g.setColour(color.withAlpha(ghost?.35f:clipFillAlpha));g.fillRoundedRectangle(r.reduced(.5f),3.f);g.setColour(ghost?(group.valid?juce::Colours::white:juce::Colours::salmon):chosen?(owner.assetSkin()?owner.selectionColour():juce::Colours::white):juce::Colour(0xff758494));g.drawRoundedRectangle(r.reduced(.375f),3.f,chosen||ghost?.75f:.5f);g.setColour(owner.assetSkin()?ObjectPalette::ink(color.getARGB()&0xFFFFFF,ghost?.35f:clipFillAlpha,arrangementCanvas):juce::Colours::whitesmoke);g.drawText(label(c.symbol),r.toNearestInt().reduced(5,0),juce::Justification::centredLeft,true);};
    for(const auto& stored:owner.project()->chords){auto c=stored;if(!copyDrag&&original){const auto at=std::find_if(group.previews.begin(),group.previews.end(),[&](const auto& item){return item.id==c.id;});if(at!=group.previews.end())c=*at;}draw(c,false);}
    if(copyDrag&&original)for(const auto& c:group.previews)draw(c,true);group.paintBox(g,owner.selectionColour());
    g.restoreState();
}
void ChordTrack::create(mrs::Tick start) {
    cancel();
    const auto p=owner.project();auto end=start+std::min(4*mrs::ppq,mrs::max_tick-start);
    for(const auto& c:p->chords)if(c.start>start){end=std::min(end,c.start);break;}
    selectGroup({owner.app.add_chord("Am",start,end)});
    arrangement.markers->clearSelection();arrangement.meters->clearSelection();arrangement.tempos->clearSelection();arrangement.sections->clearSelection();
    owner.selectedClip.reset();owner.selectedClips.clear();
}
void ChordTrack::selectGroup(const std::vector<mrs::Id>& ids){group.set(ids);selected=group.primary();}
void ChordTrack::duplicateSelected(){const auto values=group.selected(owner.project()->chords);if(values.empty())return;selectGroup(owner.app.transform_chords(values,groupEnd(values)-groupStart(values),true));}
void ChordTrack::removeSelected(){const auto values=group.selected(owner.project()->chords);if(values.empty())return;owner.app.remove_chords(values);clearSelection();}
void ChordTrack::splitSelected(mrs::Tick tick){owner.app.split_chords(group.selected(owner.project()->chords),tick);}
void ChordTrack::edit(std::optional<mrs::Chord> expected,mrs::Tick start) {
    if(owner.app.recording())throw std::invalid_argument("End recording before editing chords");
    cancel();
    arrangement.markers->clearSelection();arrangement.meters->clearSelection();arrangement.tempos->clearSelection();arrangement.sections->clearSelection();
    const auto p=owner.project();auto end=std::min(mrs::max_tick,start+4*mrs::ppq);
    if(!expected)for(const auto& c:p->chords)if(c.start>start){end=std::min(end,c.start);break;}
    auto* alert=new juce::AlertWindow(expected?"Edit chord":"Add chord",
        "Authored symbol; time in ticks (960 ticks per quarter note). Clips remain unchanged.",
        juce::MessageBoxIconType::NoIcon);
    alert->addTextEditor("symbol",expected?label(expected->symbol):"Am","Chord symbol");
    alert->addTextEditor("start",juce::String(expected?expected->start:start),"Start (ticks)");
    alert->addTextEditor("end",juce::String(expected?expected->end:end),"End (ticks, exclusive)");
    alert->addButton("Save",1,juce::KeyPress(juce::KeyPress::returnKey));
    alert->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<ChordTrack> safe(this);
    const auto store=owner.app.services().projects;
    alert->enterModalState(true,juce::ModalCallbackFunction::create([safe,alert,store,expected](int result){
        if(!safe||result!=1)return;
        safe->owner.run([&] {
            if(store!=safe->owner.app.services().projects)throw std::runtime_error("Project changed; reopen the chord editor");
            const auto parse=[](juce::String text){
                const auto str=text.trim().toStdString(); std::size_t used{};
                const auto value=std::stoll(str,&used);
                if(used!=str.size()||value<0||value>mrs::max_tick)
                    throw std::invalid_argument("Enter a non-negative tick within the project range");
                return static_cast<mrs::Tick>(value);
            };
            const auto symbol=alert->getTextEditorContents("symbol").toStdString();
            if(alert->getTextEditorContents("symbol").trim().isEmpty())throw std::invalid_argument("Enter a chord symbol");
            const auto begin=parse(alert->getTextEditorContents("start")),finish=parse(alert->getTextEditorContents("end"));
            if(expected){auto value=*expected;value.symbol=symbol;value.start=begin;value.end=finish;
                safe->owner.app.update_chord(*expected,value);safe->selectGroup({value.id});}
            else safe->selectGroup({safe->owner.app.add_chord(symbol,begin,finish)});
        });
    }),true);
}
void ChordTrack::menu(juce::Point<float> point) {
    arrangement.markers->clearSelection();arrangement.meters->clearSelection();arrangement.tempos->clearSelection();arrangement.sections->clearSelection();
    const auto expected=chordAt(point);selected=expected?std::optional<mrs::Id>{expected->id}:std::nullopt;group.set(expected?std::vector<mrs::Id>{expected->id}:std::vector<mrs::Id>{});
    owner.selectedClip.reset();owner.selectedClips.clear();owner.refresh();
    juce::PopupMenu m;m.setLookAndFeel(&owner.theme);
    const bool enabled=!owner.app.recording();
    m.addItem(1,expected?"Edit chord...":"Add chord...",enabled);
    if(expected){m.addItem(3,"Duplicate",enabled);m.addItem(2,"Remove chord (Backspace)",enabled);}
    auto color=std::make_shared<std::uint32_t>(expected?expected->color.value_or(0x414c58):0x414c58);
    if(expected){m.addSeparator();auto palette=std::make_unique<MenuColourPalette>(owner.theme,*color,color);paletteEditor=palette.get();m.addCustomItem(10,std::move(palette),nullptr,"Chord color");}
    juce::Component::SafePointer<ChordTrack> safe(this);const auto store=owner.app.services().projects;
    const auto tick=tickAt(point.x);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea({getScreenPosition().x+static_cast<int>(point.x),getScreenPosition().y+static_cast<int>(point.y),1,1}),[safe,store,expected,tick,color](int result){
        if(!safe||!result)return;
        safe->owner.run([&] {
            if(store!=safe->owner.app.services().projects)throw std::runtime_error("Project changed; reopen the menu");
            if(result==10&&expected){auto value=*expected;value.color=*color;safe->owner.app.update_chord(*expected,value);safe->selectGroup({value.id});}
            else if(result==1)safe->edit(expected,tick);
            else if(result==2&&expected){safe->owner.app.remove_chord(*expected);safe->clearSelection();}
            else if(result==3&&expected)safe->selectGroup({safe->owner.app.copy_chord(*expected,expected->end)});
        });
    });repaint();
}
void ChordTrack::mouseDown(const juce::MouseEvent& e){
    cancel();grabKeyboardFocus();arrangement.markers->clearSelection();arrangement.meters->clearSelection();arrangement.tempos->clearSelection();arrangement.sections->clearSelection();owner.selectedClip.reset();owner.selectedClips.clear();
    if(e.mods.isPopupMenu()){menu(e.position);return;}if(!e.mods.isLeftButtonDown()||e.x<Arrangement::left)return;const auto c=chordAt(e.position);
    if(!c){group.beginBox(e.position,e.mods.isShiftDown());selected=group.primary();repaint();return;}
    if(owner.knifeMode){selectGroup({c->id});owner.run([&]{splitSelected(tickAt(e.position.x));});return;}
    if(e.mods.isShiftDown()){group.toggle(c->id);selected=group.primary();owner.refresh();repaint();return;}
    if(!group.ids.contains(c->id.value))selectGroup({c->id});else selected=c->id;
    if(!owner.app.recording()){original=c;preview=c;group.originals=group.selected(owner.project()->chords);group.previews=group.originals;gestureStore=owner.app.services().projects;gestureTime=owner.project()->time;gestureZoom=arrangement.pixelsPerSecond;gestureScroll=arrangement.horizontal;dragX=e.position.x;anchor=tickAt(dragX,false);copyDrag=e.mods.isAltDown();edge=copyDrag||group.ids.size()>1?0:edgeAt(*c,dragX);}
    owner.refresh();repaint();
}
void ChordTrack::mouseDrag(const juce::MouseEvent& e){
    if(group.selecting){group.boxDrag(e.position,owner.project()->chords,[&](const auto& c){return chordRect(c);},getLocalBounds().toFloat().withTrimmedLeft(Arrangement::left));selected=group.primary();repaint();return;}
    if(!original)return;if(owner.app.recording()||gestureStore!=owner.app.services().projects||owner.project()->time!=gestureTime||arrangement.pixelsPerSecond!=gestureZoom||arrangement.horizontal!=gestureScroll){cancel();return;}
    const auto& lane=owner.project()->chords;const auto raw=tickAt(e.position.x,false)-anchor;
    group.previews=group.originals;
    if(!edge){const auto delta=groupDelta(group.originals,*original,raw,owner.snap,lane,copyDrag,[&](mrs::Tick tick){return chordRect({{},"",tick,tick+1}).getX();},[&](mrs::Tick tick){return arrangement.snapTick(tick);});for(auto& c:group.previews){c.start+=delta;c.end+=delta;}}
    else{auto& c=group.previews.front();mrs::Tick low=0,high=mrs::max_tick;for(const auto& other:lane)if(other.id!=c.id){if(other.end<=c.start)low=std::max(low,other.end);if(other.start>=c.end)high=std::min(high,other.start);}const auto snap=[&](mrs::Tick tick){return owner.snap?arrangement.snapTick(tick):tick;};if(edge<0)c.start=std::clamp(snap(c.start+raw),low,c.end-1);else c.end=std::clamp(snap(c.end+raw),c.start+1,high);}
    group.valid=groupValid(group.previews,lane,group.ids,copyDrag);preview=*std::find_if(group.previews.begin(),group.previews.end(),[&](const auto& c){return c.id==original->id;});repaint();
}
void ChordTrack::mouseUp(const juce::MouseEvent& e){
    if(group.selecting){group.endBox();selected=group.primary();owner.refresh();repaint();return;}if(!original)return;
    if(owner.project()->time!=gestureTime||arrangement.pixelsPerSecond!=gestureZoom||arrangement.horizontal!=gestureScroll){cancel();return;}
    const auto expected=group.originals,values=group.previews;const auto store=gestureStore;const bool copying=copyDrag,valid=group.valid;const auto resize=edge;const bool moved=std::abs(e.position.x-dragX)>=2;cancel();
    if(!moved||values==expected)return;if(!valid){owner.message="Chords overlap; choose a free position";owner.repaint();return;}
    owner.run([&]{if(store!=owner.app.services().projects)throw std::runtime_error("Project changed; retry chord gesture");if(resize)owner.app.update_chord(expected.front(),values.front());else selectGroup(owner.app.transform_chords(expected,values.front().start-expected.front().start,copying));});
}
void ChordTrack::mouseMove(const juce::MouseEvent& e) {
    if(owner.knifeMode){setMouseCursor(juce::MouseCursor::CrosshairCursor);return;}
    const auto c=chordAt(e.position);
    setMouseCursor(c&&!owner.app.recording()?juce::MouseCursor(edgeAt(*c,e.position.x)?
        juce::MouseCursor::LeftRightResizeCursor:juce::MouseCursor::DraggingHandCursor):juce::MouseCursor::NormalCursor);
}
void ChordTrack::mouseDoubleClick(const juce::MouseEvent& e) {if(owner.knifeMode)return;
    if(e.mods.isLeftButtonDown()&&e.x>=Arrangement::left)
        owner.run([&]{const auto c=chordAt(e.position);if(c)edit(c,c->start);else create(tickAt(e.position.x));});
}
bool ChordTrack::keyPressed(const juce::KeyPress& key) {
    if(key.getKeyCode()==juce::KeyPress::escapeKey){cancel();return true;}
    if(original||group.selecting)return true; // project shortcuts cannot commit midway through preview
    if(selected&&!key.getModifiers().isAnyModifierKeyDown()&&(key.getKeyCode()==juce::KeyPress::deleteKey||key.getKeyCode()==juce::KeyPress::backspaceKey||key.getKeyCode()==juce::KeyPress::returnKey)){
        const auto p=owner.project();const auto it=std::find_if(p->chords.begin(),p->chords.end(),[&](const auto& c){return c.id==*selected;});
        if(it!=p->chords.end()){const auto c=*it;owner.run([&]{if(key.getKeyCode()!=juce::KeyPress::returnKey)removeSelected();else edit(c,c.start);});}
        return true;
    }
    return owner.workspaceShortcut(key);
}
void ChordTrack::focusLost(FocusChangeType) { cancel(); }
} // namespace ui
