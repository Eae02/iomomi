#pragma once

#include "Dir.hpp"
#include <glm/gtx/hash.hpp>

struct VoxelRayIntersectResult
{
	bool intersected;
	glm::ivec3 voxelPosition;
	glm::vec3 intersectPosition;
	Dir normalDir;
	float intersectDist;
};

class VoxelBuffer
{
public:
	friend class World;
	
	void SetIsAir(const glm::ivec3& pos, bool isAir);
	
	bool IsAir(const glm::ivec3& pos) const
	{
		return m_voxels.count(pos) != 0;
	}
	
	void SetMaterialSafe(const glm::ivec3& pos, Dir side, int material);
	void SetMaterial(const glm::ivec3& pos, Dir side, int material);
	int GetMaterial(const glm::ivec3& pos, Dir side) const;
	std::optional<int> GetMaterialIfVisible(const glm::ivec3& pos, Dir side) const;
	
	bool IsGravityCorner(const glm::ivec3& cornerPos, Dir cornerDir) const;
	void SetIsGravityCorner(const glm::ivec3& cornerPos, Dir cornerDir, bool value);
	bool IsCorner(const glm::ivec3& cornerPos, Dir cornerDir) const
	{
		return GetGravityCornerVoxelPos(cornerPos, cornerDir).w != -1;
	}
	
	VoxelRayIntersectResult RayIntersect(const eg::Ray& ray) const;
	
	std::pair<glm::ivec3, glm::ivec3> CalculateBounds() const;
	
private:
	glm::ivec4 GetGravityCornerVoxelPos(glm::ivec3 cornerPos, Dir cornerDir) const;
	
	struct AirVoxel
	{
		//Tracks which material to use for each face
		uint8_t materials[6];
		std::bitset<12> hasGravityCorner;
		
		AirVoxel() : materials { }, hasGravityCorner(0) { }
	};
	
	std::unordered_map<glm::ivec3, AirVoxel> m_voxels;
	bool m_modified = false;
};
