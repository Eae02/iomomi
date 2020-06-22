#include "WaterBlockComp.hpp"
#include "AxisAlignedQuadComp.hpp"

void WaterBlockComp::InitFromAAQuadComponent(const AxisAlignedQuadComp& aaQuadComp, const glm::vec3& _center)
{
	blockedVoxels.clear();
	
	auto [_tangent, _biTangent] = aaQuadComp.GetTangents(0);
	center = _center;
	tangent = _tangent / 2.0f;
	biTangent = _biTangent / 2.0f;
	
	glm::vec3 tangentAndBiTangent = glm::abs(tangent + biTangent);
	
	glm::vec3 minF = _center - tangentAndBiTangent;
	glm::vec3 maxF = _center + tangentAndBiTangent;
	
	glm::ivec3 minI(glm::floor(minF));
	glm::ivec3 maxI = glm::max(minI + 1, glm::ivec3(glm::ceil(maxF)));
	for (int x = minI.x; x < maxI.x; x++)
	{
		for (int y = minI.y; y < maxI.y; y++)
		{
			for (int z = minI.z; z < maxI.z; z++)
			{
				blockedVoxels.emplace_back(x, y, z);
			}
		}
	}
}
