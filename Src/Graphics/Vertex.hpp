#pragma once

void VecEncode(const glm::vec3& v, int8_t* out);

struct WallVertex
{
	float position[3];
	float texCoord[3];
	int8_t normalAndRoughnessLo[4];
	int8_t tangentAndRoughnessHi[4];

	void SetNormal(const glm::vec3& _normal) { VecEncode(_normal, normalAndRoughnessLo); }

	void SetTangent(const glm::vec3& _tangent) { VecEncode(_tangent, tangentAndRoughnessHi); }
};

struct WallBorderVertex
{
	glm::vec4 position;
	int8_t normal1[4];
	int8_t normal2[4];
};

template <size_t N>
struct VertexSoaPXNT
{
	glm::vec3 positions[N];
	glm::vec2 textureCoordinates[N];
	uint32_t normalsAndTangents[N][2];
};

static constexpr uint32_t VERTEX_BINDING_POSITION = 0;
static constexpr uint32_t VERTEX_BINDING_TEXCOORD = 1;
static constexpr uint32_t VERTEX_BINDING_NORMAL_TANGENT = 2;
static constexpr uint32_t VERTEX_BINDING_INSTANCE_DATA = 3;

static constexpr uint32_t VERTEX_STRIDE_POSITION = 12;
static constexpr uint32_t VERTEX_STRIDE_TEXCOORD = 8;
static constexpr uint32_t VERTEX_STRIDE_NORMAL_TANGENT = 8;

void RegisterVertexFormats();

void InitPipelineVertexStateSoaPXNT(eg::GraphicsPipelineCreateInfo& pipelineCI);

void SetVertexStreamOffsetsSoaPXNT(std::span<std::optional<uint64_t>> streamOffsets, size_t numVertices);

extern const eg::IMaterial::VertexInputConfiguration VertexInputConfig_SoaPXNTI;
extern const eg::IMaterial::VertexInputConfiguration VertexInputConfig_SoaPXNT;
