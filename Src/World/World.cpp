#include "World.hpp"
#include "Voxel.hpp"
#include "../Graphics/ObjectRenderer.hpp"
#include "../Graphics/Materials/GravityCornerMaterial.hpp"
#include "../Graphics/RenderSettings.hpp"
#include "../Graphics/WallShader.hpp"

#include <yaml-cpp/yaml.h>

static const char MAGIC[] = { (char)250, 'E', 'G', 'W' };
static const uint32_t CURRENT_VERSION = 0;

World::World()
{
	
}

inline size_t VoxelBufferSize(const glm::ivec3& min, const glm::ivec3& max)
{
	glm::ivec3 delta = max - min + glm::ivec3(1);
	return (size_t)delta.x * (size_t)delta.y * (size_t)delta.z;
}

bool World::Load(std::istream& stream)
{
	char magicBuffer[sizeof(MAGIC)];
	stream.read(magicBuffer, sizeof(MAGIC));
	if (std::memcmp(magicBuffer, MAGIC, sizeof(MAGIC)))
	{
		eg::Log(eg::LogLevel::Error, "wd", "Invalid world file.");
		return false;
	}
	
	const uint32_t version = eg::BinRead<uint32_t>(stream);
	if (version > CURRENT_VERSION)
	{
		eg::Log(eg::LogLevel::Error, "wd", "Unsupported world file version.");
		return false;
	}
	
	stream.read(reinterpret_cast<char*>(&m_voxelBufferMin), sizeof(int32_t) * 3);
	stream.read(reinterpret_cast<char*>(&m_voxelBufferMax), sizeof(int32_t) * 3);
	
	size_t voxelBufferSize = VoxelBufferSize(m_voxelBufferMin, m_voxelBufferMax);
	m_voxelBuffer = std::make_unique<uint64_t[]>(voxelBufferSize);
	
	eg::ReadCompressedSection(stream, m_voxelBuffer.get(), voxelBufferSize * sizeof(uint64_t));
	
	const uint32_t numGravityCorners = eg::BinRead<uint32_t>(stream);
	m_gravityCorners.resize(numGravityCorners);
	for (uint32_t i = 0; i < numGravityCorners; i++)
	{
		m_gravityCorners[i].down1 = (Dir)eg::BinRead<uint8_t>(stream);
		m_gravityCorners[i].down2 = (Dir)eg::BinRead<uint8_t>(stream);
		m_gravityCorners[i].length = eg::BinRead<float>(stream);
		stream.read(reinterpret_cast<char*>(&m_gravityCorners[i].position), sizeof(int32_t) * 3);
	}
	
	return true;
}

void World::Save(std::ostream& outStream) const
{
	outStream.write(MAGIC, sizeof(MAGIC));
	eg::BinWrite<uint32_t>(outStream, CURRENT_VERSION);
	
	outStream.write(reinterpret_cast<const char*>(&m_voxelBufferMin), sizeof(int32_t) * 3);
	outStream.write(reinterpret_cast<const char*>(&m_voxelBufferMax), sizeof(int32_t) * 3);
	
	eg::WriteCompressedSection(outStream, m_voxelBuffer.get(),
		sizeof(uint64_t) * VoxelBufferSize(m_voxelBufferMin, m_voxelBufferMax));
	
	eg::BinWrite(outStream, (uint32_t)m_gravityCorners.size());
	for (const GravityCorner& corner : m_gravityCorners)
	{
		eg::BinWrite(outStream, (uint8_t)corner.down1);
		eg::BinWrite(outStream, (uint8_t)corner.down2);
		eg::BinWrite(outStream, corner.length);
		outStream.write(reinterpret_cast<const char*>(&corner.position), sizeof(int32_t) * 3);
	}
}

static constexpr uint64_t IS_AIR_MASK = (uint64_t)1 << (uint64_t)(8 * 6);

