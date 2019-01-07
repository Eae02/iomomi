#include "Model.hpp"

namespace gltf
{
	Model::Model(std::vector<NamedMesh> meshes)
		: m_meshes(std::move(meshes))
	{
		std::sort(m_meshes.begin(), m_meshes.end(), [&] (const NamedMesh& a, const NamedMesh& b)
		{
			return a.Name() < b.Name();
		});
	}
}
