#include "WallShader.hpp"

WallShader::WallShader()
{
	eg::ShaderProgram program;
	program.AddStageFromAsset("Shaders/Wall.vs.glsl");
	program.AddStageFromAsset("Shaders/Wall.fs.glsl");
	
	eg::FixedFuncState fixedFuncState;
	fixedFuncState.enableDepthWrite = true;
	fixedFuncState.enableDepthTest = true;
	fixedFuncState.cullMode = eg::CullMode::Back;
	fixedFuncState.vertexBindings[0] = { sizeof(WallVertex), eg::InputRate::Vertex };
	fixedFuncState.vertexAttributes[0] = { 0, eg::DataType::Float32,   3, (uint32_t)offsetof(WallVertex, position) };
	fixedFuncState.vertexAttributes[1] = { 0, eg::DataType::UInt8,     3, (uint32_t)offsetof(WallVertex, texCoord) };
	fixedFuncState.vertexAttributes[2] = { 0, eg::DataType::SInt8Norm, 3, (uint32_t)offsetof(WallVertex, normal) };
	fixedFuncState.vertexAttributes[3] = { 0, eg::DataType::SInt8Norm, 3, (uint32_t)offsetof(WallVertex, tangent) };
	m_pipeline = program.CreatePipeline(fixedFuncState);
	
	m_diffuseTexture = &eg::GetAsset<eg::Texture>("Textures/Voxel/Diffuse");
}

void WallShader::Draw(eg::BufferRef vertexBuffer, eg::BufferRef indexBuffer, uint32_t numIndices) const
{
	eg::DC.BindPipeline(m_pipeline);
	
	eg::DC.BindTexture(*m_diffuseTexture, 0);
	
	eg::DC.BindVertexBuffer(0, vertexBuffer, 0);
	eg::DC.BindIndexBuffer(eg::IndexType::UInt16, indexBuffer, 0);
	
	eg::DC.DrawIndexed(0, numIndices, 0, 1);
}
