#include "World.hpp"
#include "Voxel.hpp"
#include "../Graphics/RenderSettings.hpp"
#include "../Graphics/WallShader.hpp"

World::World()
{
	
}

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
		m_voxelBuffer.get()[idx].isAir = isAir;
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
	
	//Allocates a new voxel buffer
	size_t bufferSize = (size_t)(m_voxelBufferMax.x - m_voxelBufferMin.x + 1) *
		(size_t)(m_voxelBufferMax.y - m_voxelBufferMin.y + 1) *
		(size_t)(m_voxelBufferMax.z - m_voxelBufferMin.z + 1);
	std::unique_ptr<Voxel[]> newBuffer = std::make_unique<Voxel[]>(bufferSize);
	
	//Copies voxel buffer data to the new buffer
	if (m_voxelBuffer != nullptr)
	{
		for (int z = m_voxelBufferMin.z; z <= m_voxelBufferMax.z; z++)
		{
			for (int y = m_voxelBufferMin.y; y <= m_voxelBufferMax.y; y++)
			{
				for (int x = m_voxelBufferMin.x; x <= m_voxelBufferMax.x; x++)
				{
					const int srcIdx = VoxelBufferIdx(glm::ivec3(x, y, z), oldMin, oldMax);
					if (srcIdx != -1)
					{
						const int dstIdx = VoxelBufferIdx({x, y, z}, m_voxelBufferMin, m_voxelBufferMax);
						newBuffer[dstIdx] = m_voxelBuffer[srcIdx];
					}
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
	return idx == -1 ? false : m_voxelBuffer[idx].isAir;
}

void World::PrepareForDraw()
{
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

void World::Draw(const RenderSettings& renderSettings, const WallShader& shader)
{
	if (m_numVoxelIndices > 0)
	{
		shader.Draw(renderSettings, m_voxelVertexBuffer, m_voxelIndexBuffer, m_numVoxelIndices);
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
					if (nIdx == -1 || !m_voxelBuffer[nIdx].isAir)
						continue;
					
					//The center of the face to be emitted
					const glm::vec3 faceCenter = glm::vec3(x, y, z) + glm::vec3(voxel::normals[s]) * 0.5f;
					
					//Emits indices
					const uint16_t baseIndex = (uint16_t)verticesOut.size();
					static const uint16_t indicesRelative[] = { 0, 1, 2, 2, 1, 3 };
					for (uint16_t i : indicesRelative)
						indicesOut.push_back(baseIndex + i);
					
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
							vertex.texCoord[2] = 0;
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
			vOffset = 1;
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
