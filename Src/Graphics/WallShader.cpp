#include "WallShader.hpp"
#include "RenderSettings.hpp"

struct
{
	eg::Pipeline pipeline;
	eg::Texture* diffuseTexture;
} wr;

static void OnInit()
{
	eg::ShaderProgram program;
	program.AddStageFromAsset("Shaders/Wall.vs.glsl");
	program.AddStageFromAsset("Shaders/Wall.fs.glsl");
	
	eg::FixedFuncState fixedFuncState;
	fixedFuncState.enableDepthWrite = true;
	fixedFuncState.enableDepthTest = true;
	fixedFuncState.cullMode = eg::CullMode::Back;
	fixedFuncState.depthFormat = eg::Format::DefaultDepthStencil;
	fixedFuncState.attachments[0].format = eg::Format::DefaultColor;
	fixedFuncState.vertexBindings[0] = { sizeof(WallVertex), eg::InputRate::Vertex };
	fixedFuncState.vertexAttributes[0] = { 0, eg::DataType::Float32,   3, (uint32_t)offsetof(WallVertex, position) };
	fixedFuncState.vertexAttributes[1] = { 0, eg::DataType::UInt8,     3, (uint32_t)offsetof(WallVertex, texCoord) };
	fixedFuncState.vertexAttributes[2] = { 0, eg::DataType::SInt8Norm, 3, (uint32_t)offsetof(WallVertex, normal) };
	fixedFuncState.vertexAttributes[3] = { 0, eg::DataType::SInt8Norm, 3, (uint32_t)offsetof(WallVertex, tangent) };
	wr.pipeline = program.CreatePipeline(fixedFuncState);
	
	wr.diffuseTexture = &eg::GetAsset<eg::Texture>("Textures/Voxel/Diffuse");
}

static void OnShutdown()
{
	wr.pipeline.Destroy();
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

void DrawWalls(const class RenderSettings& renderSettings, eg::BufferRef vertexBuffer, eg::BufferRef indexBuffer,
	uint32_t numIndices)
{
	eg::DC.BindPipeline(wr.pipeline);
	
	eg::DC.BindUniformBuffer(renderSettings.Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
	
	eg::DC.BindTexture(*wr.diffuseTexture, 1);
	
	eg::DC.BindVertexBuffer(0, vertexBuffer, 0);
	eg::DC.BindIndexBuffer(eg::IndexType::UInt16, indexBuffer, 0);
	
	eg::DC.DrawIndexed(0, numIndices, 0, 0, 1);
}
