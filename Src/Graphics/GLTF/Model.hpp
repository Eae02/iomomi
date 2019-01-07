#pragma once

#include "../Mesh.hpp"

namespace gltf
{
	class Model
	{
	public:
		explicit Model(std::vector<NamedMesh> meshes);
		
	private:
		std::vector<NamedMesh> m_meshes;
	};
}
