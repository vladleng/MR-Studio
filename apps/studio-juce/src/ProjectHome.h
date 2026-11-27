#pragma once
#include "Desktop.h"
#include "HomeDesign.h"
#include <mrs/version.hpp>
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
        if(entry.path().filename().native().starts_with(L".mrs-dawproject-")||entry.is_symlink(status))it.disable_recursion_pending();
        else if(entry.is_regular_file(status)&&(juce::String(entry.path().extension().wstring().c_str()).equalsIgnoreCase(".mrsproject")||juce::String(entry.path().extension().wstring().c_str()).equalsIgnoreCase(".mrlive"))){
            juce::File f(juce::String(entry.path().wstring().c_str()));result.files.push_back({f,juce::String(entry.path().lexically_relative(base).wstring().c_str()),f.getLastModificationTime().toMilliseconds()});
        }
        it.increment(ec);
    }
    if(ec)result.error=juce::String(ec.message());
    std::sort(result.files.begin(),result.files.end(),[](const auto& a,const auto& b){if(a.modified!=b.modified)return a.modified>b.modified;return a.relative.compareIgnoreCase(b.relative)<0;});return result;
}


// Home action button: painted here so the screen does not depend on the shared
// LookAndFeel geometry. Still a juce::TextButton, so existing lookups keep working.
class HomeAction final : public juce::TextButton {
public:
    enum Kind { primary, ghost, segment };
    HomeAction(const juce::String& text, Kind style, int icon = 0)
        : juce::TextButton(text), kind(style), glyph(icon) {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus(true);
    }
    void paint(juce::Graphics& g) override {
        const auto b = getLocalBounds().toFloat();
        const bool on = getToggleState();
        const float radius = kind == segment ? 16.f : 6.f;
        auto fill = kind == primary ? HomeDesign::accent()
                  : kind == segment ? (on ? HomeDesign::raised() : juce::Colours::transparentBlack)
                                    : HomeDesign::surface();
        if ((kind == ghost || kind == segment) && isMouseOver(true)) fill = kind == segment ? HomeDesign::surface() : HomeDesign::hover();
        if (kind == primary && isMouseOver(true)) fill = HomeDesign::accent().brighter(0.08f);
        if (fill.getAlpha() > 0) { g.setColour(fill); g.fillRoundedRectangle(b, radius); }
        if (kind != segment) {
            g.setColour(kind == primary ? HomeDesign::accent().darker(0.25f) : HomeDesign::line());
            g.drawRoundedRectangle(b.reduced(0.5f), radius, 1.f);
        }
        auto ink = kind == primary ? HomeDesign::onAccent() : (on || kind == ghost ? HomeDesign::text() : HomeDesign::muted());
        if (!isEnabled()) ink = HomeDesign::muted().withAlpha(0.45f);
        g.setColour(ink);
        int textX = juce::roundToInt(b.getX()) + 16;
        if (glyph != 0) {
            const auto icon = juce::Rectangle<float>(b.getX() + 14.f, b.getCentreY() - 7.f, 14.f, 14.f);
            drawGlyph(g, icon, ink);
            textX = juce::roundToInt(icon.getRight()) + 8;
        }
        g.setFont(HomeDesign::uiSemibold(13.f));
        g.drawText(getButtonText(), textX, juce::roundToInt(b.getY()),
                   juce::roundToInt(b.getRight()) - textX - 14, juce::roundToInt(b.getHeight()),
                   juce::Justification::centredLeft, true);
        if (hasKeyboardFocus(true)) {
            g.setColour(HomeDesign::accent());
            g.drawRoundedRectangle(b.reduced(1.5f), kind == segment ? 15.f : 5.f, 2.f);
        }
    }
private:
    void drawGlyph(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour ink) const {
        juce::Path path;
        if (glyph == 1) {                                       // plus
            path.startNewSubPath(area.getCentreX(), area.getY() + 2.f);
            path.lineTo(area.getCentreX(), area.getBottom() - 2.f);
            path.startNewSubPath(area.getX() + 2.f, area.getCentreY());
            path.lineTo(area.getRight() - 2.f, area.getCentreY());
        } else if (glyph == 2) {                                 // folder
            const auto r = area.reduced(1.f, 3.f);
            path.startNewSubPath(r.getX(), r.getBottom());
            path.lineTo(r.getX(), r.getY() + 2.f);
            path.lineTo(r.getX() + 5.f, r.getY() + 2.f);
            path.lineTo(r.getX() + 6.5f, r.getY() + 4.f);
            path.lineTo(r.getRight(), r.getY() + 4.f);
            path.lineTo(r.getRight(), r.getBottom());
            path.closeSubPath();
        } else if (glyph == 3) {                                 // refresh
            path.addCentredArc(area.getCentreX(), area.getCentreY(), area.getWidth() * 0.36f,
                               area.getHeight() * 0.36f, 0.f, 0.6f, juce::MathConstants<float>::twoPi - 0.4f, true);
        }
        g.setColour(ink);
        g.strokePath(path, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    Kind kind;
    int glyph;
};

// Magnifier drawn on top of the search field (does not take the mouse).
class SearchGlyph final : public juce::Component {
public:
    SearchGlyph() { setInterceptsMouseClicks(false, false); }
    void paint(juce::Graphics& g) override {
        const auto r = getLocalBounds().toFloat();
        g.setColour(HomeDesign::muted());
        g.drawEllipse(r.getCentreX() - 4.f, r.getCentreY() - 4.f, 7.f, 7.f, 1.3f);
        g.drawLine(r.getCentreX() + 2.f, r.getCentreY() + 2.f, r.getRight() - 1.f, r.getBottom() - 1.f, 1.3f);
    }
};

// Project Home — approved layout "Slate" + type scale D (2026-11-18).
// Every child that tests and the shell rely on keeps its component id:
// home-project-list, home-search, home-new, home-open, home-refresh, home-new-live,
// home-studio-tab, home-live-tab, home-resume.
class ProjectHome final : public juce::Component,private juce::ListBoxModel,private juce::Timer {
public:
    juce::Rectangle<int> listPanel;
    explicit ProjectHome(Desktop& d):owner(d),root(juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("MR Studio")),list("MR Studio projects",this){

        homeTheme.applyPreview(HomeDesign::tokens());
        setLookAndFeel(&homeTheme);
        setComponentID("project-home");setName("MR Studio projects");
        list.setComponentID("home-project-list");list.setRowHeight(64);
        list.setColour(juce::ListBox::backgroundColourId,juce::Colours::transparentBlack);
        list.setMouseCursor(juce::MouseCursor::NormalCursor);
        list.addMouseListener(this, true); // rows are child components: track hover through them
        for(auto* c:std::array<juce::Component*,9>{&list,&create,&open,&reload,&filter,&resume,&createLive,&studioTab,&liveTab})addAndMakeVisible(c);
        resume.setComponentID("home-resume");resume.onClick=[this]{owner.showProjectHome(false);};
        create.setComponentID("home-new");open.setComponentID("home-open");reload.setComponentID("home-refresh");
        filter.setComponentID("home-search");filter.setTextToShowWhenEmpty(juce::String::fromUTF8("Поиск по имени и пути"),HomeDesign::muted());filter.setTitle("Search project names and paths");
        filter.setIndents(34, 0);addAndMakeVisible(searchIcon);
        createLive.setComponentID("home-new-live");createLive.onClick=[this]{owner.action(701);};
        studioTab.setComponentID("home-studio-tab");liveTab.setComponentID("home-live-tab");
        studioTab.onClick=[this]{live=false;applyFilter();};liveTab.onClick=[this]{live=true;applyFilter();};
        create.onClick=[this]{owner.action(1);};
        open.onClick=[this]{if(live)owner.choose(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this](const auto& file){owner.launchLive(file,false);},"*.mrlive");else owner.action(2);};
        reload.onClick=[this]{rescan();};
#if defined(MRS_PROJECT_IMPORT_UI)
        addAndMakeVisible(importProject);importProject.setComponentID("home-import-project");
        importProject.onClick=[this]{
            juce::Component::SafePointer<Desktop> safe(&owner);
            owner.projectImportMenu().showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&importProject),[safe](int selected){
                if(safe&&(selected==Desktop::previewDawprojectAction||selected==Desktop::previewSongAction))safe->action(selected);
            });
        };
#endif
        filter.onTextChange=[this]{applyFilter();};
        startTimerHz(10);
    }
    ~ProjectHome() override{list.removeMouseListener(this);setLookAndFeel(nullptr);stopTimer();cancel=true;if(worker.valid())worker.wait();list.setModel(nullptr);}
    void rescan(){if(worker.valid()){rescanPending=true;return;}cancel=false;const auto folder=root;worker=std::async(std::launch::async,[this,folder]{return findHomeProjects(folder,&cancel);});scanning=true;reload.setEnabled(false);repaint();}
    bool loading() const{return scanning;}
    void visibilityChanged() override{if(isVisible())resized();}
    void scanRoot(const juce::File& folder){root=folder;rescan();}
    void pollScan(){if(!worker.valid()||worker.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;auto result=worker.get();projects=std::move(result.files);error=result.error;scanning=false;reload.setEnabled(true);applyFilter();if(rescanPending){rescanPending=false;rescan();}}
    std::size_t projectCount() const{return visible.size();}
    const juce::File& projectFile(std::size_t i) const{return projects.at(visible.at(i)).file;}
    void openRow(int row){if(row<0||row>=static_cast<int>(visible.size()))return;const auto file=projectFile(static_cast<std::size_t>(row));juce::Component::SafePointer<Desktop> safe(&owner);juce::MessageManager::callAsync([safe,file]{if(safe&&file.hasFileExtension("mrlive")){safe->run([&]{safe->launchLive(file,false);});return;}if(safe)safe->confirmDiscard([safe,file]{if(safe)safe->run([&]{safe->openFile(file);});});});}
    void resized() override{
        const int pad=20,top=44,rightW=narrow()?0:328,gap=16;
        const int headY=top+26,actionsY=headY+82,contentY=actionsY+54,footerY=getHeight()-34;
        const int width=juce::jmax(320,getWidth()-2*pad);
        studioTab.setBounds(pad+width-172,headY+2,84,32);liveTab.setBounds(pad+width-88,headY+2,84,32);
        resume.setVisible(owner.canReturnToProject());resume.setBounds(pad+width-380,headY,190,32);
        create.setBounds(pad,actionsY,150,36);open.setBounds(pad+160,actionsY,116,36);reload.setBounds(pad+286,actionsY,126,36);
        createLive.setVisible(live);createLive.setBounds(pad+422,actionsY,130,36);
#if defined(MRS_PROJECT_IMPORT_UI)
        importProject.setVisible(!live&&!owner.standaloneLive);importProject.setBounds(pad+422,actionsY,140,36);
#endif
        const int searchW=narrow()?juce::jmax(160,width-546):300;filter.setBounds(pad+width-searchW,actionsY,searchW,36);
        searchIcon.setBounds(filter.getX()+11,filter.getY()+11,14,14);
        const int listW=rightW?width-rightW-gap:width;
        listPanel={pad,contentY,listW,juce::jmax(80,footerY-contentY-14)};
        list.setBounds(listPanel.getX(),listPanel.getY()+70,listPanel.getWidth(),juce::jmax(40,listPanel.getHeight()-70));
    }
    void paint(juce::Graphics& g) override{
        const int pad=20,top=44,rightW=narrow()?0:328,gap=16;
        const int headY=top+26,actionsY=headY+82,contentY=actionsY+54,footerY=getHeight()-34;
        const int width=juce::jmax(320,getWidth()-2*pad);
        g.fillAll(HomeDesign::canvas());
        // top bar
        g.setColour(HomeDesign::line());g.fillRect(0,top-1,getWidth(),1);
        g.setColour(HomeDesign::accent());g.drawLine(20.f,22.f,27.f,17.f,1.6f);g.drawLine(27.f,17.f,34.f,22.f,1.6f);
        const auto brand=juce::String("MOON RIVER");const auto brandFont=HomeDesign::brandFont(12.5f);
        g.setFont(brandFont);g.setColour(HomeDesign::text());g.drawText(brand,46,14,140,18,juce::Justification::centredLeft,false);
        const int brandW=juce::GlyphArrangement::getStringWidthInt(brandFont,brand);
        g.setFont(HomeDesign::mono(10.5f).withExtraKerningFactor(.06f));g.setColour(HomeDesign::muted());g.drawText("STUDIO",54+brandW,14,80,18,juce::Justification::centredLeft,false);
        g.setFont(HomeDesign::mono(11.f));g.drawText(deviceLine(),getWidth()-pad-380,14,380,18,juce::Justification::centredRight,false);
        // header
        g.setFont(HomeDesign::display(34.f));g.setColour(HomeDesign::text());g.drawText(juce::String::fromUTF8("Проекты"),pad,headY-6,300,44,juce::Justification::centredLeft,false);
        g.setFont(HomeDesign::ui(13.f));g.setColour(HomeDesign::muted());g.drawText(summaryLine(),pad,headY+38,width-380,18,juce::Justification::centredLeft,false);
        // segmented control background
        const auto seg=juce::Rectangle<int>(pad+width-180,headY,180,36).toFloat();
        g.setColour(HomeDesign::surface());g.fillRoundedRectangle(seg,18.f);
        g.setColour(HomeDesign::line());g.drawRoundedRectangle(seg.reduced(.5f),18.f,1.f);
        const auto activeTab=(live?liveTab.getBounds():studioTab.getBounds()).toFloat().reduced(3.f);
        if(!activeTab.isEmpty()){g.setColour(HomeDesign::raised());g.fillRoundedRectangle(activeTab,16.f);g.setColour(HomeDesign::lineStrong());g.drawRoundedRectangle(activeTab.reduced(.5f),16.f,1.f);}
        // panels
        paintPanel(g,listPanel);
        const int rowsBottom=list.getY()+static_cast<int>(visible.size())*64;
        const int emptyTop=juce::jmax(listPanel.getY()+72,rowsBottom);
        if(listPanel.getBottom()-emptyTop>48){
            juce::Graphics::ScopedSaveState saved(g);
            g.reduceClipRegion(listPanel.getX()+1,emptyTop,listPanel.getWidth()-2,listPanel.getBottom()-emptyTop-1);
            const float x0=static_cast<float>(listPanel.getX()),x1=static_cast<float>(listPanel.getRight());
            const float low=static_cast<float>(listPanel.getBottom());
            const auto curve=[&](juce::Graphics& target,float y,float lift,juce::Colour c,float w){
                juce::Path p;p.startNewSubPath(x0,y);
                p.cubicTo(x0+(x1-x0)*.28f,y-lift,x0+(x1-x0)*.42f,y-lift*1.35f,x0+(x1-x0)*.55f,y-lift*.5f);
                p.cubicTo(x0+(x1-x0)*.68f,y+lift*.35f,x0+(x1-x0)*.82f,y-lift*.6f,x1,y-lift*.9f);
                target.setColour(c);target.strokePath(p,juce::PathStrokeType(w));
            };
            curve(g,low-26.f,26.f,HomeDesign::muted().withAlpha(.10f),1.f);
            curve(g,low-54.f,34.f,HomeDesign::accent().withAlpha(.12f),1.2f);
            curve(g,low-84.f,20.f,HomeDesign::muted().withAlpha(.07f),1.f);
        }
        if(rightW){const int cardH=juce::jmin(268,listPanel.getHeight()-16);paintPanel(g,{listPanel.getRight()+gap,contentY,rightW,cardH});
            const int soundY=contentY+cardH+16;if(footerY-soundY>90)paintPanel(g,{listPanel.getRight()+gap,soundY,rightW,juce::jmin(150,footerY-soundY-14)});}
        // list headers
        g.setFont(HomeDesign::monoSmallCaps(10.5f));g.setColour(HomeDesign::muted());
        g.drawText(juce::String::fromUTF8("НЕДАВНИЕ"),listPanel.getX()+16,contentY+14,160,16,juce::Justification::centredLeft,false);
        g.drawText(juce::String(static_cast<int>(visible.size()))+" / "+juce::String(static_cast<int>(projects.size())),listPanel.getRight()-120,contentY+14,104,16,juce::Justification::centredRight,false);
        const auto cols=juce::Rectangle<int>(listPanel.getX(),contentY+40,listPanel.getWidth(),30);
        g.setColour(HomeDesign::line());g.fillRect(cols.getX(),cols.getBottom(),cols.getWidth(),1);
        g.setFont(HomeDesign::monoSmallCaps(10.f));g.setColour(HomeDesign::muted());
        g.drawText(juce::String::fromUTF8("ПРОЕКТ"),cols.getX()+16,cols.getY()+8,200,14,juce::Justification::centredLeft,false);
        g.drawText(juce::String::fromUTF8("ИЗМЕНЁН"),cols.getRight()-236,cols.getY()+8,130,14,juce::Justification::centredLeft,false);
        g.drawText(juce::String::fromUTF8("РАЗМЕР"),cols.getRight()-100,cols.getY()+8,84,14,juce::Justification::centredLeft,false);
        // right column content
        if(rightW){paintDetails(g,{listPanel.getRight()+gap,contentY,rightW,juce::jmin(268,listPanel.getHeight()-16)});
            const int cardH=juce::jmin(268,listPanel.getHeight()-16);const int soundY=contentY+cardH+16;
            if(footerY-soundY>90)paintSound(g,{listPanel.getRight()+gap,soundY,rightW,juce::jmin(150,footerY-soundY-14)});}
        // footer
        g.setColour(HomeDesign::line());g.fillRect(0,footerY,getWidth(),1);
        g.setFont(HomeDesign::mono(10.5f));g.setColour(HomeDesign::muted());
        g.drawText(juce::String::fromUTF8("Библиотека: ")+root.getFullPathName(),pad,footerY+10,width-260,16,juce::Justification::centredLeft,false);
        g.drawText("MR Studio "+juce::String(std::string(mrs::desktop::application_version)),pad+width-240,footerY+10,240,16,juce::Justification::centredRight,false);
        // state text
        juce::String state;
        if(!error.isEmpty())state=error;else if(!owner.message.isEmpty())state=owner.message;
        if(state.isNotEmpty()){g.setFont(HomeDesign::ui(12.f));g.setColour(HomeDesign::warn());g.drawText(state,pad,footerY-22,width,18,juce::Justification::centredLeft,false);}
        if(visible.empty()){g.setFont(HomeDesign::ui(13.f));g.setColour(HomeDesign::muted());
            g.drawText(scanning?juce::String::fromUTF8("Поиск проектов…"):filter.getText().isEmpty()?(live?juce::String::fromUTF8("Показов не найдено. Создайте показ или откройте его из другого места."):juce::String::fromUTF8("Проектов не найдено. Начните с «Новый проект».")):juce::String::fromUTF8("Ничего не найдено"),
                list.getBounds().reduced(16),juce::Justification::centred);}
    }
private:
    struct RowButton final:public juce::TextButton {
        explicit RowButton(const juce::String& text=juce::String()):juce::TextButton(text){setComponentID("home-project-name");setMouseCursor(juce::MouseCursor::PointingHandCursor);setWantsKeyboardFocus(false);}
        juce::String label;
        void paint(juce::Graphics& g) override {
            g.setFont(HomeDesign::uiSemibold(15.f));
            g.setColour(getState()!=juce::Button::buttonNormal||isMouseOver(true)?HomeDesign::accent():HomeDesign::text());
            g.drawText(label.isEmpty()?getButtonText():label,getLocalBounds(),juce::Justification::centredLeft,true);
            if(hasKeyboardFocus(true)){g.setColour(HomeDesign::accent());g.drawRect(getLocalBounds(),1);}
        }
    };
    class NameRow final:public juce::Component {
    public:
        RowButton name;
        NameRow(){setInterceptsMouseClicks(false,true);addAndMakeVisible(name);}
        void resized() override {const int w=juce::GlyphArrangement::getStringWidthInt(HomeDesign::uiSemibold(15.f),name.label.isEmpty()?name.getButtonText():name.label);name.setBounds(16,10,juce::jmax(0,juce::jmin(getWidth()-32,w+2)),22);}
    };
    void paintPanel(juce::Graphics& g,const juce::Rectangle<int>& area)const{
        g.setColour(HomeDesign::surface());g.fillRoundedRectangle(area.toFloat(),10.f);
        g.setColour(HomeDesign::line());g.drawRoundedRectangle(area.toFloat().reduced(.5f),10.f,1.f);
    }
    void paintDetails(juce::Graphics& g,const juce::Rectangle<int>& area)const{
        g.setFont(HomeDesign::monoSmallCaps(10.5f));g.setColour(HomeDesign::muted());
        g.drawText(juce::String::fromUTF8("ВЫБРАННЫЙ ПРОЕКТ"),area.getX()+16,area.getY()+14,200,16,juce::Justification::centredLeft,false);
        g.setColour(HomeDesign::line());g.fillRect(area.getX(),area.getY()+40,area.getWidth(),1);
        const auto row=list.getSelectedRow();const HomeProject* item=row>=0&&row<static_cast<int>(visible.size())?&projects[visible[static_cast<std::size_t>(row)]]:nullptr;
        if(!item){g.setFont(HomeDesign::ui(12.5f));g.setColour(HomeDesign::muted());
            g.drawText(juce::String::fromUTF8("Выберите проект в списке, чтобы увидеть детали."),area.reduced(16,0).withTrimmedTop(56),juce::Justification::topLeft,true);return;}
        int y=area.getY()+54;
        g.setFont(HomeDesign::display(19.f));g.setColour(HomeDesign::text());
        g.drawText(item->file.getFileNameWithoutExtension(),area.getX()+16,y,area.getWidth()-32,24,juce::Justification::centredLeft,true);y+=26;
        g.setFont(HomeDesign::mono(10.5f));g.setColour(HomeDesign::muted());
        g.drawFittedText(item->relative,area.getX()+16,y,area.getWidth()-32,32,juce::Justification::topLeft,2);y+=38;
        detailLine(g,area,y,juce::String::fromUTF8("Тип"),item->file.hasFileExtension("mrlive")?juce::String::fromUTF8("Показ Live"):juce::String::fromUTF8("Проект студии"));y+=26;
        detailLine(g,area,y,juce::String::fromUTF8("Изменён"),modifiedText(*item));y+=26;
        detailLine(g,area,y,juce::String::fromUTF8("Размер"),humanSize(item->file.getSize()));y+=26;
        detailLine(g,area,y,juce::String::fromUTF8("Формат"),item->file.getFileExtension().toUpperCase());y+=30;
        if(y+30<area.getBottom()){
            g.setFont(HomeDesign::ui(11.5f));g.setColour(HomeDesign::muted());
            g.drawText(juce::String::fromUTF8("Двойной щелчок открывает проект"),area.getX()+16,y,area.getWidth()-32,18,juce::Justification::centredLeft,false);
        }
    }
    void detailLine(juce::Graphics& g,const juce::Rectangle<int>& area,int y,const juce::String& key,const juce::String& value)const{
        g.setFont(HomeDesign::ui(12.5f));g.setColour(HomeDesign::muted());g.drawText(key,area.getX()+16,y,120,18,juce::Justification::centredLeft,false);
        g.setFont(HomeDesign::mono(11.f));g.setColour(HomeDesign::text());g.drawText(value,area.getX()+140,y,area.getWidth()-156,18,juce::Justification::centredLeft,true);
    }
    void paintSound(juce::Graphics& g,const juce::Rectangle<int>& area)const{
        g.setFont(HomeDesign::monoSmallCaps(10.5f));g.setColour(HomeDesign::muted());
        g.drawText(juce::String::fromUTF8("ЗВУК"),area.getX()+16,area.getY()+14,120,16,juce::Justification::centredLeft,false);
        g.setColour(HomeDesign::line());g.fillRect(area.getX(),area.getY()+40,area.getWidth(),1);
        const auto status=owner.app.device_status();
        const bool running=status.phase==mrs::audio::DevicePhase::running;
        g.setColour(running?HomeDesign::ok():HomeDesign::warn());g.fillEllipse(area.getX()+17.f,area.getY()+59.f,7.f,7.f);
        g.setFont(HomeDesign::uiSemibold(13.f));g.setColour(HomeDesign::text());
        g.drawText(juce::String::fromUTF8(owner.app.audio_name().c_str()),area.getX()+32,area.getY()+52,area.getWidth()-48,18,juce::Justification::centredLeft,true);
        g.setFont(HomeDesign::mono(10.5f));g.setColour(HomeDesign::muted());
        g.drawText(soundLine(),area.getX()+32,area.getY()+72,area.getWidth()-48,16,juce::Justification::centredLeft,true);
    }
    juce::String modifiedText(const HomeProject& item)const{return juce::Time(item.modified).formatted("%d.%m.%Y, %H:%M");}
    static juce::String humanSize(juce::int64 bytes){
        static const char* units[]={"\xd0\x91","\xd0\x9a\xd0\x91","\xd0\x9c\xd0\x91","\xd0\x93\xd0\x91","\xd0\xa2\xd0\x91"};
        double value=static_cast<double>(bytes);int unit=0;
        while(value>=1024.0&&unit<4){value/=1024.0;++unit;}
        const auto number=unit==0?juce::String(bytes):juce::String(value,1).replaceCharacter('.',',');
        return number+" "+juce::String::fromUTF8(units[unit]);
    }
    juce::String deviceLine()const{return juce::String::fromUTF8(owner.app.audio_name().c_str())+"  \xc2\xb7  "+juce::String(static_cast<int>(owner.project()->sample_rate/1000.))+juce::String::fromUTF8(" кГц");}
    juce::String soundLine()const{
        const auto status=owner.app.device_status();
        const auto ms=[](double value){return std::isfinite(value)&&value>=0?juce::String(value,1):juce::String("--");};
        return juce::String(static_cast<int>(owner.project()->sample_rate/1000.))+juce::String::fromUTF8(" кГц  ·  I ")+ms(status.input_latency_ms)+juce::String::fromUTF8(" / O ")+ms(status.output_latency_ms)+" ms";
    }
    juce::String summaryLine()const{
        const auto count=static_cast<int>(projects.size());
        juce::String text=juce::String(count)+juce::String::fromUTF8(" проектов");
        if(!projects.empty())text+=juce::String::fromUTF8("  ·  последний изменён ")+modifiedText(projects.front());
        return text;
    }
    bool narrow()const{return getWidth()<1040;}
    juce::Component* refreshComponentForRow(int row,bool,juce::Component* existing) override {
        if(row<0||row>=getNumRows()){delete existing;return nullptr;}
        auto* item=dynamic_cast<NameRow*>(existing);if(!item){delete existing;item=new NameRow;}
        const auto& project=projects[visible[static_cast<std::size_t>(row)]];
        item->name.label=project.file.getFileNameWithoutExtension();
        item->name.setButtonText(item->name.label);
        item->name.setTitle("Open "+project.file.getFileName());item->name.setTooltip(project.relative);
        juce::Component::SafePointer<ProjectHome> safe(this);item->name.onClick=[safe,row]{if(safe)safe->openRow(row);};item->resized();return item;
    }
    int getNumRows() override{return static_cast<int>(visible.size());}
    juce::String getNameForRow(int row) override{return row>=0&&row<getNumRows()?projects[visible[static_cast<std::size_t>(row)]].relative:juce::String();}
    void paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool selected) override{
        if(row<0||row>=getNumRows())return;const auto& item=projects[visible[static_cast<std::size_t>(row)]];
        const auto bounds=juce::Rectangle<int>(0,0,width,height);
        const bool hovered=hoverRow==row;
        if(selected)g.setColour(HomeDesign::raised());else if(hovered)g.setColour(HomeDesign::hover());else g.setColour(HomeDesign::surface());
        g.fillRect(bounds);
        g.setColour(HomeDesign::line().withAlpha(.55f));g.fillRect(0,height-1,width,1);
        if(selected){g.setColour(HomeDesign::accent());g.fillRoundedRectangle(0.f,8.f,2.f,static_cast<float>(height-16),1.f);}
        g.setColour(HomeDesign::muted());g.setFont(HomeDesign::mono(11.f));
        g.drawText(item.relative,16,34,width-260,16,juce::Justification::centredLeft,true);
        g.setFont(HomeDesign::mono(12.f));g.drawText(modifiedText(item),width-236,24,130,16,juce::Justification::centredLeft,false);
        g.drawText(humanSize(item.file.getSize()),width-100,24,84,16,juce::Justification::centredLeft,false);
        if(item.file.hasFileExtension("mrlive")){
            const auto pill=juce::Rectangle<float>(static_cast<float>(width-270),static_cast<float>(height-39),40.f,17.f);
            g.setColour(HomeDesign::warn().withAlpha(.14f));g.fillRoundedRectangle(pill,4.f);
            g.setColour(HomeDesign::warn().withAlpha(.55f));g.drawRoundedRectangle(pill.reduced(.5f),4.f,1.f);
            g.setColour(HomeDesign::warn());g.setFont(HomeDesign::monoSmallCaps(9.f));
            g.drawText(juce::String::fromUTF8("ШОУ"),pill,juce::Justification::centred,false);
        }
    }
    void mouseMove(const juce::MouseEvent& e) override{
        const auto point=list.getLocalPoint(e.eventComponent,e.getPosition());
        const int row=list.getRowContainingPosition(point.x,point.y);
        if(row!=hoverRow){hoverRow=row;list.repaint();}
    }
    void mouseExit(const juce::MouseEvent&) override{if(hoverRow!=-1){hoverRow=-1;list.repaint();}}
    void listBoxItemClicked(int row,const juce::MouseEvent& e) override{list.selectRow(row);if(e.mods.isLeftButtonDown()&&e.getNumberOfClicks()>1)openRow(row);repaint();}
    void returnKeyPressed(int row) override{openRow(row);}
    void applyFilter(){studioTab.setToggleState(!live,juce::dontSendNotification);liveTab.setToggleState(live,juce::dontSendNotification);list.deselectAllRows();visible.clear();for(std::size_t i=0;i<projects.size();++i)if(projects[i].file.hasFileExtension(live?"mrlive":"mrsproject")&&projects[i].relative.containsIgnoreCase(filter.getText()))visible.push_back(i);list.updateContent();repaint();}
    void timerCallback() override{try{pollScan();if(owner.canReturnToProject()!=resume.isVisible())resized();repaint();}catch(const std::exception& e){scanning=false;reload.setEnabled(true);error=juce::String::fromUTF8(e.what());repaint();}}
    Theme homeTheme;
    Desktop& owner;juce::File root;juce::ListBox list;int hoverRow{-1};bool live{};
    HomeAction createLive{juce::String::fromUTF8("Новый показ"),HomeAction::ghost};
    HomeAction studioTab{juce::String::fromUTF8("Студия"),HomeAction::segment};
    HomeAction liveTab{juce::String("Live"),HomeAction::segment};
    HomeAction create{juce::String::fromUTF8("Новый проект"),HomeAction::primary,1};
    HomeAction open{juce::String::fromUTF8("Открыть…"),HomeAction::ghost,2};
    HomeAction reload{juce::String::fromUTF8("Обновить"),HomeAction::ghost,3};
    HomeAction resume{juce::String::fromUTF8("Продолжить сессию"),HomeAction::ghost};
#if defined(MRS_PROJECT_IMPORT_UI)
    HomeAction importProject{juce::String::fromUTF8("Импорт проекта…"),HomeAction::ghost};
#endif
    SearchGlyph searchIcon;juce::TextEditor filter;
    std::vector<HomeProject> projects;std::vector<std::size_t> visible;std::future<HomeProjects> worker;std::atomic<bool> cancel{};bool scanning{},rescanPending{};juce::String error;
};
}
