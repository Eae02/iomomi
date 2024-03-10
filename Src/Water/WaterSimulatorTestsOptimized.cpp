#include <pcg_random.hpp>
#include <span>
#include <vector>
#include <random>
#include <cstdint>
#include <glm/glm.hpp>

#include "WaterConstants.hpp"

std::vector<glm::vec3> WaterTestGeneratePositions(pcg32_fast& rng, uint32_t numParticles, int minAirVoxel, int maxAirVoxel)
{
	std::uniform_real_distribution<float> positionDistribution(0.0, 1.0);

	std::uniform_int_distribution<int> cellDistribution(minAirVoxel, maxAirVoxel);

	std::vector<glm::ivec3> cells(numParticles / 100);
	for (glm::ivec3& cell : cells)
	{
		cell.x = cellDistribution(rng);
		cell.y = cellDistribution(rng);
		cell.z = cellDistribution(rng);
	}

	std::vector<glm::vec3> positions(numParticles);
	for (glm::vec3& pos : positions)
	{
		size_t cellIndex = std::uniform_int_distribution<size_t>(0, cells.size() - 1)(rng);

		pos.x = static_cast<float>(cells[cellIndex].x) + positionDistribution(rng);
		pos.y = static_cast<float>(cells[cellIndex].y) + positionDistribution(rng);
		pos.z = static_cast<float>(cells[cellIndex].z) + positionDistribution(rng);
	}

	return positions;
}

std::vector<glm::vec2> WaterTestCalculateDensities(std::span<const glm::vec3> positions)
{
	std::vector<glm::vec2> densities(positions.size());

	for (uint32_t a = 0; a < positions.size(); a++)
	{
		glm::vec2 density(1.0f);

		for (uint32_t b = 0; b < positions.size(); b++)
		{
			if (a == b)
				continue;

			float q = std::max(1.0f - glm::distance(positions[a], positions[b]) / W_INFLUENCE_RADIUS, 0.0f);
			float q2 = q * q;
			float q3 = q * q2;
			float q4 = q2 * q2;
			density += glm::vec2(q3, q4);
		}

		densities[a] = density;
	}

	return densities;
}
