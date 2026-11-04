#pragma once
#include "SeparationProject.h"
namespace ui {
class Desktop;
void showSeparation(Desktop&,std::optional<mrs::Id> source={},std::shared_ptr<mr::separation::ModelManager> manager={},std::shared_ptr<mr::separation::ProjectJob> job={});
void separationProjectSmoke();
}
