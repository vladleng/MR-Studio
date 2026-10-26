#include "Desktop.h"
#include "GraphiteWindow.h"
#include "LiveWindowLayout.h"
#include "LiveWorkspace.h"
#include <mrs/diagnostics.hpp>
#include <mrs/version.hpp>

namespace {
void applyRequest(ui::Desktop &desktop, const juce::String &args) {
    const auto tokens = juce::StringArray::fromTokens(args, true);
    for (const auto &flag : {juce::String("--new-show"), juce::String("--open-show")}) {
        const int at = tokens.indexOf(flag);
        if (at >= 0) {
            if (at + 1 >= tokens.size())
                throw std::runtime_error("Missing .mrlive show path");
            const auto file = juce::File::getCurrentWorkingDirectory().getChildFile(tokens[at + 1].unquoted());
            if (flag == "--new-show")
                desktop.createLive(file);
            else
                desktop.openFile(file);
            return;
        }
    }
}
class LiveShell final : public juce::Component {
  public:
    explicit LiveShell(bool testing = false) : desktop(testing, false, true) {
        addAndMakeVisible(viewport);
        viewport.setViewedComponent(&desktop, false);
        viewport.setScrollBarsShown(true, true);
        desktop.liveWorkspace->appearanceChanged = [this] { applyAppearance(); };
    }
    ~LiveShell() override {
        desktop.liveWorkspace->appearanceChanged = {};
        viewport.setViewedComponent(nullptr, false);
    }
    void resized() override { applyAppearance(); }
    void paint(juce::Graphics &g) override { g.fillAll(ui::liveReferenceTokens().colour(ui::ColourRole::canvas)); }
    void applyAppearance() {
        viewport.setBounds(getLocalBounds());
        const auto scale = ui::live::fitUiScale(viewport.getMaximumVisibleWidth(), viewport.getMaximumVisibleHeight(),
                                                desktop.view.liveUiZoom);
        desktop.setTransform(juce::AffineTransform::scale(scale));
        desktop.setSize(
            juce::jmax(1200,
                       static_cast<int>(std::floor(static_cast<float>(viewport.getMaximumVisibleWidth()) / scale))),
            juce::jmax(700,
                       static_cast<int>(std::floor(static_cast<float>(viewport.getMaximumVisibleHeight()) / scale))));
        if (auto *window = findParentComponentOfClass<juce::DocumentWindow>())
            window->setAlpha(static_cast<float>(desktop.view.liveWindowOpacity));
    }
    ui::Desktop desktop;

  private:
    juce::Viewport viewport;
};
class LiveWindow final : public juce::DocumentWindow, private juce::Timer {
  public:
    explicit LiveWindow(bool testing = false)
        : DocumentWindow("MR Live", ui::liveReferenceTokens().colour(ui::ColourRole::panel), allButtons) {
        setUsingNativeTitleBar(true);
        shell = new LiveShell(testing);
        setContentOwned(shell, true);
        setResizable(true, false);
        setBounds(0, 0, 1400, 850);
        const bool restored =
            host().view.windowState.isNotEmpty() && restoreWindowStateFromString(host().view.windowState);
        constrainToDisplay(!restored, !restored);
        host().liveWorkspace->fitWindowRequested = [this] { constrainToDisplay(true); };
        setVisible(true);
        ui::applyGraphiteWindowCaption(*this);
        shell->applyAppearance();
        startTimer(500);
    }
    ~LiveWindow() override {
        stopTimer();
        host().liveWorkspace->fitWindowRequested = {};
        rememberWindow();
    }
    ui::Desktop &host() { return shell->desktop; }
    void closeButtonPressed() override {
        host().confirmDiscard([this] {
            rememberWindow();
            juce::JUCEApplication::getInstance()->quit();
        });
    }

