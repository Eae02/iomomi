#include "GravityCornerMaterial.hpp"
#include "../Renderer.hpp"

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
	pipelineCI.cullMode = eg::CullMode::Back;
	//pipelineCI.frontFaceCCW = true;
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

eg::PipelineRef GravityCornerMaterial::GetPipeline(PipelineType pipelineType, bool flipWinding) const
{
	switch (pipelineType)
	{
	case PipelineType::Game: return gravityCornerPipelineGame;
	case PipelineType::Editor: return gravityCornerPipelineEditor;
	}
	EG_UNREACHABLE
}

void GravityCornerMaterial::Bind(ObjectMaterial::PipelineType boundPipeline) const
{
	
}
