#include "Vertex.hpp"

void VecEncode(const glm::vec3& v, int8_t* out)
{
	float scale = 127.0f / glm::length(v);
	for (int i = 0; i < 3; i++)
	{
		out[i] = static_cast<int8_t>(v[i] * scale);
	}
}

static const eg::ModelVertexAttribute VertexSoa_PX_Attributes[] = {
	eg::ModelVertexAttribute{
		.type = eg::ModelVertexAttributeType::Position_F32,
		.offset = 0,
		.streamIndex = 0,
	},
	eg::ModelVertexAttribute{
		.type = eg::ModelVertexAttributeType::TexCoord_F32,
		.offset = 0,
		.streamIndex = 1,
	},
};

static const std::array<uint32_t, 2> VertexSoa_PX_StreamStrides = {
	VERTEX_STRIDE_POSITION,
	VERTEX_STRIDE_TEXCOORD,
};

static const eg::ModelVertexAttribute VertexSoa_PXNT_Attributes[] = {
	eg::ModelVertexAttribute{
		.type = eg::ModelVertexAttributeType::Position_F32,
		.offset = 0,
		.streamIndex = 0,
	},
	eg::ModelVertexAttribute{
		.type = eg::ModelVertexAttributeType::TexCoord_F32,
		.offset = 0,
		.streamIndex = 1,
	},
	eg::ModelVertexAttribute{
		.type = eg::ModelVertexAttributeType::Normal_I8,
		.offset = 0,
		.streamIndex = 2,
	},
	eg::ModelVertexAttribute{
		.type = eg::ModelVertexAttributeType::Tangent_I8,
		.offset = 4,
		.streamIndex = 2,
	},
};

static const std::array<uint32_t, 3> VertexSoa_PXNT_StreamStrides = {
	VERTEX_STRIDE_POSITION,
	VERTEX_STRIDE_TEXCOORD,
	VERTEX_STRIDE_NORMAL_TANGENT,
};

void RegisterVertexFormats()
{
	eg::ModelVertexFormat::RegisterFormat(
		"IomomiSoaPXNT", eg::ModelVertexFormat{ .attributes = VertexSoa_PXNT_Attributes,
	                                            .streamsBytesPerVertex = VertexSoa_PXNT_StreamStrides });
	eg::ModelVertexFormat::RegisterFormat(
		"IomomiSoaPX", eg::ModelVertexFormat{ .attributes = VertexSoa_PX_Attributes,
	                                          .streamsBytesPerVertex = VertexSoa_PX_StreamStrides });
}

void InitPipelineVertexStateSoaPXNT(eg::GraphicsPipelineCreateInfo& pipelineCI)
{
	pipelineCI.vertexBindings[VERTEX_BINDING_POSITION] = { VERTEX_STRIDE_POSITION, eg::InputRate::Vertex };
	pipelineCI.vertexBindings[VERTEX_BINDING_TEXCOORD] = { VERTEX_STRIDE_TEXCOORD, eg::InputRate::Vertex };
	pipelineCI.vertexBindings[VERTEX_BINDING_NORMAL_TANGENT] = { VERTEX_STRIDE_NORMAL_TANGENT, eg::InputRate::Vertex };

	pipelineCI.vertexAttributes[0] = { VERTEX_BINDING_POSITION, eg::DataType::Float32, 3, 0 };
	pipelineCI.vertexAttributes[1] = { VERTEX_BINDING_TEXCOORD, eg::DataType::Float32, 2, 0 };
	pipelineCI.vertexAttributes[2] = { VERTEX_BINDING_NORMAL_TANGENT, eg::DataType::SInt8Norm, 3, 0 };
	pipelineCI.vertexAttributes[3] = { VERTEX_BINDING_NORMAL_TANGENT, eg::DataType::SInt8Norm, 3, 4 };
}

void SetVertexStreamOffsetsSoaPXNT(std::span<std::optional<uint64_t>> streamOffsets, size_t numVertices)
{
	streamOffsets[VERTEX_BINDING_POSITION] = 0;
	streamOffsets[VERTEX_BINDING_TEXCOORD] = VERTEX_STRIDE_POSITION * numVertices;
	streamOffsets[VERTEX_BINDING_NORMAL_TANGENT] = (VERTEX_STRIDE_POSITION + VERTEX_STRIDE_TEXCOORD) * numVertices;
}

const eg::IMaterial::VertexInputConfiguration VertexInputConfig_SoaPXNTI = {
	.vertexBindingsMask = 0b111,
	.instanceDataBindingIndex = VERTEX_BINDING_INSTANCE_DATA,
};

const eg::IMaterial::VertexInputConfiguration VertexInputConfig_SoaPXNT = {
	.vertexBindingsMask = 0b111,
	.instanceDataBindingIndex = std::nullopt,
};
