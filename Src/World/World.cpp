#include "World.hpp"
#include "Voxel.hpp"
#include "PrepareDrawArgs.hpp"
#include "../Graphics/ObjectRenderer.hpp"
#include "../Graphics/Materials/GravityCornerMaterial.hpp"
#include "../Graphics/RenderSettings.hpp"
#include "../Graphics/WallShader.hpp"

#include <yaml-cpp/yaml.h>

static const char MAGIC[] = { (char)250, 'E', 'G', 'W' };
static const uint32_t CURRENT_VERSION = 1;

World::World()
{
	
}

std::tuple<glm::ivec3, glm::ivec3> World::DecomposeGlobalCoordinate(const glm::ivec3& globalC)
{
	glm::ivec3 localCoord(globalC.x & (REGION_SIZE - 1), globalC.y & (REGION_SIZE - 1), globalC.z & (REGION_SIZE - 1));
	glm::ivec3 regionCoord = (globalC - localCoord) / (int)REGION_SIZE;
	return std::make_tuple(localCoord, regionCoord);
}

bool World::Load(std::istream& stream)
{
	YAML::Node rootYaml = YAML::Load(stream);
	int version = rootYaml["version"].as<int>(10000);
	if (version != CURRENT_VERSION)
	{
		eg::Log(eg::LogLevel::Warning, "wd", "Unsupported world version");
		return false;
	}
	
	std::vector<std::string_view> regParts;
	
	m_regions.clear();
	for (const YAML::Node& regionNode : rootYaml["voxels"])
	{
		const YAML::Node& regNode = regionNode["reg"];
		const YAML::Node& dataNode = regionNode["data"];
		if (!regNode || !dataNode)
			continue;
		
		std::string regStr = regNode.as<std::string>();
		regParts.clear();
		eg::SplitString(regStr, ' ', regParts);
		if (regParts.size() != 3)
			continue;
		
		Region& region = m_regions.emplace_back();
		for (int i = 0; i < 3; i++)
			region.coordinate[i] = atoi(regParts[i].data());
		region.voxelsOutOfDate = true;
		region.gravityCornersOutOfDate = true;
		region.canDraw = false;
		region.data = std::make_unique<RegionData>();
		
		YAML::Binary dataBin = dataNode.as<YAML::Binary>();
		if (!eg::Decompress(dataBin.data(), dataBin.size(), region.data->voxels, sizeof(RegionData::voxels)))
		{
			eg::Log(eg::LogLevel::Error, "wd", "Could not decompress voxels");
			return false;
		}
	}
	
	if (!std::is_sorted(m_regions.begin(), m_regions.end()))
	{
		eg::Log(eg::LogLevel::Error, "wd", "Regions are not sorted");
		return false;
	}
	
	if (const YAML::Node& entitiesNode = rootYaml["entities"])
	{
		for (const YAML::Node& entityNode : entitiesNode)
		{
			std::string typeName = entityNode["type"].as<std::string>("");
			const EntityType* type = FindEntityType(typeName);
			if (type == nullptr)
			{
				eg::Log(eg::LogLevel::Warning, "wd", "Entity type not found: {0}", typeName);
				continue;
			}
			
			std::shared_ptr<Entity> entity = type->CreateInstance();
			entity->Load(entityNode);
			AddEntity(std::move(entity));
		}
	}
	
	m_anyOutOfDate = true;
	
	return true;
}

