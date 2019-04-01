#pragma once

#include <yaml-cpp/yaml.h>

namespace WallLight
{
	eg::Entity* CreateEntity(eg::EntityManager& entityManager);
	
	void InitFromYAML(eg::Entity& entity, const YAML::Node& node);
}
