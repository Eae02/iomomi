#pragma once

#include "World/Dir.hpp"

class Water
{
public:
	Water();
	
	void InitializeWorld(const class World& world);
	
	void Step();
	
private:
	static constexpr int VOXEL_SUBDIV = 16;
	
	enum class CellMode : uint8_t
	{
		Solid,
		FallPX,
		FallNX,
		FallPY,
		FallNY,
		FallPZ,
		FallNZ
	};
	
	struct Region
	{
		glm::ivec3 minPos;
		glm::ivec3 size;
		std::unique_ptr<uint8_t[]> density;
		std::unique_ptr<CellMode[]> mode;
		
		int Idx(const glm::ivec3& pos) const
		{
			glm::ivec3 rel = pos - minPos;
			return rel.x + rel.y * size.x + rel.z * size.y * size.z;
		}
		
		template <Dir Down>
		void Step();
	};
	
	std::vector<Region> m_regions;
};