void World::Save(std::ostream& outStream) const
{
	YAML::Emitter emitter;
	emitter << YAML::BeginMap;
	emitter << YAML::Key << "version" << YAML::Value << CURRENT_VERSION;
	emitter << YAML::Key << "voxels" << YAML::Value << YAML::BeginSeq;
	
	std::vector<std::vector<char>> regionDataVectors;
	
	for (const Region& region : m_regions)
	{
		std::ostringstream coordStream;
		coordStream << region.coordinate.x << " " << region.coordinate.y << " " << region.coordinate.z;
		
		emitter << YAML::BeginMap;
		emitter << YAML::Key << "reg" << YAML::Value << coordStream.str();
		
		std::vector<char> regionData = eg::Compress(region.data->voxels, sizeof(RegionData::voxels));
		
		emitter << YAML::Key << "data" << YAML::Value <<
			YAML::Binary(reinterpret_cast<const uint8_t*>(regionData.data()), regionData.size());
		regionDataVectors.push_back(std::move(regionData));
		
		emitter << YAML::EndMap;
	}
	emitter << YAML::EndSeq;
	
	emitter << YAML::Key << "entities" << YAML::Value << YAML::BeginSeq;
	for (const std::shared_ptr<Entity>& entity : m_entities)
	{
		emitter << YAML::BeginMap;
		emitter << YAML::Key << "type" << YAML::Value << std::string(entity->TypeName());
		entity->Save(emitter);
		emitter << YAML::EndMap;
	}
	emitter << YAML::EndSeq;
	
	outStream << emitter.c_str();
}

static constexpr uint64_t IS_AIR_MASK = (uint64_t)1 << (uint64_t)60;

const World::Region* World::GetRegion(const glm::ivec3& coordinate) const
{
	auto it = std::lower_bound(m_regions.begin(), m_regions.end(), coordinate);
	if (it != m_regions.end() && it->coordinate == coordinate)
		return &*it;
	return nullptr;
}

World::Region* World::GetRegion(const glm::ivec3& coordinate, bool maybeCreate)
{
	auto it = std::lower_bound(m_regions.begin(), m_regions.end(), coordinate);
	if (it != m_regions.end() && it->coordinate == coordinate)
		return &*it;
	
	if (!maybeCreate)
		return nullptr;
	
	Region* region = &*m_regions.emplace(it);
	region->coordinate = coordinate;
	region->canDraw = false;
	region->voxelsOutOfDate = true;
	region->gravityCornersOutOfDate = true;
	region->data = std::make_unique<RegionData>();
	
	m_anyOutOfDate = true;
	
	return region;
}

void World::SetIsAir(const glm::ivec3& pos, bool isAir)
{
	auto [localC, regionC] = DecomposeGlobalCoordinate(pos);
	Region* region = GetRegion(regionC, isAir);
	if (region == nullptr)
		return;
	
	uint64_t& voxelRef = region->data->voxels[localC.x][localC.y][localC.z];
	if (isAir != ((voxelRef & IS_AIR_MASK) != 0))
	{
		if (isAir)
			voxelRef |= IS_AIR_MASK;
		else
			voxelRef &= ~IS_AIR_MASK;
		
		for (int s = 0; s < 6; s++)
		{
			int sd = s / 2;
			glm::ivec3 d = DirectionVector((Dir)s);
			if (localC[sd] + d[sd] < 0 || localC[sd] + d[sd] >= (int)REGION_SIZE)
			{
				Region* nRegion = GetRegion(regionC + d, true);
				nRegion->voxelsOutOfDate = true;
			}
		}
		
		region->voxelsOutOfDate = true;
		m_anyOutOfDate = true;
	}
}

bool World::IsAir(const glm::ivec3& pos) const
{
	auto [localC, regionC] = DecomposeGlobalCoordinate(pos);
	const Region* region = GetRegion(regionC);
	if (region == nullptr)
		return false;
	return (region->data->voxels[localC.x][localC.y][localC.z] & IS_AIR_MASK) != 0;
}

void World::SetTexture(const glm::ivec3& pos, Dir side, uint8_t textureLayer)
{
#ifndef NDEBUG
	if (!IsAir(pos))
		EG_PANIC("Attempted to set texture of a voxel which is not air.");
#endif
	
	auto [localC, regionC] = DecomposeGlobalCoordinate(pos);
	const Region* region = GetRegion(regionC);
	
	const uint64_t shift = (uint64_t)side * 8;
	uint64_t& voxelRef = region->data->voxels[localC.x][localC.y][localC.z];
	voxelRef = (voxelRef & ~(0xFFULL << shift)) | (textureLayer << shift);
}

