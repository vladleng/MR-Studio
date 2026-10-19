#include "MusicalGrid.h"
#pragma once
#include "Desktop.h"
#include <set>
#include <map>
namespace ui {
juce::MouseCursor noteTrimCursor(bool right);
// All gestures are local previews. Only release submits one shared SetMidiNotes command.
class PianoRoll final : public juce::Component, private juce::Timer {
public:
    PianoRoll(Desktop& d,mrs::Id clip):owner(d),id(std::move(clip)),playhead([this]{const float x=keys+static_cast<float>(owner.visualTick()/mrs::ppq)*beatWidth;const int top=scrollY()+ruler;return x>=scrollX()+keys&&x<getWidth()?juce::Rectangle<float>{x,static_cast<float>(top),1.f,static_cast<float>(juce::jmax(0,getHeight()-top))}:juce::Rectangle<float>{};}){setName("Piano roll");setComponentID("piano-roll");setWantsKeyboardFocus(true);addAndMakeVisible(playhead);sync();startTimer(120);}
    void resized() override{playhead.update();}
    ~PianoRoll() override{releaseNote();}
    std::function<void()> changed,viewChanged;
    std::function<void()> toolChanged;
    bool splitTool{};
    void setSplitTool(bool next){cancel();splitTool=next;if(toolChanged)toolChanged();repaint();}
    std::function<void(float,juce::ModifierKeys,float,float)> wheel;
    void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& w) override{if(wheel){const bool horizontal=std::abs(w.deltaX)>std::abs(w.deltaY);wheel(horizontal?w.deltaX:w.deltaY,horizontal?juce::ModifierKeys(e.mods.getRawFlags()|juce::ModifierKeys::shiftModifier):e.mods,e.position.x,e.position.y);}}
    std::function<void(juce::String)> error;
    std::set<std::string> selected;
    mrs::Tick grid{mrs::ppq/4},cursor{};
    float beatWidth{96.f},rowHeight{18.f};
    static constexpr int keys=64,ruler=24;
    std::vector<mrs::MidiNote> notes;std::vector<mrs::Clip> contextClips;mrs::Tick timelineEnd{};
    int auditionPitch() const{return sounding;}
    mrs::Tick playheadTick() const{const auto p=owner.project();return mrs::Timeline(p->time,p->sample_rate).to_ticks(owner.displayState().sample);}
    void transport(int command){cancel();owner.action(command);repaint();}
    void activate(mrs::Id next){cancel();id=std::move(next);selected.clear();cursor=0;setEnabled(true);sync();}
    mrs::MidiClip clip;
    void sync(){if(gesture)return;const auto p=owner.project();const auto at=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==id;});if(at==p->clips.end()||!at->midi){setEnabled(false);releaseNote();return;}track=at->track;clip=*at->midi;notes=clip.notes;contextClips.clear();timelineEnd=clip.start+clip.length;for(const auto& c:p->clips)if(c.midi&&c.track==track){contextClips.push_back(c);timelineEnd=std::max(timelineEnd,c.midi->start+c.midi->length);}std::stable_partition(contextClips.begin(),contextClips.end(),[&](const auto& c){return c.id!=id;});timelineEnd=std::min(mrs::max_tick,timelineEnd+4*mrs::ppq);if(cursor<clip.source_offset)cursor=clip.source_offset;std::erase_if(selected,[&](const auto& value){return std::none_of(notes.begin(),notes.end(),[&](const auto& n){return n.id.value==value;});});setSize(keys+static_cast<int>(std::min(1000000.,std::ceil(static_cast<double>(timelineEnd)/mrs::ppq*beatWidth)))+2,ruler+128*static_cast<int>(rowHeight));repaint();if(viewChanged)viewChanged();}
    juce::Rectangle<float> noteRect(const mrs::MidiNote& n) const{return {keys+static_cast<float>(static_cast<double>(clip.start+n.start-clip.source_offset)/mrs::ppq)*beatWidth,ruler+(127-n.pitch)*rowHeight,static_cast<float>(static_cast<double>(n.length)/mrs::ppq)*beatWidth,rowHeight-1};}
    static juce::String noteName(int pitch){static const std::array<const char*,12> names{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};return juce::String(names[static_cast<std::size_t>(pitch%12)])+juce::String(pitch/12-1);}
    juce::Colour noteColour(const mrs::MidiNote& n,const mrs::Clip& c)const{
        if(owner.view.noteClipColors)return juce::Colour(0xff000000|c.color.value_or(ObjectPalette::defaultClip));
        const auto strength=(n.velocity-1)/126.f;const juce::Colour low(0xff3797ef),middle(0xff36b9b7),high(0xffe2b44a);
        return strength<.5f?low.interpolatedWith(middle,strength*2):middle.interpolatedWith(high,(strength-.5f)*2);
    }
    int edgeAt(const mrs::MidiNote& n,juce::Point<float> point)const{const auto r=noteRect(n);if(!r.contains(point))return 0;const auto left=point.x-r.getX(),right=r.getRight()-point.x;return std::min(left,right)<std::min(6.f,r.getWidth()/3)?(left<=right?-1:1):0;}
    void mouseMove(const juce::MouseEvent& e)override{const auto hit=hitNote(e.position);if(splitTool){setMouseCursor(juce::MouseCursor::CrosshairCursor);return;}const auto edge=hit<0?0:edgeAt(notes[static_cast<std::size_t>(hit)],e.position);setMouseCursor(edge?noteTrimCursor(edge>0):juce::MouseCursor::NormalCursor);}
    void mouseExit(const juce::MouseEvent&)override{if(!gesture)setMouseCursor(juce::MouseCursor::NormalCursor);}
    void paint(juce::Graphics& g) override{
        g.fillAll(juce::Colour(surface));const auto bounds=g.getClipBounds();const int keyboardX=scrollX(),headerY=scrollY();g.setFont(owner.theme.font(10));
        const auto xAt=[&](mrs::Tick t){return keys+static_cast<float>(static_cast<double>(t)/mrs::ppq)*beatWidth;};
        for(int pitch=0;pitch<128;++pitch){const int y=ruler+(127-pitch)*static_cast<int>(rowHeight);if(y+rowHeight<bounds.getY()||y>bounds.getBottom())continue;const int pc=pitch%12;const bool black=pc==1||pc==3||pc==6||pc==8||pc==10;
            for(const auto& c:contextClips){const bool active=c.id==id;const float x=xAt(c.midi->start),right=xAt(c.midi->start+c.midi->length);g.setColour(juce::Colour(active?(black?0xff25292e:0xff30363c):(black?0xff202429:0xff292e33)));g.fillRect(x,static_cast<float>(y),right-x,rowHeight);g.setColour(juce::Colour(active?0xff41474d:0xff343a40));g.drawHorizontalLine(y,x,right);}
            g.setColour(juce::Colour(black?0xff161b20:0xffbec9d2));g.fillRect(keyboardX,y,keys-1,static_cast<int>(rowHeight)-1);if(pc==0){g.setColour(black?juce::Colours::white:juce::Colours::black);g.drawText("C"+juce::String(pitch/12-1),keyboardX+3,y,keys-6,static_cast<int>(rowHeight),juce::Justification::centredLeft);}}
        const auto step=grid>0?grid:mrs::ppq/4;const double px=static_cast<double>(step)/mrs::ppq*beatWidth;const int first=std::max(0,static_cast<int>((bounds.getX()-keys)/px)),last=static_cast<int>((bounds.getRight()-keys)/px)+1;
        for(int i=first;i<=last;++i){const auto tick=static_cast<mrs::Tick>(i)*step;if(tick>timelineEnd)break;const auto x=xAt(tick);g.setColour(juce::Colour(tick%mrs::ppq==0?0xff4a525a:0xff30363c));g.drawVerticalLine(static_cast<int>(x),ruler,static_cast<float>(getHeight()));}
        const auto gridProject=owner.project();const auto gridFirst=std::max(mrs::Tick{0},static_cast<mrs::Tick>((bounds.getX()-keys)/beatWidth*mrs::ppq));const auto gridLast=std::min(timelineEnd,static_cast<mrs::Tick>((bounds.getRight()-keys)/beatWidth*mrs::ppq));const auto gridSpacing=std::max(mrs::Tick{1},static_cast<mrs::Tick>(std::ceil(14*mrs::ppq/beatWidth)));visitMeterGrid(gridProject->time,gridFirst,gridLast,gridSpacing,[&](mrs::Tick tick,mrs::MusicalPosition pos){g.setColour(juce::Colour(pos.beat==1?outline:separatorColour));g.drawVerticalLine(static_cast<int>(xAt(tick)),ruler,static_cast<float>(getHeight()));});
        g.saveState();g.reduceClipRegion(juce::Rectangle<int>(keyboardX+keys,headerY+ruler,getWidth()-keyboardX-keys,getHeight()-headerY-ruler));
        for(const auto& c:contextClips){
            const auto& midi=*c.midi;const bool active=c.id==id;const auto& visibleNotes=active?notes:midi.notes;
            g.saveState();g.reduceClipRegion(juce::Rectangle<float>{xAt(midi.start),static_cast<float>(ruler),xAt(midi.start+midi.length)-xAt(midi.start),static_cast<float>(getHeight()-ruler)}.toNearestInt());
            for(const auto& n:visibleNotes){const juce::Rectangle<float> rect{xAt(midi.start+n.start-midi.source_offset),ruler+(127-n.pitch)*rowHeight,static_cast<float>(static_cast<double>(n.length)/mrs::ppq)*beatWidth,rowHeight-1};if(rect.intersects(bounds.toFloat()))drawNote(g,n,c,rect,active);}
            g.restoreState();
        }
        if(gesture==4){g.setColour(juce::Colours::skyblue.withAlpha(.15f));g.fillRect(box);g.setColour(juce::Colours::skyblue);g.drawRect(box,1);}
        g.restoreState();
        g.setColour(juce::Colour(surface));g.fillRect(keyboardX,headerY,getWidth(),ruler);const auto p=owner.project();const auto visibleFirst=std::max(mrs::Tick{0},static_cast<mrs::Tick>((bounds.getX()-keys)/beatWidth*mrs::ppq));const auto visibleLast=std::min(timelineEnd,static_cast<mrs::Tick>((bounds.getRight()-keys)/beatWidth*mrs::ppq));int lastLabel=keyboardX+keys-64;visitMeterGrid(p->time,visibleFirst,visibleLast,std::max(mrs::Tick{1},static_cast<mrs::Tick>(std::ceil(14*mrs::ppq/beatWidth))),[&](mrs::Tick tick,mrs::MusicalPosition pos){const int x=static_cast<int>(xAt(tick));if(x-lastLabel>=32){g.setColour(juce::Colours::lightgrey);g.drawText(juce::String(pos.bar)+":"+juce::String(pos.beat),x+3,headerY,60,ruler,juce::Justification::centredLeft);lastLabel=x;}});
        if(hasKeyboardFocus(true)){g.setColour(juce::Colours::skyblue);g.drawRect(getLocalBounds(),1);}
    }
    void mouseDown(const juce::MouseEvent& e) override{
        grabKeyboardFocus();sync();releaseNote();
        if(e.mods.isPopupMenu()){showNoteMenu(e);return;}
        if(!e.mods.isLeftButtonDown())return;
        if(e.y<scrollY()+ruler&&e.x>=scrollX()+keys){const auto p=owner.project();const auto tick=std::clamp(static_cast<mrs::Tick>(std::llround((e.x-keys)/beatWidth*mrs::ppq)),mrs::Tick{0},mrs::max_tick);owner.run([&]{owner.app.seek(mrs::Timeline(p->time,p->sample_rate).to_samples(tick));});repaint();return;}
        if(e.x>=scrollX()+keys&&e.y>=scrollY()+ruler)for(const auto& c:contextClips)if(c.id!=id&&e.x>=keys+static_cast<double>(c.midi->start)/mrs::ppq*beatWidth&&e.x<keys+static_cast<double>(c.midi->start+c.midi->length)/mrs::ppq*beatWidth){cancel();owner.activateClip(c.id);return;}
        if(!editable())return;
        if(e.x<scrollX()+keys){if(e.y>=scrollY()+ruler)audition(juce::jlimit(0,127,127-(e.y-ruler)/static_cast<int>(rowHeight)),100,0);return;}
        cursor=position(e.position.x);if(!insideActive(e.position.x))return;
        const auto hit=hitNote(e.position);selectionBefore=selected;shiftRemove=false;dragged=false;copyIds.clear();copying=false;
        if(hit>=0){
            const auto& n=notes[static_cast<std::size_t>(hit)];
            if(splitTool){if(!selected.contains(n.id.value))selected={n.id.value};splitAt(cursor);return;}
            const bool additive=e.mods.isShiftDown()||(e.mods.isCtrlDown()&&!e.mods.isAltDown());
            if(additive){shiftRemove=selected.contains(n.id.value);selected.insert(n.id.value);}
            else if(!selected.contains(n.id.value))selected={n.id.value};
            dragNote=n.id;origin=e.position;before=notes;beforeClip=clip;gestureStore=owner.app.services().projects;initialSelection=selected;
            const auto edge=edgeAt(n,e.position);copying=e.mods.isAltDown()&&!e.mods.isCtrlDown();
            gesture=e.mods.isAltDown()&&e.mods.isCtrlDown()?3:copying?1:edge<0?5:edge>0?2:1;
            audition(n.pitch,n.velocity,n.channel);
        }else{
            if(splitTool)return;
            if(!e.mods.isCtrlDown()&&!e.mods.isShiftDown())selected.clear();
            initialSelection=selected;origin=e.position;gesture=4;box={e.position,e.position};
        }
        repaint();
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override{
        if(splitTool)return;cancel();if(!editable()||e.x<scrollX()+keys||e.y<scrollY()+ruler)return;if(!e.mods.isLeftButtonDown()||!insideActive(e.position.x))return;const auto hit=hitNote(e.position);if(hit>=0){selected={notes[static_cast<std::size_t>(hit)].id.value};erase();return;}
        auto next=notes;mrs::MidiNote n;n.id=mrs::new_id();n.start=position(static_cast<float>(e.x));n.length=std::min(grid>0?grid:mrs::ppq/4,clip.source_offset+clip.length-n.start);n.pitch=juce::jlimit(0,127,127-(e.y-ruler)/static_cast<int>(rowHeight));n.velocity=100;n.channel=0;if(n.length<1)return;next.push_back(n);selected={n.id.value};submit(std::move(next));
    }
    void mouseDrag(const juce::MouseEvent& e) override{
        if(!gesture)return;dragged=dragged||origin.getDistanceFrom(e.position)>4.f;
        if(gesture==4){box=juce::Rectangle<float>(origin,e.position);selected=initialSelection;for(const auto& n:notes)if(visible(n)&&visibleRect(n).intersects(box))selected.insert(n.id.value);repaint();return;}
        if(!dragged)return;shiftRemove=false;notes=before;
        const auto dt=static_cast<mrs::Tick>(std::llround((e.position.x-origin.x)/beatWidth*mrs::ppq));const bool useSnap=grid>0&&!e.mods.isShiftDown();
        auto snapped=useSnap?static_cast<mrs::Tick>(std::llround(static_cast<double>(dt)/grid))*grid:dt;
        if(gesture==1){
            if(!e.mods.isShiftDown()&&std::abs(e.position.x-origin.x)<12.f)snapped=0;
            else if(useSnap){const auto anchor=std::find_if(before.begin(),before.end(),[&](const auto& n){return n.id==dragNote;});if(anchor!=before.end()){const auto global=clip.start+anchor->start-clip.source_offset;snapped=static_cast<mrs::Tick>(std::llround(static_cast<double>(global+dt)/grid))*grid-global;}}
        }
        if(gesture==3){const int dv=static_cast<int>(std::lround((origin.y-e.position.y)*.7f));for(auto& n:notes)if(initialSelection.contains(n.id.value))n.velocity=juce::jlimit(1,127,n.velocity+dv);}
        else{
            mrs::Tick low=-mrs::max_tick,high=mrs::max_tick;int pitchLow=-127,pitchHigh=127;
            for(const auto& n:before)if(initialSelection.contains(n.id.value)){
                const auto left=std::min(clip.source_offset,n.start),right=std::max(clip.source_offset+clip.length,n.start+n.length);
                low=std::max(low,gesture==2?1-n.length:left-n.start);
                high=std::min(high,gesture==5?n.length-1:right-n.start-n.length);
                pitchLow=std::max(pitchLow,-n.pitch);pitchHigh=std::min(pitchHigh,127-n.pitch);
            }
            const auto delta=std::clamp(snapped,low,std::max(low,high));const int dp=juce::jlimit(pitchLow,pitchHigh,static_cast<int>(std::lround((origin.y-e.position.y)/rowHeight)));
            if(copying&&(delta||dp)){
                selected.clear();
                for(auto n:before)if(initialSelection.contains(n.id.value)){if(!copyIds.contains(n.id.value))copyIds[n.id.value]=mrs::new_id();n.id=copyIds[n.id.value];n.start+=delta;n.pitch+=dp;selected.insert(n.id.value);notes.push_back(n);}
            }else{
                selected=initialSelection;
                if(!copying)for(auto& n:notes)if(initialSelection.contains(n.id.value)){if(gesture==2)n.length+=delta;else if(gesture==5){n.start+=delta;n.length-=delta;}else{n.start+=delta;n.pitch+=dp;}}
            }
        }
        if(gesture==1){const auto soundingId=copyIds.contains(dragNote.value)?copyIds.at(dragNote.value):dragNote;for(const auto& n:notes)if(n.id==soundingId&&n.pitch!=sounding){releaseNote();audition(n.pitch,n.velocity,n.channel);break;}}
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override{
        releaseNote(true);
        if(gesture&&gesture!=4){
            auto next=notes;notes=before;gesture=0;
            if(shiftRemove&&!dragged)selected.erase(dragNote.value);
            if(dragged){
                const auto p=owner.project();const auto at=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==id;});
                if(owner.app.services().projects==gestureStore&&at!=p->clips.end()&&at->midi&&*at->midi==beforeClip)submit(std::move(next));
                else{selected=selectionBefore;report("Notes or project changed during gesture; preview cancelled");sync();}
            }
        }
        gesture=0;gestureStore.reset();copying=false;copyIds.clear();shiftRemove=false;repaint();
    }
    void visibilityChanged() override{if(!isShowing())cancel();}
    void focusLost(FocusChangeType) override{cancel();}
    bool keyPressed(const juce::KeyPress& key) override{
        const int code=key.getKeyCode();const auto mods=key.getModifiers();
        if(code==juce::KeyPress::spaceKey&&!mods.isAnyModifierKeyDown()){owner.run([&]{transport(owner.app.engine()->state().playback==mrs::PlaybackState::playing?31:30);});return true;}
        if(code==juce::KeyPress::escapeKey){cancel();return true;}
        if(!mods.isAnyModifierKeyDown()&&(code=='1'||code=='3')){setSplitTool(code=='3');return true;}
        if(mods.isCtrlDown()&&!mods.isAltDown()){
            if(code=='A'){cancel();sync();for(const auto& n:notes)if(visible(n))selected.insert(n.id.value);repaint();return true;}
            if(code=='C'){copy();return true;}if(code=='V'){paste();return true;}
            if(code=='Z'||code=='Y'){cancel();owner.run([&]{if(code=='Y'||mods.isShiftDown())owner.app.redo();else owner.app.undo();owner.refresh();});sync();if(changed)changed();return true;}
        }
        if((code==juce::KeyPress::upKey||code==juce::KeyPress::downKey)&&!mods.isAltDown()&&(!mods.isCtrlDown()||mods.isShiftDown())){
            transpose((code==juce::KeyPress::upKey?1:-1)*(mods.isShiftDown()?12:1),mods.isCtrlDown()&&mods.isShiftDown());return true;
        }
        if(!mods.isAnyModifierKeyDown()&&code=='D'){duplicate();return true;}
        if(!mods.isAnyModifierKeyDown()&&code=='S'){splitAt(cursor);return true;}
        if(code==juce::KeyPress::deleteKey||code==juce::KeyPress::backspaceKey){erase();return true;}return false;
    }
    void copy(){cancel();sync();owner.noteClipboard.clear();for(const auto& n:notes)if(selected.contains(n.id.value))owner.noteClipboard.push_back(n);}
    void paste(){
        cancel();sync();if(!editable()||owner.noteClipboard.empty())return;
        const auto& source=owner.noteClipboard;const auto begin=std::min_element(source.begin(),source.end(),[](const auto& a,const auto& b){return a.start<b.start;})->start;
        for(const auto& n:source)if(n.start-begin>mrs::max_tick-cursor||n.length>clip.source_offset+clip.length-(cursor+n.start-begin)){report("Pasted group must fit inside the MIDI clip");return;}
        auto next=notes;selected.clear();for(auto n:source){n.id=mrs::new_id();n.start=cursor+n.start-begin;selected.insert(n.id.value);next.push_back(n);}submit(std::move(next));
    }
    void duplicate(){
        cancel();sync();if(!editable()||selected.empty())return;
        mrs::Tick begin=mrs::max_tick,end{};for(const auto& n:notes)if(selected.contains(n.id.value)){begin=std::min(begin,n.start);end=std::max(end,n.start+n.length);}
        const auto delta=end-begin;if(delta<1)return;
        for(const auto& n:notes)if(selected.contains(n.id.value)&&delta>clip.source_offset+clip.length-n.start-n.length){report("Duplicated group must fit inside the MIDI clip");return;}
        auto next=notes;std::set<std::string> result;for(auto n:notes)if(selected.contains(n.id.value)){n.id=mrs::new_id();n.start+=delta;result.insert(n.id.value);next.push_back(n);}selected=std::move(result);submit(std::move(next));
    }
    void transpose(int delta,bool copyNotes){
        cancel();sync();if(!editable()||selected.empty())return;
        for(const auto& n:notes)if(selected.contains(n.id.value)&&(n.pitch+delta<0||n.pitch+delta>127)){report("The whole note group must stay within MIDI pitches 0..127");return;}
        auto next=notes;if(copyNotes){std::set<std::string> result;for(auto n:notes)if(selected.contains(n.id.value)){n.id=mrs::new_id();n.pitch+=delta;result.insert(n.id.value);next.push_back(n);}selected=std::move(result);}
        else for(auto& n:next)if(selected.contains(n.id.value))n.pitch+=delta;
        submit(std::move(next));
    }
    void splitAt(mrs::Tick cut){
        cancel();sync();if(!editable()||selected.empty())return;
        auto next=notes;std::set<std::string> result=selected;
        for(std::size_t i=0;i<notes.size();++i){const auto& n=notes[i];if(selected.contains(n.id.value)&&cut>n.start&&cut<n.start+n.length){auto right=n;right.id=mrs::new_id();right.start=cut;right.length=n.start+n.length-cut;next[i].length=cut-n.start;next.push_back(right);result.insert(right.id.value);}}
        selected=std::move(result);submit(std::move(next));
    }
    void erase(){cancel();sync();if(!editable())return;auto next=notes;std::erase_if(next,[&](const auto& n){return selected.contains(n.id.value);});submit(std::move(next));selected.clear();}
    void cancel(){releaseNote();if(gesture){if(gesture!=4)notes=before;selected=selectionBefore;}gesture=0;gestureStore.reset();copying=false;shiftRemove=false;copyIds.clear();repaint();}
