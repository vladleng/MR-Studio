#include "Desktop.h"
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
class LiveWindow final : public juce::DocumentWindow {
  public:
    LiveWindow() : DocumentWindow("MR Live", juce::Colour(ui::surface), allButtons) {
        setUsingNativeTitleBar(true);
        setContentOwned(new ui::Desktop(false, false, true), true);
        setResizable(true, false);
        setResizeLimits(1200, 700, 3840, 2160);
        centreWithSize(1920, 1080);
        setVisible(true);
    }
    ui::Desktop &host() { return *static_cast<ui::Desktop *>(getContentComponent()); }
    void closeButtonPressed() override {
        host().confirmDiscard([] { juce::JUCEApplication::getInstance()->quit(); });
    }
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
