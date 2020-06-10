#include "WaterSimulator.hpp"
#include "../World/World.hpp"
#include "../World/Player.hpp"
#include "../World/Entities/EntTypes/WaterPlaneEnt.hpp"
#include "../Vec3Compare.hpp"

#include <cstdlib>
#include <execution>

struct WaterSimulatorImpl;

WaterSimulatorImpl* WSI_New(const glm::ivec3& minBounds, const glm::ivec3& maxBounds, uint8_t* isAirBuffer,
	size_t numParticles, const float* particlePositions);

void WSI_Delete(WaterSimulatorImpl* impl);

void WSI_GetPositions(WaterSimulatorImpl* impl, void* destination);

int WSI_Query(WaterSimulatorImpl* impl, const eg::AABB& aabb, glm::vec3& waterVelocity, glm::vec3& buoyancy);

void WSI_SwapBuffers(WaterSimulatorImpl* impl);

void WSI_Simulate(WaterSimulatorImpl* impl, float dt, int changeGravityParticle, Dir newGravity);

WaterSimulator::WaterSimulator()
{
	
}

void WaterSimulator::Init(World& world)
{
	Stop();
	
	auto [worldBoundsMin, worldBoundsMax] = world.CalculateBounds();
	glm::ivec3 worldSize = worldBoundsMax - worldBoundsMin;
	
	//Generates a vector of all underwater cells
	std::vector<glm::ivec3> underwaterCells;
	world.entManager.ForEachOfType<WaterPlaneEnt>([&] (WaterPlaneEnt& waterPlaneEntity)
	{
		//Add all cells from this liquid plane to the vector
		waterPlaneEntity.liquidPlane.MaybeUpdate(world);
		for (glm::ivec3 cell : waterPlaneEntity.liquidPlane.UnderwaterCells())
		{
			underwaterCells.push_back(cell);
		}
	});
	if (underwaterCells.empty())
		return;
	
	//Removes duplicates from the list of underwater cells
	std::sort(underwaterCells.begin(), underwaterCells.end(), IVec3Compare());
	size_t lastCell = std::unique(underwaterCells.begin(), underwaterCells.end()) - underwaterCells.begin();
	
	//The number of particles to generate per underwater cell
	const glm::ivec3 GEN_PER_VOXEL(3, 4, 3);
	
	std::uniform_real_distribution<float> offsetDist(0.3f, 0.7f);
	
	std::mt19937 rng(std::time(nullptr));
	
	//Generates particles
	std::vector<float> positions;
	for (size_t i = 0; i < lastCell; i++)
	{
		glm::vec3 basePos(underwaterCells[i]);
		for (int x = 0; x < GEN_PER_VOXEL.x; x++)
		{
			for (int y = 0; y < GEN_PER_VOXEL.y; y++)
			{
				for (int z = 0; z < GEN_PER_VOXEL.z; z++)
				{
					glm::vec3 pos = basePos + glm::vec3(x + offsetDist(rng), y + offsetDist(rng), z + offsetDist(rng)) / glm::vec3(GEN_PER_VOXEL);
					positions.push_back(pos.x);
					positions.push_back(pos.y);
					positions.push_back(pos.z);
				}
			}
		}
	}
	
	if (positions.empty())
		return;
	
	//Copies voxel air data to isVoxelAir
	uint8_t* isVoxelAir = (uint8_t*)std::calloc((worldSize.x * worldSize.y * worldSize.z + 7) / 8, 1);
	for (int z = 0; z < worldSize.z; z++)
	{
		for (int y = 0; y < worldSize.y; y++)
		{
			for (int x = 0; x < worldSize.x; x++)
			{
				size_t index = x + y * worldSize.x + z * worldSize.x * worldSize.y;
				isVoxelAir[index / 8] |= (uint8_t)world.IsAir(glm::ivec3(x, y, z) + worldBoundsMin) << (index % 8);
			}
		}
	}
	
	m_numParticles = positions.size() / 3;
	m_impl = WSI_New(worldBoundsMin, worldBoundsMax, isVoxelAir, m_numParticles, positions.data());
	
	uint64_t bufferSize = sizeof(float) * 4 * m_numParticles;
	m_positionsBuffer = eg::Buffer(eg::BufferFlags::CopyDst | eg::BufferFlags::VertexBuffer, bufferSize, nullptr);
	
	m_positionsUploadBuffer = eg::Buffer(eg::BufferFlags::HostAllocate | eg::BufferFlags::CopySrc | eg::BufferFlags::MapWrite,
		bufferSize * eg::MAX_CONCURRENT_FRAMES, nullptr);
	m_positionsUploadBufferMemory = (char*)m_positionsUploadBuffer.Map(0, bufferSize * eg::MAX_CONCURRENT_FRAMES);
	
	m_run = true;
	m_thread = std::thread(&WaterSimulator::ThreadTarget, this);
}

