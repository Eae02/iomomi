#pragma once

#include "../World/Dir.hpp"

struct WaterCollisionQuad
{
	glm::vec3 center;
	glm::vec3 normal;
	glm::vec3 tangent;
	float tangentLen;
	float bitangentLen;
	DirFlags blockedGravities;

	glm::vec3 Bitangent() const { return glm::normalize(glm::cross(normal, tangent)); }

	std::optional<float> NormalDistanceToAABB(const eg::AABB& aabb) const;
};

struct WaterCollisionData
{
	eg::Texture voxelDataTexture;

	eg::Buffer collisionQuadsBuffer;
	bool collisionQuadsBufferNeedsBarrier = false;

	std::vector<WaterCollisionQuad> collisionQuads;

	static WaterCollisionData Create(
		eg::CommandContext& cc, const class VoxelBuffer& voxelBuffer, std::vector<WaterCollisionQuad> collisionQuads
	);

	void SetCollisionQuadBlockedGravities(
		eg::CommandContext& cc, uint32_t collisionQuadIndex, DirFlags blockedGravities
	);

	void MaybeInsertCollisionQuadsBufferBarrier(eg::CommandContext& cc);
};
