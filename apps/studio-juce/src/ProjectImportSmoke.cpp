#include "Desktop.h"
#include "ProjectHome.h"
#include <algorithm>
#include <bit>
#include <mrs/persistence.hpp>
#include <mrs/audio.hpp>
#include <stdexcept>

namespace ui {
namespace {
using namespace mrs::project_import;
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
void archive(const juce::File& file,const std::vector<std::pair<std::string,std::string>>& files,int compression=6){
    juce::ZipFile::Builder zip;
    for(const auto& [name,bytes]:files)
        zip.addEntry(std::make_unique<juce::MemoryInputStream>(bytes.data(),bytes.size(),false),compression,juce::String(name),juce::Time::getCurrentTime());
    juce::FileOutputStream out(file);
    check(out.openedOk()&&zip.writeToStream(out,nullptr),"Fixture ZIP could not be written");
}
Preview read(const juce::File& file){return MRSProjectImport::preview(std::filesystem::path(file.getFullPathName().toWideCharPointer()));}
std::string wave(){
    juce::MemoryOutputStream bytes;
    // Studio Pro-compatible odd-length terminal PCM24 chunk, without a pad byte.
    constexpr int frames=44101, data=frames*3;
    bytes.write("RIFF",4);bytes.writeInt(36+data);bytes.write("WAVEfmt ",8);
    bytes.writeInt(16);bytes.writeShort(1);bytes.writeShort(1);bytes.writeInt(44100);bytes.writeInt(132300);bytes.writeShort(3);bytes.writeShort(24);
    bytes.write("data",4);bytes.writeInt(data);for(int i=0;i<frames;++i)bytes.write("\x00\x00\x10",3);
    return {static_cast<const char*>(bytes.getData()),bytes.getDataSize()};
}
std::filesystem::path diskPath(const juce::File& f){return std::filesystem::path(f.getFullPathName().toWideCharPointer());}
class ImportTestDevice final:public mrs::audio::IAudioDevice {
    using Phase=mrs::audio::DevicePhase;
    Phase phase=Phase::closed;
public:
    std::vector<mrs::audio::DeviceInfo> enumerate() override{return {{0,"Manual import render",{}, {"L","R"},32,2048,128,-1}};}
    void control_panel(int) override{}
    void open(const mrs::audio::DeviceConfig&,std::shared_ptr<mrs::audio::AudioEngine>) override{phase=Phase::open;}
    void start() override{phase=Phase::running;}
    void stop() override{phase=Phase::stopped;}
    void close() noexcept override{phase=Phase::closed;}
    mrs::audio::DeviceStatus status() override{return {phase,48000,0,0,0,{}};}
};
constexpr auto material=R"xml(<Project version="1.0"><Application name="Synthetic" version="1"/>
<Transport><Tempo id="tempo" unit="bpm" value="120"/><TimeSignature id="meter" numerator="4" denominator="4"/></Transport>
<Structure><Track id="folder" contentType="tracks" name="Band"><Track id="sub" contentType="tracks" name="Sub">
<Track id="sound" contentType="audio" name="Audio" color="#112233"/><Track id="keys" contentType="notes" name="Keys"/>
</Track></Track></Structure>
<Arrangement><Lanes timeUnit="beats">
<Lanes track="sound"><Clips><Clip time="2" duration="1" contentTimeUnit="seconds" playStart="0.125"><Audio duration="1" sampleRate="44100" channels="1"><File path="audio/embedded.wav"/></Audio></Clip>
<Clip time="5" duration="0.5" contentTimeUnit="seconds"><Audio duration="1" sampleRate="44100" channels="1"><File path="media/exact.wav" external="true"/></Audio></Clip></Clips></Lanes>
<Lanes track="keys"><Clips><Clip time="4" duration="2"><Lanes><Notes><Note time="0" duration="1" key="60" vel="0.8" channel="2"/></Notes>
<Points unit="normalized"><Target expression="channelController" channel="2" controller="11"/><RealPoint time="0.5" value="0.5"/></Points></Lanes></Clip></Clips></Lanes>
<Points unit="bpm"><Target parameter="tempo"/><RealPoint time="4" value="100" interpolation="hold"/></Points>
<Points><Target parameter="meter"/><TimeSignaturePoint time="4" numerator="3" denominator="4"/></Points>
</Lanes><Markers timeUnit="seconds"><Marker time="2.6" name="Section cue"/></Markers></Arrangement></Project>)xml";
constexpr auto minimal=R"xml(<Project version="1.0"><Application name="Synthetic" version="1"/></Project>)xml";
constexpr auto realistic=R"xml(<?xml version="1.0" encoding="UTF-8"?>
<Project version="1.0">
 <Application name="Synthetic multitrack" version="1"/>
 <Transport><Tempo id="tempo" unit="bpm" value="120"/><TimeSignature id="meter" numerator="4" denominator="4"/></Transport>
 <Structure>
  <Track id="folder" name="Band" contentType="tracks">
   <Track id="keys" name="Keys" contentType="notes">
    <Channel id="keysChannel" role="regular" audioChannels="2" destination="master">
     <Devices><Vst3Plugin id="plugin" deviceID="00112233445566778899AABBCCDDEEFF" deviceName="Synthetic instrument" deviceRole="instrument"><Enabled value="true"/><State path="plugins/opaque.vstpreset"/></Vst3Plugin></Devices>
     <Pan value="0.5" unit="normalized"/><Sends><Send id="send" destination="master" type="post"><Volume value="0.25" unit="linear"/></Send></Sends><Volume value="1" unit="linear"/>
    </Channel>
   </Track>
   <Track id="audio" name="Drum" contentType="audio"/>
  </Track>
  <Channel id="master" name="Master" role="master"/>
 </Structure>
 <Arrangement>
  <Lanes timeUnit="beats">
   <Lanes track="keys"><Clips><Clip name="Phrase" time="0" duration="4"><Notes><Note time="0.25" duration="1" channel="0" key="60" vel="0.8"/></Notes></Clip></Clips></Lanes>
   <Lanes track="audio" timeUnit="seconds"><Clips><Clip name="Drum" time="1.5" duration="2" playStart="0.125"><Audio duration="3" channels="2" sampleRate="48000"><File path="audio/drum.wav"/></Audio></Clip><Clip name="Missing" time="4" duration="1"><Audio duration="1" channels="1" sampleRate="44100"><File path="audio/missing.wav"/></Audio></Clip><Clip name="External" time="5" duration="1"><Audio duration="1" channels="1" sampleRate="44100"><File external="true" path="C:\External\audio.wav"/></Audio></Clip></Clips></Lanes>
   <Points unit="bpm"><Target parameter="tempo"/><RealPoint time="4" value="135" interpolation="hold"/></Points>
  </Lanes>
  <Markers timeUnit="beats"><Marker name="Verse" time="4"/></Markers>
 </Arrangement>
 <VendorExtension/>
</Project>)xml";
}

