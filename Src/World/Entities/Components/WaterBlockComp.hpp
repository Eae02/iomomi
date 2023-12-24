#pragma once

struct WaterBlockComp
{
	std::vector<glm::ivec3> blockedVoxels;
	glm::vec3 center;
	glm::vec3 tangent;
	glm::vec3 biTangent;
	bool blockedGravities[6] = {};
	bool onlyBlockDuringSimulation = false;
	int editorVersion = 0;

	void InitFromAAQuadComponent(const struct AxisAlignedQuadComp& aaQuadComp, const glm::vec3& center);
};
