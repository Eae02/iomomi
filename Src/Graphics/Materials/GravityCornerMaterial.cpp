#include "GravityCornerMaterial.hpp"

GravityCornerMaterial GravityCornerMaterial::instance;

static eg::Pipeline gravityCornerPipeline;

static void OnInit()
{
	eg::ShaderProgram gravityCornerShaderProgram;
	gravityCornerShaderProgram.AddStageFromAsset("Shaders/Common3D.vs.glsl");
	gravityCornerShaderProgram.AddStageFromAsset("Shaders/GravityCorner.fs.glsl");
	
	eg::FixedFuncState fixedFuncState;
	fixedFuncState.enableDepthWrite = true;
	fixedFuncState.enableDepthTest = true;
	fixedFuncState.cullMode = eg::CullMode::Back;
	//fixedFuncState.frontFaceCCW = true;
	fixedFuncState.depthFormat = eg::Format::DefaultDepthStencil;
	fixedFuncState.attachments[0].format = eg::Format::DefaultColor;
	fixedFuncState.vertexBindings[0] = { sizeof(eg::StdVertex), eg::InputRate::Vertex };
	fixedFuncState.vertexBindings[1] = { sizeof(float) * 16, eg::InputRate::Instance };
	fixedFuncState.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, offsetof(eg::StdVertex, position) };
	fixedFuncState.vertexAttributes[1] = { 0, eg::DataType::Float32, 2, offsetof(eg::StdVertex, texCoord) };
	fixedFuncState.vertexAttributes[2] = { 0, eg::DataType::SInt8Norm, 3, offsetof(eg::StdVertex, normal) };
	fixedFuncState.vertexAttributes[3] = { 0, eg::DataType::SInt8Norm, 3, offsetof(eg::StdVertex, tangent) };
	fixedFuncState.vertexAttributes[4] = { 1, eg::DataType::Float32, 4, 0 * sizeof(float) * 4 };
	fixedFuncState.vertexAttributes[5] = { 1, eg::DataType::Float32, 4, 1 * sizeof(float) * 4 };
	fixedFuncState.vertexAttributes[6] = { 1, eg::DataType::Float32, 4, 2 * sizeof(float) * 4 };
	fixedFuncState.vertexAttributes[7] = { 1, eg::DataType::Float32, 4, 3 * sizeof(float) * 4 };
	
	gravityCornerPipeline = gravityCornerShaderProgram.CreatePipeline(fixedFuncState);
}

static void OnShutdown()
{
	gravityCornerPipeline.Destroy();
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

eg::PipelineRef GravityCornerMaterial::GetPipeline() const
{
	return gravityCornerPipeline;
}

void GravityCornerMaterial::Bind() const
{
	
}