inline int VoxelBufferIdx(const glm::ivec3& pos, const glm::ivec3& min, const glm::ivec3& max)
{
	glm::ivec3 rel = pos - min;
	glm::ivec3 size = (max - min) + glm::ivec3(1);
	if (rel.x < 0 || rel.y < 0 || rel.z < 0 || rel.x >= size.x || rel.y >= size.y || rel.z >= size.z)
		return -1;
	return rel.x + rel.y * size.x + rel.z * size.x * size.y;
}

void World::SetIsAir(const glm::ivec3& pos, bool isAir)
{
	if (IsAir(pos) == isAir)
		return;
	
	int idx = VoxelBufferIdx(pos, m_voxelBufferMin, m_voxelBufferMax);
	if (idx != -1 && m_voxelBuffer != nullptr)
	{
		if (isAir)
			m_voxelBuffer.get()[idx] |= IS_AIR_MASK;
		else
			m_voxelBuffer.get()[idx] &= ~IS_AIR_MASK;
		m_voxelBufferOutOfDate = true;
		return;
	}
	
	// ** Expands the voxel buffer **
	
	constexpr float EXPAND_STEP = 32;
	
	glm::ivec3 oldMin = m_voxelBufferMin;
	glm::ivec3 oldMax = m_voxelBufferMax;
	m_voxelBufferMin = m_voxelBuffer ? glm::min(m_voxelBufferMin, pos) : pos;
	m_voxelBufferMax = m_voxelBuffer ? glm::max(m_voxelBufferMax, pos) : pos;
	m_voxelBufferMin = glm::floor(glm::vec3(m_voxelBufferMin) / EXPAND_STEP) * EXPAND_STEP;
	m_voxelBufferMax = glm::ceil(glm::vec3(m_voxelBufferMax) / EXPAND_STEP) * EXPAND_STEP;
	
	std::unique_ptr<uint64_t[]> newBuffer = std::make_unique<uint64_t[]>(VoxelBufferSize(m_voxelBufferMin, m_voxelBufferMax));
	
	//Copies voxel buffer data to the new buffer
	if (m_voxelBuffer != nullptr)
	{
		for (int z = m_voxelBufferMin.z; z <= m_voxelBufferMax.z; z++)
		{
			for (int y = m_voxelBufferMin.y; y <= m_voxelBufferMax.y; y++)
			{
				for (int x = m_voxelBufferMin.x; x <= m_voxelBufferMax.x; x++)
				{
					const int dstIdx = VoxelBufferIdx({x, y, z}, m_voxelBufferMin, m_voxelBufferMax);
					const int srcIdx = VoxelBufferIdx({x, y, z}, oldMin, oldMax);
					newBuffer[dstIdx] = (srcIdx != -1) ? m_voxelBuffer[srcIdx] : 0;
				}
			}
		}
	}
	
	m_voxelBuffer = std::move(newBuffer);
	SetIsAir(pos, isAir);
}

bool World::IsAir(const glm::ivec3& pos) const
{
	if (m_voxelBuffer == nullptr)
		return false;
	int idx = VoxelBufferIdx(pos, m_voxelBufferMin, m_voxelBufferMax);
	return idx == -1 ? false : (m_voxelBuffer[idx] & IS_AIR_MASK) != 0;
}

void World::SetTexture(const glm::ivec3& pos, Dir side, uint8_t textureLayer)
{
#ifndef NDEBUG
	if (!IsAir(pos))
		EG_PANIC("Attempted to set texture of a voxel which is not air.");
#endif
	
	const int idx = VoxelBufferIdx(pos, m_voxelBufferMin, m_voxelBufferMax);
	
	const uint64_t shift = (uint64_t)side * 8;
	m_voxelBuffer[idx] = (m_voxelBuffer[idx] & ~(0xFFULL << shift)) | (textureLayer << shift);
}

