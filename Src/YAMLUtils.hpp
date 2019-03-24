#pragma once

#include <yaml-cpp/yaml.h>

inline glm::vec3 ReadYAMLVec3(const YAML::Node& node)
{
	glm::vec3 v3;
	if (node)
	{
		for (int i = 0; i < 3; i++)
			v3[i] = node[i].as<float>(0.0f);
	}
	return v3;
}

inline void WriteYAMLVec3(YAML::Emitter& emitter, const char* name, const glm::vec3& v3)
{
	emitter << YAML::Key << name << YAML::Value << YAML::BeginSeq << v3.x << v3.y << v3.z << YAML::EndSeq;
}
