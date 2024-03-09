#pragma once

std::vector<glm::vec3> GenerateWaterPositions(class World& world);

std::vector<struct WaterCollisionQuad> GenerateWaterCollisionQuads(class World& world);

void UpdateWaterCollisionQuadsBlockedGravity(
	const class World& world, eg::CommandContext& cc, struct WaterCollisionData& collisionData
);
