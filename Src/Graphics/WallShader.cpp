#include "WallShader.hpp"
#include "RenderSettings.hpp"
#include "Renderer.hpp"

struct
{
	eg::Pipeline pipelineAmbient;
	eg::Pipeline pipelinePoint;
	eg::Pipeline pipelineEditor;
	eg::Pipeline pipelineBorderEditor;
	eg::Texture* diffuseTexture;
	eg::Texture* normalMapTexture;
	eg::Texture* miscMapTexture;
	eg::Texture* gridTexture;
	eg::Sampler sampler;
} wr;

void InitializeWallShader()
{
	eg::ShaderProgram ambientProgram;
	ambientProgram.AddStageFromAsset("Shaders/Fwd/Wall.vs.glsl");
	ambientProgram.AddStageFromAsset("Shaders/Fwd/Wall-Ambient.fs.glsl");
	
	//Creates the ambient light pipeline
	eg::FixedFuncState fixedFuncState;
	fixedFuncState.enableDepthWrite = true;
	fixedFuncState.enableDepthTest = true;
	fixedFuncState.cullMode = eg::CullMode::Back;
	fixedFuncState.depthFormat = Renderer::DEPTH_FORMAT;
	fixedFuncState.attachments[0].format = Renderer::COLOR_FORMAT;
	fixedFuncState.vertexBindings[0] = { sizeof(WallVertex), eg::InputRate::Vertex };
	fixedFuncState.vertexAttributes[0] = { 0, eg::DataType::Float32,   3, (uint32_t)offsetof(WallVertex, position) };
	fixedFuncState.vertexAttributes[1] = { 0, eg::DataType::UInt8,     3, (uint32_t)offsetof(WallVertex, texCoord) };
	fixedFuncState.vertexAttributes[2] = { 0, eg::DataType::SInt8Norm, 3, (uint32_t)offsetof(WallVertex, normal) };
	fixedFuncState.vertexAttributes[3] = { 0, eg::DataType::SInt8Norm, 3, (uint32_t)offsetof(WallVertex, tangent) };
	wr.pipelineAmbient = ambientProgram.CreatePipeline(fixedFuncState);
	
	//Creates the point light pipeline
	eg::ShaderProgram pointLightProgram;
	pointLightProgram.AddStageFromAsset("Shaders/Fwd/Wall.vs.glsl");
	pointLightProgram.AddStageFromAsset("Shaders/Fwd/Wall-Point.fs.glsl");
	
	fixedFuncState.enableDepthWrite = false;
	fixedFuncState.depthCompare = eg::CompareOp::Equal;
	fixedFuncState.attachments[0].blend = eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::One);
	wr.pipelinePoint = pointLightProgram.CreatePipeline(fixedFuncState);
	
	//Creates the editor pipeline
	eg::ShaderProgram editorProgram;
	editorProgram.AddStageFromAsset("Shaders/Fwd/Wall.vs.glsl");
	editorProgram.AddStageFromAsset("Shaders/Fwd/Wall-Editor.fs.glsl");
	fixedFuncState.enableDepthWrite = true;
	fixedFuncState.depthCompare = eg::CompareOp::Less;
	fixedFuncState.attachments[0].blend = { };
	fixedFuncState.depthFormat = eg::Format::DefaultDepthStencil;
	fixedFuncState.attachments[0].format = eg::Format::DefaultColor;
	wr.pipelineEditor = editorProgram.CreatePipeline(fixedFuncState);
	
	//Creates the editor border pipeline
	eg::ShaderProgram editorBorderProgram;
	editorBorderProgram.AddStageFromAsset("Shaders/Fwd/Wall-Border.vs.glsl");
	editorBorderProgram.AddStageFromAsset("Shaders/Fwd/Wall-Border.fs.glsl");
	eg::FixedFuncState edBorderFFS;
	edBorderFFS.enableDepthWrite = false;
	edBorderFFS.enableDepthTest = true;
	edBorderFFS.cullMode = eg::CullMode::None;
	edBorderFFS.topology = eg::Topology::LineList;
	edBorderFFS.depthFormat = eg::Format::DefaultDepthStencil;
	edBorderFFS.attachments[0].format = eg::Format::DefaultColor;
	edBorderFFS.vertexBindings[0] = { sizeof(WallBorderVertex), eg::InputRate::Vertex };
	edBorderFFS.vertexAttributes[0] = { 0, eg::DataType::Float32,   3, (uint32_t)offsetof(WallBorderVertex, position) };
	edBorderFFS.vertexAttributes[1] = { 0, eg::DataType::SInt8Norm, 3, (uint32_t)offsetof(WallBorderVertex, normal1) };
	edBorderFFS.vertexAttributes[2] = { 0, eg::DataType::SInt8Norm, 3, (uint32_t)offsetof(WallBorderVertex, normal2) };
	wr.pipelineBorderEditor = editorBorderProgram.CreatePipeline(edBorderFFS);
	
	wr.diffuseTexture = &eg::GetAsset<eg::Texture>("Textures/Voxel/Diffuse");
	wr.normalMapTexture = &eg::GetAsset<eg::Texture>("Textures/Voxel/NormalMap");
	wr.miscMapTexture = &eg::GetAsset<eg::Texture>("Textures/Voxel/MiscMap");
	wr.gridTexture = &eg::GetAsset<eg::Texture>("Textures/Grid.png");
	
	eg::SamplerDescription samplerDescription;
	samplerDescription.maxAnistropy = 16;
	wr.sampler = eg::Sampler(samplerDescription);
}

