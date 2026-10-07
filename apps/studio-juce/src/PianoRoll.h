#pragma once
#include "Desktop.h"
#include <set>
namespace ui {
// All gestures are local previews. Only release submits one shared SetMidiNotes command.
class PianoRoll final : public juce::Component, private juce::Timer {
public:
    PianoRoll(Desktop& d,mrs::Id clip):owner(d),id(std::move(clip)){setName("Piano roll");setComponentID("piano-roll");setWantsKeyboardFocus(true);sync();startTimer(120);}
    ~PianoRoll() override{releaseNote();}
    std::function<void()> changed,viewChanged;
    std::function<void(float,juce::ModifierKeys,float,float)> wheel;
    void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& w) override{if(wheel)wheel(w.deltaY,e.mods,e.position.x,e.position.y);}
    std::function<void(juce::String)> error;
    std::set<std::string> selected;
    mrs::Tick grid{mrs::ppq/4},cursor{};
    float beatWidth{96.f},rowHeight{18.f};
    static constexpr int keys=64,ruler=24;
    std::vector<mrs::MidiNote> notes;std::vector<mrs::Clip> contextClips;mrs::Tick timelineEnd{};
    int auditionPitch() const{return sounding;}
    mrs::Tick playheadTick() const{const auto p=owner.project();return mrs::Timeline(p->time,p->sample_rate).to_ticks(owner.app.engine()->state().sample);}
    void transport(int command){cancel();owner.action(command);repaint();}
    void activate(mrs::Id next){cancel();id=std::move(next);selected.clear();cursor=0;setEnabled(true);sync();}
    mrs::MidiClip clip;
    void sync(){if(gesture)return;const auto p=owner.project();const auto at=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==id;});if(at==p->clips.end()||!at->midi){setEnabled(false);releaseNote();return;}track=at->track;clip=*at->midi;notes=clip.notes;contextClips.clear();timelineEnd=clip.start+clip.length;for(const auto& c:p->clips)if(c.midi&&c.track==track){contextClips.push_back(c);timelineEnd=std::max(timelineEnd,c.midi->start+c.midi->length);}timelineEnd=std::min(mrs::max_tick,timelineEnd+4*mrs::ppq);if(cursor<clip.source_offset)cursor=clip.source_offset;std::erase_if(selected,[&](const auto& value){return std::none_of(notes.begin(),notes.end(),[&](const auto& n){return n.id.value==value;});});setSize(keys+static_cast<int>(std::min(1000000.,std::ceil(static_cast<double>(timelineEnd)/mrs::ppq*beatWidth)))+2,ruler+128*static_cast<int>(rowHeight));repaint();if(viewChanged)viewChanged();}
    juce::Rectangle<float> noteRect(const mrs::MidiNote& n) const{return {keys+static_cast<float>(static_cast<double>(clip.start+n.start-clip.source_offset)/mrs::ppq)*beatWidth,ruler+(127-n.pitch)*rowHeight,static_cast<float>(static_cast<double>(n.length)/mrs::ppq)*beatWidth,rowHeight-1};}
    void paint(juce::Graphics& g) override{
        g.fillAll(juce::Colour(surface));const auto bounds=g.getClipBounds();const int keyboardX=scrollX(),headerY=scrollY();g.setFont(owner.theme.font(10));
        const auto xAt=[&](mrs::Tick t){return keys+static_cast<float>(static_cast<double>(t)/mrs::ppq)*beatWidth;};
        for(int pitch=0;pitch<128;++pitch){const int y=ruler+(127-pitch)*static_cast<int>(rowHeight);if(y+rowHeight<bounds.getY()||y>bounds.getBottom())continue;const int pc=pitch%12;const bool black=pc==1||pc==3||pc==6||pc==8||pc==10;
            for(const auto& c:contextClips){const bool active=c.id==id;const float x=xAt(c.midi->start),right=xAt(c.midi->start+c.midi->length);g.setColour(juce::Colour(active?(black?0xff25292e:0xff30363c):(black?0xff202429:0xff292e33)));g.fillRect(x,static_cast<float>(y),right-x,rowHeight);g.setColour(juce::Colour(active?0xff41474d:0xff343a40));g.drawHorizontalLine(y,x,right);}
            g.setColour(juce::Colour(black?0xff161b20:0xffbec9d2));g.fillRect(keyboardX,y,keys-1,static_cast<int>(rowHeight)-1);if(pc==0){g.setColour(black?juce::Colours::white:juce::Colours::black);g.drawText("C"+juce::String(pitch/12-1),keyboardX+3,y,keys-6,static_cast<int>(rowHeight),juce::Justification::centredLeft);}}
        const auto step=grid>0?grid:mrs::ppq/4;const double px=static_cast<double>(step)/mrs::ppq*beatWidth;const int first=std::max(0,static_cast<int>((bounds.getX()-keys)/px)),last=static_cast<int>((bounds.getRight()-keys)/px)+1;
        for(int i=first;i<=last;++i){const auto tick=static_cast<mrs::Tick>(i)*step;if(tick>timelineEnd)break;const auto x=xAt(tick);g.setColour(juce::Colour(tick%mrs::ppq==0?0xff4a525a:0xff30363c));g.drawVerticalLine(static_cast<int>(x),ruler,static_cast<float>(getHeight()));}
        g.saveState();g.reduceClipRegion(juce::Rectangle<int>(keyboardX+keys,headerY+ruler,getWidth()-keyboardX-keys,getHeight()-headerY-ruler));
        for(const auto& c:contextClips){const bool active=c.id==id;const auto& midi=*c.midi;const auto& visibleNotes=active?notes:midi.notes;g.saveState();g.reduceClipRegion(juce::Rectangle<float>{xAt(midi.start),static_cast<float>(ruler),xAt(midi.start+midi.length)-xAt(midi.start),static_cast<float>(getHeight()-ruler)}.toNearestInt());for(const auto& n:visibleNotes){auto rect=juce::Rectangle<float>{xAt(midi.start+n.start-midi.source_offset),ruler+(127-n.pitch)*rowHeight,static_cast<float>(static_cast<double>(n.length)/mrs::ppq)*beatWidth,rowHeight-1};if(!rect.intersects(bounds.toFloat()))continue;const bool chosen=active&&selected.contains(n.id.value);g.setColour(juce::Colour(active?(chosen?0xff43bde0:0xff5786b9):0xff3d5267));g.fillRoundedRectangle(rect,2);g.setColour(chosen?juce::Colours::white:juce::Colour(active?0xff8bafcc:0xff607080));g.drawRoundedRectangle(rect,2,1);g.setColour(juce::Colour(0xff203d57));g.fillRect(rect.withHeight(3).withY(rect.getBottom()-3).withWidth(rect.getWidth()*n.velocity/127.f));}g.restoreState();}
        if(gesture==4){g.setColour(juce::Colours::skyblue.withAlpha(.15f));g.fillRect(box);g.setColour(juce::Colours::skyblue);g.drawRect(box,1);}
        g.setColour(juce::Colours::white);g.drawVerticalLine(static_cast<int>(xAt(playheadTick())),ruler,static_cast<float>(getHeight()));g.restoreState();
        g.setColour(juce::Colour(surface));g.fillRect(keyboardX,headerY,getWidth(),ruler);const auto p=owner.project();const mrs::Timeline time(p->time,p->sample_rate);for(int i=first;i<=last;++i){const auto tick=static_cast<mrs::Tick>(i)*step;if(tick>timelineEnd)break;if(tick%mrs::ppq==0){const auto pos=time.musical_position(tick);g.setColour(juce::Colours::lightgrey);g.drawText(juce::String(pos.bar)+":"+juce::String(pos.beat),static_cast<int>(xAt(tick))+3,headerY,60,ruler,juce::Justification::centredLeft);}}
        if(hasKeyboardFocus(true)){g.setColour(juce::Colours::skyblue);g.drawRect(getLocalBounds(),1);}
    }
    void mouseDown(const juce::MouseEvent& e) override{
        grabKeyboardFocus();sync();releaseNote();if(!e.mods.isLeftButtonDown())return;if(e.y<scrollY()+ruler&&e.x>=scrollX()+keys){const auto p=owner.project();const auto tick=std::clamp(static_cast<mrs::Tick>(std::llround((e.x-keys)/beatWidth*mrs::ppq)),mrs::Tick{0},mrs::max_tick);owner.run([&]{owner.app.seek(mrs::Timeline(p->time,p->sample_rate).to_samples(tick));});repaint();return;}if(e.x>=scrollX()+keys&&e.y>=scrollY()+ruler){for(const auto& c:contextClips)if(c.id!=id&&e.x>=keys+static_cast<double>(c.midi->start)/mrs::ppq*beatWidth&&e.x<keys+static_cast<double>(c.midi->start+c.midi->length)/mrs::ppq*beatWidth){cancel();owner.activateClip(c.id);return;}}if(!editable())return;
        if(e.x<scrollX()+keys){if(e.y>=scrollY()+ruler)audition(juce::jlimit(0,127,127-(e.y-ruler)/static_cast<int>(rowHeight)),100,0);return;}
        cursor=position(static_cast<float>(e.x));const auto hit=hitNote(e.position);
        if(!insideActive(e.position.x))return;
        if(!e.mods.isLeftButtonDown())return;
        if(hit>=0){const auto& n=notes[static_cast<std::size_t>(hit)];if(e.mods.isCtrlDown()||e.mods.isShiftDown()){if(selected.contains(n.id.value)){selected.erase(n.id.value);repaint();return;}selected.insert(n.id.value);}else if(!selected.contains(n.id.value))selected={n.id.value};
            dragNote=n.id;origin=e.position;before=notes;beforeClip=clip;gesture=e.mods.isAltDown()?3:e.x>=noteRect(n).getRight()-6?2:1;audition(n.pitch,n.velocity,n.channel);}
        else {if(!e.mods.isCtrlDown()&&!e.mods.isShiftDown())selected.clear();initialSelection=selected;origin=e.position;gesture=4;box={e.position,e.position};}repaint();
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override{
        cancel();if(!editable()||e.x<scrollX()+keys||e.y<scrollY()+ruler)return;if(!e.mods.isLeftButtonDown()||!insideActive(e.position.x))return;const auto hit=hitNote(e.position);if(hit>=0){selected={notes[static_cast<std::size_t>(hit)].id.value};erase();return;}
        auto next=notes;mrs::MidiNote n;n.id=mrs::new_id();n.start=position(static_cast<float>(e.x));n.length=std::min(grid>0?grid:mrs::ppq/4,clip.source_offset+clip.length-n.start);n.pitch=juce::jlimit(0,127,127-(e.y-ruler)/static_cast<int>(rowHeight));n.velocity=100;n.channel=0;if(n.length<1)return;next.push_back(n);selected={n.id.value};submit(std::move(next));
    }
    void mouseDrag(const juce::MouseEvent& e) override{
        if(!gesture)return;if(gesture==4){box=juce::Rectangle<float>(origin,e.position);selected=initialSelection;for(const auto& n:notes)if(noteRect(n).intersects(box))selected.insert(n.id.value);repaint();return;}
        notes=before;const auto dt=static_cast<mrs::Tick>(std::llround((e.position.x-origin.x)/beatWidth*mrs::ppq));const auto snapped=grid>0?static_cast<mrs::Tick>(std::llround(static_cast<double>(dt)/grid))*grid:dt;
        if(gesture==3){const int dv=static_cast<int>(std::lround((origin.y-e.position.y)*.7f));for(auto& n:notes)if(selected.contains(n.id.value))n.velocity=juce::jlimit(1,127,n.velocity+dv);}
        else {mrs::Tick low=-mrs::max_tick,high=mrs::max_tick;int pitchLow=-127,pitchHigh=127;for(const auto& n:before)if(selected.contains(n.id.value)){low=std::max(low,gesture==2?1-n.length:std::min(clip.source_offset,n.start)-n.start);high=std::min(high,std::max(clip.source_offset+clip.length,n.start+n.length)-n.start-n.length);pitchLow=std::max(pitchLow,-n.pitch);pitchHigh=std::min(pitchHigh,127-n.pitch);}
            const auto delta=std::clamp(snapped,low,std::max(low,high));const int dp=juce::jlimit(pitchLow,pitchHigh,static_cast<int>(std::lround((origin.y-e.position.y)/rowHeight)));for(auto& n:notes)if(selected.contains(n.id.value)){if(gesture==2)n.length+=delta;else{n.start+=delta;n.pitch+=dp;}}}if(gesture==1)for(const auto& n:notes)if(n.id==dragNote&&n.pitch!=sounding){releaseNote();audition(n.pitch,n.velocity,n.channel);break;}repaint();
    }
    void mouseUp(const juce::MouseEvent&) override{releaseNote();if(gesture&&gesture!=4){auto next=notes;notes=before;gesture=0;const auto p=owner.project();const auto at=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==id;});if(at!=p->clips.end()&&at->midi&&*at->midi==beforeClip)submit(std::move(next));else{report("Notes changed during gesture; preview cancelled");sync();}}gesture=0;repaint();}
    void visibilityChanged() override{if(!isShowing())cancel();}
    void focusLost(FocusChangeType) override{cancel();}
    bool keyPressed(const juce::KeyPress& key) override{
        const int code=key.getKeyCode();if(code==juce::KeyPress::spaceKey&&!key.getModifiers().isAnyModifierKeyDown()){transport(owner.app.engine()->state().playback==mrs::PlaybackState::playing?31:30);return true;}if(code==juce::KeyPress::escapeKey){cancel();return true;}
        if(key.getModifiers().isCtrlDown()){if(code=='A'){for(const auto& n:notes)if(noteRect(n).getRight()>keys&&noteRect(n).getX()<getWidth())selected.insert(n.id.value);repaint();return true;}if(code=='C'){copy();return true;}if(code=='V'){paste();return true;}if(code=='Z'||code=='Y'){cancel();owner.run([&]{if(code=='Y'||key.getModifiers().isShiftDown())owner.app.redo();else owner.app.undo();owner.refresh();});sync();if(changed)changed();return true;}}
        if(code==juce::KeyPress::deleteKey||code==juce::KeyPress::backspaceKey){erase();return true;}return false;
    }
    void copy(){owner.noteClipboard.clear();for(const auto& n:notes)if(selected.contains(n.id.value))owner.noteClipboard.push_back(n);}
    void paste(){cancel();sync();if(!editable()||owner.noteClipboard.empty())return;auto next=notes;const auto& source=owner.noteClipboard;const auto begin=std::min_element(source.begin(),source.end(),[](const auto& a,const auto& b){return a.start<b.start;})->start;selected.clear();for(auto n:source){n.id=mrs::new_id();n.start=cursor+n.start-begin;if(n.start>=clip.source_offset+clip.length)continue;n.length=std::min(n.length,clip.source_offset+clip.length-n.start);selected.insert(n.id.value);next.push_back(n);}submit(std::move(next));}
    void erase(){cancel();sync();if(!editable())return;auto next=notes;std::erase_if(next,[&](const auto& n){return selected.contains(n.id.value);});submit(std::move(next));selected.clear();}
    void cancel(){releaseNote();if(gesture&&gesture!=4)notes=before;gesture=0;repaint();}
