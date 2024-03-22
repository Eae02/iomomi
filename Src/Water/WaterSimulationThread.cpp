#include "WaterSimulationThread.hpp"
#include "WaterConstants.hpp"

#include <unordered_map>
#include <unordered_set>

WaterSimulationThread::WaterSimulationThread(glm::vec3 gridOrigin, uint32_t numParticles)
	: m_gridOrigin(gridOrigin), m_numParticles(numParticles),
	  m_particleDataMT(std::make_unique<glm::uvec2[]>(numParticles)),
	  m_particleDataBT(std::make_unique<glm::uvec2[]>(numParticles)), m_thread([this] { ThreadTarget(); })
{
}

WaterSimulationThread::~WaterSimulationThread()
{
	m_mutex.lock();
	m_shouldStop = true;
	m_mutex.unlock();
	m_signalForBackThread.notify_one();
	m_thread.join();
}

static inline glm::ivec3 CellIDToV3(uint32_t id)
{
	constexpr uint32_t MASK = 0x3FF;
	return glm::ivec3(id & MASK, (id >> 10) & MASK, (id >> 20) & MASK);
}

static inline uint32_t CellV3ToID(glm::ivec3 pos)
{
	if (glm::any(glm::lessThan(pos, glm::ivec3(0))) || glm::any(glm::greaterThanEqual(pos, glm::ivec3(1024))))
		return UINT32_MAX;
	return static_cast<uint32_t>(pos.x) | (static_cast<uint32_t>(pos.y) << 10) | (static_cast<uint32_t>(pos.z) << 20);
}