uint8_t World::GetTexture(const glm::ivec3& pos, Dir side) const
{
	auto [localC, regionC] = DecomposeGlobalCoordinate(pos);
	const Region* region = GetRegion(regionC);
	if (region == nullptr)
		return 0;
	return (region->data->voxels[localC.x][localC.y][localC.z] >> ((uint64_t)side * 8)) & 0xFFULL;
}

glm::mat3 GravityCorner::MakeRotationMatrix() const
{
	glm::vec3 r1 = -DirectionVector(down1);
	glm::vec3 r2 = -DirectionVector(down2);
	return glm::mat3(r1, r2, glm::cross(r1, r2));
}

void World::PrepareForDraw(PrepareDrawArgs& args)
{
	if (m_anyOutOfDate)
	{
		uint32_t totalVertices = 0;
		uint32_t totalIndices = 0;
		uint32_t totalBorderVertices = 0;
		
		bool uploadVoxels = false;
		
		for (Region& region : m_regions)
		{
			if (region.gravityCornersOutOfDate || region.voxelsOutOfDate)
			{
				BuildRegionBorderMesh(region.coordinate, *region.data);
				region.gravityCornersOutOfDate = false;
			}
			
			if (region.voxelsOutOfDate)
			{
				BuildRegionMesh(region.coordinate, *region.data, args.isEditor);
				region.canDraw = !region.data->indices.empty();
				region.voxelsOutOfDate = false;
				uploadVoxels = true;
			}
			
			totalVertices += region.data->vertices.size();
			totalIndices += region.data->indices.size();
			totalBorderVertices += region.data->borderVertices.size();
		}
		
		if (totalIndices == 0)
		{
			m_canDraw = false;
			return;
		}
		m_canDraw = true;
		
		if (uploadVoxels)
		{
			uint64_t indicesOffset = totalVertices * sizeof(WallVertex);
			uint64_t borderVerticesOffset = indicesOffset + totalIndices * sizeof(uint16_t);
			uint64_t uploadBytes = borderVerticesOffset + totalBorderVertices * sizeof(WallBorderVertex);
			
			//Creates an upload buffer and copies data to it
			eg::UploadBuffer uploadBuffer = eg::GetTemporaryUploadBuffer(uploadBytes);
			char* uploadBufferMem = reinterpret_cast<char*>(uploadBuffer.Map());
			auto* verticesOut = reinterpret_cast<WallVertex*>(uploadBufferMem);
			auto* indicesOut = reinterpret_cast<uint16_t*>(uploadBufferMem + indicesOffset);
			auto* borderVerticesOut = reinterpret_cast<WallBorderVertex*>(uploadBufferMem + borderVerticesOffset);
			
			uint32_t firstVertex = 0;
			uint32_t firstIndex = 0;
			for (Region& region : m_regions)
			{
				std::copy_n(region.data->vertices.data(), region.data->vertices.size(), verticesOut + firstVertex);
				std::copy_n(region.data->indices.data(), region.data->indices.size(), indicesOut + firstIndex);
				
				region.data->firstVertex = firstVertex;
				region.data->firstIndex = firstIndex;
				
				firstVertex += region.data->vertices.size();
				firstIndex += region.data->indices.size();
				
				if (args.isEditor)
				{
					std::copy_n(region.data->borderVertices.begin(), region.data->borderVertices.size(), borderVerticesOut);
					borderVerticesOut += region.data->borderVertices.size();
				}
			}
			
			uploadBuffer.Unmap();
			
			//Reallocates the vertex buffer if it is too small
			if (m_voxelVertexBufferCapacity < totalVertices)
			{
				m_voxelVertexBufferCapacity = eg::RoundToNextMultiple(totalVertices, 16 * 1024);
				m_voxelVertexBuffer = eg::Buffer(eg::BufferFlags::VertexBuffer | eg::BufferFlags::CopyDst,
					m_voxelVertexBufferCapacity * sizeof(WallVertex), nullptr);
			}
			
			//Reallocates the index buffer if it is too small
			if (m_voxelIndexBufferCapacity < totalIndices)
			{
				m_voxelIndexBufferCapacity = eg::RoundToNextMultiple(totalIndices, 16 * 1024);
				m_voxelIndexBuffer = eg::Buffer(eg::BufferFlags::IndexBuffer | eg::BufferFlags::CopyDst,
					m_voxelIndexBufferCapacity * sizeof(uint16_t), nullptr);
			}
			
			//Reallocates the border vertex buffer if it is too small
			if (m_borderVertexBufferCapacity < totalBorderVertices)
			{
				m_borderVertexBufferCapacity = eg::RoundToNextMultiple(totalBorderVertices, 16 * 1024);
				m_borderVertexBuffer = eg::Buffer(eg::BufferFlags::VertexBuffer | eg::BufferFlags::CopyDst,
					m_borderVertexBufferCapacity * sizeof(WallBorderVertex), nullptr);
			}
			
			//Uploads data to the vertex and index buffers
			eg::DC.CopyBuffer(uploadBuffer.buffer, m_voxelVertexBuffer, uploadBuffer.offset, 0,
				totalVertices * sizeof(WallVertex));
			eg::DC.CopyBuffer(uploadBuffer.buffer, m_voxelIndexBuffer, uploadBuffer.offset + indicesOffset, 0,
				totalIndices * sizeof(uint16_t));
			
			m_voxelVertexBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
			m_voxelIndexBuffer.UsageHint(eg::BufferUsage::IndexBuffer);
			
			if (args.isEditor && totalBorderVertices > 0)
			{
				eg::DC.CopyBuffer(uploadBuffer.buffer, m_borderVertexBuffer,
					uploadBuffer.offset + borderVerticesOffset, 0,
					totalBorderVertices * sizeof(WallBorderVertex));
				m_borderVertexBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
			}
			m_numBorderVertices = totalBorderVertices;
		}
		
		m_anyOutOfDate = false;
	}
	
	eg::Model& gravityCornerModel = eg::GetAsset<eg::Model>("Models/GravityCornerConvex.obj");
	for (const Region& region : m_regions)
	{
		if (!region.canDraw)
			continue;
		
		for (const GravityCorner& corner : region.data->gravityCorners)
		{
			const float S = 0.25f;
			
			glm::mat4 transform = glm::translate(glm::mat4(1.0f), corner.position) *
				glm::mat4(corner.MakeRotationMatrix()) *
				glm::scale(glm::mat4(1.0f), glm::vec3(S, -S, 1)) *
				glm::translate(glm::mat4(1.0f), glm::vec3(1.01f, -1.01f, 0));
			
			args.objectRenderer->Add(gravityCornerModel, GravityCornerMaterial::instance, transform);
		}
	}
	
	for (int64_t i = (int64_t)m_spotLights.size() - 1; i >= 0; i--)
	{
		if (auto spotLight = m_spotLights[i].lock())
		{
			spotLight->InitDrawData(args.spotLights.emplace_back());
		}
		else
		{
			m_spotLights[i].swap(m_spotLights.back());
			m_spotLights.pop_back();
		}
	}
}

