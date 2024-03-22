#include "WaterSimulationShaders.hpp"

WaterSimulationShaders waterSimShaders;

static uint32_t SelectSortWorkGroupSize(bool checkSubgroupSize)
{
	const auto& deviceInfo = eg::GetGraphicsDeviceInfo();

	const uint32_t options[] = { 1024, 512, 256 };
	for (uint32_t optSize : options)
	{
		if (optSize > deviceInfo.maxComputeWorkGroupSize[0] || optSize > deviceInfo.maxComputeWorkGroupInvocations)
			continue;

		if (checkSubgroupSize &&
		    optSize / deviceInfo.subgroupFeatures->maxSubgroupSize > deviceInfo.subgroupFeatures->maxWorkgroupSubgroups)
			continue;

		return optSize;
	}
	return 128;
}

static inline const eg::ShaderModuleAsset& GetWaterSimShader(std::string_view name)
{
	std::string fullPath = eg::Concat({ "Shaders/Water/Simulation/", name });
	return eg::GetAsset<eg::ShaderModuleAsset>(fullPath);
}

static bool TryLoadOctGroupPrefixSumSubgroupShaders(WaterSimulationShaders& simShaders)
{
	constexpr uint32_t MIN_REQUIRED_SUBGROUP_SIZE = 16;

	const eg::SubgroupFeatures& subgroupFeatures = eg::GetGraphicsDeviceInfo().subgroupFeatures.value();
	if (subgroupFeatures.maxSubgroupSize < MIN_REQUIRED_SUBGROUP_SIZE)
		return false;

	eg::ComputePipelineCreateInfo createInfoLevel1 = {
		.computeShader = GetWaterSimShader("2_CellPrepare/OctGroupsPrefixSumFallbackLevel1.cs.glsl").ToStageInfo(),
		.requireFullSubgroups = true,
		.label = "OctGroupsPrefixSumLevel1",
	};
	eg::ComputePipelineCreateInfo createInfoLevel2 = {
		.computeShader = GetWaterSimShader("2_CellPrepare/OctGroupsPrefixSumFallbackLevel2.cs.glsl").ToStageInfo(),
		.requireFullSubgroups = true,
		.label = "OctGroupsPrefixSumLevel1",
	};

	bool checkCreatedSubgroupSize = false;

	if (subgroupFeatures.minSubgroupSize < MIN_REQUIRED_SUBGROUP_SIZE)
	{
		if (subgroupFeatures.supportsRequiredSubgroupSize)
		{
			createInfoLevel1.requiredSubgroupSize = subgroupFeatures.maxSubgroupSize;
			createInfoLevel2.requiredSubgroupSize = subgroupFeatures.maxSubgroupSize;
		}
		else
		{
			if (!subgroupFeatures.supportsGetPipelineSubgroupSize)
			{
				// The shaders may be created with a subgroup size that is too small, and we cannot query this so we
				// will have to use the fallback shaders
				return false;
			}
			checkCreatedSubgroupSize = true;
		}
	}

	eg::Pipeline pipelineLevel1 = eg::Pipeline::Create(createInfoLevel1);
	eg::Pipeline pipelineLevel2 = eg::Pipeline::Create(createInfoLevel2);

	if (checkCreatedSubgroupSize)
	{
		if (pipelineLevel1.TryGetSubgroupSize().value_or(0) < MIN_REQUIRED_SUBGROUP_SIZE)
			return false;
		if (pipelineLevel2.TryGetSubgroupSize().value_or(0) < MIN_REQUIRED_SUBGROUP_SIZE)
			return false;
	}

	simShaders.octGroupsPrefixSumLevel1 = std::move(pipelineLevel1);
	simShaders.octGroupsPrefixSumLevel2 = std::move(pipelineLevel2);
	return true;
}

