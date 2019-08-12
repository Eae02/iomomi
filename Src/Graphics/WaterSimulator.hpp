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
	
	std::pair<float, int> RayIntersect(const eg::Ray& ray) const;
	
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
	
	std::unique_ptr<__m128[]> m_particlePos;
	std::unique_ptr<__m128[]> m_particlePos2;
	std::unique_ptr<__m128[]> m_particleVel;
	std::unique_ptr<__m128[]> m_particleVel2;
	std::unique_ptr<__m128i[]> m_particleCells;
	std::vector<float> m_particleRadius;
	std::vector<uint8_t> m_particleGravity;
	std::vector<glm::vec2> m_particleDensity;
	
	eg::Buffer m_positionsBuffer;
	
	static constexpr uint32_t MAX_PER_CELL = 512;
	static constexpr uint32_t MAX_CLOSE = 512;
	
	std::vector<uint16_t> m_cellNumParticles;
	std::vector<std::array<uint16_t, MAX_PER_CELL>> m_cellParticles;
	
	__m128i m_worldMin;
	glm::ivec3 m_worldSize;
	std::vector<bool> m_isVoxelAir;
	const class World* m_world;
	
	inline bool IsVoxelAir(__m128i voxel) const;
	
	glm::ivec3 m_gridCellsMin;
	glm::ivec3 m_numGridCellGroups;
	glm::ivec3 m_numGridCells;
	
	inline int CellIdx(__m128i coord);
	
	std::vector<uint16_t> m_numCloseParticles;
	std::vector<std::array<uint16_t, MAX_CLOSE>> m_closeParticles;
	
	eg::MeshBatch m_meshBatch;
};
