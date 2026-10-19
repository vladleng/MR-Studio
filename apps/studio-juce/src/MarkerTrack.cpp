#include "MeterTrack.h"
#include "MarkerTrack.h"
#include "ChordTrack.h"
#include "Desktop.h"
#include "TempoTrack.h"
#include "SectionTrack.h"
#include <algorithm>
namespace ui {
namespace {
const char* markerKinds[]={"Generic","Cue","Warning","Lyric","Action","Navigation"};
juce::String markerName(const mrs::Marker& m){return juce::String::fromUTF8(m.name.c_str());}
}
MarkerTrack::MarkerTrack(Desktop& d,Arrangement& a):owner(d),arrangement(a){
    setWantsKeyboardFocus(true);setTitle("Markers");
    setDescription("Double click to add or edit; select overlapping markers from the list. Backspace removes; actions are metadata only.");
    addAndMakeVisible(list);addAndMakeVisible(add);list.setTextWhenNothingSelected("Marker points");
    list.setTitle("Select marker, including coincident positions");add.setTooltip("Add marker at the playhead");
    list.onChange=[this]{const auto index=list.getSelectedId()-1;if(index>=0&&index<static_cast<int>(listed.size())){const auto id=listed[static_cast<std::size_t>(index)].id;owner.run([&]{owner.app.musical().seek_marker(id);select(id);});}};
    add.onClick=[this]{owner.run([&]{const auto p=owner.project();edit({},mrs::Timeline(p->time,p->sample_rate).to_ticks(owner.displayState().sample));});};
}
void MarkerTrack::resized(){list.setBounds(70,1,132,22);add.setBounds(208,1,34,22);}
void MarkerTrack::cancel(){const bool box=group.selecting;group.cancel();if(box)selected=group.primary();copyDrag=false;original.reset();preview.reset();gestureStore.reset();repaint();}
void MarkerTrack::select(mrs::Id id){group.set({id});selected=std::move(id);arrangement.chords->clearSelection();arrangement.meters->clearSelection();arrangement.tempos->clearSelection();arrangement.sections->clearSelection();owner.selectedClip.reset();owner.selectedClips.clear();owner.refresh();grabKeyboardFocus();}
void MarkerTrack::sync(bool replaced){
    if(replaced){nameEditor.reset();clearSelection();}const auto p=owner.project();group.sync(p->markers);add.setEnabled(!owner.app.recording());
    if(selected&&std::none_of(p->markers.begin(),p->markers.end(),[&](const auto& m){return m.id==*selected;}))selected.reset();
    if(original&&(gestureStore!=owner.app.services().projects||p->time!=gestureTime||arrangement.pixelsPerSecond!=gestureZoom||arrangement.horizontal!=gestureScroll||
        std::none_of(p->markers.begin(),p->markers.end(),[&](const auto& m){return m==*original;})))cancel();
    if(listed!=p->markers){listed=p->markers;list.clear(juce::dontSendNotification);int index=0;
        for(const auto& m:listed){++index;list.addItem(juce::String(index)+". "+markerName(m)+" @"+juce::String(m.tick)+" ["+markerKinds[static_cast<int>(m.kind)]+"]",index);}
    }
    int choice=0;for(std::size_t i=0;i<listed.size();++i)if(selected==listed[i].id)choice=static_cast<int>(i)+1;
    list.setSelectedId(choice,juce::dontSendNotification);repaint();
}
float MarkerTrack::xAt(mrs::Tick tick) const {const auto p=owner.project();return static_cast<float>(Arrangement::left+static_cast<double>(mrs::Timeline(p->time,p->sample_rate).to_samples(tick))/p->sample_rate*arrangement.pixelsPerSecond-arrangement.horizontal);}
std::vector<mrs::Marker> MarkerTrack::markersAt(float x) const {
    std::vector<mrs::Marker> hits;if(x<Arrangement::left)return hits;
    const auto p=owner.project();
    const mrs::Timeline time(p->time,p->sample_rate);
    const auto xFor=[&](mrs::Tick tick){return static_cast<float>(Arrangement::left+static_cast<double>(time.to_samples(tick))/p->sample_rate*arrangement.pixelsPerSecond-arrangement.horizontal);};
    for(const auto& m:p->markers){const auto begin=xFor(m.tick);if(x<begin-7||x>begin+128)continue;float width=120;
        for(const auto& next:p->markers)if(next.tick>m.tick)width=std::min(width,xFor(next.tick)-begin-8);
        if(std::abs(begin-x)<=7||(x>=begin+8&&x<=begin+8+std::max(0.f,width)))hits.push_back(m);
    }
    return hits;
}
void MarkerTrack::paint(juce::Graphics& g){
    g.fillAll(juce::Colour(arrangementCanvas));g.setColour(juce::Colour(surface));g.fillRect(0,0,Arrangement::left,getHeight());
    g.setFont(Theme::browserFont());g.setColour(juce::Colour(primaryText));g.drawText("Markers",8,0,60,getHeight(),juce::Justification::centredLeft);
    g.saveState();g.reduceClipRegion(Arrangement::left,0,std::max(0,getWidth()-Arrangement::left),getHeight());
    arrangement.paintGrid(g,0,static_cast<float>(getHeight()));g.setFont(Theme::browserFont());
    const auto p=owner.project();const mrs::Timeline time(p->time,p->sample_rate);std::vector<mrs::Tick> drawn;
    const auto xFor=[&](mrs::Tick tick){return static_cast<float>(Arrangement::left+static_cast<double>(time.to_samples(tick))/p->sample_rate*arrangement.pixelsPerSecond-arrangement.horizontal);};
    auto painted=p->markers;if(!copyDrag&&original)for(auto& m:painted){const auto at=std::find_if(group.previews.begin(),group.previews.end(),[&](const auto& item){return item.id==m.id;});if(at!=group.previews.end())m=*at;}
    for(const auto& stored:painted){
        if(std::find(drawn.begin(),drawn.end(),stored.tick)!=drawn.end())continue;
        auto m=stored;
        if(owner.displayedMusical.project==p&&owner.displayedMusical.current_marker)
            for(const auto& candidate:painted)if(candidate.tick==m.tick&&candidate.id==owner.displayedMusical.current_marker->id)m=candidate;
        for(const auto& candidate:painted)if(candidate.tick==m.tick&&selected==candidate.id)m=candidate;
        const auto x=xFor(m.tick);
        if(x<Arrangement::left-120||x>getWidth()+8)continue;
        const auto count=std::count_if(painted.begin(),painted.end(),[&](const auto& other){return other.tick==m.tick;});
        const bool chosen=group.ids.contains(m.id.value);
        const bool active=owner.displayedMusical.project==p&&owner.displayedMusical.current_marker&&owner.displayedMusical.current_marker->tick==m.tick;
        if(active){g.setColour(owner.selectionColour().withAlpha(.3f));g.fillRect(x-2,0.f,8.f,24.f);}
        g.setColour(chosen?juce::Colours::white:juce::Colour(0xff000000|m.color.value_or(m.kind==mrs::MarkerKind::warning?(owner.assetSkin()?ObjectPalette::warning:0xffaa55):(owner.assetSkin()?ObjectPalette::defaultMarker:0x4056D6))));
        g.drawVerticalLine(static_cast<int>(x),2,22);g.fillRect(x,2.f,6.f,5.f);
        drawn.push_back(m.tick);
        float width=120;for(const auto& next:painted)if(next.tick>m.tick)width=std::min(width,xFor(next.tick)-x-8);
        if(width>8){const auto badge=juce::Rectangle<float>(x+6,1,width,22);const auto color=juce::Colour(0xff000000|m.color.value_or(m.kind==mrs::MarkerKind::warning?(owner.assetSkin()?ObjectPalette::warning:0xffaa55):(owner.assetSkin()?ObjectPalette::defaultMarker:0x4056D6)));
            drawCard(g,badge,color.withAlpha(.25f));
            if(chosen){g.setColour(owner.selectionColour());g.drawRoundedRectangle(badge.reduced(.5f),cornerRadius,borderWidth);}
            g.setColour(juce::Colour(primaryText));
            g.drawText(markerName(m)+(count>1?" [+"+juce::String(static_cast<int>(count)-1)+"]":""),badge.toNearestInt().reduced(5,0),juce::Justification::centredLeft,true);}
    }
    if(copyDrag&&original)for(const auto& m:group.previews){const auto x=xAt(m.tick);const juce::Rectangle<float> r{x+6,1,120,22};g.setColour(juce::Colour(0xff000000|m.color.value_or(owner.assetSkin()?ObjectPalette::defaultMarker:0x4056D6)).withAlpha(.35f));g.fillRoundedRectangle(r,cornerRadius);g.setColour(juce::Colours::white);g.drawVerticalLine(juce::roundToInt(x),2,22);g.drawRoundedRectangle(r.reduced(.5f),cornerRadius,1.f);g.drawText(markerName(m),r.toNearestInt().reduced(5,0),juce::Justification::centredLeft,true);}group.paintBox(g,owner.selectionColour());g.restoreState();
}
void MarkerTrack::edit(std::optional<mrs::Marker> expected,mrs::Tick tick){
    if(owner.app.recording())throw std::invalid_argument("End recording before editing markers");cancel();
    auto* alert=new juce::AlertWindow(expected?"Edit marker":"Add marker","Markers and action kinds are metadata; editing does not trigger MIDI.",juce::MessageBoxIconType::NoIcon);
    alert->addTextEditor("name",expected?markerName(*expected):"Marker","Name");
    alert->addTextEditor("tick",juce::String(expected?expected->tick:tick),"Position (ticks, 960 per quarter)");
    juce::StringArray kinds;for(const auto* kind:markerKinds)kinds.add(kind);alert->addComboBox("kind",kinds,"Kind");
    alert->getComboBoxComponent("kind")->setSelectedItemIndex(expected?static_cast<int>(expected->kind):0);
    alert->addButton("Save",1,juce::KeyPress(juce::KeyPress::returnKey));alert->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<MarkerTrack> safe(this);const auto store=owner.app.services().projects;
    alert->enterModalState(true,juce::ModalCallbackFunction::create([safe,alert,store,expected](int result){if(!safe||result!=1)return;
        safe->owner.run([&]{if(store!=safe->owner.app.services().projects)throw std::runtime_error("Project changed; reopen the marker editor");
            const auto text=alert->getTextEditorContents("tick").trim().toStdString();std::size_t used{};const auto tickValue=std::stoll(text,&used);
            if(used!=text.size()||tickValue<0||tickValue>mrs::max_tick)throw std::invalid_argument("Enter a non-negative tick within the project range");
            const auto kind=static_cast<mrs::MarkerKind>(alert->getComboBoxComponent("kind")->getSelectedItemIndex());
            const auto name=alert->getTextEditorContents("name").toStdString();
            if(expected){auto value=*expected;value.name=name;value.tick=tickValue;value.kind=kind;safe->owner.app.update_marker(*expected,value);safe->select(value.id);}
            else safe->select(safe->owner.app.add_marker(name,tickValue,kind));
        });
    }),true);
}
void MarkerTrack::rename(const mrs::Marker& expected){
    cancel();group.set({expected.id});selected=expected.id;const auto captured=owner.app.services().projects;
    juce::Component::SafePointer<MarkerTrack> safe(this);
    nameEditor=std::make_unique<InlineNameEditor>(juce::String::fromUTF8(expected.name.c_str()),[safe,captured,expected](InlineNameEditor& input,std::optional<juce::String> text){
        if(!safe||safe->nameEditor.get()!=&input)return;
        if(text)safe->owner.run([&]{if(captured!=safe->owner.app.services().projects)throw std::runtime_error("Project changed; retry rename");auto value=expected;value.name=text->toStdString();safe->owner.app.update_marker(expected,value);});
        safe->nameEditor.reset();safe->grabKeyboardFocus();safe->repaint();
    });
    addAndMakeVisible(*nameEditor);auto bounds=juce::Rectangle<int>(static_cast<int>(xAt(expected.tick))+6,1,160,22);bounds.setX(std::max(Arrangement::left,bounds.getX()));bounds.setWidth(std::min(std::max(160,bounds.getWidth()),std::max(1,getWidth()-bounds.getX())));
    nameEditor->setBounds(bounds);nameEditor->grabKeyboardFocus();nameEditor->selectAll();
}
void MarkerTrack::selectGroup(const std::vector<mrs::Id>& ids){group.set(ids);selected=group.primary();}
void MarkerTrack::removeSelected(){const auto values=group.selected(owner.project()->markers);if(values.empty())return;owner.app.remove_markers(values);clearSelection();}
void MarkerTrack::duplicateSelected(){const auto values=group.selected(owner.project()->markers);if(values.empty())return;selectGroup(owner.app.transform_markers(values,std::max(mrs::ppq/4,groupEnd(values)-groupStart(values)),true));}
void MarkerTrack::menu(float x,std::optional<juce::Point<int>> screen){
    auto hits=markersAt(x);
    if(hits.empty()&&selected)for(const auto& m:owner.project()->markers)if(m.id==*selected)hits.push_back(m);
    juce::PopupMenu menu;menu.setLookAndFeel(&owner.theme);menu.addItem(1,"Add marker...",!owner.app.recording());
    if(!hits.empty()){menu.addItem(6,"Duplicate (Ctrl+D)",!owner.app.recording());menu.addItem(2,"Rename marker...",!owner.app.recording());menu.addItem(3,"Remove selected marker (Backspace)",!owner.app.recording());menu.addItem(4,"Go to selected marker",!owner.app.recording());menu.addItem(5,"Edit position and kind...",!owner.app.recording());}
    for(std::size_t i=0;i<hits.size();++i)menu.addItem(100+static_cast<int>(i),juce::String(static_cast<int>(i)+1)+". "+markerName(hits[i])+" @"+juce::String(hits[i].tick));
    auto expected=hits.empty()?std::optional<mrs::Marker>{}:std::optional<mrs::Marker>{hits.front()};
    for(const auto& m:hits)if(selected==m.id)expected=m;
    auto color=std::make_shared<std::uint32_t>(expected?expected->color.value_or(expected->kind==mrs::MarkerKind::warning?(owner.assetSkin()?ObjectPalette::warning:0xffaa55):(owner.assetSkin()?ObjectPalette::defaultMarker:0x4056D6)):0x4056D6);
    if(expected){menu.addSeparator();auto palette=std::make_unique<MenuColourPalette>(owner.theme,*color,color,false,owner.assetSkin()!=nullptr);paletteEditor=palette.get();menu.addCustomItem(10,std::move(palette),nullptr,"Marker color");}
    juce::Component::SafePointer<MarkerTrack> safe(this);const auto store=owner.app.services().projects;const auto tick=arrangement.chords->tickAt(x);
    auto options=juce::PopupMenu::Options().withTargetComponent(this);if(screen)options=options.withTargetScreenArea({screen->x,screen->y,1,1});
    menu.showMenuAsync(options,[safe,store,expected,hits,tick,color](int result){if(!safe||!result)return;
        safe->owner.run([&]{if(store!=safe->owner.app.services().projects)throw std::runtime_error("Project changed; reopen the marker menu");
            if(result==6&&expected)safe->select(safe->owner.app.transform_markers({*expected},mrs::ppq/4,true).front());
            else if(result==1)safe->edit({},tick);
            else if(result==2&&expected)safe->rename(*expected);
            else if(result==5&&expected)safe->edit(expected,expected->tick);
            else if(result==10&&expected){auto value=*expected;value.color=*color;safe->owner.app.update_marker(*expected,value);safe->select(value.id);}
            else if(result==4&&expected)safe->owner.app.musical().seek_marker(expected->id);
            else if(result==3&&expected){safe->owner.app.remove_marker(*expected);safe->clearSelection();}
            else if(result>=100&&result<100+static_cast<int>(hits.size()))safe->select(hits[static_cast<std::size_t>(result-100)].id);
        });
    });
}
void MarkerTrack::mouseDown(const juce::MouseEvent& e){
    cancel();grabKeyboardFocus();if(e.x<Arrangement::left)return;arrangement.chords->clearSelection();arrangement.meters->clearSelection();arrangement.tempos->clearSelection();arrangement.sections->clearSelection();owner.selectedClip.reset();owner.selectedClips.clear();
    if(e.mods.isPopupMenu()){menu(e.position.x,e.getScreenPosition());return;}if(!e.mods.isLeftButtonDown())return;const auto hits=markersAt(e.position.x);
    if(hits.empty()){group.beginBox(e.position,e.mods.isShiftDown());selected=group.primary();repaint();return;}auto m=hits.front();for(const auto& hit:hits)if(selected==hit.id)m=hit;
    if(e.mods.isShiftDown()){group.toggle(m.id);selected=group.primary();owner.refresh();repaint();return;}
    if(!group.ids.contains(m.id.value))select(m.id);else selected=m.id;if(owner.app.recording())return;
    original=m;preview=m;group.originals=group.selected(owner.project()->markers);group.previews=group.originals;copyDrag=e.mods.isAltDown();gestureStore=owner.app.services().projects;gestureTime=owner.project()->time;gestureZoom=arrangement.pixelsPerSecond;gestureScroll=arrangement.horizontal;dragX=e.position.x;anchor=arrangement.chords->tickAt(dragX,false);
}
void MarkerTrack::mouseDrag(const juce::MouseEvent& e){
    if(group.selecting){group.boxDrag(e.position,owner.project()->markers,[&](const auto& m){const auto x=xAt(m.tick);float width=120;for(const auto& next:owner.project()->markers)if(next.tick>m.tick)width=std::min(width,xAt(next.tick)-x-8);return juce::Rectangle<float>{x,1.f,width>8?width+6:6.f,22.f};},getLocalBounds().toFloat().withTrimmedLeft(Arrangement::left));selected=group.primary();repaint();return;}
    if(!original)return;if(owner.app.recording()||gestureStore!=owner.app.services().projects||owner.project()->time!=gestureTime||gestureZoom!=arrangement.pixelsPerSecond||gestureScroll!=arrangement.horizontal){cancel();return;}
    const auto delta=groupDelta(group.originals,*original,arrangement.chords->tickAt(e.position.x,false)-anchor,owner.snap,owner.project()->markers,copyDrag,[&](mrs::Tick tick){return xAt(tick);},[&](mrs::Tick tick){return arrangement.snapTick(tick);});group.previews=group.originals;for(auto& m:group.previews)m.tick+=delta;preview=*std::find_if(group.previews.begin(),group.previews.end(),[&](const auto& m){return m.id==original->id;});repaint();
}
void MarkerTrack::mouseUp(const juce::MouseEvent& e){
    if(group.selecting){group.endBox();selected=group.primary();owner.refresh();repaint();return;}if(!original)return;if(owner.project()->time!=gestureTime||gestureZoom!=arrangement.pixelsPerSecond||gestureScroll!=arrangement.horizontal){cancel();return;}
    const auto expected=group.originals,values=group.previews;const auto store=gestureStore;const bool copying=copyDrag;const bool moved=std::abs(e.position.x-dragX)>=2;cancel();
    if(moved&&values!=expected)owner.run([&]{if(store!=owner.app.services().projects)throw std::runtime_error("Project changed; retry marker gesture");selectGroup(owner.app.transform_markers(expected,values.front().tick-expected.front().tick,copying));});
}
void MarkerTrack::mouseDoubleClick(const juce::MouseEvent& e){if(!e.mods.isLeftButtonDown()||e.x<Arrangement::left)return;
    owner.run([&]{const auto hits=markersAt(e.position.x);if(hits.empty())edit({},arrangement.chords->tickAt(e.position.x));
        else{auto m=hits.front();for(const auto& hit:hits)if(selected==hit.id)m=hit;rename(m);}});
}
bool MarkerTrack::keyPressed(const juce::KeyPress& key){
    if(key.getKeyCode()==juce::KeyPress::escapeKey){cancel();return true;}if(original||group.selecting)return true;
    if(selected&&!key.getModifiers().isAnyModifierKeyDown()){
        if(key.getKeyCode()==juce::KeyPress::backspaceKey||key.getKeyCode()==juce::KeyPress::deleteKey){owner.run([&]{removeSelected();});return true;}
        if(key.getKeyCode()==juce::KeyPress::returnKey){const auto p=owner.project();for(const auto& m:p->markers)if(selected==m.id)owner.run([&]{edit(m,m.tick);});return true;}
    }return owner.workspaceShortcut(key);
}
void MarkerTrack::focusLost(FocusChangeType){cancel();}
}