static void OnShutdown()
{
	wr.pipelineAmbient.Destroy();
	wr.pipelinePoint.Destroy();
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

void DrawWalls(eg::BufferRef vertexBuffer, eg::BufferRef indexBuffer, uint32_t numIndices)
{
	eg::DC.BindPipeline(wr.pipelineAmbient);
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
	
	eg::DC.BindTexture(*wr.diffuseTexture, 1, &wr.sampler);
	eg::DC.BindTexture(*wr.normalMapTexture, 2, &wr.sampler);
	eg::DC.BindTexture(*wr.miscMapTexture, 3, &wr.sampler);
	
	eg::DC.PushConstants(0, sizeof(ambientColor), ambientColor);
	
	eg::DC.BindVertexBuffer(0, vertexBuffer, 0);
	eg::DC.BindIndexBuffer(eg::IndexType::UInt16, indexBuffer, 0);
	
	eg::DC.DrawIndexed(0, numIndices, 0, 0, 1);
	
	
	
	eg::DC.BindPipeline(wr.pipelinePoint);
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
	
	eg::DC.BindTexture(*wr.diffuseTexture, 1, &wr.sampler);
	eg::DC.BindTexture(*wr.normalMapTexture, 2, &wr.sampler);
	eg::DC.BindTexture(*wr.miscMapTexture, 3, &wr.sampler);
	
	eg::DC.BindVertexBuffer(0, vertexBuffer, 0);
	eg::DC.BindIndexBuffer(eg::IndexType::UInt16, indexBuffer, 0);
	
	LightData lights[2];
	lights[0].pos = glm::vec3(0, 1, 1.5f);
	lights[0].radiance = glm::vec3(2.0f);
	lights[1].pos = glm::vec3(0, 1, -3.5f);
	lights[1].radiance = glm::vec3(2.0f);
	
	for (const LightData& light : lights)
	{
		eg::DC.PushConstants(0, light);
		eg::DC.DrawIndexed(0, numIndices, 0, 0, 1);
	}
}

void DrawWallsEditor(eg::BufferRef vertexBuffer, eg::BufferRef indexBuffer, uint32_t numIndices)
{
	eg::DC.BindPipeline(wr.pipelineEditor);
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
	
	eg::DC.BindTexture(*wr.diffuseTexture, 1, &wr.sampler);
	eg::DC.BindTexture(*wr.normalMapTexture, 2, &wr.sampler);
	eg::DC.BindTexture(*wr.gridTexture, 3);
	
	eg::DC.BindVertexBuffer(0, vertexBuffer, 0);
	eg::DC.BindIndexBuffer(eg::IndexType::UInt16, indexBuffer, 0);
	
	eg::DC.DrawIndexed(0, numIndices, 0, 0, 1);
}

void DrawWallBordersEditor(eg::BufferRef vertexBuffer, uint32_t numVertices)
{
	eg::DC.BindPipeline(wr.pipelineBorderEditor);
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
	
	eg::DC.BindVertexBuffer(0, vertexBuffer, 0);
	
	eg::DC.Draw(0, numVertices, 0, 1);
}
