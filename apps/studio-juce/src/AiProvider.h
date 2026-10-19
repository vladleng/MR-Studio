#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <mrs/ai_query.hpp>
#include <mrs/desktop.hpp>
namespace ui::aiapi {
struct Config {
    juce::String provider{"ProxyAPI"}, base{"https://api.proxyapi.ru/v1"}, model{"openai/gpt-5.6-luna"};
    int maxTokens{1024};
    void validate() const;
};
struct Reply {int status{};juce::String body;};
struct ConversationTurn {juce::String user,assistant;};
class Conversation {
public:
    void add(juce::String user,juce::String assistant);
    void clear(){turns.clear();}
    std::vector<ConversationTurn> turns;
};
struct AnalysisCache {std::optional<mrs::ai::ContextKey> key;std::string media_version;juce::String json;};
struct AnalysisInput {std::shared_ptr<const mrs::ai::ReadView> view;std::vector<mrs::desktop::Application::AnalysisSource> sources;std::shared_ptr<AnalysisCache> cache;bool local_only{};};
using Transport=std::function<Reply(const Config&,const juce::String&,const juce::String&,const juce::String&,const mrs::ai::JobControl&)>;
Reply http(const Config&,const juce::String& key,const juce::String& route,const juce::String& body,const mrs::ai::JobControl&);
juce::String credentialTarget(const Config&);
juce::String readKey(const Config&);
void writeKey(const Config&,juce::String);
void eraseKey(const Config&);
Config loadConfig();
void saveConfig(const Config&);
mrs::ai::Task task(Config,juce::String key,bool models,bool suggestions,Transport=http,Conversation={},AnalysisInput={});
void smoke();
}
