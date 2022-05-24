#pragma once

#include "../../World/Dir.hpp"

class IWaterSimulator
{
public:
	struct QueryResults
	{
		int numIntersecting;
		glm::vec3 waterVelocity;
		glm::vec3 buoyancy;
	};
	
	struct IQueryAABB
	{
		virtual ~IQueryAABB() { }
		virtual void SetAABB(const eg::AABB& aabb) = 0;
		virtual QueryResults GetResults() = 0;
	};
	
	virtual ~IWaterSimulator() { }
	
	virtual void Update(const class World& world, const glm::vec3& cameraPos, bool paused) = 0;
	
	//Finds an intersection between the given ray and the water.
	// Returns a pair of distance and particle position.
	// distance will be infinity if no intersection was found.
	virtual std::pair<float, glm::vec3> RayIntersect(const eg::Ray& ray) const = 0;
	
	//Changes gravitation for all particles close to and connected to the given position.
	virtual void ChangeGravity(const glm::vec3& particlePos, Dir newGravity, bool highlightOnly = false) = 0;
	
	virtual eg::BufferRef GetPositionsGPUBuffer() const = 0;
	virtual eg::BufferRef GetGravitiesGPUBuffer() const = 0;
	virtual void EnableGravitiesGPUBuffer() = 0;
	
	virtual uint32_t NumParticles() const = 0;
	virtual uint32_t NumParticlesToDraw() const = 0;
	
	virtual std::shared_ptr<IQueryAABB> AddQueryAABB(const eg::AABB& aabb) = 0;
	
	//Returns the number of nanoseconds for the last update
	virtual uint64_t LastUpdateTime() const = 0;
	
	virtual bool IsPresimComplete() = 0;
};

std::unique_ptr<IWaterSimulator> CreateWaterSimulator(class World& world);

inline std::pair<float, glm::vec3> WaterRayIntersect(const IWaterSimulator* simulator, const eg::Ray& ray)
{
	if (simulator == nullptr)
		return { INFINITY, glm::vec3(0.0f) };
	return simulator->RayIntersect(ray);
}
