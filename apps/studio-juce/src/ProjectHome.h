#pragma once
#include "Desktop.h"
namespace ui {
struct HomeProject {juce::File file;juce::String relative;std::int64_t modified{};};
struct HomeProjects {std::vector<HomeProject> files;juce::String error;};
inline HomeProjects findHomeProjects(const juce::File& root,const std::atomic<bool>* cancel=nullptr){
    HomeProjects result;const std::filesystem::path base(root.getFullPathName().toWideCharPointer());std::error_code ec;
    if(!std::filesystem::exists(base,ec)){if(ec)result.error=juce::String(ec.message());return result;}
    std::filesystem::recursive_directory_iterator it(base,std::filesystem::directory_options::skip_permission_denied,ec),end;
    while(!ec&&it!=end){
        if(cancel&&cancel->load())return {};
        const auto entry=*it;std::error_code status;
        if(entry.is_symlink(status))it.disable_recursion_pending();
        else if(entry.is_regular_file(status)&&(juce::String(entry.path().extension().wstring().c_str()).equalsIgnoreCase(".mrsproject")||juce::String(entry.path().extension().wstring().c_str()).equalsIgnoreCase(".mrlive"))){
            juce::File f(juce::String(entry.path().wstring().c_str()));result.files.push_back({f,juce::String(entry.path().lexically_relative(base).wstring().c_str()),f.getLastModificationTime().toMilliseconds()});
        }
        it.increment(ec);
    }
    if(ec)result.error=juce::String(ec.message());
    std::sort(result.files.begin(),result.files.end(),[](const auto& a,const auto& b){if(a.modified!=b.modified)return a.modified>b.modified;return a.relative.compareIgnoreCase(b.relative)<0;});return result;
}
class ProjectHome final : public juce::Component,private juce::ListBoxModel,private juce::Timer {
public:
    explicit ProjectHome(Desktop& d):owner(d),root(juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("MR Studio")),list("MR Studio projects",this){
        setComponentID("project-home");setName("MR Studio projects");list.setComponentID("home-project-list");list.setRowHeight(64);list.setColour(juce::ListBox::backgroundColourId,juce::Colour(surface));
        for(auto* c:std::array<juce::Component*,9>{&list,&create,&open,&reload,&filter,&resume,&createLive,&studioTab,&liveTab})addAndMakeVisible(c);
        list.setMouseCursor(juce::MouseCursor::NormalCursor);resume.setComponentID("home-resume");resume.onClick=[this]{owner.showProjectHome(false);};
        create.setComponentID("home-new");open.setComponentID("home-open");reload.setComponentID("home-refresh");filter.setComponentID("home-search");filter.setTextToShowWhenEmpty("Search projects",juce::Colours::grey);filter.setTitle("Search project names and paths");
        createLive.setComponentID("home-new-live");createLive.onClick=[this]{owner.action(701);};
        studioTab.setComponentID("home-studio-tab");liveTab.setComponentID("home-live-tab");
        studioTab.onClick=[this]{live=false;applyFilter();};liveTab.onClick=[this]{live=true;applyFilter();};
        create.onClick=[this]{owner.action(1);};open.onClick=[this]{owner.action(2);};reload.onClick=[this]{rescan();};filter.onTextChange=[this]{applyFilter();};startTimerHz(10);
    }
    ~ProjectHome() override{stopTimer();cancel=true;if(worker.valid())worker.wait();list.setModel(nullptr);}
    void rescan(){if(worker.valid()){rescanPending=true;return;}cancel=false;const auto folder=root;worker=std::async(std::launch::async,[this,folder]{return findHomeProjects(folder,&cancel);});scanning=true;reload.setEnabled(false);repaint();}
    bool loading() const{return scanning;}
    void visibilityChanged() override{if(isVisible())resized();}
    void scanRoot(const juce::File& folder){root=folder;rescan();}
    void pollScan(){if(!worker.valid()||worker.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;auto result=worker.get();projects=std::move(result.files);error=result.error;scanning=false;reload.setEnabled(true);applyFilter();if(rescanPending){rescanPending=false;rescan();}}
    std::size_t projectCount() const{return visible.size();}
    const juce::File& projectFile(std::size_t i) const{return projects.at(visible.at(i)).file;}
    void openRow(int row){if(row<0||row>=static_cast<int>(visible.size()))return;const auto file=projectFile(static_cast<std::size_t>(row));juce::Component::SafePointer<Desktop> safe(&owner);juce::MessageManager::callAsync([safe,file]{if(safe)safe->confirmDiscard([safe,file]{if(safe)safe->run([&]{safe->openFile(file);});});});}
    void resized() override{const int width=juce::jmin(960,getWidth()-80),left=(getWidth()-width)/2;create.setBounds(left,142,150,32);open.setBounds(left+162,142,150,32);reload.setBounds(left+324,142,120,32);filter.setBounds(left+width-280,142,280,32);createLive.setBounds(left,180,230,32);studioTab.setBounds(left+246,180,120,32);liveTab.setBounds(left+374,180,120,32);resume.setBounds(left+width-190,88,190,32);resume.setVisible(owner.canReturnToProject());list.setBounds(left,250,width,juce::jmax(80,getHeight()-294));}
    void paint(juce::Graphics& g) override{
        g.fillAll(juce::Colour(surface));const int width=juce::jmin(960,getWidth()-80),left=(getWidth()-width)/2;
        g.setColour(juce::Colours::whitesmoke);g.setFont(owner.theme.font(28));g.drawText("Moon River Studio",left,40,width,48,juce::Justification::left);
        g.setFont(owner.theme.font(13));g.setColour(juce::Colours::lightgrey);g.drawText("Choose a project to continue",left,94,width,26,juce::Justification::left);

        g.setFont(owner.theme.font(11));g.setColour(juce::Colours::grey);g.drawText(root.getFullPathName(),left,getHeight()-66,width,24,juce::Justification::left);
        const auto warning=owner.message.isNotEmpty()?owner.message:error;g.setColour(juce::Colours::orange);g.drawText(warning,left,getHeight()-40,width,24,juce::Justification::left);
    }
    void paintOverChildren(juce::Graphics& g) override{if(visible.empty()){g.setFont(owner.theme.font(13));g.setColour(juce::Colours::grey);g.drawText(scanning?"Looking for projects...":filter.getText().isEmpty()?(live?"No .mrlive shows found. Create a show or open one from another location.":"No .mrsproject files found. Create a project or open one from another location."):"No matching projects",list.getBounds().reduced(16),juce::Justification::centred);}}
private:
    class NameButton final:public juce::TextButton {
    public:
        juce::Font font{juce::FontOptions(14.f)};
        NameButton(){setComponentID("home-project-name");setMouseCursor(juce::MouseCursor::PointingHandCursor);}
        void paint(juce::Graphics& g) override {
            g.setFont(font);g.setColour(getState()!=juce::Button::buttonNormal?juce::Colours::skyblue:juce::Colours::whitesmoke);
            g.drawText(getButtonText(),getLocalBounds(),juce::Justification::centredLeft,true);
            if(hasKeyboardFocus(true)){g.setColour(juce::Colours::skyblue);g.drawRect(getLocalBounds(),1);}
        }
    };
    class NameRow final:public juce::Component {
    public:
        NameButton name;
        NameRow(){setInterceptsMouseClicks(false,true);addAndMakeVisible(name);}
        void resized() override {name.setBounds(16,7,juce::jmax(0,juce::jmin(getWidth()-32,juce::GlyphArrangement::getStringWidthInt(name.font,name.getButtonText())+2)),24);}
    };
    juce::Component* refreshComponentForRow(int row,bool,juce::Component* existing) override {
        if(row<0||row>=getNumRows()){delete existing;return nullptr;}
        auto* item=dynamic_cast<NameRow*>(existing);if(!item){delete existing;item=new NameRow;}
        const auto& project=projects[visible[static_cast<std::size_t>(row)]];
        item->name.font=owner.theme.font(14);item->name.setButtonText(project.file.getFileName());item->name.setTitle("Open "+project.file.getFileName());item->name.setTooltip(project.relative);
        juce::Component::SafePointer<ProjectHome> safe(this);item->name.onClick=[safe,row]{if(safe)safe->openRow(row);};item->resized();return item;
    }
    int getNumRows() override{return static_cast<int>(visible.size());}
    juce::String getNameForRow(int row) override{return row>=0&&row<getNumRows()?projects[visible[static_cast<std::size_t>(row)]].relative:juce::String();}
    void paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool selected) override{if(row<0||row>=getNumRows())return;const auto& item=projects[visible[static_cast<std::size_t>(row)]];drawCard(g,{2.f,2.f,static_cast<float>(width-4),static_cast<float>(height-4)},juce::Colour(selected?accent:raised));g.setColour(juce::Colours::lightgrey);g.setFont(owner.theme.font(11));g.drawText(item.relative,16,33,width-32,22,juce::Justification::left);}
    void listBoxItemClicked(int row,const juce::MouseEvent& e) override{if(e.mods.isLeftButtonDown())openRow(row);}
    void returnKeyPressed(int row) override{openRow(row);}
    void applyFilter(){studioTab.setToggleState(!live,juce::dontSendNotification);liveTab.setToggleState(live,juce::dontSendNotification);list.deselectAllRows();visible.clear();for(std::size_t i=0;i<projects.size();++i)if(projects[i].file.hasFileExtension(live?"mrlive":"mrsproject")&&projects[i].relative.containsIgnoreCase(filter.getText()))visible.push_back(i);list.updateContent();repaint();}
    void timerCallback() override{try{pollScan();}catch(const std::exception& e){scanning=false;reload.setEnabled(true);error=juce::String::fromUTF8(e.what());repaint();}}
    Desktop& owner;juce::File root;juce::ListBox list;bool live{};juce::TextButton createLive{juce::String::fromUTF8("Новый проект MR Live")},studioTab{juce::String::fromUTF8("Проекты")},liveTab{"MR Live"};juce::TextButton create{"New project"},open{"Open..."},reload{"Refresh"},resume{"Continue current project"};juce::TextEditor filter;
    std::vector<HomeProject> projects;std::vector<std::size_t> visible;std::future<HomeProjects> worker;std::atomic<bool> cancel{};bool scanning{},rescanPending{};juce::String error;
};
}
