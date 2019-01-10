#pragma once

#include "Model.hpp"

#include <EGame/EG.hpp>
#include <filesystem>

namespace gltf
{
	Model Import(const std::filesystem::path& path);
}