uint8_t World::GetTexture(const glm::ivec3& pos, Dir side) const
{
	if (m_voxelBuffer == nullptr)
		return 0;
	const int idx = VoxelBufferIdx(pos, m_voxelBufferMin, m_voxelBufferMax);
	if (idx == -1)
		return 0;
	return (m_voxelBuffer[idx] >> ((uint64_t)side * 8)) & 0xFFULL;
}

glm::mat3 GravityCorner::MakeRotationMatrix() const
{
	glm::vec3 r1 = -DirectionVector(down1);
	glm::vec3 r2 = -DirectionVector(down2);
	return glm::mat3(r1, r2, glm::cross(r1, r2));
}

void World::PrepareForDraw(ObjectRenderer& objectRenderer)
{
	eg::Model& gravityCornerModel = eg::GetAsset<eg::Model>("Models/GravityCornerConvex.obj");
	for (const GravityCorner& corner : m_gravityCorners)
	{
		const float S = 0.25f;
		
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), corner.position) *
			glm::mat4(corner.MakeRotationMatrix()) *
			glm::scale(glm::mat4(1.0f), glm::vec3(S, -S, corner.length)) *
			glm::translate(glm::mat4(1.0f), glm::vec3(1.01f, -1.01f, 0));
		
		objectRenderer.Add(gravityCornerModel, GravityCornerMaterial::instance, transform);
	}
	
	if (m_voxelBufferOutOfDate)
	{
		std::vector<WallVertex> vertices;
		std::vector<uint16_t> indices;
		BuildVoxelMesh(vertices, indices);
		
		if (indices.empty())
		{
			m_voxelBufferOutOfDate = false;
			return;
		}
		
		const size_t verticesBytes = vertices.size() * sizeof(WallVertex);
		const size_t indicesBytes = indices.size() * sizeof(uint16_t);
		const size_t uploadBytes = verticesBytes + indicesBytes;
		
		//Creates an upload buffer and copies data to it
		eg::UploadBuffer uploadBuffer = eg::GetTemporaryUploadBuffer(uploadBytes);
		char* uploadBufferMem = reinterpret_cast<char*>(uploadBuffer.Map());
		std::memcpy(uploadBufferMem,                 vertices.data(), verticesBytes);
		std::memcpy(uploadBufferMem + verticesBytes, indices.data(), indicesBytes);
		uploadBuffer.Unmap();
		
		//Reallocates the vertex buffer if it is too small
		if (m_voxelVertexBufferCapacity < vertices.size())
		{
			m_voxelVertexBufferCapacity = eg::RoundToNextMultiple(vertices.size(), 16 * 1024);
			m_voxelVertexBuffer = eg::Buffer(eg::BufferFlags::VertexBuffer | eg::BufferFlags::CopyDst,
				m_voxelVertexBufferCapacity * sizeof(WallVertex), nullptr);
		}
		
		//Reallocates the index buffer if it is too small
		if (m_voxelIndexBufferCapacity < indices.size())
		{
			m_voxelIndexBufferCapacity = eg::RoundToNextMultiple(indices.size(), 16 * 1024);
			m_voxelIndexBuffer = eg::Buffer(eg::BufferFlags::IndexBuffer | eg::BufferFlags::CopyDst,
				m_voxelIndexBufferCapacity * sizeof(uint16_t), nullptr);
		}
		
		//Uploads data to the vertex and index buffers
		eg::DC.CopyBuffer(uploadBuffer.buffer, m_voxelVertexBuffer, uploadBuffer.offset, 0, verticesBytes);
		eg::DC.CopyBuffer(uploadBuffer.buffer, m_voxelIndexBuffer, uploadBuffer.offset + verticesBytes, 0, indicesBytes);
		
		m_voxelVertexBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
		m_voxelIndexBuffer.UsageHint(eg::BufferUsage::IndexBuffer);
		
		m_numVoxelIndices = (uint32_t)indices.size();
		m_voxelBufferOutOfDate = false;
	}
}

void World::Draw()
{
	if (m_numVoxelIndices > 0)
	{
		DrawWalls(m_voxelVertexBuffer, m_voxelIndexBuffer, m_numVoxelIndices);
	}
}

