#pragma once

enum class EntTypeID
{
	EntranceExit         = 0,
	WallLight            = 1,
	Decal                = 2,
	GooPlane             = 3,
	WaterPlane           = 4,
	GravitySwitch        = 5,
	FloorButton          = 6,
	ActivationLightStrip = 7,
	Cube                 = 8,
	CubeSpawner          = 9,
	Platform             = 10,
	ForceField           = 11,
	GravityBarrier       = 12,
	Ramp                 = 13,
	Window               = 14,
	Mesh                 = 15,
	WaterWall            = 16,
	SlidingWall          = 17,
	MAX
};
 
constexpr size_t NUM_UPDATABLE_ENTITY_TYPES = 9;

extern const EntTypeID entUpdateOrder[NUM_UPDATABLE_ENTITY_TYPES];

void InitEntityTypes();
