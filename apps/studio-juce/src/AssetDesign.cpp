#include "Desktop.h"
#include "LibraryFoundationPanel.h"

namespace ui {
void Strip::refreshArmState(){
    if(owner.assetSkin()&&target)arm.setToggleState(owner.app.track_armed(*target),juce::dontSendNotification);
}

void Desktop::setAssetDesign(bool enabled){
    if(enabled==(assetTheme!=nullptr))return;
    if(enabled){
        setShellDesign(true);
        assetTheme=std::make_unique<Theme>();
        // Accepted neutral surfaces/metrics, retaining the UDS semantic accents.
        auto tokens=DesignTokens::target();const DesignTokens accepted;
        for(const auto role:{ColourRole::canvas,ColourRole::window,ColourRole::panel,ColourRole::raised,ColourRole::deep,ColourRole::border})
            tokens.colours[static_cast<std::size_t>(role)]=accepted.colours[static_cast<std::size_t>(role)];
        assetTheme->applyPreview(tokens);assetTheme->useConceptAssets(true);
    }
    // Attach only to owned target controls, never Desktop/native/external editors.
    juce::Component* chrome[]{&pointerTool,&knifeTool,&undo,&redo,&split,&duplicate,&zoomIn,&zoomOut,&fit,&snapButton,
        &clickButton,&countButton,&play,&pause,&stop,&record,&previous,&next,&loop,&editButton,&mixButton,&brows};
    for(auto* c:chrome)c->setLookAndFeel(enabled?assetTheme.get():shellTheme.get());
    duplicate.getProperties().set("mrConceptIcon",enabled?"duplicate":"");
    for(auto* s:{&mixButton,&editButton,&brows})s->getProperties().set("mrConceptIcon",enabled?(s==&mixButton?"mixer_params":s==&editButton?"list":"folder"):"");
    // Detach all children before destroying the borrowed Theme.
    auto retiringTheme=std::move(assetTheme);
    if(enabled)assetTheme=std::move(retiringTheme);
    for(auto& s:mixer)s->applyAssetDesign();if(master)master->applyAssetDesign();
    if(arrangement)for(auto& s:arrangement->rows)s->applyAssetDesign();
    if(browser)browser->applyAssetDesign();
    if(!enabled)retiringTheme.reset();
    resized();repaint();
}

void Strip::applyAssetDesign(){
    const bool enabled=owner.assetSkin()!=nullptr;
    if(static_cast<bool>(getProperties()["mrConceptStrip"])==enabled&&(!enabled||&getLookAndFeel()==owner.assetLookAndFeel()))return;
    getProperties().set("mrConceptStrip",enabled);
    setLookAndFeel(owner.assetLookAndFeel());
    nameEditor.setColour(juce::TextEditor::backgroundColourId,juce::Colour(enabled?ConceptSkin::channel:panel));
    nameEditor.setColour(juce::TextEditor::focusedOutlineColourId,owner.selectionColour());
    gain.getProperties().set("mrConceptGain",true);
    for(auto* b:{&mute,&solo,&arm}){
        b->getProperties().set("mrConceptChannel",enabled);
        b->getProperties().set("mrChannelPlate",enabled);
        b->getProperties().set("mrConceptIcon",enabled?(b==&mute?"button_mute":b==&solo?"button_solo":"button_record"):"");
    }
    add.getProperties().set("mrConceptIcon",enabled?"plus":"");
    add.getProperties().set("mrIconSize",13.f);
    piano.getProperties().set("mrConceptIcon",enabled?"track_synth":"");piano.getProperties().set("mrIconSize",16.f);
    resized();repaint();gain.repaint();pan.repaint();
}

void Browser::applyAssetDesign(){
    // Scope to existing browser actions; keep list/menu/editor palette accepted.
    const bool enabled=owner.assetSkin()!=nullptr;
    const auto browserBackground=juce::Colour(surface);
    fileTree.setColour(juce::TreeView::backgroundColourId,browserBackground);
    if(enabled)tree.setColour(juce::TreeView::backgroundColourId,browserBackground);
    else tree.removeColour(juce::TreeView::backgroundColourId);
    for(auto* component:{static_cast<juce::Component*>(&tree),static_cast<juce::Component*>(&fileTree),static_cast<juce::Component*>(&search),static_cast<juce::Component*>(&folderPath),static_cast<juce::Component*>(&locations),static_cast<juce::Component*>(&scanButton),static_cast<juce::Component*>(&aiTab),static_cast<juce::Component*>(&libraryTab)})component->setLookAndFeel(owner.assetLookAndFeel());
    const auto selection=enabled?owner.selectionColour():juce::Colour(0xff375a80);
    fileTree.setColour(juce::TreeView::selectedItemBackgroundColourId,selection);
    fileTree.setColour(juce::DirectoryContentsDisplayComponent::highlightColourId,selection);
    fileTree.setColour(juce::DirectoryContentsDisplayComponent::highlightedTextColourId,enabled?juce::Colour(ConceptSkin::canvas):juce::Colours::white);
    for(auto* editor:{&search,&folderPath}){
        editor->setColour(juce::TextEditor::focusedOutlineColourId,enabled?owner.selectionColour():juce::Colour(accent));
        editor->setColour(juce::TextEditor::highlightColourId,selection);
        editor->setColour(juce::TextEditor::highlightedTextColourId,enabled?juce::Colour(ConceptSkin::canvas):juce::Colours::white);
    }
    if(libraryPanel)libraryPanel->setLookAndFeel(owner.assetLookAndFeel());
    for(auto& button:crumbs)button->setLookAndFeel(owner.assetLookAndFeel());
    for(auto* b:{&vstTab,&filesTab,&parentFolder,&chooseFolder}){
        b->setLookAndFeel(owner.assetLookAndFeel());
        b->getProperties().set("mrConceptIcon",enabled?(b==&vstTab?"plugin_fx":b==&filesTab?"folder":b==&parentFolder?"folder_open":"folder"):"");
        b->setTitle(b==&vstTab?"VST3 plugins":b==&filesTab?"Files browser":b==&parentFolder?"Parent folder":"Choose folder");
        b->setTooltip(b->getTitle());
    }
}
}
