#include "IWaterSimulator.hpp"

#ifdef IOMOMI_ENABLE_WATER
#include "WaterSimulatorImpl.hpp"
#include "../RenderSettings.hpp"
#include "../../World/World.hpp"
#include "../../World/Player.hpp"
#include "../../World/Entities/EntTypes/WaterPlaneEnt.hpp"
#include "../../World/Entities/EntTypes/PumpEnt.hpp"
#include "../../World/Entities/Components/WaterBlockComp.hpp"

#include <cstdlib>
#include <execution>
#include <unordered_set>
#include <glm/gtx/hash.hpp>
#include <pcg_random.hpp>

static std::vector<float> GenerateWater(World& world)
{
	std::unordered_set<glm::ivec3> alreadyGenerated;
	
	std::uniform_real_distribution<float> offsetDist(0.3f, 0.7f);
	
	pcg32_fast rng(0);
	std::vector<float> positions;
	
	//Generates water
	world.entManager.ForEachOfType<WaterPlaneEnt>([&] (WaterPlaneEnt& waterPlaneEntity)
	{
		//Generates water for all underwater cells in this plane
		waterPlaneEntity.liquidPlane.MaybeUpdate(world);
		for (glm::ivec3 cell : waterPlaneEntity.liquidPlane.UnderwaterCells())
		{
			if (alreadyGenerated.count(cell))
				continue;
			alreadyGenerated.insert(cell);
			
			glm::ivec3 generatePerVoxel(3, 4, 3);
			if (waterPlaneEntity.liquidPlane.IsUnderwater(cell + glm::ivec3(0, 1, 0)))
				generatePerVoxel.y += waterPlaneEntity.densityBoost;
			
			for (int x = 0; x < generatePerVoxel.x; x++)
			{
				for (int y = 0; y < generatePerVoxel.y; y++)
				{
					for (int z = 0; z < generatePerVoxel.z; z++)
					{
						glm::vec3 pos = glm::vec3(cell) +
							glm::vec3(
								static_cast<float>(x) + offsetDist(rng),
								static_cast<float>(y) + offsetDist(rng),
								static_cast<float>(z) + offsetDist(rng)) / glm::vec3(generatePerVoxel);
						if (pos.y > waterPlaneEntity.GetPosition().y)
							continue;
						positions.push_back(pos.x);
						positions.push_back(pos.y);
						positions.push_back(pos.z);
					}
				}
			}
		}
	});
	
	return positions;
}

static int* stepsPerSecondVar = eg::TweakVarInt("water_sps", 60, 1);

inline __m128 Vec3ToM128(const glm::vec3& v3)
{
	alignas(16) float data[4];
	data[0] = v3.x;
	data[1] = v3.y;
	data[2] = v3.z;
	data[3] = 0;
	return _mm_load_ps(data);
}

class WaterSimulator : public IWaterSimulator
{
public:
	static constexpr float GAME_TIME_OFFSET = 100;
	
	struct QueryAABB : IQueryAABB
	{
		friend class WaterSimulator;
		
		void SetAABB(const eg::AABB& aabb) override
		{
			m_aabbMT = aabb;
		}
		
		QueryResults GetResults() override
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			return m_results;
		}
		
		std::mutex m_mutex;
		QueryResults m_results;
		
