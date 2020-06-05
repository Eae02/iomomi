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
	void ThreadTarget();
	
	std::mutex m_mutex;
	bool m_run = false;
	
	std::atomic_uint64_t m_lastUpdateTime { 0 };
	
	std::vector<std::weak_ptr<QueryAABB>> m_queryAABBs;
	std::vector<std::shared_ptr<QueryAABB>> m_queryAABBsBT;
	
	uint32_t m_numParticles = 0;
	
	int m_changeGravityParticleMT = -1;
	Dir m_newGravityMT;
	std::vector<std::pair<int, Dir>> m_changeGravityParticles;
	
	struct WaterSimulatorImpl* m_impl;
	
	eg::Buffer m_positionsUploadBuffer;
	char* m_positionsUploadBufferMemory;
	eg::Buffer m_positionsBuffer;
	
	volatile const float* m_currentParticlePositions = nullptr;
	
	std::thread m_thread;
};
