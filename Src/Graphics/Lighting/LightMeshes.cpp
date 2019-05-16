#include "LightMeshes.hpp"

static const glm::vec3 sphereMeshVertices[42] =
{
	{ 0.000000,  -1.000000, 0.000000 },
	{ 0.723607,  -0.447220, 0.525725 },
	{ -0.276388, -0.447220, 0.850649 },
	{ -0.894426, -0.447216, 0.000000 },
	{ -0.276388, -0.447220, -0.850649 },
	{ 0.723607,  -0.447220, -0.525725 },
	{ 0.276388,  0.447220,  0.850649 },
	{ -0.723607, 0.447220,  0.525725 },
	{ -0.723607, 0.447220,  -0.525725 },
	{ 0.276388,  0.447220,  -0.850649 },
	{ 0.894426,  0.447216,  0.000000 },
	{ 0.000000,  1.000000,  0.000000 },
	{ -0.162456, -0.850654, 0.499995 },
	{ 0.425323,  -0.850654, 0.309011 },
	{ 0.262869,  -0.525738, 0.809012 },
	{ 0.850648,  -0.525736, 0.000000 },
	{ 0.425323,  -0.850654, -0.309011 },
	{ -0.525730, -0.850652, 0.000000 },
	{ -0.688189, -0.525736, 0.499997 },
	{ -0.162456, -0.850654, -0.499995 },
	{ -0.688189, -0.525736, -0.499997 },
	{ 0.262869,  -0.525738, -0.809012 },
	{ 0.951058,  0.000000,  0.309013 },
	{ 0.951058,  0.000000,  -0.309013 },
	{ 0.000000,  0.000000,  1.000000 },
	{ 0.587786,  0.000000,  0.809017 },
	{ -0.951058, 0.000000,  0.309013 },
	{ -0.587786, 0.000000,  0.809017 },
	{ -0.587786, 0.000000,  -0.809017 },
	{ -0.951058, 0.000000,  -0.309013 },
	{ 0.587786,  0.000000,  -0.809017 },
	{ 0.000000,  0.000000,  -1.000000 },
	{ 0.688189,  0.525736,  0.499997 },
	{ -0.262869, 0.525738,  0.809012 },
	{ -0.850648, 0.525736,  0.000000 },
	{ -0.262869, 0.525738,  -0.809012 },
	{ 0.688189,  0.525736,  -0.499997 },
	{ 0.162456,  0.850654,  0.499995 },
	{ 0.525730,  0.850652,  0.000000 },
	{ -0.425323, 0.850654,  0.309011 },
	{ -0.425323, 0.850654,  -0.309011 },
	{ 0.162456,  0.850654,  -0.499995 }
};

static const uint16_t sphereMeshIndices[240] =
{
	0, 13, 12, 1, 13, 15, 0, 12, 17, 0, 17, 19, 0, 19, 16, 1, 15, 22, 2, 14, 24, 3, 18, 26, 4, 20, 28, 5, 21, 30, 1,
	22, 25, 2, 24, 27, 3, 26, 29, 4, 28, 31, 5, 30, 23, 6, 32, 37, 7, 33, 39, 8, 34, 40, 9, 35, 41, 10, 36, 38, 38,
	41, 11, 38, 36, 41, 36, 9, 41, 41, 40, 11, 41, 35, 40, 35, 8, 40, 40, 39, 11, 40, 34, 39, 34, 7, 39, 39, 37, 11,
	39, 33, 37, 33, 6, 37, 37, 38, 11, 37, 32, 38, 32, 10, 38, 23, 36, 10, 23, 30, 36, 30, 9, 36, 31, 35, 9, 31, 28,
	35, 28, 8, 35, 29, 34, 8, 29, 26, 34, 26, 7, 34, 27, 33, 7, 27, 24, 33, 24, 6, 33, 25, 32, 6, 25, 22, 32, 22,
	10, 32, 30, 31, 9, 30, 21, 31, 21, 4, 31, 28, 29, 8, 28, 20, 29, 20, 3, 29, 26, 27, 7, 26, 18, 27, 18, 2, 27,
	24, 25, 6, 24, 14, 25, 14, 1, 25, 22, 23, 10, 22, 15, 23, 15, 5, 23, 16, 21, 5, 16, 19, 21, 19, 4, 21, 19, 20,
	4, 19, 17, 20, 17, 3, 20, 17, 18, 3, 17, 12, 18, 12, 2, 18, 15, 16, 5, 15, 13, 16, 13, 0, 16, 12, 14, 2, 12, 13,
	14, 13, 1, 14
};

