#pragma once

enum class EntTypeID
{
	EntranceExit  = 0,
	WallLight     = 1,
	Decal         = 2,
	GooPlane      = 3,
	WaterPlane    = 4,
	GravitySwitch = 5
};

void InitEntityTypes();