void dawprojectPreviewSmoke(const juce::File& screenshot){
    const auto folder=screenshot.getParentDirectory().getNonexistentChildFile("fixtures","",false);
    if(const auto created=folder.createDirectory();created.failed())
        throw std::runtime_error("Fixture directory could not be created: "+created.getErrorMessage().toStdString());
    struct Cleanup{juce::File folder;~Cleanup(){folder.deleteRecursively();}} cleanup{folder};
    // Synthetic format-9 fixture: no user project or vendor content.
    const auto song=folder.getChildFile("structure.song");
    archive(song, {
        {"metainfo.xml", R"xml(<MetaInformation><Attribute id="Document:FormatVersion" value="9"/><Attribute id="Document:Title" value="Synthetic structure"/><Attribute id="Document:Generator" value="Studio Pro/8.2"/></MetaInformation>)xml"},
        {"Song/song.xml", R"xml(<Song><Attributes x:id="Root"><Attributes x:id="timeContext" sampleRate="48000">
            <TempoMap x:id="tempoMap"><TempoMapSegment start="0" tempo="0.5" curveType="0"/></TempoMap>
            <TimeSignatureMap x:id="timeSignatureMap"><TimeSignatureMapSegment start="0" numerator="4" denominator="4"/></TimeSignatureMap>
            </Attributes><List x:id="Tracks">
            <MarkerTrack><MarkerEvent start="16" timeFormat="2" markerType="3" name="End"/></MarkerTrack>
            <ArrangerTrack><ArrangerEvent start="16" length="8" timeFormat="2" name="Tail" color="FF112233"/><ArrangerEvent start="0" length="16" timeFormat="2" name="Intro"/></ArrangerTrack>
            <ChordTrack><ChordEvent start="4" length="2" timeFormat="2"><Attributes x:id="chord" root="0" intervals="1 0 0 0 3 0 0 5 0 0 0 7" type="0"/></ChordEvent></ChordTrack>
            </List></Attributes></Song>)xml"},
        {"Devices/transportdevice.xml", R"xml(<TransportDevice position="3.5" loopStart="2" loopEnd="4" loopActive="1"/>)xml"}
    });
    const auto musical=StudioOneSongReader::read(diskPath(song));
    check(musical.sourceFormat=="StudioOneSong"&&musical.sourceVersion=="9"&&musical.title=="Synthetic structure","Song metadata differs");
    const auto countKind=[&](Kind kind){return std::count_if(musical.objects.begin(),musical.objects.end(),[&](const Object& o){return o.kind==kind;});};
    check(countKind(Kind::section)==2&&countKind(Kind::chord)==1&&countKind(Kind::marker)==1&&countKind(Kind::point)==2,"Song musical event count differs");
    const auto tail=std::find_if(musical.objects.begin(),musical.objects.end(),[](const Object& o){return o.kind==Kind::section;});
    check(tail!=musical.objects.end()&&tail->name=="Tail"&&tail->time==16.0&&tail->duration==8.0&&tail->timeUnit==TimeUnit::beats,"Song section at End was lost or reordered");
    const auto chord=std::find_if(musical.objects.begin(),musical.objects.end(),[](const Object& o){return o.kind==Kind::chord;});
    check(chord!=musical.objects.end()&&!chord->name.empty()&&chord->time==4.0&&chord->duration==2.0,"Song chord decoding differs");
    const auto transport=std::find_if(musical.objects.begin(),musical.objects.end(),[](const Object& o){return o.type=="TransportDevice";});
    check(transport!=musical.objects.end()&&transport->time==3.5&&transport->timeUnit==TimeUnit::seconds,"Song transport position differs");
    const auto invalidSong=folder.getChildFile("invalid.song");
    archive(invalidSong,{{"Song/song.xml","<Song/>"},{"metainfo.xml","<MetaInformation><Attribute id=\"Document:FormatVersion\" value=\"99\"/></MetaInformation>"}});
    bool rejected=false;
    try { (void)StudioOneSongReader::read(diskPath(invalidSong)); } catch (const std::exception&) { rejected=true; }
    check(rejected,"Unverified Song format was accepted");
    const auto small=folder.getChildFile("minimal.dawproject");
    archive(small,{{"project.xml",minimal}},0);
    const auto tiny=read(small);
    check(tiny.session&&tiny.session->title=="minimal"&&tiny.projects==1&&tiny.tracks==0&&tiny.clips==0&&tiny.plugins==0,"Minimal project preview differs");
    const auto full=folder.getChildFile("multitrack.dawproject");
    archive(full,{{"project.xml",realistic},{"metadata.xml","<MetaData><Title>Synthetic &amp; portable</Title></MetaData>"},{"audio/drum.wav","opaque fake audio bytes"},{"plugins/opaque.vstpreset","opaque fake state bytes"}});
    const auto p=read(full), again=read(full);
    check(p.session&&again.session&&p.tracks==3&&p.clips==4&&p.plugins==1&&p.devices==1,"Multitrack project counts differ");
    check(p.session->title=="Synthetic & portable"&&p.session->objects.size()==again.session->objects.size(),"Metadata or deterministic count differs");
    const auto& objects=p.session->objects;
    for(std::size_t i=0;i<objects.size();++i){
        const auto& a=objects[i];const auto& b=again.session->objects[i];
        check(a.kind==b.kind&&a.id==b.id&&a.parent==b.parent&&a.properties==b.properties&&a.detail==b.detail,"Preview object order or values changed");
        check(!a.parent||*a.parent<i,"Object parent is not an earlier source row");
    }
    const auto find=[&](std::string_view name){const auto it=std::find_if(objects.begin(),objects.end(),[&](const Object& o){return o.name==name;});check(it!=objects.end(),"Named preview object missing");return &*it;};
    const auto* phrase=find("Phrase");
    const auto* audio=find("Drum");
    check(phrase->trackScope&&objects[*phrase->trackScope].trackId=="keys"&&phrase->trackId.empty()&&phrase->timeUnit==TimeUnit::beats&&phrase->time==0&&audio->id=="audio","Track linkage or timeline units differ");
    check(find("Missing")->support==Support::partial&&find("External")->support==Support::partial,"Media limitation did not reach clip preview");
    using Loc=AssetReference::Location;
    const auto& assets=p.session->assets;
    check(assets.size()==4&&assets[0].pluginState&&assets[0].location==Loc::embedded&&assets[1].location==Loc::embedded&&assets[2].location==Loc::missing&&assets[3].location==Loc::externalUnchecked,"Asset reference states differ");
    check(p.session->warnings.size()==1&&p.session->warnings.front().find("VendorExtension")!=std::string::npos,"Unknown section not reported");

    const auto reject=[&](const std::string& title,const std::vector<std::pair<std::string,std::string>>& members,std::string_view reason){
        const auto file=folder.getChildFile(juce::String(title)+".dawproject");
        archive(file,members);
        const auto failure=read(file);
        if(failure.session||failure.error.find(reason)==std::string::npos)throw std::runtime_error(title+": rejected with unexpected result: "+failure.error);
        check(file.deleteFile(),"Invalid fixture archive handle was not released");
    };
    reject("missing-project",{{"metadata.xml","<MetaData/>"}},"missing project.xml");
    reject("malformed-xml",{{"project.xml","<Project version='1.0'><Application></Project>"}},"Malformed XML");
    reject("duplicates",{{"project.xml",minimal},{"PROJECT.XML",minimal}},"Duplicate ZIP");
    reject("unsafe-member-name",{{"project.xml",minimal},{"../outside.wav","not accessed"}},"Unsafe ZIP");
    reject("unsafe-reference",{{"project.xml","<Project version='1.0'><Application name='T' version='1'/><Future><File path='../outside.wav' external='true'/></Future></Project>"}},"Unsafe ZIP");
    reject("dtd-blocked",{{"project.xml",std::string("<!DOCTYPE Project SYSTEM 'file:///unused.dtd'>")+minimal}},"prohibited DTD");
    reject("unknown-version",{{"project.xml","<Project version='2.0'><Application name='T' version='1'/></Project>"}},"found 2.0");
    reject("unrecognized-exporter",{{"project.xml","<Project version='0.1'><Application name='Other' version='1'/></Project>"}},"found 0.1");
    const std::string withoutChannel="<Arrangement><Lanes><Clips><Clip time='0' duration='2'><Notes><Note time='0' duration='1' key='60' vel='0.75'/></Notes></Clip></Clips></Lanes></Arrangement></Project>";
    const std::string studioPro="<Application name='Studio Pro' version='8.2.1.115414'/>";
    reject("missing-channel-standard",{{"project.xml","<Project version='1.0'>"+studioPro+withoutChannel}},"Invalid DAWproject note channel");
    const auto legacyFile=folder.getChildFile("studio-pro-01.dawproject");
    archive(legacyFile,{{"project.xml","<Project version='0.1'>"+studioPro+withoutChannel}});
    const auto legacy=read(legacyFile);
    check(legacy.session&&legacy.session->sourceVersion=="0.1"&&legacy.clips==1&&!legacy.session->warnings.empty(),"Studio Pro 0.1 preview not accepted");
    const auto note=std::find_if(legacy.session->objects.begin(),legacy.session->objects.end(),[](const auto& o){return o.kind==Kind::note;});
    check(note!=legacy.session->objects.end()&&note->support==Support::partial&&std::none_of(note->properties.begin(),note->properties.end(),[](const auto& attr){return attr.first=="channel";}),"Legacy MIDI channel was fabricated or silently ignored");
    std::string deep;
    for(int i=0;i<65;++i)deep+="<Node>";
    for(int i=0;i<65;++i)deep+="</Node>";
    reject("too-deep",{{"project.xml",deep}},"depth limit");
    reject("too-large",{{"project.xml",std::string(9*1024*1024,' ')}},"expansion limit");
    reject("duplicate-id",{{"project.xml","<Project version='1.0'><Application name='T' version='1'/><Structure><Track id='x'/><Track id='x'/></Structure></Project>"}},"Duplicate DAWproject source ID");
    const auto broken=folder.getChildFile("broken.dawproject");
    check(broken.replaceWithText("Not a valid archive"),"Could not write malformed ZIP fixture");
    check(!read(broken).session,"Non-ZIP input was accepted");

    Desktop desktop(true,true);
    desktop.setSize(1400,850);
    const auto hasImport=[](juce::Component& component,const auto& self)->bool{
        if(component.getComponentID()=="home-import-project")return component.isVisible();
        for(auto* child:component.getChildren())if(self(*child,self))return true;
        return false;
    };
    check(hasImport(desktop,hasImport),"Import button not visible on Studio Home");
    auto menu=desktop.projectImportMenu();
    auto iterator=juce::PopupMenu::MenuItemIterator(menu);
    check(iterator.next()&&iterator.getItem().itemID==Desktop::previewDawprojectAction&&iterator.getItem().isEnabled,"DAWproject option not enabled");
    check(iterator.next()&&iterator.getItem().itemID==Desktop::previewSongAction&&iterator.getItem().isEnabled,"Song structure option not enabled");
    desktop.app.add_audio_track("Unsaved track to preserve");
    const auto store=desktop.app.services().projects;
    const auto original=store->state();
    const auto originalPath=desktop.app.path();
    const auto unchanged=[&]{
        const auto current=store->state();
        check(desktop.app.services().projects==store&&current.project==original.project&&current.revision==original.revision
              &&current.can_undo==original.can_undo&&desktop.app.path()==originalPath,"Preview mutated the current project");
    };
    const auto show=[&](const juce::File& file){
        desktop.startProjectImportPreview(file);
        desktop.projectImportPreview.wait();
        desktop.pollProjectImportPreview();
        auto* alert=desktop.projectImportDialog.getComponent();
        check(alert&&alert->getComponentID()=="dawproject-preview","Preview modal did not open");
        unchanged();
        return alert;
    };
    const auto dismiss=[&](juce::AlertWindow& alert){
        auto* button=alert.getButton("Close");
        check(button&&button->onClick,"Preview close action missing");
        button->onClick();
        check(!alert.isCurrentlyModal(),"Close did not dismiss import preview");
        delete &alert; // Process disposal without waiting for the next message-loop tick.
        unchanged();
    };
    auto* songAlert=show(song);
    check(songAlert->getButton("Create Project")!=nullptr,"Song preview Create button missing");
    dismiss(*songAlert);
    const auto songCreated=createProject({diskPath(song),diskPath(folder.getChildFile("SongProjects")),{},false});
    if(songCreated.projectFile.empty())throw std::runtime_error("Song structure creation failed: "+songCreated.error);
    const auto songDoc=mrs::persistence::load_project(songCreated.projectFile);
    const auto& sp=songDoc.project;
    check(sp.sample_rate==48000&&sp.time.tempos.size()==1&&sp.time.tempos[0].bpm==120&&sp.time.meters[0].numerator==4&&sp.time.meters[0].denominator==4,"Song native timing differs");
    check(sp.sections.size()==2&&sp.sections[0].name=="Intro"&&sp.sections[0].start==0&&sp.sections[0].end==16*mrs::ppq&&sp.sections[1].name=="Tail"&&sp.sections[1].start==16*mrs::ppq&&sp.sections[1].end==24*mrs::ppq&&sp.sections[1].color==0x112233u,"Song native section geometry or color differs");
    check(sp.chords.size()==1&&sp.chords[0].symbol==chord->name&&sp.chords[0].start==4*mrs::ppq&&sp.chords[0].end==6*mrs::ppq,"Song native chord differs");
    check(sp.markers.size()==1&&sp.markers[0].name=="End"&&sp.markers[0].tick==16*mrs::ppq&&sp.tracks.empty()&&sp.clips.empty(),"Song native marker or structure-only contract differs");
    check(!songDoc.extensions.empty()&&songDoc.extensions[0].tag=="SONG","Song source provenance missing");
    mrs::desktop::Application songReopened;
    songReopened.open_project(songCreated.projectFile);
    check(*songReopened.services().projects->state().project==sp,"Song native open changed structure");
    songReopened.save_project(songCreated.projectFile);
    const auto songResaved=mrs::persistence::load_project(songCreated.projectFile);
    check(songResaved.project==sp&&songResaved.extensions==songDoc.extensions,"Song Save/Reopen lost structure or source metadata");
    unchanged();
    // Two source time formats, shared WAV identity, static mix and non-destructive speed.
    const auto songAudioSource=folder.getChildFile("audio.song");
    // A native IEEE float64 bounce, same rate, no conversion of original bytes.
    juce::MemoryOutputStream songFloat;
    constexpr int songFrames=48000;
    songFloat.write("RIFF",4);songFloat.writeInt(36+songFrames*8);songFloat.write("WAVEfmt ",8);
    songFloat.writeInt(16);songFloat.writeShort(3);songFloat.writeShort(1);songFloat.writeInt(48000);
    songFloat.writeInt(48000*8);songFloat.writeShort(8);songFloat.writeShort(64);
    songFloat.write("data",4);songFloat.writeInt(songFrames*8);
    for(int i=0;i<songFrames;++i)songFloat.writeInt64(std::bit_cast<std::int64_t>(0.125));
    const std::string songWav(static_cast<const char*>(songFloat.getData()),songFloat.getDataSize());
    check(folder.getChildFile("Media").createDirectory().wasOk(),"Song fixture Media directory");
    check(folder.getChildFile("Media/take.wav").replaceWithData(songWav.data(),songWav.size()),"Song fixture WAV");
    archive(songAudioSource,{
        {"metainfo.xml",R"xml(<MetaInformation><Attribute id="Document:FormatVersion" value="9"/></MetaInformation>)xml"},
        {"Song/song.xml",R"xml(<Song><Attributes x:id="Root"><Attributes x:id="timeContext" sampleRate="48000"><TempoMap><TempoMapSegment start="0" tempo="0.5" curveType="0"/></TempoMap><TimeSignatureMap><TimeSignatureMapSegment start="0" numerator="4" denominator="4"/></TimeSignatureMap></Attributes><List x:id="Tracks"><MediaTrack mediaType="Audio" trackID="t" name="Audio" color="FF123456"><UID x:id="channelID" uid="c"/><List x:id="Events"><AudioEvent clipID="m" start="2" timeFormat="0" length="0.25" offset="0.125" name="Seconds" speed="1"/><AudioEvent clipID="m" start="4" timeFormat="2" length="0.5" offset="0.25" name="Beats" speed="2"/></List></MediaTrack><MediaTrack mediaType="Audio" trackID="empty" name="Empty"/></List></Attributes></Song>)xml"},
        {"Song/mediapool.xml",R"xml(<MediaPool><Attributes x:id="rootFolder"><MediaFolder name="Audio"><AudioClip mediaID="m"><Url x:id="path" url="file:///C:/Example/audio/Media/take.wav"/><Attributes x:id="format" sampleRate="48000" numChannels="1" frameCount="48000" bitDepth="64" sampleType="1"/></AudioClip></MediaFolder></Attributes></MediaPool>)xml"},
        {"Devices/audiomixer.xml",R"xml(<AudioMixer><Attributes x:id="channels"><ChannelGroup><AudioTrackChannel gain="0.5" pan="0.75" mute="1" solo="0"><UID x:id="uniqueID" uid="c"/></AudioTrackChannel></ChannelGroup></Attributes></AudioMixer>)xml"}
    });
    const auto songAudioPreview=read(songAudioSource);
    check(songAudioPreview.session&&songAudioPreview.tracks==2&&songAudioPreview.clips==2,"Song audio preview count differs");
    const auto songAudioCreated=createProject({diskPath(songAudioSource),diskPath(folder.getChildFile("SongAudioProjects")),{},false});
    if(songAudioCreated.projectFile.empty())throw std::runtime_error("Song audio creation failed: "+songAudioCreated.error+(songAudioCreated.missing.empty()?"":"; "+songAudioCreated.missing.front().reason));
    const auto songAudioDoc=mrs::persistence::load_project(songAudioCreated.projectFile);
    const auto& ap=songAudioDoc.project;
    check(ap.tracks.size()==2&&ap.tracks[0].mix.gain==0.5f&&ap.tracks[0].mix.pan==0.5f&&ap.tracks[0].mix.mute&&ap.tracks[0].color==0x123456u,"Song audio track mix/color differs");
    check(ap.clips.size()==2&&ap.clips[0].start==48000&&ap.clips[0].length==12000&&ap.clips[0].source_offset==6000&&!ap.clips[0].stretch,"Song seconds clip geometry differs");
    check(ap.clips[1].start==96000&&ap.clips[1].length==12000&&ap.clips[1].source_offset==6000&&ap.clips[1].stretch&&ap.clips[1].stretch->parameters.duration_ratio==0.5&&ap.clips[1].stretch->source_frames==24000&&ap.clips[1].stretch->window_end==24000.0&&ap.clips[0].source==ap.clips[1].source,"Song beat clip / speed / shared source differs");
    const auto importedFloat= songAudioCreated.projectFile.parent_path()/ap.clips[0].source;
    check(mrs::audio::inspect_wav(importedFloat).bits==64,"Song float64 bounce was not retained");
    check(mrs::audio::load_wav(importedFloat).samples.front()==0.125f,"Song float64 bounce decoding differs");
    mrs::persistence::save_project(songAudioCreated.projectFile,songAudioDoc);
    check(mrs::persistence::load_project(songAudioCreated.projectFile)==songAudioDoc,"Song audio Save/Reopen differs");
    unchanged();
    auto* alert=show(full);
    check(alert->getNumCustomComponents()==1&&alert->getCustomComponent(0)->getComponentID()=="dawproject-preview-details"&&alert->getButton("Create Project"),"Preview compatibility list or Create button missing");
    if(screenshot!=juce::File()){
        const auto image=alert->createComponentSnapshot(alert->getLocalBounds());
        juce::MemoryOutputStream output;
        check(juce::PNGImageFormat{}.writeImageToStream(image,output)&&screenshot.replaceWithData(output.getData(),output.getDataSize()),"Could not write preview capture");
    }
    dismiss(*alert);
    alert=show(broken);
    check(alert->getNumCustomComponents()==0,"Malformed source should not show compatibility rows");
    dismiss(*alert);

    // Optional local verification; no private fixture is copied, embedded or used by default CTest.
    const auto wav=wave();
    const auto creationSource=folder.getChildFile("creation.dawproject");
    archive(creationSource,{{"project.xml",material},{"audio/embedded.wav",wav}});
    const auto expectedReplacement=folder.getChildFile("elsewhere.wav");
    check(expectedReplacement.replaceWithData(wav.data(),wav.size()),"Could not write replacement WAV fixture");
    const auto library=folder.getChildFile("Projects");
    CreateRequest request{diskPath(creationSource),diskPath(library),{},false};
    const auto incomplete=createProject(request);
    check(incomplete.projectFile.empty()&&incomplete.missing.size()==1&&incomplete.missing.front().reference=="media/exact.wav"&&!library.exists(),"Missing media wrote a partial project or did not offer relink");
    unchanged();
    std::stop_source cancelled;cancelled.request_stop();
    check(createProject(request,cancelled.get_token()).cancelled&&!library.exists(),"Cancelled import published a project");
    request.relinks["media/exact.wav"]=diskPath(expectedReplacement);
    const auto created=createProject(request);
    if(created.projectFile.empty())throw std::runtime_error("P2 creation failed: "+created.error+(created.missing.empty()?"":"; "+created.missing.front().reason));
    check(created.missing.empty()&&findHomeProjects(library).files.size()==1,"Imported project does not appear on Home");
    const auto doc=mrs::persistence::load_project(created.projectFile);
    check(doc.project.folders.size()==2&&doc.project.folders[1].parent==doc.project.folders[0].id&&doc.project.tracks.size()==2&&doc.project.tracks[0].folder==doc.project.folders[1].id&&doc.project.tracks[0].color==0x112233u,"Folder nesting, track order or track colour lost");
    check(doc.project.time.tempos.size()==2&&doc.project.time.tempos[1].tick==3840&&doc.project.time.tempos[1].bpm==100&&doc.project.time.meters.size()==2&&doc.project.time.meters[1].bar==2&&doc.project.time.meters[1].numerator==3&&doc.project.markers.size()==1&&doc.project.markers[0].tick==4800,"Tempo/meter/marker import differs");
    check(doc.project.clips.size()==3&&doc.project.clips.front().midi.has_value(),"Arrangement clips missing");
    const auto& midi=*doc.project.clips.front().midi;
    check(midi.start==3840&&midi.length==1920&&midi.notes.size()==1&&midi.notes[0].velocity==102&&midi.notes[0].channel==2&&midi.events.size()==1&&midi.events[0].start==480&&midi.events[0].data1==11&&midi.events[0].data2==64,"MIDI notes and CC did not round-trip");
    const auto& firstAudio=doc.project.clips[1];
    check(firstAudio.start==48000&&firstAudio.length==24000&&firstAudio.source_offset==6000,"Trimmed audio placement differs");
    const auto physical=mrs::audio::inspect_wav(created.projectFile.parent_path()/"Media/audio-1.wav");
    const auto playback=mrs::audio::inspect_wav(created.projectFile.parent_path()/firstAudio.source);
    check(physical.sample_rate==44100&&physical.channels==1&&physical.frame_count==44101&&playback.sample_rate==48000&&playback.channels==1,"Original WAV or playback geometry differs");
    check(!doc.extensions.empty()&&doc.extensions[0].tag=="DAWP","Source provenance did not persist");
    mrs::desktop::Application reopened;
    reopened.open_project(created.projectFile);
    check(*reopened.services().projects->state().project==doc.project,"Shared project open did not preserve arrangement");
    reopened.connect(std::make_unique<ImportTestDevice>(),{0,48000,128,{}, {0,1}});
    std::array<float,256> output{};
    reopened.seek(firstAudio.start);reopened.play();
    for(int block=0;block<3;++block)reopened.engine()->process(nullptr,output.data(),128);
    check(std::any_of(output.begin(),output.end(),[](float v){return std::abs(v)>.03f;}),"Imported audio was silent after reopen");
    reopened.stop();reopened.engine()->process(nullptr,output.data(),128);reopened.poll();reopened.disconnect();
    reopened.save_project(created.projectFile);
    const auto resaved=mrs::persistence::load_project(created.projectFile);
    check(resaved.project==doc.project&&resaved.extensions==doc.extensions,"Save/Open lost imported arrangement or provenance");
    unchanged();
    const auto duplicate=createProject(request);
    check(!duplicate.projectFile.empty()&&duplicate.projectFile!=created.projectFile&&findHomeProjects(library).files.size()==2,"Second import overwrote an existing native project");

    if(const auto source=juce::SystemStats::getEnvironmentVariable("MRS_DAWPROJECT_CHECK_FILE",{});source.isNotEmpty()){
        const auto real=juce::File(source);
        const auto preview=read(real);
        if(!preview.session)throw std::runtime_error("Local export preview failed: "+preview.error);
        if(const auto expected=juce::SystemStats::getEnvironmentVariable("MRS_DAWPROJECT_CHECK_COUNTS",{});expected.isNotEmpty()){
            const auto actual=juce::String(static_cast<int>(preview.tracks))+","+juce::String(static_cast<int>(preview.clips))+","+juce::String(static_cast<int>(preview.plugins));
            if(actual!=expected)throw std::runtime_error("Local export counts differ; tracks,clips,plugins: "+actual.toStdString());
        }
        alert=show(real);
        check(alert->getNumCustomComponents()==1,"Local export did not show compatibility rows");
        dismiss(*alert);
        if(juce::SystemStats::getEnvironmentVariable("MRS_DAWPROJECT_CHECK_CREATE",{})=="1"){
            const auto local=createProject({diskPath(real),diskPath(folder.getChildFile("Local")),{},false});
            if(local.projectFile.empty())throw std::runtime_error("Local export creation failed: "+local.error+(local.missing.empty()?"":"; "+local.missing.front().reason));
            const auto native=mrs::persistence::load_project(local.projectFile);
            const auto midiCount=std::count_if(native.project.clips.begin(),native.project.clips.end(),[](const auto& c){return c.midi.has_value();});
            check(midiCount>0&&static_cast<std::size_t>(midiCount)<native.project.clips.size(),"Local export did not produce both audio and MIDI");
            juce::File report=screenshot.getSiblingFile("dawproject-local-counts.txt");
            check(report.replaceWithText("Tracks: "+juce::String(static_cast<int>(native.project.tracks.size()))+"  Folders: "+juce::String(static_cast<int>(native.project.folders.size()))+"  Audio clips: "+juce::String(static_cast<int>(native.project.clips.size()-static_cast<std::size_t>(midiCount)))+"  MIDI clips: "+juce::String(static_cast<int>(midiCount))+"  Markers: "+juce::String(static_cast<int>(native.project.markers.size()))),"Could not write aggregate local verification counts");
        }
    }
}
} // namespace ui
