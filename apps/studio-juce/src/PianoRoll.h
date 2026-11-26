#include "MusicalGrid.h"
#pragma once
#include "Desktop.h"
#include "DrumGridRows.h"
#include "NoteGrid.h"
#include <set>
#include <map>
namespace ui {
juce::MouseCursor noteTrimCursor(bool right);
// All gestures are local previews. Only release submits one shared SetMidiNotes command.
class PianoRoll final : public juce::Component, private juce::Timer {
public:
    PianoRoll(Desktop& d,mrs::Id clip):owner(d),id(std::move(clip)),playhead([this]{const float timelineX=static_cast<float>(owner.visualTick()/mrs::ppq)*beatWidth;if(!gesture&&followPosition)followPosition(timelineX);const float x=railWidth()+timelineX;const int top=scrollY()+ruler;return x>=scrollX()+railWidth()&&x<getWidth()?juce::Rectangle<float>{x,static_cast<float>(top),1.f,static_cast<float>(juce::jmax(0,getHeight()-top))}:juce::Rectangle<float>{};}){setName("Piano roll");setComponentID("piano-roll");setWantsKeyboardFocus(true);addAndMakeVisible(playhead);sync();startTimer(120);}
    void resized() override{playhead.update();}
    ~PianoRoll() override{releaseNote();}
    std::function<void()> changed,viewChanged;
    std::function<void(float)> followPosition;
    void refreshPlayhead(){playhead.update();}
    static constexpr unsigned velocityPreviewTailMs=1500;
    std::function<void()> toolChanged;
    std::function<void(const mrs::Id&)> toggleDrumGroup;
    bool drums{};
    int railWidth() const{return drums?188:keys;}
    const mrs::Id& trackId() const{return track;}
    void setDrums(bool active){cancel();drums=active;rowProject.reset();sync();}
    void selectVisible(){cancel();sync();for(const auto& n:notes)if(noteDisplayed(n))selected.insert(n.id.value);repaint();}
    bool noteVisible(const mrs::MidiNote& n) const{return visible(n);}
    bool noteDisplayed(const mrs::MidiNote& n) const{return drums?!displayedRect(n,clip).isEmpty():visible(n);}
    juce::Rectangle<float> displayedNoteRect(const mrs::MidiNote& n)const{return displayedRect(n,clip);}
    bool drumAddressVisible(const mrs::MidiNote& n) const{return !drums||drumRows.index(n.pitch,n.channel)>=0;}
    const DrumGridRows& drumLayout() const{return drumRows;}
    bool splitTool{};
    void setSplitTool(bool next){cancel();splitTool=next;if(toolChanged)toolChanged();repaint();}
    std::function<void(float,juce::ModifierKeys,float,float)> wheel;
    void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& w) override{if(wheel){const bool horizontal=std::abs(w.deltaX)>std::abs(w.deltaY);wheel(horizontal?w.deltaX:w.deltaY,horizontal?juce::ModifierKeys(e.mods.getRawFlags()|juce::ModifierKeys::shiftModifier):e.mods,e.position.x,e.position.y);}}
    std::function<void(juce::String)> error;
    std::set<std::string> selected;
    mrs::Tick grid{mrs::ppq/4},cursor{};
    // Note-value grid (straight/triplet/5:4/7:8/swing) and its snap switch.
    // grid stays the nominal step in ticks for callers that only need a length.
    NoteGridSpec gridSpec{};
    bool snapGrid{true};
    void setNoteGrid(const NoteGridSpec& spec,bool snapEnabled){
        cancel();gridSpec=spec;snapGrid=snapEnabled;
        grid=std::max<mrs::Tick>(1,static_cast<mrs::Tick>(std::llround(gridSpec.step())));
        repaint();
    }
    float beatWidth{96.f},rowHeight{18.f};
    static constexpr int keys=43,ruler=24;
    std::vector<mrs::MidiNote> notes;std::vector<mrs::Clip> contextClips;mrs::Tick timelineEnd{};
    int auditionPitch() const{return sounding;}
    mrs::Tick playheadTick() const{const auto p=owner.project();return mrs::Timeline(p->time,p->sample_rate).to_ticks(owner.displayState().sample);}
    void transport(int command){cancel();owner.action(command);repaint();}
    void activate(mrs::Id next){cancel();id=std::move(next);selected.clear();cursor=0;setEnabled(true);sync();}
    mrs::MidiClip clip;
    void sync(){if(gesture)return;const auto p=owner.project();const auto at=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==id;});if(at==p->clips.end()||!at->midi){setEnabled(false);releaseNote();return;}setEnabled(true);if(track!=at->track){track=at->track;liveActivity=owner.app.live_note_activity(track);liveKey=-1;}clip=*at->midi;notes=clip.notes;contextClips.clear();timelineEnd=clip.start+clip.length;for(const auto& c:p->clips)if(c.midi&&c.track==track){contextClips.push_back(c);timelineEnd=std::max(timelineEnd,c.midi->start+c.midi->length);}std::stable_partition(contextClips.begin(),contextClips.end(),[&](const auto& c){return c.id!=id;});timelineEnd=std::min(mrs::max_tick,timelineEnd+4*mrs::ppq);if((drums||owner.view.noteInstrumentColors)&&(rowProject!=p||rowTrack!=track)){const auto t=std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& value){return value.id==track;});drumRows.rebuild(t!=p->tracks.end()&&t->drum_map?&*t->drum_map:nullptr,contextClips);rowProject=p;rowTrack=track;}if(cursor<clip.source_offset)cursor=clip.source_offset;if(!selected.empty()){std::set<std::string> retained;for(const auto& n:notes)if(selected.contains(n.id.value)&&(!drums||noteDisplayed(n)))retained.insert(n.id.value);selected.swap(retained);}setSize(railWidth()+static_cast<int>(std::min(1000000.,std::ceil(static_cast<double>(timelineEnd)/mrs::ppq*beatWidth)))+2,ruler+std::max(1,rowCount())*static_cast<int>(rowHeight));repaint();if(viewChanged)viewChanged();}
    juce::Rectangle<float> noteRect(const mrs::MidiNote& n) const{return eventRect(n,clip);}
    juce::Rectangle<float> eventRect(const mrs::MidiNote& n,const mrs::MidiClip& midi) const{
        const int row=rowOf(n);if(row<0)return {};
        const float x=railWidth()+static_cast<float>(static_cast<double>(midi.start+n.start-midi.source_offset)/mrs::ppq)*beatWidth;
        const float y=ruler+row*rowHeight;
        if(drums){if(n.start<midi.source_offset||n.start>=midi.source_offset+midi.length)return {};const float size=std::clamp(rowHeight-4.f,5.f,14.f);return {x-size*.5f,y+(rowHeight-size)*.5f,size,size};}
        return {x,y,static_cast<float>(static_cast<double>(n.length)/mrs::ppq)*beatWidth,rowHeight-1};
    }
    static juce::String noteName(int pitch){static const std::array<const char*,12> names{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};return juce::String(names[static_cast<std::size_t>(pitch%12)])+juce::String(pitch/12-1);}
    juce::Colour noteColour(const mrs::MidiNote& n,const mrs::Clip& c,int velocity=-1)const{
        if(owner.view.noteInstrumentColors){const auto rgb=drumRows.color(n.pitch,n.channel);return juce::Colour(0xff000000u | rgb.value_or(c.color.value_or(ObjectPalette::defaultClip)));}
        if(owner.view.noteClipColors)return juce::Colour(0xff000000|c.color.value_or(ObjectPalette::defaultClip));
        return ObjectPalette::midiVelocityColour(velocity<0?n.velocity:velocity);
    }
    juce::Colour displayedNoteColour(const mrs::MidiNote& n,const mrs::Clip& c,bool active,int velocity=-1)const{
        const int value=velocity<0?n.velocity:velocity;
        return shadeNote(noteColour(n,c,value),value,active);
    }
    int edgeAt(const mrs::MidiNote& n,juce::Point<float> point)const{const auto r=noteRect(n);if(drums||!r.contains(point))return 0;const auto left=point.x-r.getX(),right=r.getRight()-point.x;return std::min(left,right)<std::min(6.f,r.getWidth()/3)?(left<=right?-1:1):0;}
    void mouseMove(const juce::MouseEvent& e)override{const auto hit=hitNote(e.position);if(splitTool){setMouseCursor(juce::MouseCursor::CrosshairCursor);return;}const auto edge=hit<0?0:edgeAt(notes[static_cast<std::size_t>(hit)],e.position);setMouseCursor(edge?noteTrimCursor(edge>0):juce::MouseCursor::NormalCursor);}
    void mouseExit(const juce::MouseEvent&)override{if(!gesture)setMouseCursor(juce::MouseCursor::NormalCursor);}
    void paint(juce::Graphics& g) override{
        g.fillAll(juce::Colour(surface));const auto bounds=g.getClipBounds();const int keyboardX=scrollX(),headerY=scrollY();g.setFont(owner.theme.font(10));
        const auto xAt=[&](mrs::Tick t){return railWidth()+static_cast<float>(static_cast<double>(t)/mrs::ppq)*beatWidth;};
        for(int row=0;row<rowCount();++row){const int y=ruler+row*static_cast<int>(rowHeight);if(y+rowHeight<bounds.getY()||y>bounds.getBottom())continue;const bool black=drums?drumRows.rows[static_cast<std::size_t>(row)].header||row%2!=0:isBlackKey(127-row);
            for(const auto& c:contextClips){const bool active=c.id==id;const float x=xAt(c.midi->start),right=xAt(c.midi->start+c.midi->length);g.setColour(juce::Colour(active?(black?0xff25292e:0xff30363c):(black?0xff202429:0xff292e33)));g.fillRect(x,static_cast<float>(y),right-x,rowHeight);g.setColour(juce::Colour(active?0xff41474d:0xff343a40));g.drawHorizontalLine(y,x,right);}}
        const double px=std::max(1.,gridSpec.step())/mrs::ppq*beatWidth;
        if(px>=3.){
            const int first=static_cast<int>(std::floor((bounds.getX()-railWidth())/px))-1,last=static_cast<int>(std::ceil((bounds.getRight()-railWidth())/px))+1;
            const auto alpha=snapGrid?1.f:.55f;
            const auto gridInk=juce::Colour(0xff30363c).withMultipliedAlpha(alpha),gridInkBeat=juce::Colour(0xff4a525a).withMultipliedAlpha(alpha);
            for(int i=first;i<=last;++i){const auto tick=gridSpec.position(i);if(tick<0)continue;if(tick>timelineEnd)break;const auto x=xAt(tick);g.setColour(tick%mrs::ppq==0?gridInkBeat:gridInk);g.drawVerticalLine(static_cast<int>(x),ruler,static_cast<float>(getHeight()));}
        }
        const auto gridProject=owner.project();const auto gridFirst=std::max(mrs::Tick{0},static_cast<mrs::Tick>((bounds.getX()-railWidth())/beatWidth*mrs::ppq));const auto gridLast=std::min(timelineEnd,static_cast<mrs::Tick>((bounds.getRight()-railWidth())/beatWidth*mrs::ppq));const auto gridSpacing=std::max(mrs::Tick{1},static_cast<mrs::Tick>(std::ceil(14*mrs::ppq/beatWidth)));visitMeterGrid(gridProject->time,gridFirst,gridLast,gridSpacing,[&](mrs::Tick tick,mrs::MusicalPosition pos){g.setColour(juce::Colour(pos.beat==1?outline:separatorColour));g.drawVerticalLine(static_cast<int>(xAt(tick)),ruler,static_cast<float>(getHeight()));});
        g.saveState();g.reduceClipRegion(juce::Rectangle<int>(keyboardX+railWidth(),headerY+ruler,getWidth()-keyboardX-railWidth(),getHeight()-headerY-ruler));
        for(const auto& c:contextClips){
            const auto& midi=*c.midi;const bool active=c.id==id;const auto& visibleNotes=active?notes:midi.notes;
            g.saveState();g.reduceClipRegion(juce::Rectangle<float>{xAt(midi.start),static_cast<float>(ruler),xAt(midi.start+midi.length)-xAt(midi.start),static_cast<float>(getHeight()-ruler)}.toNearestInt());
            for(const auto& n:visibleNotes){const auto rect=displayedRect(n,midi);if(rect.intersects(bounds.toFloat()))drawNote(g,n,c,rect,active);}
             g.restoreState();
        }
        if(gesture==4){g.setColour(juce::Colours::skyblue.withAlpha(.15f));g.fillRect(box);g.setColour(juce::Colours::skyblue);g.drawRect(box,1);}
        g.restoreState();
        if(drums)paintDrumRail(g,keyboardX,headerY+ruler);else paintKeyboard(g,keyboardX,headerY+ruler);
        g.setColour(juce::Colour(surface));g.fillRect(keyboardX,headerY,getWidth(),ruler);const auto p=owner.project();const auto visibleFirst=std::max(mrs::Tick{0},static_cast<mrs::Tick>((bounds.getX()-railWidth())/beatWidth*mrs::ppq));const auto visibleLast=std::min(timelineEnd,static_cast<mrs::Tick>((bounds.getRight()-railWidth())/beatWidth*mrs::ppq));int lastLabel=keyboardX+railWidth()-64;visitMeterGrid(p->time,visibleFirst,visibleLast,std::max(mrs::Tick{1},static_cast<mrs::Tick>(std::ceil(14*mrs::ppq/beatWidth))),[&](mrs::Tick tick,mrs::MusicalPosition pos){const int x=static_cast<int>(xAt(tick));if(x-lastLabel>=32){g.setColour(juce::Colours::lightgrey);g.drawText(juce::String(pos.bar)+":"+juce::String(pos.beat),x+3,headerY,60,ruler,juce::Justification::centredLeft);lastLabel=x;}});
        if(drums){g.setColour(juce::Colour(accent));g.drawText("DRUM MAP",keyboardX+10,headerY,railWidth()-16,ruler,juce::Justification::centredLeft);}
        if(hasKeyboardFocus(true)){g.setColour(juce::Colours::skyblue);g.drawRect(getLocalBounds(),1);}
    }
    void mouseDown(const juce::MouseEvent& e) override{
        grabKeyboardFocus();sync();releaseNote();
        if(e.mods.isPopupMenu()){showNoteMenu(e);return;}
        if(!e.mods.isLeftButtonDown())return;
        if(e.y<scrollY()+ruler&&e.x>=scrollX()+railWidth()){const auto p=owner.project();const auto tick=std::clamp(static_cast<mrs::Tick>(std::llround((e.x-railWidth())/beatWidth*mrs::ppq)),mrs::Tick{0},mrs::max_tick);owner.run([&]{owner.app.seek(mrs::Timeline(p->time,p->sample_rate).to_samples(tick));});repaint();return;}
        if(e.x>=scrollX()+railWidth()&&e.y>=scrollY()+ruler)for(const auto& c:contextClips)if(c.id!=id&&e.x>=railWidth()+static_cast<double>(c.midi->start)/mrs::ppq*beatWidth&&e.x<railWidth()+static_cast<double>(c.midi->start+c.midi->length)/mrs::ppq*beatWidth){cancel();owner.activateClip(c.id);return;}
        if(drums&&e.x<scrollX()+railWidth()&&e.y>=scrollY()+ruler){
            if(const auto* r=drumRowAt(e.position.y)){
                if(r->header&&!r->group.value.empty()&&toggleDrumGroup){const auto group=r->group;toggleDrumGroup(group);}
                else if(r->pitch>=0&&editable())audition(r->pitch,100,r->channel);
            }return;
        }
        if(!editable())return;
        if(e.x<scrollX()+railWidth()){if(e.y>=scrollY()+ruler)if(const int pitch=keyboardPitchAt(e.getPosition());pitch>=0)audition(pitch,100,0);return; }
        cursor=position(e.position.x);if(!insideActive(e.position.x))return;
        const auto hit=hitNote(e.position,true);selectionBefore=selected;shiftRemove=false;dragged=false;copyIds.clear();copying=false;
        if(hit>=0){
            const auto& n=notes[static_cast<std::size_t>(hit)];
            if(splitTool){if(!visible(n))return;if(!selected.contains(n.id.value))selected={n.id.value};splitAt(cursor);return;}
            const bool velocityDrag=e.mods.isCtrlDown()&&(e.mods.isShiftDown()||e.mods.isAltDown());
            const bool additive=!velocityDrag&&(e.mods.isShiftDown()||(e.mods.isCtrlDown()&&!e.mods.isAltDown()));
            if(additive){shiftRemove=selected.contains(n.id.value);selected.insert(n.id.value);}
            else if(!selected.contains(n.id.value))selected={n.id.value};
            dragNote=n.id;origin=e.position;before=notes;beforeClip=clip;gestureStore=owner.app.services().projects;initialSelection=selected;
            const auto edge=edgeAt(n,e.position);copying=e.mods.isAltDown()&&!e.mods.isCtrlDown();
            gesture=velocityDrag?3:!visible(n)?6:copying?1:edge<0?5:edge>0?2:1;
             if(gesture!=3)for(const auto& item:before)if(!visible(item))initialSelection.erase(item.id.value);
            audition(n.pitch,n.velocity,n.channel);
        }else{
            if(splitTool)return;
            if(!e.mods.isCtrlDown()&&!e.mods.isShiftDown())selected.clear();
            initialSelection=selected;origin=e.position;gesture=4;box={e.position,e.position};
        }
        repaint();
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override{
        if(splitTool)return;cancel();if(!editable()||e.x<scrollX()+railWidth()||e.y<scrollY()+ruler)return;if(!e.mods.isLeftButtonDown()||!insideActive(e.position.x))return;const auto hit=hitNote(e.position);if(hit>=0){selected={notes[static_cast<std::size_t>(hit)].id.value};erase();return;}
        auto next=notes;mrs::MidiNote n;n.id=mrs::new_id();n.start=position(static_cast<float>(e.x));n.length=std::min(snapGrid?grid:mrs::ppq/4,clip.source_offset+clip.length-n.start);n.pitch=juce::jlimit(0,127,127-(e.y-ruler)/static_cast<int>(rowHeight));n.velocity=100;n.channel=0;if(drums){const auto* r=drumRowAt(e.position.y);if(!r||r->pitch<0)return;n.pitch=r->pitch;n.channel=r->channel;}if(n.length<1)return;next.push_back(n);selected={n.id.value};submit(std::move(next));
    }
    void mouseDrag(const juce::MouseEvent& e) override{
        if(!gesture||gesture==6)return;dragged=dragged||origin.getDistanceFrom(e.position)>4.f;
        if(gesture==4){box=juce::Rectangle<float>(origin,e.position);selected=initialSelection;for(const auto& n:notes)if(noteDisplayed(n)&&visibleRect(n).intersects(box))selected.insert(n.id.value);repaint();return;}
        if(!dragged)return;shiftRemove=false;notes=before;
        const auto dt=static_cast<mrs::Tick>(std::llround((e.position.x-origin.x)/beatWidth*mrs::ppq));const bool useSnap=snapGrid&&!e.mods.isShiftDown();
        auto snapped=useSnap?gridSpec.snap(dt):dt;
        if(gesture==1){
            if(!e.mods.isShiftDown()&&std::abs(e.position.x-origin.x)<12.f)snapped=0;
            else if(useSnap){const auto anchor=std::find_if(before.begin(),before.end(),[&](const auto& n){return n.id==dragNote;});if(anchor!=before.end()){const auto global=clip.start+anchor->start-clip.source_offset;snapped=gridSpec.snap(global+dt)-global;}}
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
            int rowDelta=drums&&gesture==1?rowAt(e.position.y)-rowAt(origin.y):0;
            if(rowDelta)for(const auto& n:before)if(initialSelection.contains(n.id.value)&&!drumTarget(n,rowDelta)){rowDelta=0;break;}
            const auto movePitch=[&](mrs::MidiNote& n){if(!drums)n.pitch+=dp;else if(rowDelta)if(const auto address=drumTarget(n,rowDelta)){n.pitch=address->first;n.channel=address->second;}};
            if(copying&&(delta||(drums?rowDelta:dp))){
                selected.clear();
                for(auto n:before)if(initialSelection.contains(n.id.value)){if(!copyIds.contains(n.id.value))copyIds[n.id.value]=mrs::new_id();n.id=copyIds[n.id.value];n.start+=delta;movePitch(n);selected.insert(n.id.value);notes.push_back(n);}
            }else{
                selected=initialSelection;
                if(!copying)for(auto& n:notes)if(initialSelection.contains(n.id.value)){if(gesture==2)n.length+=delta;else if(gesture==5){n.start+=delta;n.length-=delta;}else{n.start+=delta;movePitch(n);}}
            }
        }
        if(gesture==1){const auto soundingId=copyIds.contains(dragNote.value)?copyIds.at(dragNote.value):dragNote;for(const auto& n:notes)if(n.id==soundingId&&(n.pitch!=sounding||n.channel!=soundChannel)){releaseNote();audition(n.pitch,n.velocity,n.channel);break;}}
        repaint();
    }
    void mouseUp(const juce::MouseEvent& e) override{
        constexpr int normalReleaseMs=70,quickTapMs=150,quickPreviewMs=2000;
        // Extend only a short tap; held notes and edit drags keep their usual fade.
        const int heldMs=e.getLengthOfMousePress();
        const bool quickTap=heldMs<quickTapMs&&!(gesture&&dragged);
        releaseNote(gesture==3&&dragged?velocityPreviewTailMs:static_cast<unsigned>(quickTap?quickPreviewMs-heldMs:normalReleaseMs));
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
    bool keyStateChanged(bool) override{
        const bool down=juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey);
        if(!down){owner.spaceKey(false);return false;}
        if(juce::Component::getCurrentlyModalComponent()||juce::ModifierKeys::getCurrentModifiersRealtime().isAnyModifierKeyDown())return false;
        return keyPressed(juce::KeyPress(juce::KeyPress::spaceKey));
    }
    bool keyPressed(const juce::KeyPress& key) override{
        const int code=key.getKeyCode();const auto mods=key.getModifiers();
        if(code==juce::KeyPress::spaceKey&&!mods.isAnyModifierKeyDown()){
            if(juce::Component::getCurrentlyModalComponent())return false;
            owner.spaceKey(true,true,[this]{cancel();repaint();});return true;
        }
        if(code==juce::KeyPress::escapeKey){cancel();return true;}
        if(!mods.isAnyModifierKeyDown()&&(code=='1'||code=='3')){setSplitTool(code=='3');return true;}
        if(mods.isCtrlDown()&&!mods.isAltDown()){
            if(code=='A'){selectVisible();return true;}
            if(code=='C'){copy();return true;}if(code=='V'){paste();return true;}
            if(code=='Z'||code=='Y'){cancel();owner.run([&]{if(code=='Y'||mods.isShiftDown())owner.app.redo();else owner.app.undo();owner.refresh();});sync();if(changed)changed();return true;}
        }
        if((code==juce::KeyPress::upKey||code==juce::KeyPress::downKey)&&!mods.isAltDown()&&(!mods.isCtrlDown()||mods.isShiftDown())){
            const bool up=code==juce::KeyPress::upKey;
            if(drums&&!mods.isAnyModifierKeyDown())stepDrumRows(up?-1:1);
            else transpose((up?1:-1)*(mods.isShiftDown()?12:1),mods.isCtrlDown()&&mods.isShiftDown());
            return true;
        }
        if(!mods.isAnyModifierKeyDown()&&code=='D'){duplicate();return true;}
        if(!mods.isAnyModifierKeyDown()&&code=='S'){splitAt(cursor);return true;}
        if(code==juce::KeyPress::deleteKey||code==juce::KeyPress::backspaceKey){erase();return true;}return false;
    }
    void copy(){cancel();sync();owner.noteClipboard.clear();for(const auto& n:notes)if((!drums||visible(n))&&selected.contains(n.id.value))owner.noteClipboard.push_back(n);}
    void paste(){
        cancel();sync();if(!editable()||owner.noteClipboard.empty())return;
        const auto& source=owner.noteClipboard;
        if(drums&&std::any_of(source.begin(),source.end(),[&](const auto& n){return !drumAddressVisible(n);})){report("Show the destination rows before pasting notes");return;}
        const auto begin=std::min_element(source.begin(),source.end(),[](const auto& a,const auto& b){return a.start<b.start;})->start;
        for(const auto& n:source)if(n.start-begin>mrs::max_tick-cursor||n.length>clip.source_offset+clip.length-(cursor+n.start-begin)){report("Pasted group must fit inside the MIDI clip");return;}
        auto next=notes;selected.clear();for(auto n:source){n.id=mrs::new_id();n.start=cursor+n.start-begin;selected.insert(n.id.value);next.push_back(n);}submit(std::move(next));
    }
    void duplicate(){
        cancel();sync();if(!editable()||selected.empty())return;
        mrs::Tick begin=mrs::max_tick,end{};for(const auto& n:notes)if((!drums||visible(n))&&selected.contains(n.id.value)){begin=std::min(begin,n.start);end=std::max(end,n.start+n.length);}
        const auto delta=end-begin;if(delta<1)return;
        for(const auto& n:notes)if((!drums||visible(n))&&selected.contains(n.id.value)&&delta>clip.source_offset+clip.length-n.start-n.length){report("Duplicated group must fit inside the MIDI clip");return;}
        auto next=notes;std::set<std::string> result;for(auto n:notes)if((!drums||visible(n))&&selected.contains(n.id.value)){n.id=mrs::new_id();n.start+=delta;result.insert(n.id.value);next.push_back(n);}selected=std::move(result);submit(std::move(next));
    }
    void stepDrumRows(int direction){
        cancel();sync();if(!editable()||selected.empty())return;
        auto next=notes;bool changedNotes=false;
        for(auto& n:next)if(selected.contains(n.id.value)){
            const int originRow=rowOf(n);
            if(originRow<0){report("Expand the selected drum rows before moving notes");return;}
            std::optional<DrumGridRows::Address> address;
            for(int row=originRow+direction;row>=0&&row<rowCount();row+=direction){
                const auto& target=drumRows.rows[static_cast<std::size_t>(row)];
                if(target.header && target.group.value.empty())break; // keep Unmapped separate
                if((address=drumTarget(n,row-originRow)))break;
            }
            if(!address)return;
            n.pitch=address->first;n.channel=address->second;changedNotes=true;
        }
        if(changedNotes)submit(std::move(next));
    }
    void transpose(int delta,bool copyNotes){
        cancel();sync();if(!editable()||selected.empty())return;
        for(const auto& n:notes)if((!drums||visible(n))&&selected.contains(n.id.value)&&(n.pitch+delta<0||n.pitch+delta>127)){report("The whole note group must stay within MIDI pitches 0..127");return;}
        if(drums)for(auto n:notes)if((!drums||visible(n))&&selected.contains(n.id.value)){n.pitch+=delta;if(!drumAddressVisible(n)){report("Target pitch is outside the visible drum rows");return;}}
        auto next=notes;if(copyNotes){std::set<std::string> result;for(auto n:notes)if((!drums||visible(n))&&selected.contains(n.id.value)){n.id=mrs::new_id();n.pitch+=delta;result.insert(n.id.value);next.push_back(n);}selected=std::move(result);}
        else for(auto& n:next)if((!drums||visible(n))&&selected.contains(n.id.value))n.pitch+=delta;
        submit(std::move(next));
    }
    void splitAt(mrs::Tick cut){
        cancel();sync();if(!editable()||selected.empty())return;
        auto next=notes;std::set<std::string> result=selected;
        for(std::size_t i=0;i<notes.size();++i){const auto& n=notes[i];if((!drums||visible(n))&&selected.contains(n.id.value)&&cut>n.start&&cut<n.start+n.length){auto right=n;right.id=mrs::new_id();right.start=cut;right.length=n.start+n.length-cut;next[i].length=cut-n.start;next.push_back(right);result.insert(right.id.value);}}
        selected=std::move(result);submit(std::move(next));
    }
    void erase(){cancel();sync();if(!editable())return;auto next=notes;std::erase_if(next,[&](const auto& n){return (!drums||visible(n))&&selected.contains(n.id.value);});submit(std::move(next));selected.clear();}
    void cancel(){releaseNote();if(gesture){if(gesture!=4)notes=before;selected=selectionBefore;}gesture=0;gestureStore.reset();copying=false;shiftRemove=false;copyIds.clear();repaint();}
private:
    friend void stateBusySmoke();
    friend void pianoRoll214Smoke();
    Desktop& owner;mrs::Id id,track,dragNote;int gesture{};juce::Point<float> origin;juce::Rectangle<float> box;std::set<std::string> initialSelection;std::vector<mrs::MidiNote> before;mrs::MidiClip beforeClip;
    bool copying{},dragged{},shiftRemove{};
    std::set<std::string> selectionBefore;
    std::map<std::string,mrs::Id> copyIds;
    std::shared_ptr<mrs::IProjectStore> gestureStore;
    std::shared_ptr<const mrs::Project> rowProject;
    mrs::Id rowTrack;
    DrumGridRows drumRows;
    int rowCount() const{return drums?static_cast<int>(drumRows.rows.size()):128;}
    int rowOf(const mrs::MidiNote& n) const{return drums?drumRows.index(n.pitch,n.channel):127-n.pitch;}
    int rowAt(float y) const{return static_cast<int>(std::floor((y-ruler)/rowHeight));}
    const DrumGridRows::Row* drumRowAt(float y) const{
        const int row=rowAt(y);return row>=0&&row<rowCount()?&drumRows.rows[static_cast<std::size_t>(row)]:nullptr;
    }
    std::optional<DrumGridRows::Address> drumTarget(const mrs::MidiNote& note,int delta) const{
        const int row=rowOf(note)+delta;if(row<0||row>=rowCount())return {};
        if (rowOf(note)==row) return DrumGridRows::Address{note.pitch,note.channel};
        const auto& r=drumRows.rows[static_cast<std::size_t>(row)];if(r.pitch<0)return {};
        return DrumGridRows::Address{r.pitch,drumRows.index(r.pitch,note.channel)==row?note.channel:r.channel};
    }
    std::uint64_t liveActivity{};
    int liveKey{-1};
    double liveUntil{};
    void paintDrumRail(juce::Graphics& g,int x,int top) const{
        const juce::Graphics::ScopedSaveState scoped(g);
        if(!g.reduceClipRegion(juce::Rectangle<int>{x,top,railWidth(),getHeight()-top}))return;
        g.setColour(juce::Colour(0xff202832));g.fillRect(x,top,railWidth(),getHeight()-top);
        const int liveRow=liveKey<0?-1:drumRows.index(liveKey%128,liveKey/128);
        for(int i=0;i<rowCount();++i){const auto& row=drumRows.rows[static_cast<std::size_t>(i)];
            juce::Rectangle<int> bounds{x,ruler+i*static_cast<int>(rowHeight),railWidth(),static_cast<int>(rowHeight)};
            if(!g.clipRegionIntersects(bounds))continue;
            const auto fill=row.header?(row.depth==0?0xff344253:0xff2c3948):(i%2==0?0xff252f3a:0xff202932);
            g.setColour(juce::Colour(i==liveRow?0xff295a62:fill));g.fillRect(bounds);g.setColour(juce::Colour(outline));g.fillRect(bounds.withHeight(1));
            if(row.color&&row.header){g.setColour(juce::Colour(0xff000000u|*row.color));g.fillRect(bounds.withWidth(3));}
            if(row.header&&!row.group.value.empty()){
                const float cx=static_cast<float>(bounds.getX()+12+row.depth*13),cy=static_cast<float>(bounds.getCentreY());
                juce::Path arrow;if(row.closed)arrow.addTriangle(cx-2,cy-4,cx+3,cy,cx-2,cy+4);else arrow.addTriangle(cx-4,cy-2,cx+4,cy-2,cx,cy+3);
                g.setColour(juce::Colour(accent));g.fillPath(arrow);
            }
            if(row.pitch>=0){g.setColour(juce::Colours::lightgrey.withAlpha(.7f));g.setFont(owner.theme.font(10));g.drawText(juce::String(row.pitch),bounds.removeFromRight(29),juce::Justification::centred);}
            g.setColour(juce::Colours::whitesmoke.withAlpha(!row.header&&row.pitch<0?.45f:1.f));
            g.setFont(owner.theme.font(std::clamp(rowHeight-3.f,8.f,13.f)));
            g.drawFittedText(juce::String::fromUTF8(row.name.c_str()),bounds.withTrimmedLeft(23+row.depth*13).reduced(2,0),juce::Justification::centredLeft,1);
        }
    }
    static bool isBlackKey(int pitch){const int pc=pitch%12;return pc==1||pc==3||pc==6||pc==8||pc==10;}
    int keyCentre(int pitch) const{const int h=static_cast<int>(rowHeight);return ruler+(127-pitch)*h+h/2;}
    juce::Rectangle<int> blackKeyBounds(int pitch,int x) const{
        const int h=static_cast<int>(rowHeight);
        return {x,ruler+(127-pitch)*h+1,keys*3/5,h-2};
    }
    juce::Rectangle<int> whiteKeyBounds(int pitch,int x) const{
        const int centre=keyCentre(pitch);
        // White keys meet midway between their pitch centres. Black keys straddle those joins.
        const int top=pitch==127?ruler:(centre+keyCentre(pitch+(isBlackKey(pitch+1)?2:1)))/2;
        const int bottom=pitch==0?getHeight():(centre+keyCentre(pitch-(isBlackKey(pitch-1)?2:1)))/2;
        return {x,top,keys,bottom-top};
    }
    int keyboardPitchAt(juce::Point<int> point) const{
        const int x=scrollX();
        for(int pitch=0;pitch<128;++pitch)if(isBlackKey(pitch)&&blackKeyBounds(pitch,x).contains(point))return pitch;
        for(int pitch=0;pitch<128;++pitch)if(!isBlackKey(pitch)&&whiteKeyBounds(pitch,x).contains(point))return pitch;
        return -1;
    }
    void paintKeyboard(juce::Graphics& g,int x,int top) const{
        const juce::Graphics::ScopedSaveState state(g);
        const juce::Rectangle<int> keyboard{x,top,keys,getHeight()-top};
        if(keyboard.isEmpty()||!g.reduceClipRegion(keyboard))return;
        const auto pale=juce::Colour(0xffedf0f1);
        g.setGradientFill({juce::Colour(0xff949a9f),static_cast<float>(x),0.f,pale,static_cast<float>(x+14),0.f,false});
        g.fillRect(keyboard);
        for(int pitch=0;pitch<128;++pitch){
            if(isBlackKey(pitch))continue;
            const auto key=whiteKeyBounds(pitch,x);
            if(!g.clipRegionIntersects(key))continue;
            if((pitch==sounding||(liveKey>=0&&pitch==liveKey%128))){g.setColour(pale.interpolatedWith(juce::Colour(accent),.45f));g.fillRect(key);}
            g.setColour(juce::Colour(0xffa4aab0));g.fillRect(key.withHeight(1));
            if(pitch%12==0){
                g.setFont(owner.theme.font(10));g.setColour(juce::Colour(0xff192026));
                g.drawText(noteName(pitch),key.reduced(4,0),juce::Justification::centredRight);
            }
        }
        for(int pitch=0;pitch<128;++pitch){
            if(!isBlackKey(pitch))continue;
            auto key=blackKeyBounds(pitch,x);
            if(!g.clipRegionIntersects(key))continue;
            const auto dark=juce::Colour((pitch==sounding||(liveKey>=0&&pitch==liveKey%128))?accent:0xff333a40);
            g.setColour(juce::Colour(0xff13181c));g.fillRect(key);
            key=key.reduced(1);
            const auto tip=key.removeFromRight(3);
            g.setGradientFill({dark.darker(.26f),static_cast<float>(key.getX()),0.f,dark,static_cast<float>(key.getRight()),0.f,false});
            g.fillRect(key);
            g.setColour(dark.darker(.55f));g.fillRect(tip);
        }
        g.setColour(juce::Colour(0xff181d21));g.fillRect(keyboard.withWidth(1));g.fillRect(keyboard.withLeft(keyboard.getRight()-1));
    }
    juce::Rectangle<float> displayedRect(const mrs::MidiNote& note,const mrs::MidiClip& midi)const{
        const auto normal=eventRect(note,midi);
        if(!normal.isEmpty()||!drums||note.start<midi.source_offset||note.start>=midi.source_offset+midi.length)return normal;
        const int row=drumRows.summaryIndex(note.pitch,note.channel);if(row<0)return {};
        const float size=std::clamp(rowHeight-4.f,5.f,14.f);
        const float x=railWidth()+static_cast<float>(static_cast<double>(midi.start+note.start-midi.source_offset)/mrs::ppq)*beatWidth;
        return {x-size*.5f,ruler+row*rowHeight+(rowHeight-size)*.5f,size,size};
    }
    bool visible(const mrs::MidiNote& n)const{return drumAddressVisible(n)&&n.start<clip.source_offset+clip.length&&(drums?n.start>=clip.source_offset:n.start+n.length>clip.source_offset);}
    juce::Rectangle<float> visibleRect(const mrs::MidiNote& n)const{const auto left=std::max(static_cast<float>(scrollX()+railWidth()),railWidth()+static_cast<float>(clip.start)/mrs::ppq*beatWidth),right=railWidth()+static_cast<float>(clip.start+clip.length)/mrs::ppq*beatWidth;return displayedNoteRect(n).getIntersection({left,static_cast<float>(scrollY()+ruler),std::max(0.f,right-left),static_cast<float>(getHeight()-scrollY()-ruler)});}
    static juce::Colour shadeNote(juce::Colour base,int velocity,bool active){
        const float unit=velocity/127.f;
        return base.withMultipliedBrightness(.76f+.24f*unit).withAlpha((active?1.f:.55f)*(1.f-.286f*(1.f-unit)));
    }
    void drawNote(juce::Graphics& g,const mrs::MidiNote& n,const mrs::Clip& c,juce::Rectangle<float> rect,bool active){
        juce::Graphics::ScopedSaveState noteState(g);
        const auto color=noteColour(n,c);
        const auto shaded=displayedNoteColour(n,c,active);
        if(drums){
            const auto center=rect.getCentre();juce::Path diamond;diamond.startNewSubPath(center.x,rect.getY());diamond.lineTo(rect.getRight(),center.y);diamond.lineTo(center.x,rect.getBottom());diamond.lineTo(rect.getX(),center.y);diamond.closeSubPath();
            const bool chosen=active&&selected.contains(n.id.value);
            g.setColour(shaded);g.fillPath(diamond);
            g.setColour(chosen?juce::Colour(accent):juce::Colour(surface));g.strokePath(diamond,juce::PathStrokeType(chosen?1.6f:1.f));return;
        }
        juce::Path shape;shape.addRoundedRectangle(rect,2.f);g.saveState();g.reduceClipRegion(shape);
        g.setColour(shaded);g.fillRect(rect);g.restoreState();
        const bool chosen=active&&selected.contains(n.id.value);
        g.setColour(chosen?juce::Colour(accent):shaded);g.drawRoundedRectangle(rect.reduced(.5f),2.f,chosen?1.5f:1.f);
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
    void timerCallback() override{
        const auto current = owner.app.live_note_activity(track);
        const auto now = juce::Time::getMillisecondCounterHiRes();
        if (current != liveActivity) { liveActivity = current; liveKey = current && isShowing() ? static_cast<int>(current & 2047) : -1; liveUntil = now + 600; }
        if (now >= liveUntil || !isShowing()) liveKey = -1;
        mrs::audio::RealtimeState state;if(gesture&&(owner.app.recording()||!owner.app.engine()->try_state(state)||state.playback==mrs::PlaybackState::playing))cancel();repaint();if(!gesture){const auto previous=notes;const auto previousRows=rowProject;sync();if(isEnabled()&&(notes!=previous||(drums&&rowProject!=previousRows))&&changed)changed();}}
    void report(const juce::String& text){if(error)error(text);}
    bool editable(){mrs::audio::RealtimeState state;if(!owner.app.engine()->try_state(state)){report("Audio state updating; try again");return false;}if(owner.app.recording()||state.playback==mrs::PlaybackState::playing){report("Pause or stop before editing / auditioning notes");return false;}return isEnabled();}
    mrs::Tick position(float x) const{auto tick=static_cast<mrs::Tick>(std::llround((x-railWidth())/beatWidth*mrs::ppq));if(snapGrid)tick=gridSpec.snap(tick);return clip.source_offset+std::clamp(tick-clip.start,mrs::Tick{0},std::max(mrs::Tick{0},clip.length-1));}
    int scrollX() const{if(auto* viewport=findParentComponentOfClass<juce::Viewport>())return viewport->getViewPositionX();return 0;}
    int scrollY() const{if(auto* viewport=findParentComponentOfClass<juce::Viewport>())return viewport->getViewPositionY();return 0;}
    bool insideActive(float x) const{return x>=railWidth()+static_cast<double>(clip.start)/mrs::ppq*beatWidth&&x<railWidth()+static_cast<double>(clip.start+clip.length)/mrs::ppq*beatWidth;}
    int hitNote(juce::Point<float> p,bool includeFolded=false) const{if(!insideActive(p.x))return -1;if(p.x<scrollX()+railWidth()||p.y<scrollY()+ruler)return -1;for(std::size_t i=notes.size();i>0;--i)if((includeFolded?displayedNoteRect(notes[i-1]):noteRect(notes[i-1])).contains(p))return static_cast<int>(i-1);return -1;}
    void submit(std::vector<mrs::MidiNote> next){try{releaseNote();if(next==notes)return;mrs::diagnostics::event("midi_note_commit_begin clip="+id.value+" count="+std::to_string(next.size()));owner.app.set_midi_notes(id,std::move(next));owner.refresh();sync();if(changed)changed();mrs::diagnostics::event("midi_note_commit_end");}catch(const std::exception& e){mrs::diagnostics::event(std::string("midi_note_error ")+e.what());report(juce::String::fromUTF8(e.what()));sync();}}
    void audition(int pitch,int velocity,int channel){if(owner.app.audition_note(track,pitch,velocity,channel)){sounding=pitch;soundChannel=channel;}}
    void releaseNote(unsigned releaseMs=0){if(sounding>=0){owner.app.audition_note(track,sounding,100,soundChannel,false,releaseMs);sounding=-1;}}
};
}
