#include "WaterSimulationShaders.hpp"

WaterSimulationShaders waterSimShaders;

static uint32_t SelectSortWorkGroupSize()
{
	const auto& deviceInfo = eg::GetGraphicsDeviceInfo();

	const uint32_t options[] = { 1024, 512, 256 };
	for (uint32_t optSize : options)
	{
		if (optSize <= deviceInfo.maxComputeWorkGroupSize[0] && optSize <= deviceInfo.maxComputeWorkGroupInvocations &&
		    optSize / deviceInfo.subgroupFeatures->maxSubgroupSize <=
		        deviceInfo.subgroupFeatures->maxWorkgroupSubgroups)
		{
			return optSize;
		}
	}
	return 128;
}

void WaterSimulationShaders::Initialize()
{
	if (!eg::GetGraphicsDeviceInfo().subgroupFeatures.has_value() ||
	    !eg::GetGraphicsDeviceInfo().subgroupFeatures->supportsRequiredSubgroupSize ||
	    !eg::GetGraphicsDeviceInfo().subgroupFeatures->supportsRequireFullSubgroups)
	{
		return;
	}

	waterSimShaders.isWaterSupported = true;

	uint32_t maxSubgroupSize = eg::GetGraphicsDeviceInfo().subgroupFeatures->maxSubgroupSize;

	sortWorkGroupSize = SelectSortWorkGroupSize();
	commonWorkGroupSize = maxSubgroupSize;

	const eg::SpecializationConstantEntry sortSpecConstants[] = { { 0, sortWorkGroupSize } };
	const eg::SpecializationConstantEntry subgroupSizeSpecConstants[] = { { 0, maxSubgroupSize } };

	sortPipelineInitial = eg::Pipeline::Create(eg::ComputePipelineCreateInfo{
		.computeShader = {
			.shaderModule = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/Simulation/1_Sort/SortInitial.cs.glsl").DefaultVariant(),
			.specConstants = sortSpecConstants,
		},
		.setBindModes = { eg::BindMode::DescriptorSet },
		.requireFullSubgroups = true,
		.requiredSubgroupSize = maxSubgroupSize,
		.label = "WaterSimSortInitial",
	});

	sortPipelineNear = eg::Pipeline::Create(eg::ComputePipelineCreateInfo{
		.computeShader = {
			.shaderModule = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/Simulation/1_Sort/SortNear.cs.glsl").DefaultVariant(),
			.specConstants = sortSpecConstants,
		},
		.setBindModes = { eg::BindMode::DescriptorSet },
		.requireFullSubgroups = true,
		.requiredSubgroupSize = maxSubgroupSize,
		.label = "WaterSimSortNear",
	});

	sortPipelineFar = eg::Pipeline::Create(eg::ComputePipelineCreateInfo{
		.computeShader = {
			.shaderModule = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/Simulation/1_Sort/SortFar.cs.glsl").DefaultVariant(),
			.specConstants = sortSpecConstants,
		},
		.setBindModes = { eg::BindMode::DescriptorSet },
		.label = "WaterSimSortFar",
	});

	detectCellEdges = eg::Pipeline::Create(eg::ComputePipelineCreateInfo{
		.computeShader = {
			.shaderModule = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/Simulation/2_CellPrepare/DetectCellEdges.cs.glsl").DefaultVariant(),
			.specConstants = subgroupSizeSpecConstants,
		},
		.setBindModes = { eg::BindMode::DescriptorSet },
		.requireFullSubgroups = true,
		.requiredSubgroupSize = maxSubgroupSize,
		.label = "DetectCellEdges",
	});

	octGroupsPrefixSum = eg::Pipeline::Create(eg::ComputePipelineCreateInfo{
		.computeShader = {
			.shaderModule = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/Simulation/2_CellPrepare/OctGroupsPrefixSum.cs.glsl").DefaultVariant(),
			.specConstants = subgroupSizeSpecConstants,
		},
		.setBindModes = { eg::BindMode::DescriptorSet },
		.requireFullSubgroups = true,
		.requiredSubgroupSize = maxSubgroupSize,
		.label = "OctGroupsPrefixSum",
	});

	writeOctGroups = eg::Pipeline::Create(eg::ComputePipelineCreateInfo{
		.computeShader = {
			.shaderModule = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/Simulation/2_CellPrepare/WriteOctGroups.cs.glsl").DefaultVariant(),
			.specConstants = subgroupSizeSpecConstants,
		},
		.setBindModes = { eg::BindMode::DescriptorSet },
		.requireFullSubgroups = true,
		.requiredSubgroupSize = maxSubgroupSize,
		.label = "WriteOctGroups",
	});

	calculateDensity = eg::Pipeline::Create(eg::ComputePipelineCreateInfo{
		.computeShader =
			eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/Simulation/CalcDensity.cs.glsl").ToStageInfo(),
		.setBindModes = { eg::BindMode::DescriptorSet, eg::BindMode::DescriptorSet },
		.requireFullSubgroups = true,
		.requiredSubgroupSize = maxSubgroupSize,
		.label = "CalcDensity",
	});

	calculateAcceleration = eg::Pipeline::Create(eg::ComputePipelineCreateInfo{
		.computeShader =
			eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/Simulation/CalcAccel.cs.glsl").ToStageInfo(),
		.setBindModes = { eg::BindMode::DescriptorSet, eg::BindMode::DescriptorSet },
		.requireFullSubgroups = true,
		.requiredSubgroupSize = maxSubgroupSize,
		.label = "CalcAccel",
	});

	velocityDiffusion = eg::Pipeline::Create(eg::ComputePipelineCreateInfo{
		.computeShader =
			eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/Simulation/VelocityDiffusion.cs.glsl").ToStageInfo(),
		.setBindModes = { eg::BindMode::DescriptorSet },
		.requireFullSubgroups = true,
		.requiredSubgroupSize = maxSubgroupSize,
		.label = "VelocityDiffusion",
	});

	moveAndCollision = eg::Pipeline::Create(eg::ComputePipelineCreateInfo{
		.computeShader =
			eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/Simulation/MoveAndCollision.cs.glsl").ToStageInfo(),
		.setBindModes = { eg::BindMode::DescriptorSet },
		.label = "WaterMoveAndCollision",
	});

	changeGravity = eg::Pipeline::Create(eg::ComputePipelineCreateInfo{
		.computeShader =
			eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/Simulation/ChangeGravity.cs.glsl").ToStageInfo(),
		.setBindModes = { eg::BindMode::DescriptorSet },
		.label = "WaterChangeGravity",
	});
}

static void OnShutdown()
{
	waterSimShaders = {};
}
EG_ON_SHUTDOWN(OnShutdown)