void World::Draw()
{
	if (m_canDraw)
	{
		eg::DC.BindVertexBuffer(0, m_voxelVertexBuffer, 0);
		eg::DC.BindIndexBuffer(eg::IndexType::UInt16, m_voxelIndexBuffer, 0);
		
		DrawWalls([&]
		{
			for (const Region& region : m_regions)
			{
				if (region.canDraw)
				{
					eg::DC.DrawIndexed(region.data->firstIndex, (uint32_t)region.data->indices.size(),
						region.data->firstVertex, 0, 1);
				}
			}
		});
	}
}

void World::DrawEditor()
{
	if (m_canDraw)
	{
		eg::DC.BindVertexBuffer(0, m_voxelVertexBuffer, 0);
		eg::DC.BindIndexBuffer(eg::IndexType::UInt16, m_voxelIndexBuffer, 0);
		
		DrawWallsEditor([&]
		{
			for (const Region& region : m_regions)
			{
				if (region.canDraw)
				{
					eg::DC.DrawIndexed(region.data->firstIndex, (uint32_t)region.data->indices.size(),
						region.data->firstVertex, 0, 1);
				}
			}
		});
		
		if (m_numBorderVertices > 0)
		{
			DrawWallBordersEditor(m_borderVertexBuffer, m_numBorderVertices);
		}
	}
}

