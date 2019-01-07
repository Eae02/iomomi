#pragma once

#include "Model.hpp"
#include "../../Utils.hpp"

#include <filesystem>

namespace gltf
{
	Model Import(const std::filesystem::path& path);
}
