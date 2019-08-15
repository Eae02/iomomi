#pragma once

#include <memory>
#include <future>
#include "../World/Dir.hpp"

class WaterSimulator
{
public:
	WaterSimulator();
	
	void Init(const class World& world);
	
	void BeginUpdate(float dt, const class Player& player);
	
	void WaitForUpdateCompletion();
	
	//Finds an intersection between the given ray and the water.
	// Returns a pair of distance and particleIndex.
	// particleIndex will be -1 if no intersection was found.
	std::pair<float, int> RayIntersect(const eg::Ray& ray) const;
	
	//Changes gravitation for all particles connected to the given particle.
	void ChangeGravity(int particle, Dir newGravity)
	{
		m_changeGravityParticle = particle;
		m_newGravity = newGravity;
	}
	
	eg::BufferRef GetPositionsBuffer() const;
	
	uint32_t NumParticles() const
	{
		return m_numParticles;
	}
	
	//Returns the number of particles intersecting the player.
	// Used to detect when the player is underwater.
	uint32_t NumIntersectingPlayer() const
	{
		return m_numIntersectsPlayerCopy;
	}
	
private:
	void Update(float dt, int changeGravityParticle, Dir newGravity);
	
	alignas(16) int32_t m_cellProcOffsets[4 * 3 * 3 * 3];
	
	std::mt19937 m_rng;
	
	bool m_canWait;
	std::future<void> m_completeFuture;
	
	eg::AABB m_playerAABB;
	std::atomic_uint32_t m_numIntersectsPlayer;
	uint32_t m_numIntersectsPlayerCopy = 0;
	
	uint32_t m_numParticles;
	
	int m_changeGravityParticle = -1;
	Dir m_newGravity;
	
	//Memory for particle data
	std::unique_ptr<void, eg::FreeDel> m_memory;
	
	//SoA particle data, points into m_memory
	__m128* m_particlePos;
	__m128* m_particlePos2;
	__m128* m_particleVel;
	__m128* m_particleVel2;
	float* m_particleRadius;
	uint8_t* m_particleGravity;
	glm::vec2* m_particleDensity;
	
	//Stores which cell each particle belongs to
	__m128i* m_particleCells;
	
	eg::Buffer m_positionsBuffer;
	
	//Maximum number of particles per partition grid cell
	static constexpr uint32_t MAX_PER_PART_CELL = 512;
	
	//Maximum number of close particles
	static constexpr uint32_t MAX_CLOSE = 512;
	
	//For each partition cell, stores how many particles belong to that cell.
	std::vector<uint16_t> m_cellNumParticles;
	
	//For each partition cell, stores a list of particle indices that belong to that cell.
	std::vector<std::array<uint16_t, MAX_PER_PART_CELL>> m_cellParticles;
	
	__m128i m_worldMin;
	glm::ivec3 m_worldSize;
	std::vector<bool> m_isVoxelAir;
	const class World* m_world;
	
	inline bool IsVoxelAir(__m128i voxel) const;
	
	glm::ivec3 m_partGridMin;
	glm::ivec3 m_partGridNumCellGroups;
	glm::ivec3 m_partGridNumCells;
	
	inline int CellIdx(__m128i coord);
	
	std::vector<uint16_t> m_numCloseParticles;
	std::vector<std::array<uint16_t, MAX_CLOSE>> m_closeParticles;
	
	eg::MeshBatch m_meshBatch;
};