		eg::AABB m_aabbMT;
		eg::AABB m_aabbBT;
	};
	
	WaterSimulator(World& world, std::span<const float> positions)
	{
		auto [worldBoundsMin, worldBoundsMax] = world.voxels.CalculateBounds();
		glm::ivec3 worldSize = worldBoundsMax - worldBoundsMin;
		
		//Copies voxel air data to isVoxelAir
		uint8_t* isVoxelAir = (uint8_t*)std::calloc((worldSize.x * worldSize.y * worldSize.z + 7) / 8, 1);
		for (int z = 0; z < worldSize.z; z++)
		{
			for (int y = 0; y < worldSize.y; y++)
			{
				for (int x = 0; x < worldSize.x; x++)
				{
					size_t index = x + y * worldSize.x + z * worldSize.x * worldSize.y;
					if (world.voxels.IsAir(glm::ivec3(x, y, z) + worldBoundsMin))
						isVoxelAir[index / 8] |= static_cast<uint8_t>(1 << (index % 8));
				}
			}
		}
		
		m_numParticles = eg::UnsignedNarrow<uint32_t>(positions.size() / 3) + world.extraWaterParticles;
		
		WSINewArgs newArgs;
		newArgs.minBounds = worldBoundsMin;
		newArgs.maxBounds = worldBoundsMax;
		newArgs.isAirBuffer = isVoxelAir;
		newArgs.numParticles = eg::UnsignedNarrow<uint32_t>(positions.size() / 3);
		newArgs.extraParticles = world.extraWaterParticles;
		newArgs.particlePositions = positions.data();
		m_impl = WSI_New(newArgs);
		
		uint64_t bufferSize = sizeof(float) * 4 * m_numParticles;
		m_positionsBuffer = eg::Buffer(eg::BufferFlags::CopyDst | eg::BufferFlags::StorageBuffer, bufferSize, nullptr);
		
		m_positionsUploadBuffer = eg::Buffer(
			eg::BufferFlags::HostAllocate | eg::BufferFlags::CopySrc | eg::BufferFlags::MapWrite,
			bufferSize * eg::MAX_CONCURRENT_FRAMES, nullptr);
		m_positionsUploadBufferMemory = static_cast<char*>(m_positionsUploadBuffer.Map(0, bufferSize * eg::MAX_CONCURRENT_FRAMES));
		
		m_lastGravityBufferVersion = UINT32_MAX;
		m_presimIterationsCompleted = 0;
		m_targetPresimIterations = world.waterPresimIterations;
		m_run = true;
		m_pausedSH = true;
		m_thread = std::thread(&WaterSimulator::ThreadTarget, this);
	}
	
	~WaterSimulator()
	{
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (!m_run)
				return;
			m_run = false;
			m_pausedSH = false;
			m_unpausedSignal.notify_one();
		}
		m_thread.join();
		WSI_Delete(m_impl);
	}
	
	bool IsPresimComplete() override
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return !m_run || m_presimIterationsCompleted >= m_targetPresimIterations;
	}
	
	void Update(const World& world, const glm::vec3& cameraPos, bool paused) override
	{
		if (m_numParticles == 0)
		{
			for (const std::weak_ptr<QueryAABB>& queryAABB : m_queryAABBs)
			{
				if (std::shared_ptr<QueryAABB> aabb = queryAABB.lock())
				{
					aabb->m_results.numIntersecting = 0;
					aabb->m_results.buoyancy = glm::vec3(0);
					aabb->m_results.waterVelocity = glm::vec3(0);
				}
			}
			return;
		}
		
		//Collects information about water blockers
		m_waterBlockersMT.clear();
		const_cast<EntityManager&>(world.entManager).ForEachWithComponent<WaterBlockComp>([&] (const Ent& entity)
		{
			const WaterBlockComp& component = *entity.GetComponent<WaterBlockComp>();
			
			if (!eg::Contains(component.blockedGravities, true))
				return;
			
			float tangentLen = glm::length(component.tangent);
			float biTangentLen = glm::length(component.biTangent);
			glm::vec3 normal = glm::normalize(glm::cross(component.tangent, component.biTangent));
			
			WaterBlocker blocker;
			blocker.tangent = Vec3ToM128(component.tangent / tangentLen);
			blocker.biTangent = Vec3ToM128(component.biTangent / biTangentLen);
			blocker.tangentLen = tangentLen;
			blocker.biTangentLen = biTangentLen;
			blocker.center = Vec3ToM128(component.center);
			blocker.blockedGravities = 0;
			for (int i = 0; i < 6; i++)
			{
				if (component.blockedGravities[i])
					blocker.blockedGravities |= static_cast<uint8_t>(1 << i);
			}
			
			for (int dir = -1; dir <= 1; dir += 2)
			{
				WaterBlocker& addedBlocker = m_waterBlockersMT.emplace_back(blocker);
				addedBlocker.center = Vec3ToM128(component.center + normal * (static_cast<float>(dir) * 0.1f));
				addedBlocker.normal = Vec3ToM128(normal * static_cast<float>(dir));
			}
		});
		
		//Collects information about water pumps
		m_waterPumpsMT.clear();
		const_cast<EntityManager&>(world.entManager).ForEachOfType<PumpEnt>([&] (const PumpEnt& ent)
		{
			if (std::optional<WaterPumpDescription> desc = ent.GetPumpDescription())
			{
				m_waterPumpsMT.push_back(*desc);
			}
		});
		
		const uint64_t uploadBufferOffset = eg::CFrameIdx() * m_numParticles * 4 * sizeof(float);
		const uint64_t gravitiesUploadBufferOffset = eg::CFrameIdx() * m_numParticles;
		bool gravitiesBufferChanged = false;
		
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			
			//Copies AABB data for the background thread
			m_queryAABBsBT.clear();
			for (int64_t i = eg::ToInt(m_queryAABBs.size()) - 1 ; i >= 0; i--)
			{
				if (std::shared_ptr<QueryAABB> aabb = m_queryAABBs[i].lock())
				{
					aabb->m_aabbBT = aabb->m_aabbMT;
					m_queryAABBsBT.push_back(std::move(aabb));
				}
				else
				{
					m_queryAABBs[i].swap(m_queryAABBs.back());
					m_queryAABBs.pop_back();
				}
			}
			
			m_waterBlockersSH = m_waterBlockersMT;
			m_waterPumpsSH = m_waterPumpsMT;
			
			if (m_changeGravityParticleMT.has_value())
			{
				bool shouldPushBack = true;
				if (m_changeGravityParticleMT->highlightOnly)
				{
					//If this is a highlight only particle reference, try to update an existing entry
					for (ChangeGravityParticleRef& enqueuedRef : m_changeGravityParticles)
					{
						if (enqueuedRef.highlightOnly)
						{
							enqueuedRef = *m_changeGravityParticleMT;
							shouldPushBack = false;
							break;
						}
					}
				}
				if (shouldPushBack)
				{
					m_changeGravityParticles.push_back(*m_changeGravityParticleMT);
				}
				m_changeGravityParticleMT.reset();
			}
			
			m_gameTimeSH = RenderSettings::instance->gameTime + GAME_TIME_OFFSET;
			m_pausedSH = paused;
			m_numParticlesToDraw = WSI_CopyToOutputBuffer(m_impl);
			
			if (m_gravitiesBuffer.handle)
			{
				uint32_t gravityBufferVersion;
				void* gravitiesBufferData = WSI_GetGravitiesOutputBuffer(m_impl, gravityBufferVersion);
				if (m_lastGravityBufferVersion != gravityBufferVersion)
				{
					std::memcpy(
						m_gravitiesUploadBufferMemory + gravitiesUploadBufferOffset,
						gravitiesBufferData,
						m_numParticles);
					m_lastGravityBufferVersion = gravityBufferVersion;
					gravitiesBufferChanged = true;
				}
			}
		}
		
		if (!paused)
		{
			m_unpausedSignal.notify_one();
		}
		
		if (gravitiesBufferChanged)
		{
			m_gravitiesUploadBuffer.Flush(gravitiesUploadBufferOffset, m_numParticles);
			eg::DC.CopyBuffer(m_gravitiesUploadBuffer, m_gravitiesBuffer, gravitiesUploadBufferOffset, 0, m_numParticles);
			m_gravitiesBuffer.UsageHint(eg::BufferUsage::StorageBufferRead, eg::ShaderAccessFlags::Compute);
			eg::Log(eg::LogLevel::Info, "w", "updated water gravities buffer");
		}
		
		{
			alignas(16) float cameraPos4[] = { cameraPos.x, cameraPos.y, cameraPos.z, 0 };
			auto sortTimer = eg::StartCPUTimer("Water Sort");
			WSI_SortOutputBuffer(m_impl, m_numParticlesToDraw, cameraPos4);
			std::memcpy(
				m_positionsUploadBufferMemory + uploadBufferOffset,
				WSI_GetOutputBuffer(m_impl),
				m_numParticlesToDraw * sizeof(float) * 4
			);
		}
		
		const uint64_t uploadBufferRange = m_numParticlesToDraw * 4 * sizeof(float);
		
		m_changeGravityParticleMT.reset();
		
		m_positionsUploadBuffer.Flush(uploadBufferOffset, uploadBufferRange);
		m_currentParticlePositions = reinterpret_cast<float*>(m_positionsUploadBufferMemory + uploadBufferOffset);
		
		eg::DC.CopyBuffer(m_positionsUploadBuffer, m_positionsBuffer, uploadBufferOffset, 0, uploadBufferRange);
		m_positionsBuffer.UsageHint(eg::BufferUsage::StorageBufferRead, eg::ShaderAccessFlags::Vertex);
	}
	
	void ThreadTarget()
	{
		using Clock = std::chrono::high_resolution_clock;
		
		std::vector<WaterBlocker> waterBlockers;
		std::vector<WaterPumpDescription> waterPumps;
		
		bool presimDone = false;
		Clock::time_point lastStepEnd = Clock::now();
		while (true)
		{
			WSISimulateArgs simulateArgs;
			simulateArgs.shouldChangeParticleGravity = false;
			int stepsPerSecond;
			
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				while (m_pausedSH)
				{
					m_unpausedSignal.wait(lock);
				}
				
				if (!m_run)
					break;
				
				if (m_presimIterationsCompleted != 0)
					WSI_SwapBuffers(m_impl);
				if (m_presimIterationsCompleted >= m_targetPresimIterations)
					presimDone = true;
				else
					m_presimIterationsCompleted++;
				
				stepsPerSecond = *stepsPerSecondVar;
				
				waterBlockers = m_waterBlockersSH;
				waterPumps = m_waterPumpsSH;
				simulateArgs.gameTime = m_gameTimeSH;
				
				if (!m_changeGravityParticles.empty())
				{
					simulateArgs.shouldChangeParticleGravity        = true;
					simulateArgs.changeGravityParticlePos           = m_changeGravityParticles.back().particlePos;
					simulateArgs.newGravity                         = m_changeGravityParticles.back().newGravity;
					simulateArgs.changeGravityParticleHighlightOnly = m_changeGravityParticles.back().highlightOnly;
					m_changeGravityParticles.pop_back();
				}
			}
			
			simulateArgs.dt = 1.0f / static_cast<float>(stepsPerSecond);
			simulateArgs.waterBlockers = waterBlockers;
			simulateArgs.waterPumps = waterPumps;
			WSI_Simulate(m_impl, simulateArgs);
			
			for (const std::shared_ptr<QueryAABB>& qaabb : m_queryAABBsBT)
			{
				glm::vec3 waterVelocity;
				glm::vec3 buoyancy;
				int numIntersecting = WSI_Query(m_impl, qaabb->m_aabbBT, waterVelocity, buoyancy);
				
				std::lock_guard<std::mutex> lock(qaabb->m_mutex);
				qaabb->m_results.numIntersecting = numIntersecting;
				qaabb->m_results.waterVelocity = waterVelocity;
				qaabb->m_results.buoyancy = buoyancy;
			}
			
			m_lastUpdateTime = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - lastStepEnd).count();
			
			if (presimDone)
				std::this_thread::sleep_until(lastStepEnd + std::chrono::nanoseconds(1000000000LL / stepsPerSecond));
			lastStepEnd = Clock::now();
		}
	}
	
	std::pair<float, glm::vec3> RayIntersect(const eg::Ray& ray) const override
	{
		float minDst = INFINITY;
		glm::vec3 particlePos(0.0f);
		if (m_currentParticlePositions != nullptr)
		{
			for (uint32_t i = 0; i < m_numParticlesToDraw; i++)
			{
				glm::vec3 posCopy(m_currentParticlePositions[i * 4],
					m_currentParticlePositions[i * 4 + 1],
					m_currentParticlePositions[i * 4 + 2]);
				float dst;
				if (ray.Intersects(eg::Sphere(posCopy, 0.3f), dst))
				{
					if (dst > 0 && dst < minDst)
					{
						minDst = dst;
						particlePos = posCopy;
					}
				}
			}
		}
		return { minDst, particlePos };
	}
	
	void ChangeGravity(const glm::vec3& particlePos, Dir newGravity, bool highlightOnly) override
	{
		m_changeGravityParticleMT = ChangeGravityParticleRef { particlePos, newGravity, highlightOnly };
	}
	
	void EnableGravitiesGPUBuffer() override
	{
		if (m_gravitiesBuffer.handle)
			return;
		
		uint64_t gravitiesBufferSize = sizeof(uint8_t) * eg::RoundToNextMultiple(m_numParticles, 4);
		m_gravitiesBuffer = eg::Buffer(
			eg::BufferFlags::CopyDst | eg::BufferFlags::StorageBuffer, gravitiesBufferSize,
			nullptr);
		
		m_gravitiesUploadBuffer = eg::Buffer(
			eg::BufferFlags::HostAllocate | eg::BufferFlags::CopySrc | eg::BufferFlags::MapWrite,
			gravitiesBufferSize * eg::MAX_CONCURRENT_FRAMES, nullptr);
		m_gravitiesUploadBufferMemory = (char*)m_gravitiesUploadBuffer.Map(0, gravitiesBufferSize * eg::MAX_CONCURRENT_FRAMES);
	}
	
	eg::BufferRef GetPositionsGPUBuffer() const override { return m_positionsBuffer; }
	eg::BufferRef GetGravitiesGPUBuffer() const override { return m_gravitiesBuffer; }
	
	uint32_t NumParticles() const override { return m_numParticles; }
	uint32_t NumParticlesToDraw() const override { return m_numParticlesToDraw; }
	uint64_t LastUpdateTime() const override { return m_lastUpdateTime; }
	
	std::shared_ptr<IQueryAABB> AddQueryAABB(const eg::AABB& aabb) override
	{
		std::shared_ptr<QueryAABB> queryAABB = std::make_shared<QueryAABB>();
		queryAABB->m_aabbMT = aabb;
		m_queryAABBs.emplace_back(queryAABB);
		return queryAABB;
	}
	
	std::mutex m_mutex;
	bool m_run = false;
	
	uint32_t m_presimIterationsCompleted = 0;
	uint32_t m_targetPresimIterations = 0;
	
	uint32_t m_lastGravityBufferVersion = 0;
	
	std::atomic_uint64_t m_lastUpdateTime { 0 };
	
	std::vector<std::weak_ptr<QueryAABB>> m_queryAABBs;
	std::vector<std::shared_ptr<QueryAABB>> m_queryAABBsBT;
	
	uint32_t m_numParticles = 0;
	uint32_t m_numParticlesToDraw = 0;
	
	struct ChangeGravityParticleRef
	{
		glm::vec3 particlePos;
		Dir newGravity;
		bool highlightOnly;
	};
	
	std::optional<ChangeGravityParticleRef> m_changeGravityParticleMT;
	std::vector<ChangeGravityParticleRef> m_changeGravityParticles;
	
	std::vector<WaterBlocker> m_waterBlockersMT;
	std::vector<WaterBlocker> m_waterBlockersSH;
	
	std::vector<WaterPumpDescription> m_waterPumpsMT;
	std::vector<WaterPumpDescription> m_waterPumpsSH;
	
	float m_gameTimeSH = 0;
	
	bool m_pausedSH = false;
	std::condition_variable m_unpausedSignal;
	
	struct WaterSimulatorImpl* m_impl;
	
	eg::Buffer m_positionsUploadBuffer;
	char* m_positionsUploadBufferMemory;
	eg::Buffer m_positionsBuffer;
	
	eg::Buffer m_gravitiesUploadBuffer;
	char* m_gravitiesUploadBufferMemory;
	eg::Buffer m_gravitiesBuffer;
	
	volatile const float* m_currentParticlePositions = nullptr;
	
	std::thread m_thread;
};

std::unique_ptr<IWaterSimulator> CreateWaterSimulator(class World& world)
{
	std::vector<float> waterPositions = GenerateWater(world);
	if (waterPositions.empty() && world.extraWaterParticles == 0)
		return nullptr;
	return std::make_unique<WaterSimulator>(world, waterPositions);
}

#else

std::unique_ptr<IWaterSimulator> CreateWaterSimulator(class World& world) { return nullptr; }

#endif
