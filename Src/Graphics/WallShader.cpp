#include "WallShader.hpp"

#include "../Editor/EditorGraphics.hpp"
#include "GraphicsCommon.hpp"
#include "Lighting/PointLightShadowMapper.hpp"
#include "RenderSettings.hpp"
#include "RenderTargets.hpp"

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
	wallMaterials[0] = { true, "No Draw", 2.0f, 0.6f, 1.0f };
	wallMaterials[1] = { true, "Tactile Gray", 2.0f, 0.6f, 1.0f };
	wallMaterials[2] = { true, "Hex Panels", 2.0f, 0.2f, 0.7f };
	wallMaterials[3] = { true, "Metal Grid", 2.5f, 0.2f, 0.7f };
	wallMaterials[4] = { true, "Cement", 2.0f, 0.8f, 1.0f };
	wallMaterials[5] = { true, "Concrete Panels", 2.0f, 0.8f, 1.0f };
	wallMaterials[6] = { true, "Smooth Panels", 1.0f, 0.1f, 0.5f };
	wallMaterials[7] = { true, "Concrete Panels (S)", 2.0f, 0.8f, 1.0f };
	wallMaterials[8] = { true, "Tiles", 2.0f, 0.1f, 0.5f };
}

static constexpr float FRONT_FACE_SHADOW_BIAS = 0.5f;
static constexpr float BACK_FACE_SHADOW_BIAS = -0.05f;

