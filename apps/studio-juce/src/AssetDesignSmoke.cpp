#include "Desktop.h"
#include "AssetDesign.h"

namespace ui {
void pluginChromeSmoke(Desktop&,std::optional<mrs::Id>,mrs::Id);
void browserDialogSmoke(Desktop&);
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void saveImage(const juce::Image& image,const juce::String& name){
    auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(name+".png").createOutputStream();
    require(stream!=nullptr,"Asset image stream");stream->setPosition(0);stream->truncate();
    require(juce::PNGImageFormat().writeImageToStream(image,*stream),"Asset image export");
}
juce::MouseEvent mouse(juce::Component& c,juce::Point<float> p,bool fine=false){
    const auto now=juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier|(fine?juce::ModifierKeys::ctrlModifier:0)),1,0,0,0,0,&c,&c,now,p,now,1,true};
}
bool same(const juce::Image& a,const juce::Image& b){
    if(a.getWidth()!=b.getWidth()||a.getHeight()!=b.getHeight())return false;
    for(int y=0;y<a.getHeight();++y)for(int x=0;x<a.getWidth();++x)if(a.getPixelAt(x,y)!=b.getPixelAt(x,y))return false;
    return true;
}
}

void assetDesignSmoke(){
    Desktop d(true);d.addToDesktop(juce::ComponentPeer::windowIsTemporary);d.setVisible(true);
    d.stopTimer();d.app.stop();d.app.disconnect();d.setShellDesign(true);d.grabKeyboardFocus();
    const auto revision=[&]{return d.app.services().projects->state().revision;};
    const auto startRevision=revision();
    const auto themeColour=d.theme.findColour(juce::TextButton::buttonColourId);
    const auto baseline=d.createComponentSnapshot(d.getLocalBounds());
    d.setAssetDesign(true);
    require(d.assetSkin()!=nullptr&&d.shellDesignEnabled(),"Live asset activation");
    require(d.theme.findColour(juce::TextButton::buttonColourId)==themeColour,"Global Theme unchanged");
    {
        const auto selected=d.mixButton.createComponentSnapshot(d.mixButton.getLocalBounds());int ink=0;
        for(int y=8;y<24;++y)for(int x=d.mixButton.getWidth()/2-8;x<d.mixButton.getWidth()/2+8;++x){
            const auto c=selected.getPixelAt(x,y);if(c.getRed()>120&&c.getGreen()>140&&c.getBlue()>160)++ink;}
        require(ink>5,"Selected frameless action glyph needs light contrasting ink");
    }
    require(revision()==startRevision,"Visual activation created history");
    require(!same(baseline,d.createComponentSnapshot(d.getLocalBounds())),"Assets did not affect actual Desktop");
    {
        kit::MRFader shared({"gain","Gain",kit::Unit::db,-60,12,0,.1,kit::Mapping::linear,1},true);
        shared.setLookAndFeel(d.assetLookAndFeel());shared.setSize(100,240);
        shared.change(-6);require(std::abs(shared.value()+6)<.001,"Shared Kit fader value remains physical dB");
        saveImage(shared.createComponentSnapshot(shared.getLocalBounds(),true,2),"runtime-shared-kit-fader");
        shared.setLookAndFeel(nullptr);
    }
    const auto empty=juce::File::getCurrentWorkingDirectory().getChildFile("browser-fixture");
    require(empty.createDirectory().wasOk(),"Browser fixture directory");d.browser->navigate(empty);
    int snapshots=0;
    for(int scene=0;scene<3;++scene){
        d.browser->showFiles(scene==2);d.setWorkspace(scene==0?mrs::desktop::Workspace::mix:mrs::desktop::Workspace::arrange);
        for(const auto size:std::array<juce::Point<int>,3>{{{1200,700},{1400,850},{1800,1000}}}){
            d.setSize(size.x,size.y);
            // Asset skin intentionally keeps accepted neutral surface colours, not the target UDS background.
            const auto window=DesignTokens{}.colour(ColourRole::window);
            const auto desktopImage=d.createComponentSnapshot(d.getLocalBounds());
            if(scene==0){
                require(d.mixerViewport.isVisible(),"Mix workspace exposes the mixer");
                require(desktopImage.getPixelAt(d.mixArea.getCentreX(),d.mixArea.getCentreY())==window,"Empty mixer matches transport background");
            }else require(!d.mixerViewport.isVisible(),"Arrange workspace hides the mixer");
            const auto browserImage=d.browser->createComponentSnapshot(d.browser->getLocalBounds());
            require(browserImage.getPixelAt(12,d.browser->getHeight()-28)==window,"Browser matches transport background");
            for(auto* action:{&d.mixer.front()->mute,&d.mixer.front()->solo,&d.mixer.front()->arm}){
                const auto beforeState=action->getToggleState();
                for(bool on:{false,true}){
                    action->setToggleState(on,juce::dontSendNotification);
                    const auto image=action->createComponentSnapshot(action->getLocalBounds());
                    saveImage(image,"action-"+action->getButtonText()+juce::String(on?1:0));
                    require(static_cast<bool>(action->getProperties()["mrChannelPlate"])&&image.getPixelAt(2,10).getAlpha()>0,"Issue213 M/S/O plate like input monitor");
                    require(image.getPixelAt(0,0).getAlpha()==0,"Channel plate retains rounded transparent corners");
                }
                action->setToggleState(beforeState,juce::dontSendNotification);
            }
            for(const auto& row:d.arrangement->rows)require(row->input.getY()>=94,"Mini-strip dB field overlaps input");
            for(const auto scale:{1.f,1.25f,1.5f,2.f}){
                saveImage(d.createComponentSnapshot(d.getLocalBounds(),true,scale,juce::SoftwareImageType{}),
                    "runtime-"+juce::String(scene)+"-"+juce::String(size.x)+"x"+juce::String(size.y)+"-"+juce::String(juce::roundToInt(scale*100)));
                ++snapshots;
            }
        }
    }
    require(revision()==startRevision,"Workspace/render changed project history");
    {
        const auto r=d.mixer.front()->createComponentSnapshot(d.mixer.front()->getLocalBounds());
        require(r.getPixelAt(3,100)==juce::Colour(ConceptSkin::channel),"Channel uses previous button surface");
        auto result=std::make_shared<std::uint32_t>(0);
        MenuColourPalette clips(d.theme,0,result,true,true),markers(d.theme,0,result,false,true);
        int w{},h{};clips.getIdealSize(w,h);clips.setSize(w,h);markers.getIdealSize(w,h);markers.setSize(w,h);
        require(clips.getNumChildComponents()==32&&markers.getNumChildComponents()==8,"Expanded clip vs shared marker palette");
        for(auto* c:clips.getChildren())require(clips.getLocalBounds().contains(c->getBounds()),"Clip swatches fit popup");
        for(const auto scale:{1.f,1.25f,1.5f,2.f}){
            saveImage(clips.createComponentSnapshot(clips.getLocalBounds(),true,scale),"clip-palette-"+juce::String(juce::roundToInt(scale*100)));
            saveImage(markers.createComponentSnapshot(markers.getLocalBounds(),true,scale),"marker-palette-"+juce::String(juce::roundToInt(scale*100)));
        }
        // Existing arbitrary RGB survives style changes; new swatches use the
        // existing command/history/persistence contract without a schema change.
        require(!d.project()->clips.empty(),"Color fixture needs actual clip");
        const auto id=d.project()->clips.front().id;
        const auto before=d.project()->clips.front().color;
        d.app.set_clip_colors({id},0x123456u);
        d.setAssetDesign(false);d.setAssetDesign(true);
        require(d.project()->clips.front().color==0x123456u,"Style switch preserves arbitrary RGB");
        require(d.app.undo()&&d.project()->clips.front().color==before,"Color undo restores optional RGB");
        d.app.redo();d.app.set_clip_colors({id},ObjectPalette::clips.back().rgb);
        require(d.app.undo()&&d.project()->clips.front().color==0x123456u,"Expanded palette color undo");
        require(d.app.redo()&&d.project()->clips.front().color==ObjectPalette::clips.back().rgb,"Expanded palette color redo");
        d.app.undo();d.app.undo();d.refresh();
    }
    for(auto* strip:{d.mixer.front().get(),d.arrangement->rows.front().get()}){
        for(auto* b:{&strip->mute,&strip->solo,&strip->arm,&strip->monitor})if(b->getWidth()!=20||b->getHeight()!=20)
            throw std::runtime_error("Channel square: "+b->getTitle().toStdString()+" "+std::to_string(b->getWidth())+"x"+std::to_string(b->getHeight())+(strip->mini?" Arrange":" Mixer"));
    }
    {
        auto& pan=d.mixer.front()->pan;
        const auto original=pan.value;
        std::array<int,3> white{};std::size_t index=0;
        for(const auto value:{0.,.5,1.}){pan.value=value;const auto image=pan.createComponentSnapshot(pan.getLocalBounds(),true,3);
            for(int y=0;y<image.getHeight();++y)for(int x=0;x<image.getWidth();++x){const auto c=image.getPixelAt(x,y);if(c.getAlpha()>200&&c.getRed()>245&&c.getGreen()>245&&c.getBlue()>245)++white[index];}
            ++index;saveImage(image,"balance-"+juce::String(value,1));}
        require(white[0]>white[1]+10&&white[2]>white[1]+10,"Balance must show white deviation on both sides of centre");
        pan.value=original;
        auto& rotary=d.arrangement->rows.front()->pan;
        const auto before=rotary.createComponentSnapshot(rotary.getLocalBounds());
        const auto centre=rotary.knob().getCentre();rotary.mouseDown(mouse(rotary,centre));rotary.mouseUp(mouse(rotary,centre));
        require(same(before,rotary.createComponentSnapshot(rotary.getLocalBounds())),"Mouse focus must not add a pan frame");
        rotary.focusGained(juce::Component::focusChangedByTabKey);
        require(!same(before,rotary.createComponentSnapshot(rotary.getLocalBounds())),"Keyboard focus stays visible");
        rotary.mouseDown(mouse(rotary,{0,0}));
        require(same(before,rotary.createComponentSnapshot(rotary.getLocalBounds())),"Edge click clears keyboard-only focus frame without changing the parameter");
        rotary.focusGained(juce::Component::focusChangedByMouseClick);
        require(same(before,rotary.createComponentSnapshot(rotary.getLocalBounds())),"Mouse-acquired focus at control edges must not add a frame");
        d.grabKeyboardFocus();
    }
    d.setSize(1400,850);d.setWorkspace(mrs::desktop::Workspace::mix);d.browser->showFiles(false);
    const auto id=d.project()->tracks.front().id;
    const auto gainValue=[&]{for(const auto& t:d.project()->tracks)if(t.id==id)return t.mix.gain;throw std::runtime_error("Missing track");};
    auto* strip=d.mixer.front().get();const auto original=gainValue();
    auto& fader=strip->gain;const auto p=fader.knob().getCentre();const auto rev=revision();
    fader.mouseDown(mouse(fader,p));require(fader.dragging(),"Existing thumb hit target");
    fader.mouseDrag(mouse(fader,p.translated(0,-5)));require(revision()==rev,"Drag preview entered history");
    fader.mouseUp(mouse(fader,p.translated(0,-5)));
    require(revision()==rev+1&&gainValue()!=original,"One gain gesture must commit once");
    require(d.app.undo(),"Undo gain");d.refresh();require(gainValue()==original,"Undo restored exact gain");
    require(d.app.redo(),"Redo gain");d.refresh();
    strip=d.mixer.front().get();const auto cancelGain=gainValue();const auto cancelRevision=revision();
    const auto cp=strip->gain.knob().getCentre();strip->gain.mouseDown(mouse(strip->gain,cp));
    strip->gain.mouseDrag(mouse(strip->gain,cp.translated(0,8),true));strip->gain.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));strip->gain.mouseUp(mouse(strip->gain,cp));
    require(revision()==cancelRevision&&gainValue()==cancelGain,"Escape restores preview without history");
    strip->gain.keyPressed(juce::KeyPress(juce::KeyPress::downKey));require(gainValue()!=cancelGain,"Arrow key updates host");
    const auto mute=d.project()->tracks.front().mix.mute;strip->mute.onClick();d.refresh();
    require(d.project()->tracks.front().mix.mute!=mute,"Mute uses actual model");
    d.mixer.front()->mute.onClick();d.refresh();
    const auto solo=d.project()->tracks.front().mix.solo;d.mixer.front()->solo.onClick();d.refresh();
    require(d.project()->tracks.front().mix.solo!=solo,"Solo uses actual model");
    d.mixer.front()->solo.onClick();d.refresh();
    const auto armed=d.app.track_armed(id);d.mixer.front()->arm.onClick();d.refresh();require(d.app.track_armed(id)!=armed,"Arm uses existing host state");
    d.mixer.front()->arm.onClick();d.refresh();
    require(d.mixer.front()->arm.getToggleState()==d.app.track_armed(id)&&d.arrangement->rows.front()->arm.getToggleState()==d.app.track_armed(id),"Arm state agrees across Arrange and Mixer without a model revision");
    // Faders/meters render supplied live values. State-sample SVGs are never used.
    d.mixer.front()->gain.commit(60./72.);d.refresh(true);
    require(std::abs(gainValue()-1.f)<.00001f,"Unity fader retains actual host gain");
    d.peaks[0]={0,0};const auto noSignal=d.mixer.front()->createComponentSnapshot(d.mixer.front()->getLocalBounds());
    d.peaks[0]={.18f,.6f};d.masterPeak={.8f,.9f};
    require(!same(noSignal,d.mixer.front()->createComponentSnapshot(d.mixer.front()->getLocalBounds())),"Meter renders current fixture values");
    d.mixer.front()->gain.grabKeyboardFocus();saveImage(d.createComponentSnapshot(d.getLocalBounds()),"runtime-live-meter-focus");
    d.refresh();
    d.setWorkspace(mrs::desktop::Workspace::arrange);
    const auto beforeSlide=gainValue();d.arrangement->rows.front()->gain.keyPressed(juce::KeyPress(juce::KeyPress::upKey));
    require(gainValue()!=beforeSlide,"Arrange Slide controls same host gain");
    d.app.add_audio_track("SVG rebuild fixture");d.refresh();
    require(&d.mixer.back()->getLookAndFeel()==d.assetLookAndFeel(),"Rebuilt strips retain Theme");
    // P2: actual stereo data and MIDI notes, not decorative waveform art.
    d.app.new_project();
    const auto wav=juce::File::getCurrentWorkingDirectory().getChildFile("p2-stereo-fixture.wav");
    {
        juce::FileOutputStream stream(wav);stream.setPosition(0);stream.truncate();
        constexpr int frames=192000,bytes=frames*4;
        stream.write("RIFF",4);stream.writeInt(36+bytes);stream.write("WAVEfmt ",8);stream.writeInt(16);
        stream.writeShort(1);stream.writeShort(2);stream.writeInt(48000);stream.writeInt(192000);stream.writeShort(4);stream.writeShort(16);stream.write("data",4);stream.writeInt(bytes);
        for(int i=0;i<frames;++i){const auto envelope=.2+.7*std::abs(std::sin(i*.00004));stream.writeShort(static_cast<short>(22000*envelope*std::sin(i*.047)));stream.writeShort(static_cast<short>(14000*envelope*std::sin(i*.031)));}
    }
    d.app.import_wavs({std::filesystem::path(wav.getFullPathName().toWideCharPointer())});
    const auto audio=d.project()->clips.front().track;
    d.app.rename_track(audio,"Audio — long stereo track name / Длинная дорожка");
    const auto keys=d.app.add_instrument_track("Keys — длинное имя инструмента");
    const auto midi=d.app.create_midi_clip(keys,2*mrs::ppq,8*mrs::ppq);
    d.app.set_midi_notes(midi,{{mrs::new_id(),0,mrs::ppq,60,90,0},{mrs::new_id(),mrs::ppq,2*mrs::ppq,67,100,0}});
    d.app.add_bus("BUS — Group");d.app.add_fx_channel("FX — Return");
    const auto source=d.project()->clips.front().source;
    for(int i=0;i<1000&&!d.app.waveform(source);++i){d.app.poll();juce::Thread::sleep(1);}
    require(d.app.waveform(source)&&d.app.waveform(source)->channels()==2,"P2 real stereo waveform ready");
    d.selectedTrack=audio;d.selectedClip=midi;d.setWorkspace(mrs::desktop::Workspace::arrange);d.refresh(true);
    d.arrangement->pixelsPerSecond=120;d.arrangement->horizontal=0;d.arrangement->vertical=0;
    const auto projectBefore=*d.project();const auto visualRevision=revision();
    for(const int height:{40,56,80,100,128,200}){
        d.setSize(1400,850);d.arrangement->trackHeight=height;d.arrangement->resized();
        for(const auto& row:d.arrangement->rows){
            require(!row->nameBounds().intersects(row->mute.getBounds()),"P2 title vs channel actions");
            require(row->gain.isVisible()&&row->getLocalBounds().contains(row->gain.getBounds()),"Compact gain stays visible inside header");if(row->pan.isVisible())require(row->getLocalBounds().contains(row->pan.getBounds()),"Visible pan inside header");
            if(row->pan.isVisible())require(!row->gain.getBounds().intersects(row->pan.getBounds()),"Visible gain/pan do not overlap");
            if(row->input.isVisible())require(!row->input.getBounds().intersects(row->gain.getBounds())&&!row->input.getBounds().intersects(row->pan.getBounds()),"P2 input does not overlap controls");
        }
        for(const auto scale:{1.f,1.25f,1.5f,2.f})saveImage(d.createComponentSnapshot(d.getLocalBounds(),true,scale),"p2-height-"+juce::String(height)+"-"+juce::String(juce::roundToInt(scale*100)));
    }
    d.arrangement->trackHeight=128;d.arrangement->resized();
    for(const auto size:std::array<juce::Point<int>,3>{{{1200,700},{1400,850},{1800,1000}}})for(const auto scale:{1.f,1.25f,1.5f,2.f}){
        d.setSize(size.x,size.y);saveImage(d.createComponentSnapshot(d.getLocalBounds(),true,scale),"p2-mixed-"+juce::String(size.x)+"x"+juce::String(size.y)+"-"+juce::String(juce::roundToInt(scale*100)));
    }
    require(revision()==visualRevision&&*d.project()==projectBefore,"P2 density and drawing never modify project/history");
    const auto rect=d.arrangement->clipRect(d.project()->clips.front());
    d.arrangement->mouseMove(mouse(*d.arrangement,rect.getCentre()));
    require(d.arrangement->getTooltip().contains("Audio"),"P2 full clip name/type tooltip");
    d.setSize(1400,850);d.arrangement->trackHeight=56;d.arrangement->resized();
    d.arrangement->rows.front()->gain.keyPressed(juce::KeyPress(juce::KeyPress::downKey));
    require(d.app.undo()&&*d.project()==projectBefore,"P2 compact gain Undo restores exact project");
    d.refresh();d.arrangement->trackHeight=128;d.arrangement->resized();
    const auto file=juce::File::getCurrentWorkingDirectory().getChildFile("runtime-fixture-"+juce::String(mrs::new_id().value)+".mrsproject");
    d.app.save_project(std::filesystem::path(file.getFullPathName().toWideCharPointer()));
    // First Save owns WAV media and canonicalizes runtime keys to Media refs.
    const auto saved=d.app.snapshot().project;
    d.app.open_project(std::filesystem::path(file.getFullPathName().toWideCharPointer()));d.refresh();
    require(*d.project()==saved,"Project round-trip preserves canonical media, notes, colors and mix");
    // P3 routing labels follow the real model; Undo restores exact routing.
    const auto routeTrack=d.project()->tracks.front().id;
    d.app.set_hardware_output(routeTrack,{0,1});d.refresh();
    require(d.mixer.front()->output.getButtonText()=="Out: HW 1,2","P3 track hardware channels shown one-based");
    require(d.mixer.front()->output.getTitle().contains("HW 1,2"),"P3 accessible routing description");
    require(d.app.undo(),"P3 hardware route Undo");d.refresh();
    d.app.set_hardware_output({},{2,3});d.refresh();
    require(d.master->output.getButtonText()=="Out: HW 3,4","P3 Master reflects actual hardware mapping");
    require(d.app.undo(),"P3 Master route Undo");d.refresh();
    require(*d.project()==saved,"P3 routing display retains exact model after Undo");
    mrs::NativeInsert chromeFixture;chromeFixture.id=mrs::new_id();chromeFixture.kind=mrs::InsertKind::gain;
    d.app.set_inserts(routeTrack,{chromeFixture});d.refresh(true);
    pluginChromeSmoke(d,routeTrack,chromeFixture.id);
    require(d.app.undo(),"P3 remove chrome fixture through Undo");d.refresh(true);
    require(*d.project()==saved,"P3 host chrome fixture preserves project");
    d.setWorkspace(mrs::desktop::Workspace::mix);d.setSize(1400,850);
    for(const auto scale:{1.f,1.5f})saveImage(d.createComponentSnapshot(d.getLocalBounds(),true,scale),"p3-mixer-"+juce::String(juce::roundToInt(scale*100)));
    browserDialogSmoke(d);
    for(int i=0;i<8;++i){d.setAssetDesign(false);d.setAssetDesign(true);}
    require(d.assetSkin()!=nullptr,"Repeated attach/detach");
    d.setAssetDesign(false);require(d.assetSkin()==nullptr,"Theme detached before teardown");
    juce::File::getCurrentWorkingDirectory().getChildFile("runtime-results.txt").replaceWithText(
        "PASS: actual Desktop asset integration, existing gain drag/one commit/Undo/Redo/Escape/arrows, M/S/arm, Arrange gain, rebuild, project round-trip, scoped Theme lifetime.\nSnapshots="+juce::String(snapshots)+" + live meter fixture.\nHardware ASIO/listening/xrun/mixed DPI NOT TESTED.\n");
}
}
