#pragma once

#include "PointLight.hpp"

#include <yaml-cpp/yaml.h>

void SerializePointLight(YAML::Emitter& emitter, const PointLight& pointLight);
void DeserializePointLight(const YAML::Node& node, PointLight& pointLight);