void World::DrawEditor()
{
	if (m_numVoxelIndices > 0)
	{
		DrawWallsEditor(m_voxelVertexBuffer, m_voxelIndexBuffer, m_numVoxelIndices);
	}
}

void World::BuildVoxelMesh(std::vector<WallVertex>& verticesOut, std::vector<uint16_t>& indicesOut) const
{
	for (int z = m_voxelBufferMin.z - 1; z <= m_voxelBufferMax.z + 1; z++)
	{
		for (int y = m_voxelBufferMin.y - 1; y <= m_voxelBufferMax.y + 1; y++)
		{
			for (int x = m_voxelBufferMin.x - 1; x <= m_voxelBufferMax.x + 1; x++)
			{
				//Only emit triangles for solid voxels
				if (IsAir({ x, y, z }))
					continue;
				
				for (int s = 0; s < 6; s++)
				{
					const glm::ivec3 nPos = glm::ivec3(x, y, z) + voxel::normals[s];
					const int nIdx = VoxelBufferIdx(nPos, m_voxelBufferMin, m_voxelBufferMax);
					
					//Only emit triangles for voxels that face into an air voxel
					if (nIdx == -1 || !(m_voxelBuffer[nIdx] & IS_AIR_MASK))
						continue;
					
					//The center of the face to be emitted
					const glm::vec3 faceCenter = glm::vec3(x, y, z) + 0.5f + glm::vec3(voxel::normals[s]) * 0.5f;
					
					//Emits indices
					const uint16_t baseIndex = (uint16_t)verticesOut.size();
					static const uint16_t indicesRelative[] = { 0, 1, 2, 2, 1, 3 };
					for (uint16_t i : indicesRelative)
						indicesOut.push_back(baseIndex + i);
					
					const int oppositeS = (int)OppositeDir((Dir)s);
					const uint8_t textureLayer = (m_voxelBuffer[nIdx] >> (uint64_t)(oppositeS * 8)) & 0xFFULL;
					
					//Emits vertices
					glm::vec3 tangent = voxel::tangents[s];
					glm::vec3 biTangent = voxel::biTangents[s];
					for (int fx = 0; fx < 2; fx++)
					{
						for (int fy = 0; fy < 2; fy++)
						{
							WallVertex& vertex = verticesOut.emplace_back();
							vertex.position = faceCenter + tangent * (fy - 0.5f) + biTangent * (fx - 0.5f);
							vertex.texCoord[0] = (uint8_t)(fx * 255);
							vertex.texCoord[1] = (uint8_t)((1 - fy) * 255);
							vertex.texCoord[2] = textureLayer;
							vertex.SetNormal(voxel::normals[s]);
							vertex.SetTangent(tangent);
						}
					}
				}
			}
		}
	}
}

void World::CalcClipping(ClippingArgs& args) const
{
	glm::vec3 endMin = args.aabbMin + args.move;
	glm::vec3 endMax = args.aabbMax + args.move;
	
	constexpr float EP = 0.0001f;
	
	//Clipping against voxels
	for (int axis = 0; axis < 3; axis++)
	{
		int vOffset;
		float beginA, endA;
		if (args.move[axis] < 0)
		{
			beginA = args.aabbMin[axis];
			endA = endMin[axis];
			vOffset = -1;
		}
		else
		{
			beginA = args.aabbMax[axis];
			endA = endMax[axis];
			vOffset = 0;
		}
		
		int minI = (int)std::floor(std::min(beginA, endA) - EP);
		int maxI = (int)std::ceil(std::max(beginA, endA) + EP);
		
		int axisB = (axis + 1) % 3;
		int axisC = (axis + 2) % 3;
		
		for (int v = minI; v <= maxI; v++)
		{
			float t = (v - beginA) / args.move[axis];
			if (t > args.clipDist || t < 0)
				continue;
			
			int minB = (int)std::floor(glm::mix(args.aabbMin[axisB], endMin[axisB], t));
			int maxB = (int)std::ceil(glm::mix(args.aabbMax[axisB], endMax[axisB], t));
			int minC = (int)std::floor(glm::mix(args.aabbMin[axisC], endMin[axisC], t));
			int maxC = (int)std::ceil(glm::mix(args.aabbMax[axisC], endMax[axisC], t));
			for (int vb = minB; vb < maxB; vb++)
			{
				for (int vc = minC; vc < maxC; vc++)
				{
					glm::ivec3 coord;
					coord[axis] = v + vOffset;
					coord[axisB] = vb;
					coord[axisC] = vc;
					if (!IsAir(coord))
					{
						args.colPlaneNormal = glm::vec3(0);
						args.colPlaneNormal[axis] = args.move[axis] < 0 ? 1.0f : -1.0f;
						
						args.clipDist = t;
						break;
					}
				}
			}
		}
	}
}

