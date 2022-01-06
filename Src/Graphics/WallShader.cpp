#include "WallShader.hpp"
#include "RenderSettings.hpp"
#include "DeferredRenderer.hpp"
#include "GraphicsCommon.hpp"
#include "Lighting/PointLightShadowMapper.hpp"
#include "../Settings.hpp"

struct
{
	eg::Pipeline pipelineDeferredGeom;
	eg::Pipeline pipelineEditor;
	eg::Pipeline pipelineBorderEditor;
	eg::Pipeline pipelinePLShadow;
	eg::Texture* diffuseTexture;
	eg::Texture* normalMapTexture;
	eg::Texture* miscMapTexture;
	eg::Texture* gridTexture;
	eg::Texture* noDrawTexture;
	eg::DescriptorSet gameDescriptorSet;
	eg::DescriptorSet editorDescriptorSet;
} wr;

WallMaterial wallMaterials[MAX_WALL_MATERIALS];

static inline void InitializeMaterials()
{
	//                   true, editor name,       texscale, rmin, rmax
	wallMaterials[0] = { true, "No Draw",             2.0f, 0.6f, 1.0f };
	wallMaterials[1] = { true, "Tactile Gray",        2.0f, 0.6f, 1.0f };
	wallMaterials[2] = { true, "Hex Panels",          2.0f, 0.2f, 0.7f };
	wallMaterials[3] = { true, "Metal Grid",          2.5f, 0.2f, 0.7f };
	wallMaterials[4] = { true, "Cement",              2.0f, 0.8f, 1.0f };
	wallMaterials[5] = { true, "Concrete Panels",     2.0f, 0.8f, 1.0f };
	wallMaterials[6] = { true, "Smooth Panels",       1.0f, 0.1f, 0.5f };
	wallMaterials[7] = { true, "Concrete Panels (S)", 2.0f, 0.8f, 1.0f };
	wallMaterials[8] = { true, "Tiles",               2.0f, 0.1f, 0.5f };
}

static constexpr float FRONT_FACE_SHADOW_BIAS = 0.5f;
static constexpr float BACK_FACE_SHADOW_BIAS = -0.05f;