void InitializeWallShader()
{
	InitializeMaterials();

	// Creates the game pipeline
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Wall.vs.glsl").ToStageInfo();
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Wall.fs.glsl").ToStageInfo();
	pipelineCI.enableDepthWrite = true;
	pipelineCI.enableDepthTest = true;
	pipelineCI.cullMode = eg::CullMode::Back;
	pipelineCI.numColorAttachments = 2;
	pipelineCI.vertexBindings[0] = { sizeof(WallVertex), eg::InputRate::Vertex };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, offsetof(WallVertex, position) };
	pipelineCI.vertexAttributes[1] = { 0, eg::DataType::Float32, 3, offsetof(WallVertex, texCoord) };
	pipelineCI.vertexAttributes[2] = { 0, eg::DataType::SInt8Norm, 4, offsetof(WallVertex, normalAndRoughnessLo) };
	pipelineCI.vertexAttributes[3] = { 0, eg::DataType::SInt8Norm, 4, offsetof(WallVertex, tangentAndRoughnessHi) };
	pipelineCI.colorAttachmentFormats[0] = GB_COLOR_FORMAT;
	pipelineCI.colorAttachmentFormats[1] = GB_COLOR_FORMAT;
	pipelineCI.depthAttachmentFormat = GB_DEPTH_FORMAT;
	wr.pipelineDeferredGeom = eg::Pipeline::Create(pipelineCI);

	// Creates the editor pipeline
	eg::GraphicsPipelineCreateInfo editorPipelineCI;
	editorPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Wall-Editor.vs.glsl").ToStageInfo();
	editorPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Wall-Editor.fs.glsl").ToStageInfo();
	editorPipelineCI.enableDepthWrite = true;
	editorPipelineCI.enableDepthTest = true;
	editorPipelineCI.cullMode = eg::CullMode::Back;
	editorPipelineCI.depthCompare = eg::CompareOp::Less;
	editorPipelineCI.frontFaceCCW = false;
	editorPipelineCI.numColorAttachments = 1;
	editorPipelineCI.vertexBindings[0] = { sizeof(WallVertex), eg::InputRate::Vertex };
	editorPipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, offsetof(WallVertex, position) };
	editorPipelineCI.vertexAttributes[1] = { 0, eg::DataType::Float32, 3, offsetof(WallVertex, texCoord) };
	editorPipelineCI.vertexAttributes[2] = { 0, eg::DataType::SInt8Norm, 4,
		                                     offsetof(WallVertex, normalAndRoughnessLo) };
	editorPipelineCI.vertexAttributes[3] = { 0, eg::DataType::SInt8Norm, 4,
		                                     offsetof(WallVertex, tangentAndRoughnessHi) };
	editorPipelineCI.colorAttachmentFormats[0] = EDITOR_COLOR_FORMAT;
	editorPipelineCI.depthAttachmentFormat = EDITOR_DEPTH_FORMAT;
	wr.pipelineEditor = eg::Pipeline::Create(editorPipelineCI);

	// Creates the editor border pipeline
	eg::GraphicsPipelineCreateInfo borderPipelineCI;
	borderPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Wall-Border.vs.glsl").ToStageInfo();
	borderPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Wall-Border.fs.glsl").ToStageInfo();
	borderPipelineCI.enableDepthWrite = false;
	borderPipelineCI.enableDepthTest = true;
	borderPipelineCI.cullMode = eg::CullMode::None;
	borderPipelineCI.topology = eg::Topology::LineList;
	borderPipelineCI.vertexBindings[0] = { sizeof(WallBorderVertex), eg::InputRate::Vertex };
	borderPipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 4, offsetof(WallBorderVertex, position) };
	borderPipelineCI.vertexAttributes[1] = { 0, eg::DataType::SInt8Norm, 4, offsetof(WallBorderVertex, normal1) };
	borderPipelineCI.vertexAttributes[2] = { 0, eg::DataType::SInt8Norm, 4, offsetof(WallBorderVertex, normal2) };
	borderPipelineCI.colorAttachmentFormats[0] = EDITOR_COLOR_FORMAT;
	borderPipelineCI.depthAttachmentFormat = EDITOR_DEPTH_FORMAT;
	wr.pipelineBorderEditor = eg::Pipeline::Create(borderPipelineCI);

	const eg::SpecializationConstantEntry plsPipelineSpecConstants[] = { { 10, FRONT_FACE_SHADOW_BIAS },
		                                                                 { 11, BACK_FACE_SHADOW_BIAS } };

	// Creates the point light shadow pipeline
	eg::GraphicsPipelineCreateInfo plsPipelineCI;
	plsPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Wall-PLShadow.vs.glsl").ToStageInfo();
	plsPipelineCI.fragmentShader = {
		.shaderModule =
			eg::GetAsset<eg::ShaderModuleAsset>("Shaders/PointLightShadow.fs.glsl").GetVariant("VNoAlphaTest"),
		.specConstants = plsPipelineSpecConstants,
	};
	plsPipelineCI.enableDepthWrite = true;
	plsPipelineCI.enableDepthTest = true;
	plsPipelineCI.frontFaceCCW = PointLightShadowMapper::FlippedLightMatrix();
	plsPipelineCI.cullMode = eg::CullMode::None;
	plsPipelineCI.descriptorSetBindings[0] = PointLightShadowDrawArgs::PARAMETERS_DS_BINDINGS;
	plsPipelineCI.vertexBindings[0] = { sizeof(WallVertex), eg::InputRate::Vertex };
	plsPipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, offsetof(WallVertex, position) };
	plsPipelineCI.numColorAttachments = 0;
	plsPipelineCI.depthAttachmentFormat = PointLightShadowMapper::SHADOW_MAP_FORMAT;
	wr.pipelinePLShadow = eg::Pipeline::Create(plsPipelineCI);

	wr.diffuseTexture = &eg::GetAsset<eg::Texture>("Textures/Wall/Albedo");
	wr.normalMapTexture = &eg::GetAsset<eg::Texture>("Textures/Wall/NormalMap");
	wr.miscMapTexture = &eg::GetAsset<eg::Texture>("Textures/Wall/MiscMap");
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
	wr.gameDescriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0);
	wr.gameDescriptorSet.BindTexture(*wr.diffuseTexture, 1);
	wr.gameDescriptorSet.BindTexture(*wr.normalMapTexture, 2);
	wr.gameDescriptorSet.BindTexture(*wr.miscMapTexture, 3);
	wr.gameDescriptorSet.BindSampler(samplers::linearRepeatAnisotropic, 4);

	wr.editorDescriptorSet = { wr.pipelineEditor, 0 };
	wr.editorDescriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0);
	wr.editorDescriptorSet.BindTexture(*wr.diffuseTexture, 1);
	wr.editorDescriptorSet.BindTexture(*wr.normalMapTexture, 2);
	wr.editorDescriptorSet.BindTexture(*wr.gridTexture, 3);
	wr.editorDescriptorSet.BindTexture(*wr.noDrawTexture, 4);
	wr.editorDescriptorSet.BindSampler(samplers::linearRepeatAnisotropic, 5);
}

static void OnShutdown()
{
	wr = {};
}

EG_ON_SHUTDOWN(OnShutdown)

void BindWallShaderGame()
{
	eg::DC.BindPipeline(wr.pipelineDeferredGeom);

	eg::DC.BindDescriptorSet(wr.gameDescriptorSet, 0);
}

void BindWallShaderEditor()
{
	eg::DC.BindPipeline(wr.pipelineEditor);

	eg::DC.BindDescriptorSet(wr.editorDescriptorSet, 0);
}

void BindWallShaderPointLightShadow(const PointLightShadowDrawArgs& renderArgs)
{
	eg::DC.BindPipeline(wr.pipelinePLShadow);
	renderArgs.BindParametersDescriptorSet(eg::DC, 0);
}

void DrawWallBordersEditor(eg::BufferRef vertexBuffer, uint32_t numVertices)
{
	eg::DC.BindPipeline(wr.pipelineBorderEditor);

	RenderSettings::instance->BindVertexShaderDescriptorSet();

	eg::DC.BindVertexBuffer(0, vertexBuffer, 0);

	eg::DC.Draw(0, numVertices, 0, 1);
}
