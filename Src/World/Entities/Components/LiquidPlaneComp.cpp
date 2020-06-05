#include "LiquidPlaneComp.hpp"
#include "../../World.hpp"
#include "../../../Vec3Compare.hpp"

bool LiquidPlaneComp::IsUnderwater(const glm::ivec3& pos) const
{
	auto it = std::lower_bound(m_underwater.begin(), m_underwater.end(), pos, Vec3Compare());
	return it != m_underwater.end() && *it == pos;
}

bool LiquidPlaneComp::IsUnderwater(const Ent& entity, const glm::vec3& pos)
{
	return entity.GetComponent<LiquidPlaneComp>()->IsUnderwater(glm::ivec3(pos)) && pos.y < entity.Pos().y;
}

bool LiquidPlaneComp::IsUnderwater(const Ent& entity, const eg::Sphere& sphere)
{
	return IsUnderwater(entity, sphere.position) && sphere.position.y + sphere.radius < entity.Pos().y;
}

void LiquidPlaneComp::MaybeUpdate(const Ent& entity, const World& world)
{
	if (!m_outOfDate)
		return;
	m_outOfDate = true;
	
	glm::vec3 startF = entity.Pos() + glm::vec3(DirectionVector(entity.Direction())) * 0.5f;
	
	std::set<glm::ivec3, Vec3Compare> visited;
	std::queue<glm::ivec3> bfsQueue;
	
	glm::ivec3 startI = glm::floor(startF);
	bfsQueue.push(startI);
	visited.insert(startI);
	
	const glm::ivec3 deltas[] = 
	{
		glm::ivec3(1, 0, 0), glm::ivec3(-1, 0, 0),
		glm::ivec3(0, 1, 0), glm::ivec3(0, -1, 0),
		glm::ivec3(0, 0, 1), glm::ivec3(0, 0, -1)
	};
	
	while (!bfsQueue.empty())
	{
		glm::ivec3 pos = bfsQueue.front();
		bfsQueue.pop();
		
		for (const glm::ivec3 d : deltas)
		{
			glm::ivec3 next = pos + d;
			if (next.y <= startI.y && world.IsAir(next) && visited.find(next) == visited.end())
			{
				visited.insert(next);
				bfsQueue.push(next);
			}
		}
	}
	
	m_underwater.clear();
	m_underwater.resize(visited.size());
	std::copy(visited.begin(), visited.end(), m_underwater.begin());
	
	if (m_shouldGenerateMesh)
	{
		GenerateMesh(entity);
	}
}

void LiquidPlaneComp::GenerateMesh(const Ent& entity)
{
	std::map<glm::ivec3, uint16_t, Vec3Compare> indexMap;
	std::vector<glm::vec3> vertices;
	std::vector<uint16_t> indices;
	
	for (const glm::ivec3& waterCoord : m_underwater)
	{
		if (IsUnderwater(waterCoord + glm::ivec3(0, 1, 0)))
			continue;
		
		float y = std::min<float>(entity.Pos().y, waterCoord.y + 1);
		
		uint16_t vIndices[2][2];
		for (int dx = 0; dx < 2; dx++)
		{
			for (int dz = 0; dz < 2; dz++)
			{
				glm::ivec3 pos = waterCoord + glm::ivec3(dx, 0, dz);
				auto it = indexMap.find(pos);
				
				if (it == indexMap.end())
				{
					it = indexMap.emplace(pos, (uint16_t)vertices.size()).first;
					vertices.emplace_back(pos.x, y, pos.z);
				}
				
				vIndices[dx][dz] = it->second;
			}
		}
		
		const glm::ivec2 faceIndexOffsets[] =
		{
			{ 0, 0 }, { 0, 1 }, { 1, 0 },
			{ 0, 1 }, { 1, 1 }, { 1, 0 }
		};
		for (const glm::ivec2& indexOffset : faceIndexOffsets)
		{
			indices.push_back(vIndices[indexOffset.x][indexOffset.y]);
		}
	}
	
	//Allocates an upload buffer
	size_t verticesSize = vertices.size() * sizeof(glm::vec3);
	size_t indicesSize = indices.size() * sizeof(uint16_t);
	eg::UploadBuffer uploadBuffer = eg::GetTemporaryUploadBuffer(verticesSize + indicesSize);
	
	//Copies vertices and indices to the upload buffer
	void* uploadMem = uploadBuffer.Map();
	std::memcpy(uploadMem, vertices.data(), verticesSize);
	std::memcpy(static_cast<char*>(uploadMem) + verticesSize, indices.data(), indicesSize);
	uploadBuffer.Flush();
	
	if (m_verticesCapacity < vertices.size())
	{
		m_verticesCapacity = eg::RoundToNextMultiple(vertices.size(), 1024);
		m_vertexBuffer = eg::Buffer(eg::BufferFlags::CopyDst | eg::BufferFlags::VertexBuffer,
			m_verticesCapacity * sizeof(glm::vec3), nullptr);
	}
	
	if (m_indicesCapacity < indices.size())
	{
		m_indicesCapacity = eg::RoundToNextMultiple(indices.size(), 1024);
		m_indexBuffer = eg::Buffer(eg::BufferFlags::CopyDst | eg::BufferFlags::IndexBuffer,
			m_indicesCapacity * sizeof(uint16_t), nullptr);
	}
	
	eg::DC.CopyBuffer(uploadBuffer.buffer, m_vertexBuffer, uploadBuffer.offset, 0, verticesSize);
	eg::DC.CopyBuffer(uploadBuffer.buffer, m_indexBuffer, uploadBuffer.offset + verticesSize, 0, indicesSize);
	
	m_vertexBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
	m_indexBuffer.UsageHint(eg::BufferUsage::IndexBuffer);
	
	m_numIndices = indices.size();
}

eg::MeshBatch::Mesh LiquidPlaneComp::GetMesh() const
{
	eg::MeshBatch::Mesh mesh;
	mesh.vertexBuffer = m_vertexBuffer;
	mesh.indexBuffer = m_indexBuffer;
	mesh.firstIndex = 0;
	mesh.firstVertex = 0;
	mesh.indexType = eg::IndexType::UInt16;
	mesh.numElements = m_numIndices;
	return mesh;
}
