#include "WallShader.hpp"
#include "RenderSettings.hpp"
#include "Renderer.hpp"
#include "GraphicsCommon.hpp"

struct
{
	eg::Pipeline pipelineDeferredGeom;
	eg::Pipeline pipelinePoint;
	eg::Pipeline pipelineEditor;
	eg::Pipeline pipelineBorderEditor;
	eg::Buffer materialSettingsBuffer;
	eg::Texture* diffuseTexture;
	eg::Texture* normalMapTexture;
	eg::Texture* miscMapTexture;
	eg::Texture* gridTexture;
	eg::Texture* noDrawTexture;
} wr;

struct MaterialSettings
{
	float textureScale;
	float minRoughness;
	float maxRoughness;
	float _padding;
};

MaterialSettings materialSettings[2] =
{
	{ 2.0f, 0.35f, 1.0f },
	{ 4.0f, 0.2f, 1.0f }
};

void InitializeWallShader()
{
	//Creates the ambient light pipeline
	eg::PipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModule>("Shaders/Wall.vs.glsl").Handle();
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModule>("Shaders/Wall.fs.glsl").Handle();
	pipelineCI.enableDepthWrite = true;
	pipelineCI.enableDepthTest = true;
	pipelineCI.cullMode = eg::CullMode::Back;
	pipelineCI.numColorAttachments = 2;
	pipelineCI.vertexBindings[0] = { sizeof(WallVertex), eg::InputRate::Vertex };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32,   3, (uint32_t)offsetof(WallVertex, position) };
	pipelineCI.vertexAttributes[1] = { 0, eg::DataType::UInt8,     4, (uint32_t)offsetof(WallVertex, texCoordAO) };
	pipelineCI.vertexAttributes[2] = { 0, eg::DataType::SInt8Norm, 3, (uint32_t)offsetof(WallVertex, normal) };
	pipelineCI.vertexAttributes[3] = { 0, eg::DataType::SInt8Norm, 3, (uint32_t)offsetof(WallVertex, tangent) };
	wr.pipelineDeferredGeom = eg::Pipeline::Create(pipelineCI);
	wr.pipelineDeferredGeom.FramebufferFormatHint(Renderer::GEOMETRY_FB_FORMAT);
	
	//Creates the editor pipeline
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModule>("Shaders/Wall-Editor.fs.glsl").Handle();
	pipelineCI.enableDepthWrite = true;
	pipelineCI.depthCompare = eg::CompareOp::Less;
	pipelineCI.numColorAttachments = 1;
	wr.pipelineEditor = eg::Pipeline::Create(pipelineCI);
	wr.pipelineEditor.FramebufferFormatHint(eg::Format::DefaultColor, eg::Format::DefaultDepthStencil);
	
	//Creates the editor border pipeline
	eg::PipelineCreateInfo borderPipelineCI;
	borderPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModule>("Shaders/Wall-Border.vs.glsl").Handle();
	borderPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModule>("Shaders/Wall-Border.fs.glsl").Handle();
	borderPipelineCI.enableDepthWrite = false;
	borderPipelineCI.enableDepthTest = true;
	borderPipelineCI.cullMode = eg::CullMode::None;
	borderPipelineCI.topology = eg::Topology::LineList;
	borderPipelineCI.vertexBindings[0] = { sizeof(WallBorderVertex), eg::InputRate::Vertex };
	borderPipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32,   4, (uint32_t)offsetof(WallBorderVertex, position) };
	borderPipelineCI.vertexAttributes[1] = { 0, eg::DataType::SInt8Norm, 3, (uint32_t)offsetof(WallBorderVertex, normal1) };
	borderPipelineCI.vertexAttributes[2] = { 0, eg::DataType::SInt8Norm, 3, (uint32_t)offsetof(WallBorderVertex, normal2) };
	wr.pipelineBorderEditor = eg::Pipeline::Create(borderPipelineCI);
	wr.pipelineBorderEditor.FramebufferFormatHint(eg::Format::DefaultColor, eg::Format::DefaultDepthStencil);
	
	wr.diffuseTexture = &eg::GetAsset<eg::Texture>("Textures/Voxel/Diffuse");
	wr.normalMapTexture = &eg::GetAsset<eg::Texture>("Textures/Voxel/NormalMap");
	wr.miscMapTexture = &eg::GetAsset<eg::Texture>("Textures/Voxel/MiscMap");
	wr.gridTexture = &eg::GetAsset<eg::Texture>("Textures/Grid.png");
	wr.noDrawTexture = &eg::GetAsset<eg::Texture>("Textures/NoDraw.png");
	
	wr.materialSettingsBuffer = eg::Buffer(eg::BufferFlags::UniformBuffer, sizeof(materialSettings), materialSettings);
	wr.materialSettingsBuffer.UsageHint(eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Vertex);
}

static void OnShutdown()
{
	wr.pipelineDeferredGeom.Destroy();
	wr.pipelinePoint.Destroy();
	wr.pipelineEditor.Destroy();
	wr.pipelineBorderEditor.Destroy();
}

EG_ON_SHUTDOWN(OnShutdown)

static const float ambientColor[] = { 0.15f, 0.12f, 0.13f, 1.0f };

#pragma pack(push, 1)
struct LightData
{
	glm::vec3 pos;
	float _padding1;
	glm::vec3 radiance;
};
#pragma pack(pop)

void DrawWalls(const std::function<void()>& drawCallback)
{
	eg::DC.BindPipeline(wr.pipelineDeferredGeom);
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
	eg::DC.BindUniformBuffer(wr.materialSettingsBuffer, 1, 0, sizeof(materialSettings));
	
	eg::DC.BindTexture(*wr.diffuseTexture, 2, &GetCommonTextureSampler());
	eg::DC.BindTexture(*wr.normalMapTexture, 3, &GetCommonTextureSampler());
	eg::DC.BindTexture(*wr.miscMapTexture, 4, &GetCommonTextureSampler());
	
	drawCallback();
}

void DrawWallsEditor(const std::function<void()>& drawCallback)
{
	eg::DC.BindPipeline(wr.pipelineEditor);
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
	eg::DC.BindUniformBuffer(wr.materialSettingsBuffer, 1, 0, sizeof(materialSettings));
	
	eg::DC.BindTexture(*wr.diffuseTexture, 2, &GetCommonTextureSampler());
	eg::DC.BindTexture(*wr.normalMapTexture, 3, &GetCommonTextureSampler());
	eg::DC.BindTexture(*wr.gridTexture, 4);
	eg::DC.BindTexture(*wr.noDrawTexture, 5);
	
	drawCallback();
}

void DrawWallBordersEditor(eg::BufferRef vertexBuffer, uint32_t numVertices)
{
	eg::DC.BindPipeline(wr.pipelineBorderEditor);
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
	
	eg::DC.BindVertexBuffer(0, vertexBuffer, 0);
	
	eg::DC.Draw(0, numVertices, 0, 1);
}