void World::BuildRegionMesh(glm::ivec3 coordinate, World::RegionData& region, bool includeNoDraw)
{
	glm::ivec3 globalBase = coordinate * (int)REGION_SIZE;
	region.vertices.clear();
	region.indices.clear();
	
	for (uint32_t x = 0; x < REGION_SIZE; x++)
	{
		for (uint32_t y = 0; y < REGION_SIZE; y++)
		{
			for (uint32_t z = 0; z < REGION_SIZE; z++)
			{
				//Only emit triangles for air voxels
				if ((region.voxels[x][y][z] & IS_AIR_MASK) == 0)
					continue;
				
				glm::ivec3 globalPos = globalBase + glm::ivec3(x, y, z);
				
				for (int s = 0; s < 6; s++)
				{
					const glm::ivec3 nPos = globalPos - voxel::normals[s];
					
					//Only emit triangles for voxels that face into a solid voxel
					if (IsAir(nPos))
						continue;
					const uint8_t textureLayer = (region.voxels[x][y][z] >> (8 * s)) & 0xFFULL;
					if (textureLayer == 0 && !includeNoDraw)
						continue;
					
					//The center of the face to be emitted
					const glm::ivec3 faceCenter2 = globalPos * 2 + 1 - voxel::normals[s];
					
					//Emits indices
					const uint16_t baseIndex = (uint16_t)region.vertices.size();
					static const uint16_t indicesRelative[] = { 0, 1, 2, 2, 1, 3 };
					for (uint16_t i : indicesRelative)
						region.indices.push_back(baseIndex + i);
					
					//Emits vertices
					glm::ivec3 tangent = voxel::tangents[s];
					glm::ivec3 biTangent = voxel::biTangents[s];
					for (int fx = 0; fx < 2; fx++)
					{
						for (int fy = 0; fy < 2; fy++)
						{
							glm::ivec3 tDir = tangent * (fy * 2 - 1);
							glm::ivec3 bDir = biTangent * (fx * 2 - 1);
							glm::ivec3 diagDir = tDir + bDir;
							
							WallVertex& vertex = region.vertices.emplace_back();
							vertex.position = glm::vec3(faceCenter2 + diagDir) * 0.5f;
							vertex.texCoordAO[0] = (uint8_t)(fx * 255);
							vertex.texCoordAO[1] = (uint8_t)((1 - fy) * 255);
							vertex.texCoordAO[2] = textureLayer;
							vertex.texCoordAO[3] = 0;
							vertex.SetNormal(voxel::normals[s]);
							vertex.SetTangent(tangent);
							
							if (!IsAir(globalPos + tDir))
								vertex.texCoordAO[3] |= fy + 1;
							if (!IsAir(globalPos + bDir))
								vertex.texCoordAO[3] |= (fx + 1) << 2;
							if (!IsAir(globalPos + diagDir) && vertex.texCoordAO[3] == 0)
								vertex.texCoordAO[3] = 1 << (fx + 2 * fy);
						}
					}
				}
			}
		}
	}
}

