#pragma once

struct WaterSimulationShaders
{
	bool isWaterSupported = false;
	
	uint32_t sortWorkGroupSize;

	uint32_t commonWorkGroupSize;

	eg::Pipeline sortPipelineInitial;
	eg::Pipeline sortPipelineNear;
	eg::Pipeline sortPipelineFar;

	eg::Pipeline detectCellEdges;
	eg::Pipeline octGroupsPrefixSum;
	eg::Pipeline writeOctGroups;

	eg::Pipeline calculateDensity;
	eg::Pipeline calculateAcceleration;
	eg::Pipeline velocityDiffusion;

	eg::Pipeline moveAndCollision;

	eg::Pipeline changeGravity;

	void Initialize();
};

extern WaterSimulationShaders waterSimShaders;
