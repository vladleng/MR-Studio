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
        else if(entry.is_regular_file(status)&&juce::String(entry.path().extension().wstring().c_str()).equalsIgnoreCase(".mrsproject")){
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
        for(auto* c:std::array<juce::Component*,6>{&list,&create,&open,&reload,&filter,&resume})addAndMakeVisible(c);
        list.setMouseCursor(juce::MouseCursor::PointingHandCursor);resume.setComponentID("home-resume");resume.onClick=[this]{owner.showProjectHome(false);};
        create.setComponentID("home-new");open.setComponentID("home-open");reload.setComponentID("home-refresh");filter.setComponentID("home-search");filter.setTextToShowWhenEmpty("Search projects",juce::Colours::grey);filter.setTitle("Search project names and paths");
        create.onClick=[this]{owner.action(1);};open.onClick=[this]{owner.action(2);};reload.onClick=[this]{rescan();};filter.onTextChange=[this]{applyFilter();};startTimerHz(10);
    }
    ~ProjectHome() override{stopTimer();cancel=true;if(worker.valid())worker.wait();list.setModel(nullptr);}
    void rescan(){if(worker.valid())return;cancel=false;const auto folder=root;worker=std::async(std::launch::async,[this,folder]{return findHomeProjects(folder,&cancel);});scanning=true;reload.setEnabled(false);repaint();}
    bool loading() const{return scanning;}
    void visibilityChanged() override{if(isVisible())resized();}
    void scanRoot(const juce::File& folder){root=folder;rescan();}
    void pollScan(){if(!worker.valid()||worker.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;auto result=worker.get();projects=std::move(result.files);error=result.error;scanning=false;reload.setEnabled(true);applyFilter();}
    std::size_t projectCount() const{return visible.size();}
    const juce::File& projectFile(std::size_t i) const{return projects.at(visible.at(i)).file;}
    void openRow(int row){if(row<0||row>=static_cast<int>(visible.size()))return;const auto file=projectFile(static_cast<std::size_t>(row));juce::Component::SafePointer<Desktop> safe(&owner);juce::MessageManager::callAsync([safe,file]{if(safe)safe->confirmDiscard([safe,file]{if(safe)safe->run([&]{safe->openFile(file);});});});}
    void resized() override{const int width=juce::jmin(960,getWidth()-80),left=(getWidth()-width)/2;create.setBounds(left,142,150,32);open.setBounds(left+162,142,150,32);reload.setBounds(left+324,142,120,32);filter.setBounds(left+width-280,142,280,32);resume.setBounds(left+width-190,88,190,32);resume.setVisible(owner.canReturnToProject());list.setBounds(left,220,width,juce::jmax(80,getHeight()-294));}
    void paint(juce::Graphics& g) override{
        g.fillAll(juce::Colour(surface));const int width=juce::jmin(960,getWidth()-80),left=(getWidth()-width)/2;
        g.setColour(juce::Colours::whitesmoke);g.setFont(owner.theme.font(28));g.drawText("Moon River Studio",left,40,width,48,juce::Justification::left);
        g.setFont(owner.theme.font(13));g.setColour(juce::Colours::lightgrey);g.drawText("Choose a project to continue",left,94,width,26,juce::Justification::left);
        g.drawText("Projects  /  "+juce::String(static_cast<int>(visible.size())),left,188,width,26,juce::Justification::left);
        g.setFont(owner.theme.font(11));g.setColour(juce::Colours::grey);g.drawText(root.getFullPathName(),left,getHeight()-66,width,24,juce::Justification::left);
        const auto warning=owner.message.isNotEmpty()?owner.message:error;g.setColour(juce::Colours::orange);g.drawText(warning,left,getHeight()-40,width,24,juce::Justification::left);
    }
    void paintOverChildren(juce::Graphics& g) override{if(visible.empty()){g.setFont(owner.theme.font(13));g.setColour(juce::Colours::grey);g.drawText(scanning?"Looking for projects...":filter.getText().isEmpty()?"No .mrsproject files found. Create a project or open one from another location.":"No matching projects",list.getBounds().reduced(16),juce::Justification::centred);}}
private:
    int getNumRows() override{return static_cast<int>(visible.size());}
    juce::String getNameForRow(int row) override{return row>=0&&row<getNumRows()?projects[visible[static_cast<std::size_t>(row)]].relative:juce::String();}
    void paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool selected) override{if(row<0||row>=getNumRows())return;const auto& item=projects[visible[static_cast<std::size_t>(row)]];g.setColour(juce::Colour(selected?accent:panel));g.fillRoundedRectangle(2.f,2.f,static_cast<float>(width-4),static_cast<float>(height-4),3.f);g.setColour(juce::Colours::whitesmoke);g.setFont(owner.theme.font(14));g.drawText(item.file.getFileName(),16,7,width-32,24,juce::Justification::left);g.setColour(juce::Colours::lightgrey);g.setFont(owner.theme.font(11));g.drawText(item.relative,16,33,width-32,22,juce::Justification::left);}
    void listBoxItemClicked(int row,const juce::MouseEvent& e) override{if(e.mods.isLeftButtonDown())openRow(row);}
    void returnKeyPressed(int row) override{openRow(row);}
    void applyFilter(){list.deselectAllRows();visible.clear();for(std::size_t i=0;i<projects.size();++i)if(projects[i].relative.containsIgnoreCase(filter.getText()))visible.push_back(i);list.updateContent();repaint();}
    void timerCallback() override{try{pollScan();}catch(const std::exception& e){scanning=false;reload.setEnabled(true);error=juce::String::fromUTF8(e.what());repaint();}}
    Desktop& owner;juce::File root;juce::ListBox list;juce::TextButton create{"New project"},open{"Open..."},reload{"Refresh"},resume{"Continue current project"};juce::TextEditor filter;
    std::vector<HomeProject> projects;std::vector<std::size_t> visible;std::future<HomeProjects> worker;std::atomic<bool> cancel{};bool scanning{};juce::String error;
};
}