void World::BuildRegionBorderMesh(glm::ivec3 coordinate, World::RegionData& region)
{
	glm::ivec3 globalBase = coordinate * (int)REGION_SIZE;
	region.borderVertices.clear();
	region.gravityCorners.clear();
	
	for (uint32_t x = 0; x < REGION_SIZE; x++)
	{
		for (uint32_t y = 0; y < REGION_SIZE; y++)
		{
			for (uint32_t z = 0; z < REGION_SIZE; z++)
			{
				uint64_t voxel = region.voxels[x][y][z];
				if ((voxel & IS_AIR_MASK) == 0)
					continue;
				glm::ivec3 gPos = globalBase + glm::ivec3(x, y, z);
				glm::vec3 cPos = glm::vec3(gPos) + 0.5f;
				
				for (int dl = 0; dl < 3; dl++)
				{
					glm::ivec3 dlV, uV, vV;
					dlV[dl] = 1;
					uV[(dl + 1) % 3] = 1;
					vV[(dl + 2) % 3] = 1;
					Dir uDir = (Dir)(((dl + 1) % 3) * 2);
					Dir vDir = (Dir)(((dl + 2) % 3) * 2);
					
					for (int u = 0; u < 2; u++)
					{
						glm::ivec3 uSV = uV * (u * 2 - 1);
						for (int v = 0; v < 2; v++)
						{
							glm::ivec3 vSV = vV * (v * 2 - 1);
							bool uAir = IsAir(gPos + uSV);
							bool vAir = IsAir(gPos + vSV);
							bool diagAir = IsAir(gPos + uSV + vSV);
							if (diagAir || uAir != vAir)
								continue;
							
							WallBorderVertex& v1 = region.borderVertices.emplace_back();
							v1.position = glm::vec4(cPos + 0.5f * glm::vec3(uSV + vSV + dlV), 0.0f);
							VecEncode(-uSV, v1.normal1);
							VecEncode(-vSV, v1.normal2);
							
							WallBorderVertex& v2 = region.borderVertices.emplace_back(v1);
							v2.position -= glm::vec4(dlV, -1);
							
							int gBit = (48 + dl * 4 + (1 - v) * 2 + (1 - u));
							if ((voxel >> gBit) & 1)
							{
								GravityCorner& corner = region.gravityCorners.emplace_back();
								corner.position = cPos + 0.5f * glm::vec3(uSV + vSV - dlV);
								corner.down1 = u ? uDir : OppositeDir(uDir);
								corner.down2 = v ? vDir : OppositeDir(vDir);
								if (u != v)
									corner.position += dlV;
							}
						}
					}
				}
			}
		}
	}
}

void World::CalcClipping(ClippingArgs& args) const
{
	glm::vec3 endMin = args.aabbMin + args.move;
	glm::vec3 endMax = args.aabbMax + args.move;
	
	constexpr float EP = 0.0001f;
	
	//Clipping against voxels
	for (int axis = 0; axis < 3; axis++)
	{
		int vOffset;
		float beginA, endA;
		if (args.move[axis] < 0)
		{
			beginA = args.aabbMin[axis];
			endA = endMin[axis];
			vOffset = -1;
		}
		else
		{
			beginA = args.aabbMax[axis];
			endA = endMax[axis];
			vOffset = 0;
		}
		
		int minI = (int)std::floor(std::min(beginA, endA) - EP);
		int maxI = (int)std::ceil(std::max(beginA, endA) + EP);
		
		int axisB = (axis + 1) % 3;
		int axisC = (axis + 2) % 3;
		
		for (int v = minI; v <= maxI; v++)
		{
			float t = (v - beginA) / args.move[axis];
			if (t > args.clipDist || t < 0)
				continue;
			
			int minB = (int)std::floor(glm::mix(args.aabbMin[axisB], endMin[axisB], t));
			int maxB = (int)std::ceil(glm::mix(args.aabbMax[axisB], endMax[axisB], t));
			int minC = (int)std::floor(glm::mix(args.aabbMin[axisC], endMin[axisC], t));
			int maxC = (int)std::ceil(glm::mix(args.aabbMax[axisC], endMax[axisC], t));
			for (int vb = minB; vb < maxB; vb++)
			{
				for (int vc = minC; vc < maxC; vc++)
				{
					glm::ivec3 coord;
					coord[axis] = v + vOffset;
					coord[axisB] = vb;
					coord[axisC] = vc;
					if (!IsAir(coord))
					{
						args.colPlaneNormal = glm::vec3(0);
						args.colPlaneNormal[axis] = args.move[axis] < 0 ? 1.0f : -1.0f;
						
						args.clipDist = t;
						break;
					}
				}
			}
		}
	}
}