private:
    Desktop& owner;mrs::Id id,track,dragNote;int gesture{};juce::Point<float> origin;juce::Rectangle<float> box;std::set<std::string> initialSelection;std::vector<mrs::MidiNote> before;mrs::MidiClip beforeClip;
    int sounding{-1},soundChannel{};
    void timerCallback() override{if(gesture&&(owner.app.recording()||owner.app.engine()->state().playback==mrs::PlaybackState::playing))cancel();repaint();if(!gesture){const auto previous=notes;sync();if(notes!=previous&&changed)changed();}}
    void report(const juce::String& text){if(error)error(text);}
    bool editable(){if(owner.app.recording()||owner.app.engine()->state().playback==mrs::PlaybackState::playing){report("Pause or stop before editing / auditioning notes");return false;}return isEnabled();}
    mrs::Tick position(float x) const{auto tick=static_cast<mrs::Tick>(std::llround((x-keys)/beatWidth*mrs::ppq));if(grid>0)tick=static_cast<mrs::Tick>(std::llround(static_cast<double>(tick)/grid))*grid;return clip.source_offset+std::clamp(tick-clip.start,mrs::Tick{0},std::max(mrs::Tick{0},clip.length-1));}
    int scrollX() const{if(auto* viewport=findParentComponentOfClass<juce::Viewport>())return viewport->getViewPositionX();return 0;}
    int scrollY() const{if(auto* viewport=findParentComponentOfClass<juce::Viewport>())return viewport->getViewPositionY();return 0;}
    bool insideActive(float x) const{return x>=keys+static_cast<double>(clip.start)/mrs::ppq*beatWidth&&x<keys+static_cast<double>(clip.start+clip.length)/mrs::ppq*beatWidth;}
    int hitNote(juce::Point<float> p) const{if(!insideActive(p.x))return -1;if(p.x<scrollX()+keys||p.y<scrollY()+ruler)return -1;for(std::size_t i=notes.size();i>0;--i)if(noteRect(notes[i-1]).contains(p))return static_cast<int>(i-1);return -1;}
    void submit(std::vector<mrs::MidiNote> next){try{releaseNote();if(next==notes)return;owner.app.set_midi_notes(id,std::move(next));owner.refresh();sync();if(changed)changed();}catch(const std::exception& e){report(juce::String::fromUTF8(e.what()));sync();}}
    void audition(int pitch,int velocity,int channel){if(owner.app.audition_note(track,pitch,velocity,channel)){sounding=pitch;soundChannel=channel;}}
    void releaseNote(){if(sounding>=0){owner.app.audition_note(track,sounding,100,soundChannel,false);sounding=-1;}}
};
}