private:
    friend void stateBusySmoke();
    friend void pianoRoll214Smoke();
    Desktop& owner;mrs::Id id,track,dragNote;int gesture{};juce::Point<float> origin;juce::Rectangle<float> box;std::set<std::string> initialSelection;std::vector<mrs::MidiNote> before;mrs::MidiClip beforeClip;
    bool copying{},dragged{},shiftRemove{};
    std::set<std::string> selectionBefore;
    std::map<std::string,mrs::Id> copyIds;
    std::shared_ptr<mrs::IProjectStore> gestureStore;
    bool visible(const mrs::MidiNote& n)const{return n.start<clip.source_offset+clip.length&&n.start+n.length>clip.source_offset;}
    juce::Rectangle<float> visibleRect(const mrs::MidiNote& n)const{const auto left=std::max(static_cast<float>(scrollX()+keys),keys+static_cast<float>(clip.start)/mrs::ppq*beatWidth),right=keys+static_cast<float>(clip.start+clip.length)/mrs::ppq*beatWidth;return noteRect(n).getIntersection({left,static_cast<float>(scrollY()+ruler),std::max(0.f,right-left),static_cast<float>(getHeight()-scrollY()-ruler)});}
    void drawNote(juce::Graphics& g,const mrs::MidiNote& n,const mrs::Clip& c,juce::Rectangle<float> rect,bool active){
        juce::Graphics::ScopedSaveState noteState(g);
        const auto color=noteColour(n,c);const auto alpha=active?.8f:.4f;const auto fill=rect.getWidth()*n.velocity/127.f;
        juce::Path shape;shape.addRoundedRectangle(rect,2.f);g.saveState();g.reduceClipRegion(shape);
        g.setColour(color.darker(.42f).withAlpha(alpha));g.fillRect(rect.withTrimmedLeft(fill));
        g.setColour(color.withAlpha(alpha));g.fillRect(rect.withWidth(fill));g.restoreState();
        const bool chosen=active&&selected.contains(n.id.value);
        g.setColour(chosen?juce::Colour(accent):color.withAlpha(alpha));g.drawRoundedRectangle(rect.reduced(.5f),2.f,chosen?1.5f:1.f);
        if(rect.getWidth()>=34&&rect.getHeight()>=15){
            const auto ink=ObjectPalette::ink(color.getARGB()&0xffffff,.8f,surface);g.setFont(owner.theme.font(std::min(12.f,rect.getHeight()-3)));
            g.setColour(ink==juce::Colour(0xff111720)?juce::Colours::white.withAlpha(.4f):juce::Colours::black.withAlpha(.65f));
            g.drawText(noteName(n.pitch),rect.reduced(3,0).translated(.5f,.5f),juce::Justification::centredLeft);
            g.setColour(ink.withMultipliedAlpha(active?1.f:.6f));g.drawText(noteName(n.pitch),rect.reduced(3,0),juce::Justification::centredLeft);
        }
    }
    void showNoteMenu(const juce::MouseEvent& e){
        cancel();sync();const auto hit=hitNote(e.position);if(hit>=0&&!selected.contains(notes[static_cast<std::size_t>(hit)].id.value))selected={notes[static_cast<std::size_t>(hit)].id.value};cursor=position(e.position.x);
        const auto store=owner.app.services().projects;const auto expected=clip;const auto clipId=id;const auto cut=cursor;juce::PopupMenu menu;menu.setLookAndFeel(&owner.theme);
        const bool has=!selected.empty();menu.addItem(1,"Copy notes (Ctrl+C)",has);menu.addItem(2,"Paste notes (Ctrl+V)",!owner.noteClipboard.empty());menu.addItem(3,"Duplicate notes (D)",has);menu.addItem(4,"Split selected notes (S)",has);menu.addItem(5,"Delete selected notes",has);menu.addItem(6,"Select all notes (Ctrl+A)");
        juce::Component::SafePointer<PianoRoll> safe(this);menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({e.getScreenX(),e.getScreenY(),1,1}),[safe,store,expected,clipId,cut](int action){if(!safe||!action||safe->id!=clipId||safe->owner.app.services().projects!=store)return;const auto p=safe->owner.project();const auto at=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==clipId;});if(at==p->clips.end()||!at->midi||*at->midi!=expected)return;if(action==1)safe->copy();else if(action==2)safe->paste();else if(action==3)safe->duplicate();else if(action==4)safe->splitAt(cut);else if(action==5)safe->erase();else safe->keyPressed(juce::KeyPress('A',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0));});repaint();
    }
    int sounding{-1},soundChannel{};
    PlayheadLine playhead;
    void timerCallback() override{mrs::audio::RealtimeState state;if(gesture&&(owner.app.recording()||!owner.app.engine()->try_state(state)||state.playback==mrs::PlaybackState::playing))cancel();repaint();if(!gesture){const auto previous=notes;sync();if(notes!=previous&&changed)changed();}}
    void report(const juce::String& text){if(error)error(text);}
    bool editable(){mrs::audio::RealtimeState state;if(!owner.app.engine()->try_state(state)){report("Audio state updating; try again");return false;}if(owner.app.recording()||state.playback==mrs::PlaybackState::playing){report("Pause or stop before editing / auditioning notes");return false;}return isEnabled();}
    mrs::Tick position(float x) const{auto tick=static_cast<mrs::Tick>(std::llround((x-keys)/beatWidth*mrs::ppq));if(grid>0)tick=static_cast<mrs::Tick>(std::llround(static_cast<double>(tick)/grid))*grid;return clip.source_offset+std::clamp(tick-clip.start,mrs::Tick{0},std::max(mrs::Tick{0},clip.length-1));}
    int scrollX() const{if(auto* viewport=findParentComponentOfClass<juce::Viewport>())return viewport->getViewPositionX();return 0;}
    int scrollY() const{if(auto* viewport=findParentComponentOfClass<juce::Viewport>())return viewport->getViewPositionY();return 0;}
    bool insideActive(float x) const{return x>=keys+static_cast<double>(clip.start)/mrs::ppq*beatWidth&&x<keys+static_cast<double>(clip.start+clip.length)/mrs::ppq*beatWidth;}
    int hitNote(juce::Point<float> p) const{if(!insideActive(p.x))return -1;if(p.x<scrollX()+keys||p.y<scrollY()+ruler)return -1;for(std::size_t i=notes.size();i>0;--i)if(noteRect(notes[i-1]).contains(p))return static_cast<int>(i-1);return -1;}
    void submit(std::vector<mrs::MidiNote> next){try{releaseNote();if(next==notes)return;mrs::diagnostics::event("midi_note_commit_begin clip="+id.value+" count="+std::to_string(next.size()));owner.app.set_midi_notes(id,std::move(next));owner.refresh();sync();if(changed)changed();mrs::diagnostics::event("midi_note_commit_end");}catch(const std::exception& e){mrs::diagnostics::event(std::string("midi_note_error ")+e.what());report(juce::String::fromUTF8(e.what()));sync();}}
    void audition(int pitch,int velocity,int channel){if(owner.app.audition_note(track,pitch,velocity,channel)){sounding=pitch;soundChannel=channel;}}
    void releaseNote(bool soft=false){if(sounding>=0){owner.app.audition_note(track,sounding,100,soundChannel,false,soft);sounding=-1;}}
};
}