void InitializeWallShader()
{
	InitializeMaterials();
	
	//Creates the game pipeline
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Wall.vs.glsl").DefaultVariant();
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Wall.fs.glsl").DefaultVariant();
	pipelineCI.enableDepthWrite = true;
	pipelineCI.enableDepthTest = true;
	pipelineCI.cullMode = eg::CullMode::Back;
	pipelineCI.numColorAttachments = 2;
	pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	pipelineCI.vertexBindings[0] = { sizeof(WallVertex), eg::InputRate::Vertex };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32,   3, (uint32_t)offsetof(WallVertex, position) };
	pipelineCI.vertexAttributes[1] = { 0, eg::DataType::Float32,   3, (uint32_t)offsetof(WallVertex, texCoord) };
	pipelineCI.vertexAttributes[2] = { 0, eg::DataType::SInt8Norm, 4, (uint32_t)offsetof(WallVertex, normalAndRoughnessLo) };
	pipelineCI.vertexAttributes[3] = { 0, eg::DataType::SInt8Norm, 4, (uint32_t)offsetof(WallVertex, tangentAndRoughnessHi) };
	wr.pipelineDeferredGeom = eg::Pipeline::Create(pipelineCI);
	wr.pipelineDeferredGeom.FramebufferFormatHint(DeferredRenderer::GEOMETRY_FB_FORMAT);
	
	//Creates the editor pipeline
	eg::GraphicsPipelineCreateInfo editorPipelineCI;
	editorPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Wall-Editor.vs.glsl").DefaultVariant();
	editorPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Wall-Editor.fs.glsl").DefaultVariant();
	editorPipelineCI.enableDepthWrite = true;
	editorPipelineCI.enableDepthTest = true;
	editorPipelineCI.cullMode = eg::CullMode::Back;
	editorPipelineCI.depthCompare = eg::CompareOp::Less;
	editorPipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	editorPipelineCI.frontFaceCCW = false;
	editorPipelineCI.numColorAttachments = 1;
	editorPipelineCI.vertexBindings[0] = { sizeof(WallVertex), eg::InputRate::Vertex };
	editorPipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32,   3, (uint32_t)offsetof(WallVertex, position) };
	editorPipelineCI.vertexAttributes[1] = { 0, eg::DataType::Float32,   3, (uint32_t)offsetof(WallVertex, texCoord) };
	editorPipelineCI.vertexAttributes[2] = { 0, eg::DataType::SInt8Norm, 3, (uint32_t)offsetof(WallVertex, normalAndRoughnessLo) };
	editorPipelineCI.vertexAttributes[3] = { 0, eg::DataType::SInt8Norm, 3, (uint32_t)offsetof(WallVertex, tangentAndRoughnessHi) };
	wr.pipelineEditor = eg::Pipeline::Create(editorPipelineCI);
	wr.pipelineEditor.FramebufferFormatHint(eg::Format::DefaultColor, eg::Format::DefaultDepthStencil);
	
	//Creates the editor border pipeline
	eg::GraphicsPipelineCreateInfo borderPipelineCI;
	borderPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Wall-Border.vs.glsl").DefaultVariant();
	borderPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Wall-Border.fs.glsl").DefaultVariant();
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
	
	const eg::SpecializationConstantEntry plsPipelineSpecConstants[2] = { { 10, 0, 4 }, { 11, 4, 4 } };
	float plsPipelineSpecConstantData[2] = { FRONT_FACE_SHADOW_BIAS, BACK_FACE_SHADOW_BIAS };
	
	//Creates the point light shadow pipeline
	eg::GraphicsPipelineCreateInfo plsPipelineCI;
	plsPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Wall-PLShadow.vs.glsl").DefaultVariant();
	plsPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/PointLightShadow.fs.glsl").GetVariant("VNoAlphaTest");
	plsPipelineCI.enableDepthWrite = true;
	plsPipelineCI.enableDepthTest = true;
	plsPipelineCI.frontFaceCCW = eg::CurrentGraphicsAPI() == eg::GraphicsAPI::Vulkan;
	plsPipelineCI.cullMode = eg::CullMode::None;
	plsPipelineCI.vertexBindings[0] = { sizeof(WallVertex), eg::InputRate::Vertex };
	plsPipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32,   3, (uint32_t)offsetof(WallVertex, position) };
	plsPipelineCI.fragmentShader.specConstants = plsPipelineSpecConstants;
	plsPipelineCI.fragmentShader.specConstantsData = plsPipelineSpecConstantData;
	plsPipelineCI.fragmentShader.specConstantsDataSize = sizeof(plsPipelineSpecConstantData);
	wr.pipelinePLShadow = eg::Pipeline::Create(plsPipelineCI);
	
	eg::FramebufferFormatHint plsFormatHint;
	plsFormatHint.sampleCount = 1;
	plsFormatHint.depthStencilFormat = PointLightShadowMapper::SHADOW_MAP_FORMAT;
	wr.pipelinePLShadow.FramebufferFormatHint(plsFormatHint);
	
	wr.diffuseTexture = &eg::GetAsset<eg::Texture>("WallTextures/Albedo");
	wr.normalMapTexture = &eg::GetAsset<eg::Texture>("WallTextures/NormalMap");
	wr.miscMapTexture = &eg::GetAsset<eg::Texture>("WallTextures/MiscMap");
	wr.gridTexture = &eg::GetAsset<eg::Texture>("Textures/Grid.png");
	wr.noDrawTexture = &eg::GetAsset<eg::Texture>("Textures/NoDraw.png");
	
	float materialSettings[(MAX_WALL_MATERIALS - 1) * 4];
	for (size_t i = 1; i < MAX_WALL_MATERIALS; i++)
	{
		float* settingsDst = materialSettings + (i - 1) * 4;
		settingsDst[0] = wallMaterials[i].textureScale;
		settingsDst[1] = wallMaterials[i].minRoughness;
		settingsDst[2] = wallMaterials[i].maxRoughness;
		settingsDst[3] = 0;
	}
	
	wr.gameDescriptorSet = { wr.pipelineDeferredGeom, 0 };
	wr.gameDescriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
	wr.gameDescriptorSet.BindTexture(*wr.diffuseTexture, 1, &commonTextureSampler);
	wr.gameDescriptorSet.BindTexture(*wr.normalMapTexture, 2, &commonTextureSampler);
	wr.gameDescriptorSet.BindTexture(*wr.miscMapTexture, 3, &commonTextureSampler);
	
	wr.editorDescriptorSet = { wr.pipelineEditor, 0 };
	wr.editorDescriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
	wr.editorDescriptorSet.BindTexture(*wr.diffuseTexture, 1, &commonTextureSampler);
	wr.editorDescriptorSet.BindTexture(*wr.normalMapTexture, 2, &commonTextureSampler);
	wr.editorDescriptorSet.BindTexture(*wr.gridTexture, 3);
	wr.editorDescriptorSet.BindTexture(*wr.noDrawTexture, 4);
}

static void OnShutdown()
{
	wr = { };
}

EG_ON_SHUTDOWN(OnShutdown)

void BindWallShaderGame()
{
	eg::DC.BindPipeline(wr.pipelineDeferredGeom);
	
	eg::DC.BindDescriptorSet(wr.gameDescriptorSet, 0);
}

void BindWallShaderEditor(bool drawGrid)
{
	eg::DC.BindPipeline(wr.pipelineEditor);
	
	eg::DC.BindDescriptorSet(wr.editorDescriptorSet, 0);
	
	float pc = drawGrid ? 1 : 0;
	eg::DC.PushConstants(0, sizeof(float), &pc);
}

void BindWallShaderPointLightShadow(const PointLightShadowDrawArgs& renderArgs)
{
	eg::DC.BindPipeline(wr.pipelinePLShadow);
	renderArgs.SetPushConstants();
}

void DrawWallBordersEditor(eg::BufferRef vertexBuffer, uint32_t numVertices)
{
	eg::DC.BindPipeline(wr.pipelineBorderEditor);
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	
	eg::DC.BindVertexBuffer(0, vertexBuffer, 0);
	
	eg::DC.Draw(0, numVertices, 0, 1);
}