const GravityCorner* World::FindGravityCorner(const ClippingArgs& args, Dir currentDown) const
{
	glm::vec3 pos = (args.aabbMin + args.aabbMax) / 2.0f;
	
	float minDist = INFINITY;
	const GravityCorner* ret = nullptr;
	
	constexpr float ACTIVATE_DIST = 0.5f;
	constexpr float ACTIVATE_MARGIN = 0.2f;
	constexpr float MAX_HEIGHT_DIFF = 0.1f;
	
	glm::vec3 currentDownDir = DirectionVector(currentDown);
	
	for (const GravityCorner& corner : m_gravityCorners)
	{
		glm::vec3 otherDown;
		if (corner.down1 == currentDown)
			otherDown = DirectionVector(corner.down2);
		else if (corner.down2 == currentDown)
			otherDown = DirectionVector(corner.down1);
		else
			continue;
		
		//Checks that the player is moving towards the corner
		if (glm::dot(args.move, otherDown) < 0.0001f)
			continue;
		
		//Checks that the player is positioned correctly along the side of the corner
		glm::vec3 cornerVec = glm::cross(DirectionVector(corner.down1), DirectionVector(corner.down2));
		float t1 = glm::dot(cornerVec, args.aabbMin - corner.position);
		float t2 = glm::dot(cornerVec, args.aabbMax - corner.position);
		if (t1 > t2)
			std::swap(t1, t2);
		
		if (t1 < -ACTIVATE_MARGIN || t2 > corner.length + ACTIVATE_MARGIN)
			continue;
		
		//Checks that the player is positioned at the same height as the corner
		float h1 = glm::dot(currentDownDir, corner.position - args.aabbMin);
		float h2 = glm::dot(currentDownDir, corner.position - args.aabbMax);
		if (std::abs(h1) > MAX_HEIGHT_DIFF && std::abs(h2) > MAX_HEIGHT_DIFF)
			continue;
		
		//Checks that the player is within range of the corner
		float dist = glm::dot(otherDown, corner.position - pos);
		if (dist > -0.1f && dist < ACTIVATE_DIST)
		{
			float absDist = abs(dist);
			if (absDist < minDist)
			{
				ret = &corner;
				minDist = absDist;
			}
		}
	}
	return ret;
}

PickWallResult World::PickWall(const eg::Ray& ray) const
{
	float minDist = INFINITY;
	PickWallResult result;
	result.intersected = false;
	
	for (int dim = 0; dim < 3; dim++)
	{
		glm::vec3 dn = DirectionVector((Dir)(dim * 2));
		for (int s = m_voxelBufferMin[dim]; s <= m_voxelBufferMax[dim]; s++)
		{
			if (std::signbit(s - ray.GetStart()[dim]) != std::signbit(ray.GetDirection()[dim]))
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
					result.normalDir = (Dir)(dim * 2 + (voxelPosP[dim] > voxelPosN[dim] ? 0 : 1));
					result.intersected = true;
					minDist = intersectDist;
				}
			}
		}
	}
	
	return result;
}