static const glm::vec3 coneMeshVertices[] = 
{
	glm::vec3(0.000000f, 1.000000f, -1.000000f),
	glm::vec3(0.000000f, 0.000000f, 0.000000f),
	glm::vec3(0.382683f, 1.000000f, -0.923880f),
	glm::vec3(0.707107f, 1.000000f, -0.707107f),
	glm::vec3(0.923880f, 1.000000f, -0.382683f),
	glm::vec3(1.000000f, 1.000000f, 0.000000f),
	glm::vec3(0.923880f, 1.000000f, 0.382684f),
	glm::vec3(0.707107f, 1.000000f, 0.707107f),
	glm::vec3(0.382683f, 1.000000f, 0.923880f),
	glm::vec3(0.000000f, 1.000000f, 1.000000f),
	glm::vec3(-0.382683f, 1.000000f, 0.923880f),
	glm::vec3(-0.707107f, 1.000000f, 0.707107f),
	glm::vec3(-0.923880f, 1.000000f, 0.382684f),
	glm::vec3(-1.000000f, 1.000000f, -0.000000f),
	glm::vec3(-0.923879f, 1.000000f, -0.382684f),
	glm::vec3(-0.707107f, 1.000000f, -0.707107f),
	glm::vec3(-0.382683f, 1.000000f, -0.923880f)
};

static const uint16_t coneMeshIndices[] = 
{
	0, 2, 1, 2, 3, 1, 3, 4, 1, 4, 5, 1, 5, 6, 1, 6, 7, 1, 7, 8, 1, 8, 9, 1, 9, 10, 1, 10, 11, 1, 11, 12, 1, 12, 13,
	1, 13, 14, 1, 14, 15, 1, 15, 16, 1, 16, 0, 1, 10, 6, 14, 2, 0, 16, 16, 15, 14, 14, 13, 12, 12, 11, 10, 10, 9, 8,
	8, 7, 6, 6, 5, 4, 4, 3, 2, 2, 16, 14, 14, 12, 10, 10, 8, 6, 6, 4, 2, 2, 14, 6
};

const uint32_t SPOT_LIGHT_MESH_INDICES = eg::ArrayLen(coneMeshIndices);
const uint32_t POINT_LIGHT_MESH_INDICES = eg::ArrayLen(sphereMeshIndices);

static eg::Buffer indexBuffer;
static eg::Buffer vertexBuffer;

static void Initialize()
{
	constexpr uint64_t sphereVerticesOffset = 0;
	constexpr uint64_t coneVerticesOffset = sphereVerticesOffset + sizeof(sphereMeshVertices);
	constexpr uint64_t sphereIndicesOffset = coneVerticesOffset + sizeof(coneMeshVertices);
	constexpr uint64_t coneIndicesOffset = sphereIndicesOffset + sizeof(sphereMeshIndices);
	constexpr uint64_t uploadBytes = coneIndicesOffset + sizeof(coneMeshIndices);
	
	eg::Buffer uploadBuffer(eg::BufferFlags::CopySrc | eg::BufferFlags::MapWrite | eg::BufferFlags::HostAllocate,
		uploadBytes, nullptr);
	
	char* data = static_cast<char*>(uploadBuffer.Map(0, uploadBytes));
	std::memcpy(data + sphereVerticesOffset, sphereMeshVertices, sizeof(sphereMeshVertices));
	std::memcpy(data + coneVerticesOffset, coneMeshVertices, sizeof(coneMeshVertices));
	std::memcpy(data + sphereIndicesOffset, sphereMeshIndices, sizeof(sphereMeshIndices));
	std::memcpy(data + coneIndicesOffset, coneMeshIndices, sizeof(coneMeshIndices));
	
	constexpr uint64_t indicesBytes = sizeof(sphereMeshIndices) + sizeof(coneMeshIndices);
	constexpr uint64_t verticesBytes = sizeof(sphereMeshVertices) + sizeof(coneMeshVertices);
	indexBuffer = { eg::BufferFlags::CopyDst | eg::BufferFlags::IndexBuffer, indicesBytes, nullptr };
	vertexBuffer = { eg::BufferFlags::CopyDst | eg::BufferFlags::VertexBuffer, verticesBytes, nullptr };
	
	uploadBuffer.Flush(0, uploadBytes);
	
	eg::DC.CopyBuffer(uploadBuffer, vertexBuffer, 0, 0, verticesBytes);
	eg::DC.CopyBuffer(uploadBuffer, indexBuffer, verticesBytes, 0, indicesBytes);
	 
	vertexBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
	indexBuffer.UsageHint(eg::BufferUsage::IndexBuffer);
}

static void Destroy()
{
	indexBuffer.Destroy();
	vertexBuffer.Destroy();
}

EG_ON_INIT(Initialize)
EG_ON_SHUTDOWN(Destroy)

void BindSpotLightMesh()
{
	eg::DC.BindVertexBuffer(0, vertexBuffer, sizeof(sphereMeshVertices));
	eg::DC.BindIndexBuffer(eg::IndexType::UInt16, indexBuffer, sizeof(sphereMeshIndices));
}

void BindPointLightMesh()
{
	eg::DC.BindVertexBuffer(0, vertexBuffer, 0);
	eg::DC.BindIndexBuffer(eg::IndexType::UInt16, indexBuffer, 0);
}
