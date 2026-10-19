#include "MeterTrack.h"
#include "SectionTrack.h"
#include "Desktop.h"
#include "TempoTrack.h"
#include "ChordTrack.h"
#include "MarkerTrack.h"
#include <algorithm>
namespace ui {
namespace {
juce::String label(const std::string& s){return juce::String::fromUTF8(s.c_str());}
juce::String position(const mrs::Timeline& time,mrs::Tick tick){const auto p=time.musical_position(tick);return juce::String(p.bar)+":"+juce::String(p.beat)+":"+juce::String(p.tick);}
mrs::Tick parsePosition(const mrs::Timeline& time,juce::String text){const auto parts=juce::StringArray::fromTokens(text.trim(),":","");if(parts.size()!=3)throw std::invalid_argument("Use bar:beat:tick for section bounds");for(const auto& part:parts)if(part.isEmpty()||part.length()>12||!part.containsOnly("0123456789"))throw std::invalid_argument("Use positive bar/beat and nonnegative tick");const auto beat=parts[1].getLargeIntValue();if(beat<1||beat>64)throw std::invalid_argument("Beat outside meter range");return time.to_ticks({parts[0].getLargeIntValue(),static_cast<int>(beat),parts[2].getLargeIntValue()});}
struct SectionFields {
    juce::String name,start,end,color;
};
// A menu owns this component; the callback owns only the copied field state.
// Closing the popup without Apply (including Escape/outside click) never edits.
class SectionEditor final:public juce::PopupMenu::CustomComponent {
public:
    SectionEditor(std::shared_ptr<SectionFields> fields,Theme& theme,bool modern,std::vector<ViewSettings::SectionTemplate> entries,std::function<void(ViewSettings::SectionTemplate)> save):CustomComponent(false),templates(std::move(entries)),value(std::move(fields)){
        setName("Section editor");setLookAndFeel(&theme);paletteCount=modern?8:7;
        const std::array<juce::String,4> names{"Name","Start","End","RGB"};
        const std::array<juce::String,4> texts{value->name,value->start,value->end,value->color};
        for(std::size_t i=0;i<input.size();++i){
            addAndMakeVisible(input[i]);input[i].setName(names[i]);input[i].setFont(Theme::browserFont());
            input[i].setText(texts[i],false);input[i].setSelectAllWhenFocused(true);
            input[i].onReturnKey=[this]{commit();};input[i].onEscapeKey=[]{juce::PopupMenu::dismissAllActiveMenus();};
        }
        input[1].setTooltip("bar:beat:tick");input[2].setTooltip("bar:beat:tick; end is exclusive");
        input[3].setInputRestrictions(6,"0123456789abcdefABCDEF");
        addAndMakeVisible(apply);apply.onClick=[this]{commit();};
        addAndMakeVisible(templateChoice);templateChoice.setName("Section templates");templateChoice.setTextWhenNothingSelected("Choose section template...");refreshTemplates();templateChoice.onChange=[this]{const auto i=templateChoice.getSelectedId()-1;if(i>=0&&i<static_cast<int>(templates.size())){input[0].setText(templates[static_cast<std::size_t>(i)].name);input[3].setText(juce::String::toHexString(static_cast<int>(templates[static_cast<std::size_t>(i)].color)).paddedLeft('0',6).toUpperCase());}};
        addAndMakeVisible(saveTemplate);saveTemplate.setName("Save section template");saveTemplate.onClick=[this,save]{const auto name=input[0].getText().trim(),hex=input[3].getText();if(name.isEmpty()||name.length()>128||hex.length()!=6||!hex.containsOnly("0123456789abcdefABCDEF"))return;ViewSettings::SectionTemplate entry{name,static_cast<std::uint32_t>(hex.getHexValue32())};auto at=std::find_if(templates.begin(),templates.end(),[&](const auto& t){return t.name==name;});if(at==templates.end()){if(templates.size()>=64)return;templates.push_back(entry);}else *at=entry;save(entry);refreshTemplates();};
        const std::array<std::uint32_t,7> colors{0x4056D6,0x7050A0,0x287B89,0x487D52,0xD5B63E,0xAD5677,0x505B70};
        const std::array<juce::String,7> titles{"Blue","Purple","Teal","Green","Amber","Rose","Grey"};
        for(std::size_t i=0;i<paletteCount;++i){
            addAndMakeVisible(palette[i]);const auto rgb=modern?ObjectPalette::tags[i].rgb:colors[i];const juce::String title=modern?ObjectPalette::tags[i].name:titles[i];
            palette[i].setName(title);palette[i].setTooltip(title);
            palette[i].setColour(juce::TextButton::buttonColourId,juce::Colour(0xff000000|rgb));
            palette[i].setToggleState(static_cast<std::uint32_t>(value->color.getHexValue32())==rgb,juce::dontSendNotification);
            palette[i].setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff000000|rgb));
            palette[i].onClick=[this,color=rgb]{input[3].setText(juce::String::toHexString(static_cast<int>(color)).paddedLeft('0',6).toUpperCase());commit();};
        }
    }
    ~SectionEditor()override{setLookAndFeel(nullptr);}
    void getIdealSize(int& width,int& height)override{width=300;height=282;}
    void resized()override{
        for(int i=0;i<4;++i)input[static_cast<std::size_t>(i)].setBounds(70,8+i*32,getWidth()-82,26);
        apply.setBounds(70,140,getWidth()-82,26);
        for(std::size_t i=0;i<paletteCount;++i)palette[i].setBounds(12+static_cast<int>(i)*(paletteCount==8?34:40),183,28,26);
        templateChoice.setBounds(12,220,276,26);saveTemplate.setBounds(12,250,276,26);
    }
    void paint(juce::Graphics& g)override{
        g.setFont(Theme::browserFont());g.setColour(juce::Colour(primaryText));
        const std::array<juce::String,4> names{"Name","Start","End","RGB"};
        for(int i=0;i<4;++i)g.drawText(names[static_cast<std::size_t>(i)],12,8+i*32,56,26,juce::Justification::centredLeft);
        g.setColour(juce::Colour(separatorColour));g.drawHorizontalLine(174,12.f,static_cast<float>(getWidth()-12));
        for(const auto& button:palette)if(button.getToggleState()){
            g.setColour(juce::Colours::white);g.drawRoundedRectangle(button.getBounds().toFloat().reduced(1.f),cornerRadius,1.f);
        }
    }
private:
    void refreshTemplates(){templateChoice.clear(juce::dontSendNotification);int id=1;for(const auto& entry:templates)templateChoice.addItem(entry.name,id++);}
    std::vector<ViewSettings::SectionTemplate> templates;juce::ComboBox templateChoice;juce::TextButton saveTemplate{"Save text + color as template"};
    void commit(){value->name=input[0].getText();value->start=input[1].getText();value->end=input[2].getText();value->color=input[3].getText();triggerMenuItem();}
    std::shared_ptr<SectionFields> value;std::array<juce::TextEditor,4> input;
    juce::TextButton apply{"Apply"};std::array<juce::TextButton,8> palette;std::size_t paletteCount{};
};

}
SectionTrack::SectionTrack(Desktop& d,Arrangement& a):owner(d),arrangement(a){setWantsKeyboardFocus(true);setTitle("Sections: colors and bounds");setDescription("Click to seek. Drag edges to resize. Double click to rename; right click to edit bounds and color.");addAndMakeVisible(add);add.setTooltip("Create a section for color and boundary editing");add.onClick=[this]{owner.run([&]{const auto p=owner.project();const mrs::Timeline t(p->time,p->sample_rate);auto start=t.to_ticks(owner.displayState().sample);for(const auto& s:p->sections)if(s.start<=start&&start<s.end)start=s.end;edit({},start);});};}
void SectionTrack::resized(){add.setBounds(208,4,34,24);}
void SectionTrack::cancel(){copyDrag=false;selecting=false;selectionBox={};selectionBase.clear();originals.clear();previews.clear();original.reset();preview.reset();store.reset();setMouseCursor(juce::MouseCursor::NormalCursor);repaint();}
void SectionTrack::sync(bool replaced){add.setEnabled(!owner.app.recording());if(replaced){nameEditor.reset();chosen.clear();selected.reset();cancel();}const auto p=owner.project();std::erase_if(chosen,[&](const auto& id){return std::none_of(p->sections.begin(),p->sections.end(),[&](const auto& item){return item.id.value==id;});});if(original&&(store!=owner.app.services().projects||p->time!=time||zoom!=arrangement.pixelsPerSecond||scroll!=arrangement.horizontal||std::any_of(originals.begin(),originals.end(),[&](const auto& item){return std::find(p->sections.begin(),p->sections.end(),item)==p->sections.end();})))cancel();if(selected&&std::none_of(p->sections.begin(),p->sections.end(),[&](const auto& s){return s.id==*selected;}))selected.reset();repaint();}
juce::Rectangle<float> SectionTrack::sectionRect(const mrs::ArrangerSection& s)const{const auto p=owner.project();const mrs::Timeline t(p->time,p->sample_rate);const auto x=[&](mrs::Tick tick){return static_cast<float>(Arrangement::left+static_cast<double>(t.to_samples(tick))/p->sample_rate*arrangement.pixelsPerSecond-arrangement.horizontal);};return {x(s.start),1.f,std::max(1.f,x(s.end)-x(s.start)),30.f};}
std::optional<mrs::ArrangerSection> SectionTrack::sectionAt(juce::Point<float> point)const{if(point.x<Arrangement::left)return {};for(const auto& s:owner.project()->sections)if(sectionRect(s).contains(point))return s;return {};}
int SectionTrack::edgeAt(const mrs::ArrangerSection& s,float x)const{const auto r=sectionRect(s);const auto a=x-r.getX(),b=r.getRight()-x;return std::min(a,b)<std::min(6.f,r.getWidth()/3.f)?(a<=b?-1:1):0;}
void SectionTrack::paint(juce::Graphics& g){
    g.fillAll(juce::Colour(arrangementCanvas));g.setColour(juce::Colour(surface));g.fillRect(0,0,Arrangement::left,getHeight());g.setFont(Theme::browserFont());g.setColour(juce::Colours::lightgrey);g.drawText("Sections",8,0,150,33,juce::Justification::left);
    juce::Graphics::ScopedSaveState save(g);g.reduceClipRegion(Arrangement::left,0,std::max(0,getWidth()-Arrangement::left),getHeight());arrangement.paintGrid(g,0,static_cast<float>(getHeight()));g.setFont(Theme::browserFont());
    const auto draw=[&](const mrs::ArrangerSection& item,bool ghost){auto r=sectionRect(item);const auto color=juce::Colour(0xff000000|item.color);g.setColour(color.withAlpha(ghost?.35f:clipFillAlpha));g.fillRoundedRectangle(r.reduced(.5f),3.f);g.setColour(ghost?(previewValid?juce::Colours::white:juce::Colours::salmon):chosen.contains(item.id.value)?(owner.assetSkin()?owner.selectionColour():juce::Colours::white):juce::Colour(outline));g.drawRoundedRectangle(r.reduced(.5f),3.f,borderWidth);g.setColour(owner.assetSkin()?ObjectPalette::ink(item.color,ghost?.35f:clipFillAlpha,arrangementCanvas):juce::Colours::white);g.drawText(label(item.name),r.toNearestInt().reduced(7,0),juce::Justification::centredLeft,true);};
    for(const auto& stored:owner.project()->sections){auto item=stored;if(!copyDrag&&original){const auto at=std::find_if(previews.begin(),previews.end(),[&](const auto& value){return value.id==stored.id;});if(at!=previews.end())item=*at;}draw(item,false);}
    if(copyDrag&&original)for(const auto& item:previews)draw(item,true);
    if(selecting){g.setColour(owner.selectionColour().withAlpha(.2f));g.fillRect(selectionBox);g.setColour(owner.selectionColour());g.drawRect(selectionBox,1.f);}
}
void SectionTrack::edit(std::optional<mrs::ArrangerSection> expected,mrs::Tick start,std::optional<juce::Point<int>> screen){
    juce::PopupMenu::dismissAllActiveMenus();cancel();
    arrangement.chords->clearSelection();arrangement.meters->clearSelection();arrangement.tempos->clearSelection();arrangement.markers->clearSelection();owner.selectedClip.reset();owner.selectedClips.clear();
    chosen.clear();selected=expected?std::optional<mrs::Id>{expected->id}:std::nullopt;if(selected)chosen.insert(selected->value);repaint();
    const auto p=owner.project();const auto capturedTime=p->time;const mrs::Timeline t(capturedTime,p->sample_rate);
    auto end=std::min(mrs::max_tick,start+4*mrs::ppq);
    if(!expected)for(const auto& s:p->sections)if(s.start>start){end=std::min(end,s.start);break;}
    auto fields=std::make_shared<SectionFields>();fields->name=expected?label(expected->name):"Section";
    fields->start=position(t,expected?expected->start:start);fields->end=position(t,expected?expected->end:end);
    fields->color=juce::String::toHexString(static_cast<int>(expected?expected->color:(owner.assetSkin()?ObjectPalette::defaultSection:0x4056D6))).paddedLeft('0',6).toUpperCase();
    juce::Component::SafePointer<SectionTrack> templateOwner(this);
    auto form=std::make_unique<SectionEditor>(fields,owner.theme,owner.assetSkin()!=nullptr,owner.view.sectionTemplates,[templateOwner](ViewSettings::SectionTemplate entry){if(!templateOwner)return;auto& d=templateOwner->owner;d.run([&]{auto values=d.view.sectionTemplates;auto at=std::find_if(values.begin(),values.end(),[&](const auto& t){return t.name==entry.name;});if(at==values.end())values.push_back(entry);else *at=entry;auto previous=d.view.sectionTemplates;d.view.sectionTemplates=std::move(values);try{d.saveSettings();}catch(...){d.view.sectionTemplates=std::move(previous);throw;}});});editor=form.get();
    juce::PopupMenu menu;menu.setLookAndFeel(&owner.theme);
    if(expected){menu.addItem(3,"Duplicate section (Ctrl+D)",!owner.app.recording());menu.addItem(2,"Remove section (Backspace)",!owner.app.recording());menu.addSeparator();}
    menu.addCustomItem(1,std::move(form),nullptr,"Section name, bounds and colors");
    juce::Component::SafePointer<SectionTrack> safe(this);const auto projectStore=owner.app.services().projects;
    auto options=juce::PopupMenu::Options().withTargetComponent(this).withMinimumWidth(300);
    if(screen)options=options.withTargetScreenArea({screen->x,screen->y,1,1});
    menu.showMenuAsync(options,[safe,fields,projectStore,capturedTime,expected](int result){
        if(!safe||!result)return;
        safe->owner.run([&]{
            if(projectStore!=safe->owner.app.services().projects||capturedTime!=safe->owner.project()->time)throw std::runtime_error("Project/time changed; reopen the section menu");
            if(result==3&&expected){safe->selectOne(safe->owner.app.copy_section(*expected,expected->end));return;}
            if(result==2&&expected){safe->owner.app.remove_section(*expected);safe->clearSelection();return;}
            const mrs::Timeline current(capturedTime,safe->owner.project()->sample_rate);
            auto value=expected.value_or(mrs::ArrangerSection{});value.name=fields->name.trim().toStdString();
            value.start=parsePosition(current,fields->start);value.end=parsePosition(current,fields->end);
            const auto hex=fields->color.trim();if(hex.length()!=6||!hex.containsOnly("0123456789abcdefABCDEF"))throw std::invalid_argument("Enter six RGB hex digits");
            value.color=static_cast<std::uint32_t>(hex.getHexValue32());
            if(expected){safe->owner.app.update_section(*expected,value);safe->selectOne(value.id);}
            else safe->selectOne(safe->owner.app.add_section(value.name,value.start,value.end,value.color));
        });
        safe->grabKeyboardFocus();safe->repaint();
    });
}
void SectionTrack::rename(const mrs::ArrangerSection& expected){
    cancel();selectOne(expected.id);const auto captured=owner.app.services().projects;
    juce::Component::SafePointer<SectionTrack> safe(this);
    nameEditor=std::make_unique<InlineNameEditor>(juce::String::fromUTF8(expected.name.c_str()),[safe,captured,expected](InlineNameEditor& input,std::optional<juce::String> text){
        if(!safe||safe->nameEditor.get()!=&input)return;
        if(text)safe->owner.run([&]{if(captured!=safe->owner.app.services().projects)throw std::runtime_error("Project changed; retry rename");auto value=expected;value.name=text->toStdString();safe->owner.app.update_section(expected,value);});
        safe->nameEditor.reset();safe->grabKeyboardFocus();safe->repaint();
    });
    addAndMakeVisible(*nameEditor);auto bounds=sectionRect(expected).toNearestInt();bounds.setX(std::max(Arrangement::left,bounds.getX()));bounds.setWidth(std::min(std::max(160,bounds.getWidth()),std::max(1,getWidth()-bounds.getX())));
    nameEditor->setBounds(bounds);nameEditor->grabKeyboardFocus();nameEditor->selectAll();
}
void SectionTrack::selectOne(mrs::Id id){chosen={id.value};selected=std::move(id);}
std::vector<mrs::ArrangerSection> SectionTrack::selectedSections()const{std::vector<mrs::ArrangerSection> result;for(const auto& item:owner.project()->sections)if(chosen.contains(item.id.value))result.push_back(item);return result;}
void SectionTrack::duplicateSelected(){const auto values=selectedSections();if(values.empty())return;const auto ids=owner.app.transform_sections(values,values.back().end-values.front().start,true);chosen.clear();for(const auto& id:ids)chosen.insert(id.value);selected=ids.front();}
void SectionTrack::removeSelected(){const auto values=selectedSections();if(values.empty())return;owner.app.remove_sections(values);clearSelection();}
void SectionTrack::splitSelected(mrs::Tick tick){owner.app.split_sections(selectedSections(),tick);}
void SectionTrack::mouseDown(const juce::MouseEvent& e){
    grabKeyboardFocus();cancel();arrangement.chords->clearSelection();arrangement.meters->clearSelection();arrangement.tempos->clearSelection();arrangement.markers->clearSelection();owner.selectedClip.reset();owner.selectedClips.clear();
    const auto item=sectionAt(e.position);if(e.mods.isPopupMenu()){owner.run([&]{edit(item,arrangement.chords->tickAt(e.position.x),e.getScreenPosition());});return;}
    if(!e.mods.isLeftButtonDown()||e.x<Arrangement::left)return;
    if(owner.knifeMode&&item){selectOne(item->id);owner.run([&]{splitSelected(arrangement.chords->tickAt(e.position.x));});return;}
    if(!item){if(!e.mods.isShiftDown()){chosen.clear();selected.reset();}selectionBase=chosen;selectionOrigin=e.position;selecting=true;repaint();return;}
    if(e.mods.isShiftDown()){if(chosen.contains(item->id.value))chosen.erase(item->id.value);else chosen.insert(item->id.value);selected=chosen.empty()?std::nullopt:std::optional<mrs::Id>{mrs::Id{*chosen.begin()}};repaint();owner.refresh();return;}
    if(!chosen.contains(item->id.value))selectOne(item->id);else selected=item->id;
    copyDrag=e.mods.isAltDown();edge=copyDrag||chosen.size()>1?0:edgeAt(*item,e.position.x);
    const auto state=owner.displayState();const bool editable=!owner.app.recording()&&!state.count_remaining&&state.playback!=mrs::PlaybackState::playing&&!state.loop;
    if(editable){original=item;preview=item;originals=selectedSections();previews=originals;store=owner.app.services().projects;time=owner.project()->time;zoom=arrangement.pixelsPerSecond;scroll=arrangement.horizontal;dragX=e.position.x;anchor=arrangement.chords->tickAt(dragX,false);}
    else if(edge||copyDrag){owner.message="Pause/stop and turn off loop before editing sections";owner.repaint();return;}
    if(!edge&&!copyDrag)owner.run([&]{owner.app.musical().seek_section(item->id);});repaint();
}
void SectionTrack::mouseDrag(const juce::MouseEvent& e){
    if(selecting){selectionBox=juce::Rectangle<float>(selectionOrigin,e.position).getIntersection(juce::Rectangle<float>(static_cast<float>(Arrangement::left),0.f,static_cast<float>(std::max(0,getWidth()-Arrangement::left)),static_cast<float>(getHeight())));chosen=selectionBase;for(const auto& item:owner.project()->sections)if(sectionRect(item).intersects(selectionBox))chosen.insert(item.id.value);selected=chosen.empty()?std::nullopt:std::optional<mrs::Id>{mrs::Id{*chosen.begin()}};repaint();return;}
    if(!original)return;sync();if(!original)return;const auto state=owner.displayState();if(owner.app.recording()||state.playback==mrs::PlaybackState::playing||state.loop||state.count_remaining){cancel();return;}
    const auto rawDelta=arrangement.chords->tickAt(e.position.x,false)-anchor;const auto snap=[&](mrs::Tick tick){return owner.snap?arrangement.snapTick(tick):tick;};previews=originals;previewValid=true;
    if(!edge){const auto first=originals.front().start,last=originals.back().end;auto delta=std::clamp(snap(original->start+rawDelta)-original->start,-first,mrs::max_tick-last);
        // Paint/hit rounding can differ by a fraction of a pixel. Capture exact touching edges.
        double distance=7.;const auto x=[&](mrs::Tick tick){return sectionRect({{},"",tick,tick+1,0}).getX();};
        for(const auto& other:owner.project()->sections)if(copyDrag||!chosen.contains(other.id.value))for(const auto candidate:{other.end-first,other.start-last}){if(candidate<-first||candidate>mrs::max_tick-last)continue;const auto d=std::abs(x(original->start+std::clamp(rawDelta,-first,mrs::max_tick-last))-x(original->start+candidate));if(d<distance){distance=d;delta=candidate;}}
        for(auto& item:previews){item.start+=delta;item.end+=delta;}
    }else{auto& item=previews.front();mrs::Tick low=0,high=mrs::max_tick;for(const auto& other:owner.project()->sections)if(other.id!=item.id){if(other.end<=item.start)low=std::max(low,other.end);if(other.start>=item.end)high=std::min(high,other.start);}if(edge<0)item.start=std::clamp(snap(item.start+rawDelta),low,item.end-1);else item.end=std::clamp(snap(item.end+rawDelta),item.start+1,high);}
    for(const auto& item:previews)for(const auto& other:owner.project()->sections)if((copyDrag||!chosen.contains(other.id.value))&&item.start<other.end&&other.start<item.end)previewValid=false;
    preview=*std::find_if(previews.begin(),previews.end(),[&](const auto& item){return item.id==original->id;});repaint();
}
void SectionTrack::mouseUp(const juce::MouseEvent& e){
    if(selecting){selecting=false;selectionBox={};selectionBase.clear();owner.refresh();repaint();return;}
    if(!original)return;sync();if(!original)return;const auto expected=originals,values=previews;const auto capturedStore=store;const bool copying=copyDrag,valid=previewValid;const auto capturedEdge=edge;const bool moved=std::abs(e.position.x-dragX)>=2;cancel();
    if(!moved||values==expected)return;if(!valid){owner.message="Sections overlap; choose a free position";owner.repaint();return;}
    owner.run([&]{if(capturedStore!=owner.app.services().projects)throw std::runtime_error("Project changed; retry section gesture");if(capturedEdge)owner.app.update_section(expected.front(),values.front());else{const auto ids=owner.app.transform_sections(expected,values.front().start-expected.front().start,copying);chosen.clear();for(const auto& id:ids)chosen.insert(id.value);selected=ids.front();}});
}
void SectionTrack::mouseMove(const juce::MouseEvent& e){if(owner.knifeMode){setMouseCursor(juce::MouseCursor::CrosshairCursor);return;}const auto s=sectionAt(e.position);setMouseCursor(s&&edgeAt(*s,e.position.x)?juce::MouseCursor::LeftRightResizeCursor:juce::MouseCursor::NormalCursor);if(s){const auto p=owner.project();const mrs::Timeline t(p->time,p->sample_rate);setDescription(label(s->name)+": "+position(t,s->start)+" – "+position(t,s->end));}}
void SectionTrack::mouseDoubleClick(const juce::MouseEvent& e){if(owner.knifeMode)return;if(e.mods.isLeftButtonDown()&&e.x>=Arrangement::left)owner.run([&]{const auto s=sectionAt(e.position);if(s)rename(*s);else edit({},arrangement.chords->tickAt(e.position.x));});}
bool SectionTrack::keyPressed(const juce::KeyPress& k){if(k.getKeyCode()==juce::KeyPress::escapeKey){cancel();return true;}if(original)return true;if(!k.getModifiers().isAnyModifierKeyDown()&&(k.getKeyCode()==juce::KeyPress::deleteKey||k.getKeyCode()==juce::KeyPress::backspaceKey)){owner.run([&]{removeSelected();});return true;}if(k.getKeyCode()==juce::KeyPress::returnKey&&selected){for(const auto& s:owner.project()->sections)if(s.id==*selected){owner.run([&]{edit(s);});break;}return true;}return owner.workspaceShortcut(k);}
}
