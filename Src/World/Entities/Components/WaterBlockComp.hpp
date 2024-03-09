#pragma once

#include "../../Dir.hpp"

struct WaterBlockComp
{
	std::vector<glm::ivec3> blockedVoxels;
	glm::vec3 center;
	glm::vec3 tangent;
	glm::vec3 biTangent;
	DirFlags blockedGravities = DirFlags::All;
	bool onlyBlockDuringSimulation = false;
	int editorVersion = 0;

	std::optional<uint32_t> indexFromSimulator; // assigned by the water simulator on level load

	void InitFromAAQuadComponent(const struct AxisAlignedQuadComp& aaQuadComp, const glm::vec3& center);
};