glm::ivec4 World::GetGravityCornerVoxelPos(glm::ivec3 cornerPos, Dir cornerDir) const
{
	const int cornerDim = (int)cornerDir / 2;
	if ((int)cornerDir % 2)
		cornerPos[cornerDim]--;
	
	const Dir uDir = (Dir)((cornerDim + 1) % 3 * 2);
	const Dir vDir = (Dir)((cornerDim + 2) % 3 * 2);
	const glm::ivec3 uV = DirectionVector(uDir);
	const glm::ivec3 vV = DirectionVector(vDir);
	
	int numAir = 0;
	glm::ivec2 airPos, notAirPos;
	for (int u = -1; u <= 0; u++)
	{
		for (int v = -1; v <= 0; v++)
		{
			glm::ivec3 pos = cornerPos + uV * u + vV * v;
			if (IsAir(pos))
			{
				numAir++;
				airPos = { u, v };
			}
			else
			{
				notAirPos = { u, v };
			}
		}
	}
	
	if (numAir == 3)
	{
		airPos.x = -1 - notAirPos.x;
		airPos.y = -1 - notAirPos.y;
	}
	else if (numAir != 1)
		return { 0, 0, 0, -1 };
	
	const int bit = cornerDim * 4 + (airPos.y + 1) * 2 + airPos.x + 1;
	return glm::ivec4(cornerPos + uV * airPos.x + vV * airPos.y, bit);
}

bool World::IsGravityCorner(const glm::ivec3& cornerPos, Dir cornerDir) const
{
	glm::ivec4 voxelPos = GetGravityCornerVoxelPos(cornerPos, cornerDir);
	if (voxelPos.w == -1)
		return false;
	
	auto [localC, regionC] = DecomposeGlobalCoordinate(glm::ivec3(voxelPos));
	const Region* region = GetRegion(regionC);
	if (region == nullptr)
		return false;
	
	return (bool)((region->data->voxels[localC.x][localC.y][localC.z] >> (48ULL + voxelPos.w)) & 1);
}

void World::SetIsGravityCorner(const glm::ivec3& cornerPos, Dir cornerDir, bool value)
{
	glm::ivec4 voxelPos = GetGravityCornerVoxelPos(cornerPos, cornerDir);
	if (voxelPos.w == -1)
		return;
	
	auto [localC, regionC] = DecomposeGlobalCoordinate(glm::ivec3(voxelPos));
	Region* region = GetRegion(regionC, true);
	if (region == nullptr)
		return;
	
	uint64_t& voxelRef = region->data->voxels[localC.x][localC.y][localC.z];
	uint64_t mask = 1ULL << (48ULL + voxelPos.w);
	voxelRef = value ? (voxelRef | mask) : (voxelRef & ~mask);
	
	m_anyOutOfDate = true;
	region->gravityCornersOutOfDate = true;
}

bool World::IsCorner(const glm::ivec3& cornerPos, Dir cornerDir) const
{
	glm::ivec4 voxelPos = GetGravityCornerVoxelPos(cornerPos, cornerDir);
	return voxelPos.w != -1;
}

