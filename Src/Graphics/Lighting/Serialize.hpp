#pragma once

#include "PointLight.hpp"
#include "SpotLight.hpp"

#include <yaml-cpp/yaml.h>

void SerializePointLight(YAML::Emitter& emitter, const PointLight& pointLight);
void DeserializePointLight(const YAML::Node& node, PointLight& pointLight);

void SerializeSpotLight(YAML::Emitter& emitter, const SpotLight& spotLight);
void DeserializeSpotLight(const YAML::Node& node, SpotLight& spotLight);
