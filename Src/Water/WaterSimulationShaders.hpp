#pragma once

struct WaterSimulationShaders
{
	bool isWaterSupported = false;
	bool forceUseFallbackShaders = false;

	uint32_t sortWorkGroupSize;

	eg::Pipeline sortPipelineInitial;
	eg::Pipeline sortPipelineNear;
	eg::Pipeline sortPipelineFar;

	eg::Pipeline detectCellEdges;
	eg::Pipeline octGroupsPrefixSumLevel1;
	eg::Pipeline octGroupsPrefixSumLevel2;
	eg::Pipeline writeOctGroups;

	eg::Pipeline calculateDensity;
	eg::Pipeline calculateAcceleration;
	eg::Pipeline velocityDiffusion;

	eg::Pipeline moveAndCollision;

	eg::Pipeline changeGravity;

	eg::Pipeline clearCellOffsets;

	void Initialize();
};

extern WaterSimulationShaders waterSimShaders;