const GravityCorner* World::FindGravityCorner(const ClippingArgs& args, Dir currentDown) const
{
	glm::vec3 pos = (args.aabbMin + args.aabbMax) / 2.0f;
	
	float minDist = INFINITY;
	const GravityCorner* ret = nullptr;
	
	constexpr float ACTIVATE_DIST = 0.8f;
	constexpr float MAX_HEIGHT_DIFF = 0.1f;
	
	glm::vec3 currentDownDir = DirectionVector(currentDown);
	
	auto CheckCorner = [&] (const GravityCorner& corner)
	{
		glm::vec3 otherDown;
		if (corner.down1 == currentDown)
			otherDown = DirectionVector(corner.down2);
		else if (corner.down2 == currentDown)
			otherDown = DirectionVector(corner.down1);
		else
			return;
		
		//Checks that the player is moving towards the corner
		if (glm::dot(args.move, otherDown) < 0.0001f)
			return;
		
		//Checks that the player is positioned correctly along the side of the corner
		glm::vec3 cornerVec = glm::cross(glm::vec3(DirectionVector(corner.down1)), glm::vec3(DirectionVector(corner.down2)));
		float t = glm::dot(cornerVec, pos - corner.position);
		
		if (t < 0.0f || t > 1.0f)
			return;
		
		//Checks that the player is positioned at the same height as the corner
		float h1 = glm::dot(currentDownDir, corner.position - args.aabbMin);
		float h2 = glm::dot(currentDownDir, corner.position - args.aabbMax);
		if (std::abs(h1) > MAX_HEIGHT_DIFF && std::abs(h2) > MAX_HEIGHT_DIFF)
			return;
		
		//Checks that the player is within range of the corner
		float dist = glm::dot(otherDown, corner.position - pos);
		if (dist > -0.1f && dist < ACTIVATE_DIST)
		{
			float absDist = abs(dist);
			if (absDist < minDist)
			{
				ret = &corner;
				minDist = absDist;
			}
		}
	};
	
	glm::ivec3 gMin = glm::ivec3(glm::floor(glm::min(args.aabbMin, args.aabbMin + args.move))) - 2;
	glm::ivec3 gMax = glm::ivec3(glm::ceil(glm::max(args.aabbMax, args.aabbMax + args.move))) + 1;
	
	glm::ivec3 rMin = std::get<1>(DecomposeGlobalCoordinate(gMin));
	glm::ivec3 rMax = std::get<1>(DecomposeGlobalCoordinate(gMax));
	
	for (int rx = rMin.x; rx <= rMax.x; rx++)
	{
		for (int ry = rMin.y; ry <= rMax.y; ry++)
		{
			for (int rz = rMin.z; rz <= rMax.z; rz++)
			{
				if (const Region* region = GetRegion({rx, ry, rz}))
				{
					for (const GravityCorner& corner : region->data->gravityCorners)
					{
						CheckCorner(corner);
					}
				}
			}
		}
	}
	
	return ret;
}

PickWallResult World::PickWall(const eg::Ray& ray) const
{
	float minDist = INFINITY;
	PickWallResult result;
	result.intersected = false;
	
	for (int dim = 0; dim < 3; dim++)
	{
		glm::vec3 dn = DirectionVector((Dir)(dim * 2));
		for (int s = -100; s <= 100; s++)
		{
			if (std::signbit(s - ray.GetStart()[dim]) != std::signbit(ray.GetDirection()[dim]))
				continue;
			
			eg::Plane plane(dn, s);
			float intersectDist;
			if (ray.Intersects(plane, intersectDist) && intersectDist < minDist)
			{
				glm::vec3 iPos = ray.GetPoint(intersectDist);
				glm::vec3 posDel = ray.GetDirection() * dn * 0.1f;
				glm::ivec3 voxelPosN = glm::floor(iPos + posDel);
				glm::ivec3 voxelPosP = glm::floor(iPos - posDel);
				if (IsAir(voxelPosP) && !IsAir(voxelPosN))
				{
					result.intersectPosition = iPos;
					result.voxelPosition = voxelPosN;
					result.normalDir = (Dir)(dim * 2 + (voxelPosP[dim] > voxelPosN[dim] ? 0 : 1));
					result.intersected = true;
					minDist = intersectDist;
				}
			}
		}
	}
	
	return result;
}

void World::AddEntity(std::shared_ptr<Entity> entity)
{
	if (auto spotLight = std::dynamic_pointer_cast<SpotLightEntity>(entity))
		m_spotLights.emplace_back(spotLight);
	m_entities.push_back(std::move(entity));
}