  private:
    void rememberWindow() {
        host().view.windowState = getWindowStateAsString();
        try {
            host().saveSettings();
        } catch (...) {
        }
    }
    juce::Rectangle<int> workArea(bool initial = false) const {
        const auto &displays = juce::Desktop::getInstance().getDisplays();
        const auto *display = initial ? displays.getDisplayForPoint(juce::Desktop::getMousePosition().toFloat())
                                      : displays.getDisplayForRect(getBounds());
        if (!display)
            display = displays.getPrimaryDisplay();
        return display ? display->userBounds.toNearestInt() : juce::Rectangle<int>(0, 0, 1280, 720);
    }
    void constrainToDisplay(bool centre, bool initial = false) {
        const auto work = workArea(initial);
        lastWorkArea = work;
        juce::BorderSize<int> frame(32, 8, 8, 8);
        if (auto *peer = getPeer())
            if (const auto actual = peer->getFrameSizeIfPresent())
                frame = *actual;
        const auto available = frame.subtractedFrom(work.reduced(4));
        setResizeLimits(juce::jmin(900, available.getWidth()), juce::jmin(540, available.getHeight()),
                        juce::jmax(1, available.getWidth()), juce::jmax(1, available.getHeight()));
        if (!isFullScreen())
            setBounds(ui::live::fitWindow(getBounds(), work, frame, centre));
    }
    void timerCallback() override {
        if (workArea() != lastWorkArea)
            constrainToDisplay(false);
    }
    LiveShell *shell{};
    juce::Rectangle<int> lastWorkArea;
};
class LiveApplication final : public juce::JUCEApplication {
  public:
    const juce::String getApplicationName() override { return "MR Live"; }
    const juce::String getApplicationVersion() override {
        return juce::String(mrs::desktop::application_version.data());
    }
    bool moreThanOneInstanceAllowed() override { return false; }
    void initialise(const juce::String &args) override {
        if (args.contains("--live-application-smoke")) {
            try {
                const auto check = [](bool value, const char *error) {
                    if (!value)
                        throw std::runtime_error(error);
                };
                {
                    LiveWindow native(true);
                    const auto &displays = juce::Desktop::getInstance().getDisplays();
                    const auto *display = displays.getDisplayForRect(native.getBounds());
                    check(display && display->userBounds.toNearestInt().contains(native.getBounds()),
                          "MR Live window fits the work area on real Windows peer creation");
                    if (auto *peer = native.getPeer())
                        if (const auto border = peer->getFrameSizeIfPresent())
                            check(display->userBounds.toNearestInt().contains((*border).addedTo(native.getBounds())),
                                  "Native Windows caption and frame also fit the working screen");
                    check(native.getContentComponent()->getLocalBounds().contains(native.host().getBoundsInParent()),
                          "Automatic scale keeps the full interface reachable");
                    const auto before = native.host().documentDirty();
                    native.host().view.liveUiZoom = 1.5;
                    native.host().view.liveMenuOpacity = .7;
                    native.host().liveWorkspace->appearanceChanged();
                    check(native.host().getTransform() == juce::AffineTransform::scale(1.5f) &&
                              native.host().documentDirty() == before,
                          "Live UI zoom changes presentation without editing the show");
                }
                ui::Desktop live(true, false, true);
                check(!live.projectHomeVisible() && live.project()->tracks.empty(),
                      "MR Live starts without Studio Home or demo tracks");
                live.setWorkspace(mrs::desktop::Workspace::arrange);
                check(live.app.workspace() == mrs::desktop::Workspace::live, "MR Live cannot become Studio Arrange");
                live.showProjectHome(true);
                check(!live.projectHomeVisible(), "MR Live cannot become Studio Home");
                check(!live.workspaceShortcut(juce::KeyPress(juce::KeyPress::F3Key)),
                      "Studio workspace shortcut is absent");
                bool rejected = false;
                try {
                    live.openFile(juce::File::getCurrentWorkingDirectory().getChildFile("studio.mrsproject"));
                } catch (const std::exception &) {
                    rejected = true;
                }
                check(rejected && live.liveDocument.has_value(), "MR Live rejects direct Studio project opening");
                auto *workspace = live.liveWorkspace.get();
                check(workspace != nullptr, "Dedicated MR Live interface exists");
                for (auto *child : workspace->getChildren())
                    if (auto *button = dynamic_cast<juce::TextButton *>(child))
                        check(button->getButtonText() != "Studio", "No Studio transition in MR Live");
                const auto tokens = juce::StringArray::fromTokens(args, true);
                const int fixtureAt = tokens.indexOf("--show-fixture");
                if (fixtureAt < 0 || fixtureAt + 1 >= tokens.size())
                    throw std::runtime_error("Missing show fixture");
                live.importLiveShow(juce::File(tokens[fixtureAt + 1].unquoted()));
                check(live.project()->tracks.empty() && live.liveDocument->projects.empty(),
                      "Imported Live show does not load a Studio song");
                const auto saved = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                       .getNonexistentChildFile("mr-live-application", ".mrlive", false);
                live.saveFile(saved);
                live.openFile(saved);
                check(live.liveDocument->interfaceState["items"].size() == 3 && !live.documentDirty(),
                      "Dedicated MR Live application saves and reopens its own show");
                check(saved.deleteFile(), "MR Live application fixture cleanup");
                ui::Desktop studio(true, true);
                const auto project = studio.project();
                bool launched = false;
                studio.liveLaunchOverride = [&](const auto &file, bool create) {
                    launched = create && file.hasFileExtension("mrlive");
                };
                studio.launchLive(juce::File::getCurrentWorkingDirectory().getChildFile("show.mrlive"), true);
                check(launched && studio.project() == project && studio.projectHomeVisible() && !studio.liveDocument,
                      "Home dispatches MR Live without replacing the Studio project");
                setApplicationReturnValue(0);
            } catch (const std::exception &error) {
                juce::File::getCurrentWorkingDirectory()
                    .getChildFile("live-application-failure.txt")
                    .replaceWithText(error.what());
                setApplicationReturnValue(1);
            }
            quit();
            return;
        }
        const auto executable = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
        const auto logs =
            juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("MRLive/logs");
        diagnostics = std::make_unique<mrs::diagnostics::Session>(
            std::filesystem::path(logs.getFullPathName().toWideCharPointer()),
            std::filesystem::path(
                executable.getSiblingFile("mrs_crash_reporter.exe").getFullPathName().toWideCharPointer()),
            mrs::desktop::application_version.data());
        window = std::make_unique<LiveWindow>();
        window->host().run([&] { applyRequest(window->host(), args); });
    }
    void anotherInstanceStarted(const juce::String &args) override {
        if (!window)
            return;
        window->toFront(true);
        window->host().confirmDiscard(
            [this, args] { window->host().run([&] { applyRequest(window->host(), args); }); });
    }
    void shutdown() override {
        window.reset();
        diagnostics.reset();
    }

  private:
    std::unique_ptr<LiveWindow> window;
    std::unique_ptr<mrs::diagnostics::Session> diagnostics;
};
} // namespace
START_JUCE_APPLICATION(LiveApplication)