static void LoadOctGroupPrefixSumFallbackShaders(WaterSimulationShaders& simShaders)
{
	simShaders.octGroupsPrefixSumLevel1 = eg::Pipeline::Create(eg::ComputePipelineCreateInfo{
		.computeShader = GetWaterSimShader("2_CellPrepare/OctGroupsPrefixSumFallbackLevel1.cs.glsl").ToStageInfo(),
		.label = "OctGroupsPrefixSumLevel1",
	});

	simShaders.octGroupsPrefixSumLevel2 = eg::Pipeline::Create(eg::ComputePipelineCreateInfo{
		.computeShader = GetWaterSimShader("2_CellPrepare/OctGroupsPrefixSumFallbackLevel2.cs.glsl").ToStageInfo(),
		.label = "OctGroupsPrefixSumLevel1",
	});
}

void WaterSimulationShaders::Initialize()
{
	if (!eg::HasFlag(eg::GetGraphicsDeviceInfo().features, eg::DeviceFeatureFlags::ComputeShaderAndSSBO))
	{
		return;
	}

	bool useFallbackShaders = forceUseFallbackShaders;
	if (!eg::GetGraphicsDeviceInfo().subgroupFeatures.has_value())
	{
		useFallbackShaders = true;
	}
	else
	{
		constexpr eg::SubgroupFeatureFlags REQUIRED_SUBGROUP_FEATURE_FLAGS =
			eg::SubgroupFeatureFlags::Basic | eg::SubgroupFeatureFlags::Vote | eg::SubgroupFeatureFlags::Arithmetic |
			eg::SubgroupFeatureFlags::Ballot | eg::SubgroupFeatureFlags::Shuffle;

		const eg::SubgroupFeatures& subgroupFeatures = *eg::GetGraphicsDeviceInfo().subgroupFeatures;
		if (!subgroupFeatures.supportsRequireFullSubgroups ||
		    (subgroupFeatures.featureFlags & REQUIRED_SUBGROUP_FEATURE_FLAGS) != REQUIRED_SUBGROUP_FEATURE_FLAGS)
		{
			useFallbackShaders = true;
		}
	}

	waterSimShaders.isWaterSupported = true;

	sortWorkGroupSize = SelectSortWorkGroupSize(!useFallbackShaders);

	eg::Log(
		eg::LogLevel::Info, "water", "Initializing water shaders. UseSubgroups:{0} SortWGSize:{1}",
		useFallbackShaders ? "No" : "Yes", sortWorkGroupSize
	);

	const eg::SpecializationConstantEntry sortSpecConstants[] = { { 0, sortWorkGroupSize } };

	const std::string_view sortVariantName = useFallbackShaders ? "VNoSubgroups" : "VDefault";

	sortPipelineInitial = eg::Pipeline::Create(eg::ComputePipelineCreateInfo{
		.computeShader = {
			.shaderModule = GetWaterSimShader("1_Sort/SortInitial.cs.glsl").GetVariant(sortVariantName),
			.specConstants = sortSpecConstants,
		},
		.requireFullSubgroups = !useFallbackShaders,
		.label = "WaterSimSortInitial",
	});

	sortPipelineNear = eg::Pipeline::Create(eg::ComputePipelineCreateInfo{
		.computeShader = {
			.shaderModule = GetWaterSimShader("1_Sort/SortNear.cs.glsl").GetVariant(sortVariantName),
			.specConstants = sortSpecConstants,
		},
		.requireFullSubgroups = !useFallbackShaders,
		.label = "WaterSimSortNear",
	});

	sortPipelineFar = eg::Pipeline::Create(eg::ComputePipelineCreateInfo{
		.computeShader = {
			.shaderModule = GetWaterSimShader("1_Sort/SortFar.cs.glsl").DefaultVariant(),
			.specConstants = sortSpecConstants,
		},
		.label = "WaterSimSortFar",
	});

	detectCellEdges = eg::Pipeline::Create(eg::ComputePipelineCreateInfo{
		.computeShader = GetWaterSimShader("2_CellPrepare/DetectCellEdges.cs.glsl").ToStageInfo(),
		.label = "DetectCellEdges",
	});

	if (useFallbackShaders || !TryLoadOctGroupPrefixSumSubgroupShaders(*this))
	{
		LoadOctGroupPrefixSumFallbackShaders(*this);
	}

	writeOctGroups = eg::Pipeline::Create(eg::ComputePipelineCreateInfo{
		.computeShader = GetWaterSimShader("2_CellPrepare/WriteOctGroups.cs.glsl").ToStageInfo(),
		.label = "WriteOctGroups",
	});

	std::optional<uint32_t> forEachNearRequiredSubgroupSize;
	std::string_view forEachNearVariantName = "VNoSubgroups";
	bool forEachNearRequireFullSubgroups = false;
	if (!useFallbackShaders)
	{
		constexpr uint32_t FOR_EACH_NEAR_MIN_REQUIRED_SUBGROUP_SIZE = 8;
		const eg::SubgroupFeatures& subgroupFeatures = *eg::GetGraphicsDeviceInfo().subgroupFeatures;
		if (subgroupFeatures.maxSubgroupSize >= FOR_EACH_NEAR_MIN_REQUIRED_SUBGROUP_SIZE)
		{
			bool minSubgroupSizeTooSmall = subgroupFeatures.minSubgroupSize < FOR_EACH_NEAR_MIN_REQUIRED_SUBGROUP_SIZE;
			if (!minSubgroupSizeTooSmall || subgroupFeatures.supportsRequiredSubgroupSize)
			{
				if (minSubgroupSizeTooSmall)
					forEachNearRequiredSubgroupSize = subgroupFeatures.maxSubgroupSize;
				forEachNearRequireFullSubgroups = true;

				if (eg::HasFlag(subgroupFeatures.featureFlags, eg::SubgroupFeatureFlags::Clustered))
					forEachNearVariantName = "VSubgroups";
				else
					forEachNearVariantName = "VSubgroupsNoClustered";
			}
		}
	}

	calculateDensity = eg::Pipeline::Create(eg::ComputePipelineCreateInfo{
		.computeShader = GetWaterSimShader("CalcDensity.cs.glsl").ToStageInfo(forEachNearVariantName),
		.requireFullSubgroups = forEachNearRequireFullSubgroups,
		.requiredSubgroupSize = forEachNearRequiredSubgroupSize,
		.label = "CalcDensity",
	});

	calculateAcceleration = eg::Pipeline::Create(eg::ComputePipelineCreateInfo{
		.computeShader = GetWaterSimShader("CalcAccel.cs.glsl").ToStageInfo(forEachNearVariantName),
		.requireFullSubgroups = forEachNearRequireFullSubgroups,
		.requiredSubgroupSize = forEachNearRequiredSubgroupSize,
		.label = "CalcAccel",
	});

	velocityDiffusion = eg::Pipeline::Create(eg::ComputePipelineCreateInfo{
		.computeShader = GetWaterSimShader("VelocityDiffusion.cs.glsl").ToStageInfo(forEachNearVariantName),
		.requireFullSubgroups = forEachNearRequireFullSubgroups,
		.requiredSubgroupSize = forEachNearRequiredSubgroupSize,
		.label = "VelocityDiffusion",
	});

	moveAndCollision = eg::Pipeline::Create(eg::ComputePipelineCreateInfo{
		.computeShader = GetWaterSimShader("MoveAndCollision.cs.glsl").ToStageInfo(),
		.label = "WaterMoveAndCollision",
	});

	changeGravity = eg::Pipeline::Create(eg::ComputePipelineCreateInfo{
		.computeShader = GetWaterSimShader("ChangeGravity.cs.glsl").ToStageInfo(),
		.label = "WaterChangeGravity",
	});

	clearCellOffsets = eg::Pipeline::Create(eg::ComputePipelineCreateInfo{
		.computeShader = GetWaterSimShader("ClearCellOffsets.cs.glsl").ToStageInfo(),
		.label = "WaterClearCellOffsets",
	});
}

static void OnShutdown()
{
	waterSimShaders = {};
}
EG_ON_SHUTDOWN(OnShutdown)
