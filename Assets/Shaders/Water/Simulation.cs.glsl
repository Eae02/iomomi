#version 450 core

layout(constant_id=0) const int LSIZE = 32;

layout(local_size_x=LSIZE, local_size_y=LSIZE, local_size_z=LSIZE) in;

layout(binding=0, format=u8) readonly uimage3D inMode;
layout(binding=1, format=r32ui) readonly image3D inDensity;

layout(binding=2, format=u8) writeonly uimage3D outMode;
layout(binding=3, format=r32ui) writeonly image3D outDensity;

layout(push_constant) uniform PC
{
	ivec3 regSize;
};

const uint MODE_SOLID = 0;
const uint MODE_FALL_PX = 1;
const uint MODE_FALL_NX = 2;
const uint MODE_FALL_PY = 3;
const uint MODE_FALL_NY = 4;
const uint MODE_FALL_PZ = 5;
const uint MODE_FALL_NZ = 6;

const ivec3 DIRECTIONS[] = ivec3[] (
	ivec3(1, 0, 0), ivec3(-1, 0, 0),
	ivec3(0, 1, 0), ivec3(0, -1, 0),
	ivec3(0, 0, 1), ivec3(0, 0, -1)
);

const uint UINT_MAX = 0xFFFFFFFFU;

void main()
{
	uint modeHere = imageLoad(inMode, gl_GlobalInvocationID).r;
	if (modeHere == MODE_SOLID)
	{
		imageStore(outMode, gl_GlobalInvocationID, uvec4(MODE_SOLID));
		return;
	}
	
	uint waterHere = imageLoad(inDensity, gl_GlobalInvocationID).r;
	
	if (waterHere == 0)
	{
		for (int down = 0; down < 6; down++)
		{
			uint water = imageLoad(inDensity, gl_GlobalInvocationID - DIRECTIONS[down]).r;
			if (water > 0)
			{
				imageStore(outDensity, gl_GlobalInvocationID, uvec4(water));
				imageStore(outMode, gl_GlobalInvocationID, uvec4(2 + down));
				return;
			}
		}
		imageStore(outMode, gl_GlobalInvocationID, uvec4(MODE_EMPTY));
		return;
	}
	
	uint down = modeHere - 2;
	
	uint waterBelow = imageLoad(inDensity, gl_GlobalInvocationID + DIRECTIONS[down]).r;
	uint waterAbove = imageLoad(inDensity, gl_GlobalInvocationID - DIRECTIONS[down]).r;
	
	uint modeBelow = imageLoad(inMode, gl_GlobalInvocationID + DIRECTIONS[down]).r;
	uint modeAbove = imageLoad(inMode, gl_GlobalInvocationID - DIRECTIONS[down]).r;
	
	//Removes water from this cell that will be moved to the cell below
	uint initWaterHere = waterHere;
	if (modeBelow != MODE_SOLID && (waterBelow == 0 || modeBelow == modeHere))
	{
		waterHere -= min(UINT_MAX - waterBelow, waterHere);
	}
	
	//Adds water to this cell from the one above
	uint transferI = initWaterHere + min(UINT_MAX - initWaterHere, waterAbove);
	uint transfer = waterHere + min(UINT_MAX - waterHere, waterAbove);
	
	
}