void WaterSimulator::Stop()
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (!m_run)
			return;
		m_run = false;
	}
	m_thread.join();
	m_thread = { };
	WSI_Delete(m_impl);
	m_numParticles = 0;
	m_currentParticlePositions = nullptr;
	m_positionsUploadBufferMemory = nullptr;
	m_positionsUploadBuffer = { };
	m_positionsBuffer = { };
}

static int* stepsPerSecondVar = eg::TweakVarInt("water_sps", 60, 1);

void WaterSimulator::ThreadTarget()
{
	using Clock = std::chrono::high_resolution_clock;
	
	bool firstStep = true;
	Clock::time_point lastStepEnd = Clock::now();
	while (true)
	{
		int changeGravityParticle = -1;
		Dir newGravity = Dir::PosX; 
		int stepsPerSecond;
		
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (!m_run)
				break;
			
			if (!firstStep)
				WSI_SwapBuffers(m_impl);
			else
				firstStep = false;
			stepsPerSecond = *stepsPerSecondVar;
			
			if (!m_changeGravityParticles.empty())
			{
				changeGravityParticle = m_changeGravityParticles.back().first;
				newGravity = m_changeGravityParticles.back().second;
				m_changeGravityParticles.pop_back();
			}
		}
		
		WSI_Simulate(m_impl, 1.0f / stepsPerSecond, changeGravityParticle, newGravity);
		
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
		
		std::this_thread::sleep_until(lastStepEnd + std::chrono::nanoseconds(1000000000LL / stepsPerSecond));
		lastStepEnd = Clock::now();
	}
}

eg::BufferRef WaterSimulator::GetPositionsBuffer() const
{
	return m_positionsBuffer;
}

void WaterSimulator::Update()
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
	
	const uint64_t uploadBufferRange = m_numParticles * 4 * sizeof(float);
	const uint64_t uploadBufferOffset = eg::CFrameIdx() * uploadBufferRange;
	
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		
		//Copies AABB data for the background thread
		m_queryAABBsBT.clear();
		for (int64_t i = (int64_t)m_queryAABBs.size() - 1 ; i >= 0; i--)
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
		
		if (m_changeGravityParticleMT != -1)
		{
			m_changeGravityParticles.emplace_back(m_changeGravityParticleMT, m_newGravityMT);
			m_changeGravityParticleMT = -1;
		}
		
		WSI_GetPositions(m_impl, m_positionsUploadBufferMemory + uploadBufferOffset);
	}
	
	m_changeGravityParticleMT = -1;
	
	m_positionsUploadBuffer.Flush(uploadBufferOffset, uploadBufferRange);
	m_currentParticlePositions = reinterpret_cast<float*>(m_positionsUploadBufferMemory + uploadBufferOffset);
	
	eg::DC.CopyBuffer(m_positionsUploadBuffer, m_positionsBuffer, uploadBufferOffset, 0, uploadBufferRange);
	m_positionsBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
}

std::pair<float, int> WaterSimulator::RayIntersect(const eg::Ray& ray) const
{
	float minDst = INFINITY;
	int particle = -1;
	if (m_currentParticlePositions != nullptr)
	{
		for (uint32_t i = 0; i < m_numParticles; i++)
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
					particle = i;
				}
			}
		}
	}
	return { minDst, particle };
}
