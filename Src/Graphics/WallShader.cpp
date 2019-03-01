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
	eg::ShaderProgram deferredGeomProgram;
	deferredGeomProgram.AddStageFromAsset("Shaders/Wall.vs.glsl");
	deferredGeomProgram.AddStageFromAsset("Shaders/Wall.fs.glsl");
	
	//Creates the ambient light pipeline
	eg::FixedFuncState fixedFuncState;
	fixedFuncState.enableDepthWrite = true;
	fixedFuncState.enableDepthTest = true;
	fixedFuncState.cullMode = eg::CullMode::Back;
	fixedFuncState.depthFormat = Renderer::DEPTH_FORMAT;
	fixedFuncState.attachments[0].format = eg::Format::R8G8B8A8_UNorm;
	fixedFuncState.attachments[1].format = eg::Format::R8G8B8A8_UNorm;
	fixedFuncState.vertexBindings[0] = { sizeof(WallVertex), eg::InputRate::Vertex };
	fixedFuncState.vertexAttributes[0] = { 0, eg::DataType::Float32,   3, (uint32_t)offsetof(WallVertex, position) };
	fixedFuncState.vertexAttributes[1] = { 0, eg::DataType::UInt8,     4, (uint32_t)offsetof(WallVertex, texCoordAO) };
	fixedFuncState.vertexAttributes[2] = { 0, eg::DataType::SInt8Norm, 3, (uint32_t)offsetof(WallVertex, normal) };
	fixedFuncState.vertexAttributes[3] = { 0, eg::DataType::SInt8Norm, 3, (uint32_t)offsetof(WallVertex, tangent) };
	wr.pipelineDeferredGeom = deferredGeomProgram.CreatePipeline(fixedFuncState);
	
	//Creates the editor pipeline
	eg::ShaderProgram editorProgram;
	editorProgram.AddStageFromAsset("Shaders/Wall.vs.glsl");
	editorProgram.AddStageFromAsset("Shaders/Wall-Editor.fs.glsl");
	fixedFuncState.enableDepthWrite = true;
	fixedFuncState.depthCompare = eg::CompareOp::Less;
	fixedFuncState.attachments[0].blend = { };
	fixedFuncState.depthFormat = eg::Format::DefaultDepthStencil;
	fixedFuncState.attachments[0].format = eg::Format::DefaultColor;
	fixedFuncState.attachments[1].format = eg::Format::Undefined;
	wr.pipelineEditor = editorProgram.CreatePipeline(fixedFuncState);
	
	//Creates the editor border pipeline
	eg::ShaderProgram editorBorderProgram;
	editorBorderProgram.AddStageFromAsset("Shaders/Wall-Border.vs.glsl");
	editorBorderProgram.AddStageFromAsset("Shaders/Wall-Border.fs.glsl");
	eg::FixedFuncState edBorderFFS;
	edBorderFFS.enableDepthWrite = false;
	edBorderFFS.enableDepthTest = true;
	edBorderFFS.cullMode = eg::CullMode::None;
	edBorderFFS.topology = eg::Topology::LineList;
	edBorderFFS.depthFormat = eg::Format::DefaultDepthStencil;
	edBorderFFS.attachments[0].format = eg::Format::DefaultColor;
	edBorderFFS.vertexBindings[0] = { sizeof(WallBorderVertex), eg::InputRate::Vertex };
	edBorderFFS.vertexAttributes[0] = { 0, eg::DataType::Float32,   4, (uint32_t)offsetof(WallBorderVertex, position) };
	edBorderFFS.vertexAttributes[1] = { 0, eg::DataType::SInt8Norm, 3, (uint32_t)offsetof(WallBorderVertex, normal1) };
	edBorderFFS.vertexAttributes[2] = { 0, eg::DataType::SInt8Norm, 3, (uint32_t)offsetof(WallBorderVertex, normal2) };
	wr.pipelineBorderEditor = editorBorderProgram.CreatePipeline(edBorderFFS);
	
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
