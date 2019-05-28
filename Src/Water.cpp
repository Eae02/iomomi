#include "Water.hpp"
#include "World/World.hpp"

Water::Water()
{
	
}

void Water::InitializeWorld(const World& world)
{
	
}

void Water::Step()
{
	for (Region& region : m_regions)
	{
		region.Step<Dir::PosX>();
		region.Step<Dir::NegX>();
		region.Step<Dir::PosY>();
		region.Step<Dir::NegY>();
		region.Step<Dir::PosZ>();
		region.Step<Dir::NegZ>();
	}
}

template <Dir Down>
void Water::Region::Step()
{
	constexpr int DIM_A = (int)Down / 2;
	constexpr int DIM_B = (DIM_A + 1) % 3;
	constexpr int DIM_C = (DIM_A + 2) % 3;
	
	int aBegin = ((int)Down % 2) ? size[DIM_A] - 2 : 1;
	int aEnd = ((int)Down % 2) ? 0 : size[DIM_A] - 1;
	int da = ((int)Down % 2) ? -1 : 1;
	
	size_t layerDensitySize = size[DIM_B] * size[DIM_C] * sizeof(int);
	int* layerDensity = reinterpret_cast<int*>(alloca(layerDensitySize));
	
	auto IdxABC = [&] (int a, int b, int c)
	{
		glm::ivec3 pos;
		pos[DIM_A] = a;
		pos[DIM_B] = b;
		pos[DIM_C] = c;
		return Idx(pos);
	};
	
	auto LayerIdx = [&] (int b, int c)
	{
		return b * size[DIM_C] + c;
	};
	
	static const std::pair<int, int> deltas[] = { { 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 } };
	
	for (int a = aBegin; a != aEnd; a += da)
	{
		for (int b = 1; b < size[DIM_B] - 1; b++)
		{
			for (int c = 1; c < size[DIM_C] - 1; c++)
			{
				int idxHere = IdxABC(a, b, c);
				int waterHere = density[idxHere];
				
				int sumHeight = waterHere;
				int heightDiv = 1;
				
				int idxBelow = IdxABC(a, b, c - da);
				if (mode[idxBelow] == CellMode::Solid || density[idxBelow] == 255)
				{
					for (std::pair<int, int> d : deltas)
					{
						int idxNext = IdxABC(a, b + d.first, c + d.second);
						if (mode[idxNext] == mode[idxHere])
						{
							sumHeight += density[idxNext];
							heightDiv++;
						}
					}
				}
				
				layerDensity[LayerIdx(b, c)] += sumHeight / heightDiv;
			}
		}
		
		for (int b = 1; b < size[DIM_B] - 1; b++)
		{
			for (int c = 1; c < size[DIM_C] - 1; c++)
			{
				int curIdx = IdxABC(a, b, c);
				int idxAbove = IdxABC(a, b, c + da);
				
				int waterHere = layerDensity[LayerIdx(b, c)];
				CellMode modeAbove = mode[idxAbove];
				
				if (mode[curIdx] != CellMode::Solid && (modeAbove == mode[curIdx] || waterHere == 0))
				{
					int transfer = std::min((int)density[idxAbove], 255 - waterHere);
					waterHere += transfer;
					density[idxAbove] -= transfer;
					mode[curIdx] = modeAbove;
				}
				
				layerDensity[LayerIdx(b, c)] = std::clamp(waterHere, 0, 255);
			}
		}
		
		for (int b = 1; b < size[DIM_B] - 1; b++)
		{
			for (int c = 1; c < size[DIM_C] - 1; c++)
			{
				glm::ivec3 pos;
				pos[DIM_A] = a;
				pos[DIM_B] = b;
				pos[DIM_C] = c;
				density[Idx(pos)] = layerDensity[LayerIdx(b, c)];
			}
		}
	}
}