void WaterSimulationThread::ThreadTarget()
{
	std::vector<std::weak_ptr<WaterQueryAABB>> waterQueries;

	std::vector<GravityChangeResult> gravityChangeResults;

	std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> cellIDToIndexRange;

	while (true)
	{
		{
			std::unique_lock lock(m_mutex);
			m_signalForBackThread.wait(lock, [&] { return m_workAvailable || m_shouldStop; });
			if (m_shouldStop)
				break;
			m_workAvailable = false;
			std::swap(m_particleDataMT, m_particleDataBT);
			std::swap(m_gravityChangesMT, m_gravityChangesBT);
			m_gravityChangesMT.clear();

			for (std::weak_ptr<WaterQueryAABB>& newQuery : m_newWaterQueries)
				waterQueries.push_back(std::move(newQuery));
			m_newWaterQueries.clear();
		}

		// Initializes cellIDToIndexRange
		cellIDToIndexRange.clear();
		cellIDToIndexRange.emplace(m_particleDataBT[0].x, std::make_pair(0, 0));
		for (uint32_t i = 1; i < m_numParticles; i++)
		{
			if (m_particleDataBT[i].x == m_particleDataBT[i - 1].x)
				continue;
			cellIDToIndexRange.emplace(m_particleDataBT[i].x, std::make_pair(i, 0));
			cellIDToIndexRange[m_particleDataBT[i - 1].x].second = i;
		}
		cellIDToIndexRange[m_particleDataBT[m_numParticles - 1].x].second = m_numParticles;

		// Updates water queries
		for (int64_t i = static_cast<int64_t>(waterQueries.size()) - 1; i >= 0; i--)
		{
			std::shared_ptr<WaterQueryAABB> query = waterQueries[i].lock();

			// Remove the query if it has no strong references
			if (query == nullptr)
			{
				std::swap(waterQueries[i], waterQueries.back());
				waterQueries.pop_back();
				continue;
			}

			query->m_mutex.lock();
			eg::AABB aabb = query->m_aabb;
			query->m_mutex.unlock();

			glm::ivec3 minRange = glm::floor((aabb.min - m_gridOrigin) / W_INFLUENCE_RADIUS);
			glm::ivec3 maxRange = glm::ceil((aabb.max - m_gridOrigin) / W_INFLUENCE_RADIUS);

			WaterQueryResults queryResults;

			for (int z = minRange.z; z < maxRange.z; z++)
				for (int y = minRange.y; y < maxRange.y; y++)
					for (int x = minRange.x; x < maxRange.x; x++)
					{
						uint32_t cellID = CellV3ToID(glm::ivec3(x, y, z));
						auto cellIt = cellIDToIndexRange.find(cellID);
						if (cellIt == cellIDToIndexRange.end())
							continue;
						auto [indexLo, indexHi] = cellIt->second;
						queryResults.numIntersecting += indexHi - indexLo;

						for (uint32_t idx = indexLo; idx < indexHi; idx++)
						{
							uint32_t gravityIndex = m_particleDataBT[idx].y & 0x7;
							if (gravityIndex < 6)
								queryResults.buoyancy -= DirectionVector(static_cast<Dir>(gravityIndex));
						}
					}

			query->m_mutex.lock();
			query->m_results = queryResults;
			query->m_mutex.unlock();
		}

		// Processes gravity changes
		for (GravityChange gravityChange : m_gravityChangesBT)
		{
			std::vector<glm::ivec3> gridCellsDfsStack;
			std::unordered_set<uint32_t> visitedCells;

			std::unique_ptr<uint32_t[]> changeGravityBits = std::make_unique<uint32_t[]>(m_numParticles / 32);
			std::memset(changeGravityBits.get(), 0, (m_numParticles / 32) * sizeof(uint32_t));
			auto MarkGravityBits = [&](uint32_t lo, uint32_t hi)
			{
				for (uint32_t i = lo; i < hi; i++)
				{
					uint32_t persistentID = m_particleDataBT[i].y >> 16;
					if (persistentID < m_numParticles)
					{
						changeGravityBits[persistentID / 32] |= 1u << (persistentID % 32);
					}
				}
			};

			for (auto [cellId, cellRange] : cellIDToIndexRange)
			{
				glm::ivec3 cellPos = CellIDToV3(cellId);
				glm::vec3 cellPosWorld = glm::vec3(cellPos) * W_INFLUENCE_RADIUS + m_gridOrigin;
				eg::Sphere cellSphere(cellPosWorld + glm::vec3(W_INFLUENCE_RADIUS / 2.0f), W_INFLUENCE_RADIUS);

				float intersectDist;
				if (gravityChange.ray.Intersects(cellSphere, intersectDist) &&
				    intersectDist < gravityChange.maxDistance)
				{
					gridCellsDfsStack.push_back(cellPos);
					visitedCells.insert(cellId);
					MarkGravityBits(cellRange.first, cellRange.second);
				}
			}

			static const glm::ivec3 toNeighbors[6] = {
				glm::ivec3(-1, 0, 0), glm::ivec3(1, 0, 0),  glm::ivec3(0, -1, 0),
				glm::ivec3(0, 1, 0),  glm::ivec3(0, 0, -1), glm::ivec3(0, 0, 1),
			};

			while (!gridCellsDfsStack.empty())
			{
				glm::ivec3 currentPos = gridCellsDfsStack.back();
				gridCellsDfsStack.pop_back();
				for (glm::ivec3 toNeighbor : toNeighbors)
				{
					const glm::ivec3 neighborPos = currentPos + toNeighbor;
					const uint32_t neighborCellID = CellV3ToID(neighborPos);
					if (!visitedCells.contains(neighborCellID))
					{
						auto rangeIt = cellIDToIndexRange.find(neighborCellID);
						if (rangeIt != cellIDToIndexRange.end())
						{
							gridCellsDfsStack.push_back(neighborPos);
							visitedCells.insert(neighborCellID);
							auto [indexLo, indexHi] = rangeIt->second;
							MarkGravityBits(indexLo, indexHi);
						}
					}
				}
			}

			if (!visitedCells.empty())
			{
				gravityChangeResults.push_back(GravityChangeResult{
					.newGravity = gravityChange.newGravity,
					.changeGravityBits = std::move(changeGravityBits),
				});
			}
		}

		if (!gravityChangeResults.empty())
		{
			m_mutex.lock();
			for (GravityChangeResult& result : gravityChangeResults)
				m_gravityChangeResults.push_back(std::move(result));
			m_mutex.unlock();
			gravityChangeResults.clear();
		}
	}
}

void WaterSimulationThread::PushGravityChangeMT(const GravityChange& gravityChange)
{
	std::lock_guard lock(m_mutex);
	m_gravityChangesMT.push_back(gravityChange);
}

void WaterSimulationThread::AddQueryMT(std::weak_ptr<WaterQueryAABB> query)
{
	std::lock_guard lock(m_mutex);
	m_newWaterQueries.push_back(std::move(query));
}

void WaterSimulationThread::OnFrameBeginMT(std::span<const glm::uvec2> particleData)
{
	EG_ASSERT(particleData.size() == m_numParticles);

	std::unique_lock lock(m_mutex);

	std::memcpy(m_particleDataMT.get(), particleData.data(), particleData.size_bytes());
	m_workAvailable = true;

	m_signalForBackThread.notify_one();
}

std::optional<WaterSimulationThread::GravityChangeResult> WaterSimulationThread::PopGravityChangeResult()
{
	std::lock_guard lock(m_mutex);
	if (m_gravityChangeResults.empty())
		return std::nullopt;
	GravityChangeResult result = std::move(m_gravityChangeResults.front());
	m_gravityChangeResults.erase(m_gravityChangeResults.begin()); // The vector probably won't be very long
	return result;
}
