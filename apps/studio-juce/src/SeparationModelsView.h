#pragma once
#include <memory>
#include "SeparationRunner.h"
namespace mr::separation { class ModelManager; }
namespace ui {
class Desktop;
std::shared_ptr<mr::separation::ModelManager> separationModelManager(mr::separation::Backend = mr::separation::Backend::python);
void showSeparationModels(Desktop&,std::shared_ptr<mr::separation::ModelManager> = {});
void separationModelsSmoke();
}
