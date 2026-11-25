#pragma once
#include "ProjectImport.h"
#include <map>

namespace mrs::project_import {
struct RelinkRequest {
    std::string reference, reason;
};
struct CreateRequest {
    std::filesystem::path source, projectsDirectory;
    std::map<std::string,std::filesystem::path> relinks;
    bool skipMissing{};
};
struct CreateResult {
    std::filesystem::path projectFile;
    std::vector<RelinkRequest> missing;
    std::vector<std::string> warnings;
    std::string error;
    bool cancelled{};
};
// Offline worker only. Builds a native archive in a private staging directory and
// publishes it without opening/replacing the running Studio session.
CreateResult createProject(const CreateRequest&, std::stop_token = {});
} // namespace mrs::project_import
