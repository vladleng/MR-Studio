#pragma once
#include <memory>
namespace mr::separation { class ModelManager; }
namespace ui {
class Desktop;
std::shared_ptr<mr::separation::ModelManager> separationModelManager();
void showSeparationModels(Desktop&,std::shared_ptr<mr::separation::ModelManager> = {});
void separationModelsSmoke();
}
