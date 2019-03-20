#include "GravityCornerMaterial.hpp"
#include "../Renderer.hpp"
#include "MeshDrawArgs.hpp"
#include "../RenderSettings.hpp"

GravityCornerMaterial GravityCornerMaterial::instance;

static eg::Pipeline gravityCornerPipelineEditor;
static eg::Pipeline gravityCornerPipelineGame;

static void OnInit()
{
	eg::PipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModule>("Shaders/Common3D.vs.glsl").Handle();
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModule>("Shaders/GravityCorner.fs.glsl").Handle();
	pipelineCI.enableDepthWrite = true;
	pipelineCI.enableDepthTest = true;
	pipelineCI.frontFaceCCW = true;
	pipelineCI.cullMode = eg::CullMode::Back;
	pipelineCI.vertexBindings[0] = { sizeof(eg::StdVertex), eg::InputRate::Vertex };
	pipelineCI.vertexBindings[1] = { sizeof(float) * 16, eg::InputRate::Instance };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, offsetof(eg::StdVertex, position) };
	pipelineCI.vertexAttributes[1] = { 0, eg::DataType::Float32, 2, offsetof(eg::StdVertex, texCoord) };
	pipelineCI.vertexAttributes[2] = { 0, eg::DataType::SInt8Norm, 3, offsetof(eg::StdVertex, normal) };
	pipelineCI.vertexAttributes[3] = { 0, eg::DataType::SInt8Norm, 3, offsetof(eg::StdVertex, tangent) };
	pipelineCI.vertexAttributes[4] = { 1, eg::DataType::Float32, 4, 0 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[5] = { 1, eg::DataType::Float32, 4, 1 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[6] = { 1, eg::DataType::Float32, 4, 2 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[7] = { 1, eg::DataType::Float32, 4, 3 * sizeof(float) * 4 };
	pipelineCI.numColorAttachments = 2;
	gravityCornerPipelineGame = eg::Pipeline::Create(pipelineCI);
	gravityCornerPipelineGame.FramebufferFormatHint(Renderer::GEOMETRY_FB_FORMAT);
	
	pipelineCI.numColorAttachments = 1;
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModule>("Shaders/GravityCorner-Editor.fs.glsl").Handle();
	pipelineCI.cullMode = eg::CullMode::None;
	gravityCornerPipelineEditor = eg::Pipeline::Create(pipelineCI);
	gravityCornerPipelineEditor.FramebufferFormatHint(eg::Format::DefaultColor, eg::Format::DefaultDepthStencil);
}

static void OnShutdown()
{
	gravityCornerPipelineEditor.Destroy();
	gravityCornerPipelineGame.Destroy();
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

size_t GravityCornerMaterial::PipelineHash() const
{
	return typeid(GravityCornerMaterial).hash_code();
}

inline static eg::PipelineRef GetPipeline(const MeshDrawArgs& drawArgs)
{
	switch (drawArgs.drawMode)
	{
	case MeshDrawMode::Game: return gravityCornerPipelineGame;
	case MeshDrawMode::Editor: return gravityCornerPipelineEditor;
	case MeshDrawMode::PointLightShadow: return eg::PipelineRef();
	}
	EG_UNREACHABLE
}

bool GravityCornerMaterial::BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	MeshDrawArgs* mDrawArgs = static_cast<MeshDrawArgs*>(drawArgs);
	eg::PipelineRef pipeline = GetPipeline(*mDrawArgs);
	if (pipeline.handle == nullptr)
		return false;
	cmdCtx.BindPipeline(pipeline);
	cmdCtx.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
	return true;
}

bool GravityCornerMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	return true;
}
