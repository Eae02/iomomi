#include "GLTFData.hpp"

#include <EGame/EG.hpp>

namespace gltf
{
	ElementType GLTFData::ParseElementType(std::string_view name)
	{
		if (name == "SCALAR")
			return ElementType::SCALAR;
		if (name == "VEC2")
			return ElementType::VEC2;
		if (name == "VEC3")
			return ElementType::VEC3;
		if (name == "VEC4")
			return ElementType::VEC4;
		EG_PANIC("Invalid element type");
	}
}
