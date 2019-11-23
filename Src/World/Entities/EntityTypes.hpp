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
	Cube                 = 8
};

constexpr size_t NUM_UPDATABLE_ENTITY_TYPES = 3;

extern const EntTypeID entUpdateOrder[NUM_UPDATABLE_ENTITY_TYPES];

void InitEntityTypes();
