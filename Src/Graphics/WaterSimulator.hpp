#pragma once

#include <memory>
#include <future>
#include <thread>
#include <mutex>
#include "../World/Dir.hpp"

class WaterSimulator
{
public:
	struct QueryResults
	{
		int numIntersecting;
		glm::vec3 waterVelocity;
		glm::vec3 buoyancy;
	};
	
	class QueryAABB
	{
	public:
		friend class WaterSimulator;
		
		void SetAABB(const eg::AABB& aabb)
		{
			m_aabbMT = aabb;
		}
		
		QueryResults GetResults()
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			return m_results;
		}
		
	private:
		static constexpr uint32_t MAX_PARTICLE_INFO = 128;
		
		bool m_wantsParticleInfo;
		
		std::mutex m_mutex;
		QueryResults m_results;
		
		eg::AABB m_aabbMT;
		eg::AABB m_aabbBT;
	};
	
	WaterSimulator();
	
	~WaterSimulator()
	{
		Stop();
	}
	
	void Init(class World& world);
	
	void Stop();
	
	void Update();
	
	//Finds an intersection between the given ray and the water.
	// Returns a pair of distance and particleIndex.
	// particleIndex will be -1 if no intersection was found.
	std::pair<float, int> RayIntersect(const eg::Ray& ray) const;
	
	//Changes gravitation for all particles connected to the given particle.
	void ChangeGravity(int particle, Dir newGravity)
	{
		m_changeGravityParticleMT = particle;
		m_newGravityMT = newGravity;
	}
	
	eg::BufferRef GetPositionsBuffer() const;
	
	uint32_t NumParticles() const
	{
		return m_numParticles;
	}
	
	void AddQueryAABB(std::weak_ptr<QueryAABB> queryAABB)
	{
		m_queryAABBs.push_back(std::move(queryAABB));
	}
	
	//Returns the number of nanoseconds for the last update
	uint64_t LastUpdateTime() const
	{
		return m_lastUpdateTime;
	}
	
private:
	void Step(float dt, const eg::AABB& playerAABB, int changeGravityParticle, Dir newGravity);
	
	void ThreadTarget();
	
	alignas(16) int32_t m_cellProcOffsets[4 * 3 * 3 * 3];
	
	std::mt19937 m_rng;
	
	std::mutex m_mutex;
	bool m_run = false;
	
	std::atomic_uint64_t m_lastUpdateTime { 0 };
	
	std::vector<std::weak_ptr<QueryAABB>> m_queryAABBs;
	std::vector<std::shared_ptr<QueryAABB>> m_queryAABBsBT;
	
	uint32_t m_numParticles;
	
	int m_changeGravityParticleMT = -1;
	Dir m_newGravityMT;
	std::vector<std::pair<int, Dir>> m_changeGravityParticles;
	
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
	glm::vec4* m_particlePosMT;
	
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
	
	std::thread m_thread;
};
