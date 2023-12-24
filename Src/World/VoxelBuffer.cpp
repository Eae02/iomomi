#include "VoxelBuffer.hpp"

std::pair<glm::ivec3, glm::ivec3> VoxelBuffer::CalculateBounds() const
{
	glm::ivec3 boundsMin(INT_MAX);
	glm::ivec3 boundsMax(INT_MIN);
	for (const auto& voxel : m_voxels)
	{
		boundsMin = glm::min(boundsMin, voxel.first);
		boundsMax = glm::max(boundsMax, voxel.first);
	}
	boundsMin -= 1;
	boundsMax += 2;
	return std::make_pair(boundsMin, boundsMax);
}

void VoxelBuffer::SetIsAir(const glm::ivec3& pos, bool air)
{
	auto voxelIt = m_voxels.find(pos);
	bool alreadyAir = voxelIt != m_voxels.end();

	if (alreadyAir == air)
		return;

	if (alreadyAir)
	{
		m_voxels.erase(voxelIt);
	}
	else
	{
		m_voxels.emplace(pos, AirVoxel());
	}
}

void VoxelBuffer::SetMaterialSafe(const glm::ivec3& pos, Dir side, int material)
{
	auto voxelIt = m_voxels.find(pos);
	if (voxelIt != m_voxels.end())
	{
		voxelIt->second.materials[static_cast<int>(side)] = material;
		m_modified = true;
	}
}

void VoxelBuffer::SetMaterial(const glm::ivec3& pos, Dir side, int material)
{
	auto voxelIt = m_voxels.find(pos);
	EG_ASSERT(voxelIt != m_voxels.end())
	voxelIt->second.materials[static_cast<int>(side)] = material;
	m_modified = true;
}

int VoxelBuffer::GetMaterial(const glm::ivec3& pos, Dir side) const
{
	auto voxelIt = m_voxels.find(pos);
	if (voxelIt == m_voxels.end())
		return 0;
	return voxelIt->second.materials[static_cast<int>(side)];
}

std::optional<int> VoxelBuffer::GetMaterialIfVisible(const glm::ivec3& pos, Dir side) const
{
	if (!IsAir(pos) || IsAir(pos - DirectionVector(side)))
		return {};
	auto voxelIt = m_voxels.find(pos);
	if (voxelIt == m_voxels.end())
		return {};
	return voxelIt->second.materials[static_cast<int>(side)];
}

glm::ivec4 VoxelBuffer::GetGravityCornerVoxelPos(glm::ivec3 cornerPos, Dir cornerDir) const
{
	const int cornerDim = static_cast<int>(cornerDir) / 2;
	if (static_cast<int>(cornerDir) % 2)
		cornerPos[cornerDim]--;

	const Dir uDir = static_cast<Dir>((cornerDim + 1) % 3 * 2);
	const Dir vDir = static_cast<Dir>((cornerDim + 2) % 3 * 2);
	const glm::ivec3 uV = DirectionVector(uDir);
	const glm::ivec3 vV = DirectionVector(vDir);

	int numAir = 0;
	glm::ivec2 airPos, notAirPos;
	for (int u = -1; u <= 0; u++)
	{
		for (int v = -1; v <= 0; v++)
		{
			glm::ivec3 pos = cornerPos + uV * u + vV * v;
			if (IsAir(pos))
			{
				numAir++;
				airPos = { u, v };
			}
			else
			{
				notAirPos = { u, v };
			}
		}
	}

	if (numAir == 3)
	{
		airPos.x = -1 - notAirPos.x;
		airPos.y = -1 - notAirPos.y;
	}
	else if (numAir != 1)
		return { 0, 0, 0, -1 };

	const int bit = cornerDim * 4 + (airPos.y + 1) * 2 + airPos.x + 1;
	return glm::ivec4(cornerPos + uV * airPos.x + vV * airPos.y, bit);
}

bool VoxelBuffer::IsGravityCorner(const glm::ivec3& cornerPos, Dir cornerDir) const
{
	glm::ivec4 voxelPos = GetGravityCornerVoxelPos(cornerPos, cornerDir);
	if (voxelPos.w == -1)
		return false;

	auto voxelIt = m_voxels.find(glm::ivec3(voxelPos));
	if (voxelIt == m_voxels.end())
		return false;

	return voxelIt->second.hasGravityCorner[voxelPos.w];
}

void VoxelBuffer::SetIsGravityCorner(const glm::ivec3& cornerPos, Dir cornerDir, bool value)
{
	glm::ivec4 voxelPos = GetGravityCornerVoxelPos(cornerPos, cornerDir);
	if (voxelPos.w == -1)
		return;

	auto voxelIt = m_voxels.find(glm::ivec3(voxelPos));
	if (voxelIt == m_voxels.end())
		return;

	voxelIt->second.hasGravityCorner[voxelPos.w] = value;
	m_modified = true;
}

VoxelRayIntersectResult VoxelBuffer::RayIntersect(const eg::Ray& ray) const
{
	float minDist = INFINITY;
	VoxelRayIntersectResult result;
	result.intersected = false;

	for (int dim = 0; dim < 3; dim++)
	{
		glm::vec3 dn = DirectionVector(static_cast<Dir>(dim * 2));
		for (int s = -100; s <= 100; s++)
		{
			if (std::signbit(static_cast<float>(s) - ray.GetStart()[dim]) != std::signbit(ray.GetDirection()[dim]))
				continue;

			eg::Plane plane(dn, s);
			float intersectDist;
			if (ray.Intersects(plane, intersectDist) && intersectDist < minDist)
			{
				glm::vec3 iPos = ray.GetPoint(intersectDist);
				glm::vec3 posDel = ray.GetDirection() * dn * 0.1f;
				glm::ivec3 voxelPosN = glm::floor(iPos + posDel);
				glm::ivec3 voxelPosP = glm::floor(iPos - posDel);
				if (IsAir(voxelPosP) && !IsAir(voxelPosN))
				{
					result.intersectPosition = iPos;
					result.voxelPosition = voxelPosN;
					result.normalDir = static_cast<Dir>(dim * 2 + (voxelPosP[dim] > voxelPosN[dim] ? 0 : 1));
					result.intersectDist = intersectDist;
					result.intersected = true;
					minDist = intersectDist;
				}
			}
		}
	}

	return result;
}
