#include "WaterGenerate.hpp"
#include "../World/Entities/Components/WaterBlockComp.hpp"
#include "../World/Entities/EntTypes/WaterPlaneEnt.hpp"
#include "../World/World.hpp"
#include "WaterCollisionData.hpp"
#include "WaterSimulator.hpp"

#include <pcg_random.hpp>
#include <unordered_set>

std::vector<glm::vec3> GenerateWaterPositions(class World& world)
{
	std::unordered_set<glm::ivec3, IVec3Hash> alreadyGenerated;

	std::uniform_real_distribution<float> offsetDist(0.0f, 1.0f / 6.0f);

	pcg32_fast rng(0);
	std::vector<glm::vec3> positions;

	constexpr glm::ivec3 PARTICLES_PER_VOXEL(6, 7, 6);

	// Generates water
	world.entManager.ForEachOfType<WaterPlaneEnt>(
		[&](WaterPlaneEnt& waterPlaneEntity)
		{
			// Generates water for all underwater cells in this plane
			waterPlaneEntity.liquidPlane.MaybeUpdate(world);
			for (glm::ivec3 cell : waterPlaneEntity.liquidPlane.UnderwaterCells())
			{
				if (alreadyGenerated.count(cell))
					continue;
				alreadyGenerated.insert(cell);

				glm::ivec3 generatePerVoxel = PARTICLES_PER_VOXEL;
				if (waterPlaneEntity.liquidPlane.IsUnderwater(cell + glm::ivec3(0, 1, 0)))
					generatePerVoxel.y += waterPlaneEntity.densityBoost;

				for (int x = 0; x < generatePerVoxel.x; x++)
				{
					for (int y = 0; y < generatePerVoxel.y; y++)
					{
						for (int z = 0; z < generatePerVoxel.z; z++)
						{
							glm::vec3 offset(
								static_cast<float>(x) + offsetDist(rng), static_cast<float>(y) + offsetDist(rng),
								static_cast<float>(z) + offsetDist(rng)
							);

							glm::vec3 pos = glm::vec3(cell) + offset / glm::vec3(generatePerVoxel);
							if (pos.y > waterPlaneEntity.GetPosition().y)
								continue;
							positions.push_back(pos);
						}
					}
				}
			}
		}
	);

	uint32_t numPositionsToRemove = positions.size() % WaterSimulator2::NUM_PARTICLES_ALIGNMENT;
	for (uint32_t i = 0; i < numPositionsToRemove; i++)
	{
		size_t index = std::uniform_int_distribution<size_t>(0, positions.size() - 1)(rng);
		positions[index] = positions.back();
		positions.pop_back();
	}

	return positions;
}

std::vector<WaterCollisionQuad> GenerateWaterCollisionQuads(World& world)
{
	std::vector<WaterCollisionQuad> collisionQuads;

	world.entManager.ForEachWithComponent<WaterBlockComp>(
		[&](Ent& entity)
		{
			WaterBlockComp& component = *entity.GetComponentMut<WaterBlockComp>();

			// Skip this blocker if no gravities are blocked
			if (component.blockedGravities == DirFlags::None)
				return;

			component.indexFromSimulator = eg::UnsignedNarrow<uint32_t>(collisionQuads.size());

			float tangentLen = glm::length(component.tangent);
			float biTangentLen = glm::length(component.biTangent);
			glm::vec3 normal = glm::normalize(glm::cross(component.tangent, component.biTangent));

			collisionQuads.push_back(WaterCollisionQuad{
				.center = component.center,
				.normal = normal,
				.tangent = component.tangent / tangentLen,
				.tangentLen = tangentLen,
				.bitangentLen = biTangentLen,
				.blockedGravities = component.blockedGravities,
			});
		}
	);

	return collisionQuads;
}

void UpdateWaterCollisionQuadsBlockedGravity(
	const World& world, eg::CommandContext& cc, WaterCollisionData& collisionData
)
{
	const_cast<EntityManager&>(world.entManager)
		.ForEachWithComponent<WaterBlockComp>(
			[&](const Ent& entity)
			{
				const WaterBlockComp& component = *entity.GetComponent<WaterBlockComp>();
				if (component.indexFromSimulator.has_value())
				{
					collisionData.SetCollisionQuadBlockedGravities(
						cc, *component.indexFromSimulator, component.blockedGravities
					);
				}
			}
		);

	collisionData.MaybeInsertCollisionQuadsBufferBarrier(cc);
}
