#include "World.hpp"
#include "PrepareDrawArgs.hpp"
#include "Entities/ECRigidBody.hpp"
#include "Entities/WallLight.hpp"
#include "Entities/GravitySwitch.hpp"
#include "EntityTypes.hpp"
#include "Entities/ECActivator.hpp"
#include "Entities/Cube.hpp"
#include "Entities/ECActivationLightStrip.hpp"
#include "Entities/Entrance.hpp"
#include "Entities/GravityBarrier.hpp"
#include "Entities/CubeSpawner.hpp"
#include "Entities/ForceField.hpp"
#include "../Graphics/Materials/GravityCornerLightMaterial.hpp"
#include "../Graphics/Materials/StaticPropMaterial.hpp"
#include "../Graphics/RenderSettings.hpp"
#include "../Graphics/WallShader.hpp"

#include <yaml-cpp/yaml.h>

std::tuple<glm::ivec3, glm::ivec3> World::DecomposeGlobalCoordinate(const glm::ivec3& globalC)
{
	glm::ivec3 localCoord(globalC.x & (REGION_SIZE - 1), globalC.y & (REGION_SIZE - 1), globalC.z & (REGION_SIZE - 1));
	glm::ivec3 regionCoord = (globalC - localCoord) / (int)REGION_SIZE;
	return std::make_tuple(localCoord, regionCoord);
}

static constexpr uint64_t IS_AIR_MASK = (uint64_t)1 << (uint64_t)60;

static char MAGIC[] = { (char)0xFF, 'G', 'W', 'D' };

std::unique_ptr<World> World::Load(std::istream& stream, bool isEditor)
{
	char magicBuf[sizeof(MAGIC)];
	stream.read(magicBuf, sizeof(magicBuf));
	if (std::memcmp(magicBuf, MAGIC, sizeof(MAGIC)))
	{
		eg::Log(eg::LogLevel::Error, "wd", "Invalid world file");
		return nullptr;
	}
	
	std::unique_ptr<World> world(new World(nullptr));
	
	const uint32_t version = eg::BinRead<uint32_t>(stream);
	if (version != 1 && version != 2)
	{
		eg::Log(eg::LogLevel::Error, "wd", "Unsupported world format");
		return nullptr;
	}
	
	world->m_boundsMin = glm::ivec3(INT_MAX);
	world->m_boundsMax = glm::ivec3(INT_MIN);
	const uint32_t numRegions = eg::BinRead<uint32_t>(stream);
	for (uint32_t i = 0; i < numRegions; i++)
	{
		Region& region = world->m_regions.emplace_back();
		for (int c = 0; c < 3; c++)
		{
			region.coordinate[c] = eg::BinRead<int32_t>(stream);
		}
		region.voxelsOutOfDate = true;
		region.gravityCornersOutOfDate = true;
		region.canDraw = false;
		region.data = std::make_unique<RegionData>();
		
		if (!eg::ReadCompressedSection(stream, region.data->voxels, sizeof(RegionData::voxels)))
		{
			eg::Log(eg::LogLevel::Error, "wd", "Could not decompress voxels");
			return nullptr;
		}
		
		glm::ivec3 regionMin = region.coordinate * (int)REGION_SIZE;
		for (uint32_t x = 0; x < REGION_SIZE; x++)
		{
			for (uint32_t y = 0; y < REGION_SIZE; y++)
			{
				for (uint32_t z = 0; z < REGION_SIZE; z++)
				{
					if (region.data->voxels[x][y][z] & IS_AIR_MASK)
					{
						world->m_boundsMin = glm::min(world->m_boundsMin, regionMin + glm::ivec3(x, y, z));
						world->m_boundsMax = glm::max(world->m_boundsMax, regionMin + glm::ivec3(x, y, z));
					}
				}
			}
		}
	}
	world->m_boundsMin -= 1;
	world->m_boundsMax += 2;
	
	if (!std::is_sorted(world->m_regions.begin(), world->m_regions.end()))
	{
		eg::Log(eg::LogLevel::Error, "wd", "Regions are not sorted");
		return nullptr;
	}
	
	if (version == 2)
	{
		world->playerHasGravityGun = eg::BinRead<uint8_t>(stream);
	}
	
	world->m_entityManager.reset(eg::EntityManager::Deserialize(stream, entitySerializers));
	
	ECActivator::Initialize(*world->m_entityManager);
	
	if (!isEditor)
	{
		for (const eg::Entity& entranceEntity : world->EntityManager().GetEntitySet(ECEntrance::EntitySignature))
		{
			world->m_doors.push_back(ECEntrance::GetDoorDescription(entranceEntity));
		}
	}
	
	world->m_anyOutOfDate = true;
	
	return world;
}

void World::Save(std::ostream& outStream) const
{
	static const uint32_t CURRENT_VERSION = 2;
	
	outStream.write(MAGIC, sizeof(MAGIC));
	eg::BinWrite(outStream, CURRENT_VERSION);
	
	eg::BinWrite(outStream, (uint32_t)m_regions.size());
	for (const Region& region : m_regions)
	{
		for (int c = 0; c < 3; c++)
		{
			eg::BinWrite(outStream, (int32_t)region.coordinate[c]);
		}
		
		eg::WriteCompressedSection(outStream, region.data->voxels, sizeof(RegionData::voxels));
	}
	
	eg::BinWrite<uint8_t>(outStream, playerHasGravityGun);
	
	m_entityManager->Serialize(outStream);
}

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
		
		region->voxelsOutOfDate = true;
		region = nullptr; //region is not valid after this, since GetRegion below can invalidate the pointer
		
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

void World::SetMaterial(const glm::ivec3& pos, Dir side, WallSideMaterial material)
{
#ifndef NDEBUG
	if (!IsAir(pos))
		EG_PANIC("Attempted to set texture of a voxel which is not air.");
#endif
	
	auto [localC, regionC] = DecomposeGlobalCoordinate(pos);
	Region* region = GetRegion(regionC, true);
	
	uint64_t state = material.texture & 127;
	if (material.enableReflections)
		state |= 128;
	
	const uint64_t shift = (uint64_t)side * 8;
	uint64_t& voxelRef = region->data->voxels[localC.x][localC.y][localC.z];
	voxelRef = (voxelRef & ~(0xFFULL << shift)) | (state << shift);
	m_anyOutOfDate = true;
	region->voxelsOutOfDate = true;
}

WallSideMaterial World::GetMaterial(const glm::ivec3& pos, Dir side) const
{
	auto [localC, regionC] = DecomposeGlobalCoordinate(pos);
	const Region* region = GetRegion(regionC);
	if (region == nullptr)
		return { };
	uint8_t state = (region->data->voxels[localC.x][localC.y][localC.z] >> ((uint64_t)side * 8)) & 0xFFULL;
	return { (int)(state & 127), (state & 128) != 0 };
}

glm::mat3 GravityCorner::MakeRotationMatrix() const
{
	const glm::vec3 r1 = -DirectionVector(down1);
	const glm::vec3 r2 = -DirectionVector(down2);
	return glm::mat3(r1, r2, glm::cross(r1, r2));
}

void World::Update(const WorldUpdateArgs& args)
{
	if (m_bulletWorld)
	{
		auto physicsCPUTimer = eg::StartCPUTimer("Physics");
		
		ECGravityBarrier::Update(args);
		
		ECForceField::Update(args.dt, *m_entityManager);
		
		CubeSpawner::Update(args);
		
		Cube::UpdatePreSim(args);
		
		m_bulletWorld->stepSimulation(args.dt, 10, 1.0f / 120.0f);
		
		Cube::UpdatePostSim(args);
	}
	
	ECActivationLightStrip::Update(*m_entityManager, args.dt);
	
	ECActivator::Update(args);
	
	m_entityManager->EndFrame();
}

void World::PrepareForDraw(PrepareDrawArgs& args)
{
	PrepareRegionMeshes(args.isEditor);
	
	eg::Model& gravityCornerModel = eg::GetAsset<eg::Model>("Models/GravityCornerConvex.obj");
	const eg::IMaterial& gravityCornerMat = eg::GetAsset<StaticPropMaterial>("Materials/GravityCorner.yaml");
	
	int endMeshIndex = -1;
	int midMeshIndex = -1;
	for (int i = 0; i < (int)gravityCornerModel.NumMeshes(); i++)
	{
		if (gravityCornerModel.GetMesh(i).name == "End")
			endMeshIndex = i;
		else if (gravityCornerModel.GetMesh(i).name == "Mid")
			midMeshIndex = i;
	}
	
	for (const Region& region : m_regions)
	{
		if (!region.canDraw)
			continue;
		
		for (const GravityCorner& corner : region.data->gravityCorners)
		{
			for (int s = 0; s < 2; s++)
			{
				glm::mat4 transform = glm::translate(glm::mat4(1.0f), corner.position) *
				                      glm::mat4(corner.MakeRotationMatrix()) *
				                      glm::translate(glm::mat4(1.0f), glm::vec3(0.01f, 0.01f, 0));
				
				if (s)
				{
					transform = transform *
					            glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 1)) *
					            glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, -1));
				}
				
				glm::mat4 transformLight = transform * glm::translate(glm::mat4(1.0f), glm::vec3(0.01f, 0.01f, 0));
				
				int meshIndex = corner.isEnd[s] ? endMeshIndex : midMeshIndex;
				args.meshBatch->AddModelMesh(gravityCornerModel, meshIndex, GravityCornerLightMaterial::instance, transformLight);
				args.meshBatch->AddModelMesh(gravityCornerModel, meshIndex, gravityCornerMat, transform);
			}
		}
	}
	
	if (!args.isEditor)
	{
		for (ReflectionPlane& plane : m_wallReflectionPlanes)
		{
			args.reflectionPlanes.push_back(&plane);
		}
		
		ECGravityBarrier::PrepareForDraw(*args.player, *m_entityManager);
		
		DrawMessage drawMessage;
		drawMessage.world = this;
		drawMessage.meshBatch = args.meshBatch;
		drawMessage.transparentMeshBatch = args.transparentMeshBatch;
		drawMessage.reflectionPlanes = &args.reflectionPlanes;
		m_entityManager->SendMessageToAll(drawMessage);
	}
	
	static eg::EntitySignature pointLightSignature = eg::EntitySignature::Create<PointLight>();
	for (const eg::Entity& entity : m_entityManager->GetEntitySet(pointLightSignature))
	{
		args.pointLights.push_back(entity.GetComponent<PointLight>().GetDrawData(eg::GetEntityPosition(entity)));
	}
}

void World::Draw()
{
	if (!m_canDraw)
		return;
	
	BindWallShaderGame();
	
	eg::DC.BindVertexBuffer(0, m_voxelVertexBuffer, 0);
	eg::DC.BindIndexBuffer(eg::IndexType::UInt16, m_voxelIndexBuffer, 0);
	
	int reflectiveCur = -1;
	auto UpdateReflective = [&] (bool reflective)
	{
		if (reflectiveCur != (int)reflective)
		{
			reflectiveCur = (int)reflective;
			eg::DC.SetStencilValue(eg::StencilValue::Reference, reflectiveCur);
		}
	};
	
	for (const Region& region : m_regions)
	{
		if (region.canDraw)
		{
			if (!region.data->indices.empty())
			{
				UpdateReflective(false);
				eg::DC.DrawIndexed(region.data->firstIndex, (uint32_t)region.data->indices.size(),
				                   region.data->firstVertex, 0, 1);
			}
			
			if (!region.data->indicesReflective.empty())
			{
				UpdateReflective(true);
				eg::DC.DrawIndexed(region.data->firstIndex + region.data->indices.size(),
				                   (uint32_t)region.data->indicesReflective.size(), region.data->firstVertex, 0, 1);
			}
		}
	}
}

void World::DrawPointLightShadows(const struct PointLightShadowRenderArgs& renderArgs)
{
	if (!m_canDraw)
		return;
	
	BindWallShaderPointLightShadow(renderArgs);
	
	eg::DC.BindVertexBuffer(0, m_voxelVertexBuffer, 0);
	eg::DC.BindIndexBuffer(eg::IndexType::UInt16, m_voxelIndexBuffer, 0);
	
	for (const Region& region : m_regions)
	{
		if (region.canDraw)
		{
			eg::DC.DrawIndexed(region.data->firstIndex,
				(uint32_t)(region.data->indices.size() + region.data->indicesReflective.size()),
				region.data->firstVertex, 0, 1);
		}
	}
}

void World::DrawPlanarReflections(const eg::Plane& plane)
{
	if (!m_canDraw)
		return;
	
	BindWallShaderPlanarReflections(plane);
	
	eg::DC.BindVertexBuffer(0, m_voxelVertexBuffer, 0);
	eg::DC.BindIndexBuffer(eg::IndexType::UInt16, m_voxelIndexBuffer, 0);
	
	for (const Region& region : m_regions)
	{
		if (region.canDraw)
		{
			eg::DC.DrawIndexed(region.data->firstIndex,
				(uint32_t)(region.data->indices.size() + region.data->indicesReflective.size()),
				region.data->firstVertex, 0, 1);
		}
	}
}

void World::DrawEditor()
{
	if (!m_canDraw)
		return;
	
	BindWallShaderEditor();
	
	eg::DC.BindVertexBuffer(0, m_voxelVertexBuffer, 0);
	eg::DC.BindIndexBuffer(eg::IndexType::UInt16, m_voxelIndexBuffer, 0);
	
	int reflectiveCur = -1;
	auto UpdateReflective = [&] (bool reflective)
	{
		if (reflectiveCur != (int)reflective)
		{
			reflectiveCur = (int)reflective;
			eg::DC.PushConstants(0, 4, &reflectiveCur);
		}
	};
	
	for (const Region& region : m_regions)
	{
		if (region.canDraw)
		{
			if (!region.data->indices.empty())
			{
				UpdateReflective(false);
				eg::DC.DrawIndexed(region.data->firstIndex, (uint32_t)region.data->indices.size(),
				                   region.data->firstVertex, 0, 1);
			}
			
			if (!region.data->indicesReflective.empty())
			{
				UpdateReflective(true);
				eg::DC.DrawIndexed(region.data->firstIndex + region.data->indices.size(),
				                   (uint32_t)region.data->indicesReflective.size(), region.data->firstVertex, 0, 1);
			}
		}
	}
	
	if (m_numBorderVertices > 0)
	{
		DrawWallBordersEditor(m_borderVertexBuffer, m_numBorderVertices);
	}
}

void World::PrepareRegionMeshes(bool isEditor)
{
	if (m_anyOutOfDate)
	{
		uint32_t totalVertices = 0;
		uint32_t totalIndices = 0;
		uint32_t totalBorderVertices = 0;
		
		bool uploadVoxels = false;
		
		m_wallReflectionPlanes.clear();
		std::vector<VoxelReflectionPlane> seenReflectionPlanes;
		
		for (Region& region : m_regions)
		{
			if (region.gravityCornersOutOfDate || region.voxelsOutOfDate)
			{
				BuildRegionBorderMesh(region.coordinate, *region.data);
				region.gravityCornersOutOfDate = false;
			}
			
			if (region.voxelsOutOfDate)
			{
				BuildRegionMesh(region.coordinate, *region.data, isEditor);
				region.canDraw = !region.data->indices.empty() || !region.data->indicesReflective.empty();
				region.voxelsOutOfDate = false;
				uploadVoxels = true;
			}
			
			if (!isEditor)
			{
				for (const VoxelReflectionPlane& reflectionPlane : region.data->reflectionPlanes)
				{
					if (!eg::Contains(seenReflectionPlanes, reflectionPlane))
					{
						ReflectionPlane& actualPlane = m_wallReflectionPlanes.emplace_back();
						actualPlane.plane = eg::Plane(DirectionVector(reflectionPlane.normalDir),
							reflectionPlane.distance);
						
						seenReflectionPlanes.push_back(reflectionPlane);
					}
				}
			}
			
			totalVertices += region.data->vertices.size();
			totalIndices += region.data->indices.size() + region.data->indicesReflective.size();
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
			const uint64_t indicesOffset = totalVertices * sizeof(WallVertex);
			const uint64_t borderVerticesOffset = indicesOffset + totalIndices * sizeof(uint16_t);
			const uint64_t uploadBytes = borderVerticesOffset + totalBorderVertices * sizeof(WallBorderVertex);
			
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
				std::copy_n(region.data->indicesReflective.data(), region.data->indicesReflective.size(),
					indicesOut + firstIndex + region.data->indices.size());
				
				region.data->firstVertex = firstVertex;
				region.data->firstIndex = firstIndex;
				
				firstVertex += region.data->vertices.size();
				firstIndex += region.data->indices.size() + region.data->indicesReflective.size();
				
				if (isEditor)
				{
					std::copy_n(region.data->borderVertices.begin(), region.data->borderVertices.size(), borderVerticesOut);
					borderVerticesOut += region.data->borderVertices.size();
				}
			}
			
			uploadBuffer.Flush();
			
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
			
			if (isEditor && totalBorderVertices > 0)
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
}

void World::BuildRegionMesh(glm::ivec3 coordinate, World::RegionData& region, bool includeNoDraw)
{
	const glm::ivec3 globalBase = coordinate * (int)REGION_SIZE;
	region.vertices.clear();
	region.indices.clear();
	region.indicesReflective.clear();
	region.reflectionPlanes.clear();
	
	std::vector<glm::vec3> collisionVertices;
	std::vector<uint32_t> collisionIndices;
	
	for (uint32_t x = 0; x < REGION_SIZE; x++)
	{
		for (uint32_t y = 0; y < REGION_SIZE; y++)
		{
			for (uint32_t z = 0; z < REGION_SIZE; z++)
			{
				//Only emit triangles for air voxels
				if ((region.voxels[x][y][z] & IS_AIR_MASK) == 0)
					continue;
				
				const glm::ivec3 globalPos = globalBase + glm::ivec3(x, y, z);
				
				for (int s = 0; s < 6; s++)
				{
					glm::ivec3 normal = DirectionVector((Dir)s);
					const glm::ivec3 nPos = globalPos - normal;
					
					//Only emit triangles for voxels that face into a solid voxel
					if (IsAir(nPos))
						continue;
					const uint8_t material = (region.voxels[x][y][z] >> (uint64_t)(8 * s)) & 0xFFULL;
					int texture = material & 127;
					if (texture == 0 && !includeNoDraw)
						continue;
					
					bool reflective = (material & 128) != 0;
					std::vector<uint16_t>* indices = reflective ? &region.indicesReflective : &region.indices;
					
					//The center of the face to be emitted
					const glm::ivec3 faceCenter2 = globalPos * 2 + 1 - normal;
					
					if (reflective)
					{
						VoxelReflectionPlane plane;
						plane.normalDir = (Dir)s;
						plane.distance = (faceCenter2[s / 2] / 2) * (1 - (s % 2) * 2);
						if (!eg::Contains(region.reflectionPlanes, plane))
							region.reflectionPlanes.push_back(plane);
					}
					
					//Emits indices
					const uint16_t baseIndex = (uint16_t)region.vertices.size();
					static const uint16_t indicesRelative[] = { 0, 1, 2, 2, 1, 3 };
					for (uint16_t i : indicesRelative)
						indices->push_back(baseIndex + i);
					
					uint32_t baseIndexCol = collisionVertices.size();
					bool hasCollision = true;
					
					//Emits vertices
					const glm::ivec3 tangent = voxel::tangents[s];
					const glm::ivec3 biTangent = voxel::biTangents[s];
					for (int fx = 0; fx < 2; fx++)
					{
						for (int fy = 0; fy < 2; fy++)
						{
							const glm::ivec3 tDir = tangent * (fy * 2 - 1);
							const glm::ivec3 bDir = biTangent * (fx * 2 - 1);
							const glm::ivec3 diagDir = tDir + bDir;
							const glm::vec3 pos = glm::vec3(faceCenter2 + diagDir) * 0.5f;
							
							float doorDist = INFINITY;
							for (const Door& door : m_doors)
							{
								if (std::abs(glm::dot(door.normal, glm::vec3(normal))) > 0.5f)
									doorDist = std::min(doorDist, glm::distance(pos, door.position) - door.radius);
							}
							const float doorDist255 = (glm::clamp(doorDist, -1.0f, 1.0f) + 1.0f) * 127.0f;
							
							WallVertex& vertex = region.vertices.emplace_back();
							vertex.position = pos;
							vertex.misc[WallVertex::M_TexLayer] = texture;
							vertex.misc[WallVertex::M_DoorDist] = (uint8_t)doorDist255;
							vertex.misc[WallVertex::M_AO] = 0;
							vertex.SetNormal(normal);
							vertex.SetTangent(tangent);
							
							if (doorDist < 0.0f)
								hasCollision = false;
							if (hasCollision)
								collisionVertices.push_back(pos);
							
							if (!IsAir(globalPos + tDir))
								vertex.misc[WallVertex::M_AO] |= fy + 1;
							if (!IsAir(globalPos + bDir))
								vertex.misc[WallVertex::M_AO] |= (fx + 1) << 2;
							if (!IsAir(globalPos + diagDir) && vertex.misc[WallVertex::M_AO] == 0)
								vertex.misc[WallVertex::M_AO] = 1 << (fx + 2 * fy);
						}
					}
					
					if (hasCollision)
					{
						for (uint16_t i : indicesRelative)
						{
							collisionIndices.push_back(baseIndexCol + i);
						}
					}
					else
					{
						while (collisionVertices.size() > baseIndexCol)
						{
							collisionVertices.pop_back();
						}
					}
				}
			}
		}
	}
	
	region.collisionMesh = eg::CollisionMesh::CreateV3<uint32_t>(collisionVertices, collisionIndices);
	region.collisionMesh.FlipWinding();
}

void World::BuildRegionBorderMesh(glm::ivec3 coordinate, World::RegionData& region)
{
	const glm::ivec3 globalBase = coordinate * (int)REGION_SIZE;
	region.borderVertices.clear();
	region.gravityCorners.clear();
	
	for (uint32_t x = 0; x < REGION_SIZE; x++)
	{
		for (uint32_t y = 0; y < REGION_SIZE; y++)
		{
			for (uint32_t z = 0; z < REGION_SIZE; z++)
			{
				const uint64_t voxel = region.voxels[x][y][z];
				if ((voxel & IS_AIR_MASK) == 0)
					continue;
				const glm::ivec3 gPos = globalBase + glm::ivec3(x, y, z);
				const glm::vec3 cPos = glm::vec3(gPos) + 0.5f;
				
				for (int dl = 0; dl < 3; dl++)
				{
					glm::ivec3 dlV, uV, vV;
					dlV[dl] = 1;
					uV[(dl + 1) % 3] = 1;
					vV[(dl + 2) % 3] = 1;
					const Dir uDir = (Dir)(((dl + 1) % 3) * 2);
					const Dir vDir = (Dir)(((dl + 2) % 3) * 2);
					
					for (int u = 0; u < 2; u++)
					{
						const glm::ivec3 uSV = uV * (u * 2 - 1);
						for (int v = 0; v < 2; v++)
						{
							const glm::ivec3 vSV = vV * (v * 2 - 1);
							const bool uAir = IsAir(gPos + uSV);
							const bool vAir = IsAir(gPos + vSV);
							const bool diagAir = IsAir(gPos + uSV + vSV);
							if (diagAir || uAir != vAir)
								continue;
							
							WallBorderVertex& v1 = region.borderVertices.emplace_back();
							v1.position = glm::vec4(cPos + 0.5f * glm::vec3(uSV + vSV + dlV), 0.0f);
							VecEncode(-uSV, v1.normal1);
							VecEncode(-vSV, v1.normal2);
							
							WallBorderVertex& v2 = region.borderVertices.emplace_back(v1);
							v2.position -= glm::vec4(dlV, -1);
							
							const uint64_t gBit = (48 + dl * 4 + (1 - v) * 2 + (1 - u));
							if ((voxel >> gBit) & 1U)
							{
								GravityCorner& corner = region.gravityCorners.emplace_back();
								corner.position = cPos + 0.5f * glm::vec3(uSV + vSV - dlV);
								corner.down1 = u ? uDir : OppositeDir(uDir);
								corner.down2 = v ? vDir : OppositeDir(vDir);
								if (u != v)
									corner.position += dlV;
								
								for (int s = 0; s < 2; s++)
								{
									glm::ivec3 nextGlobalPos = gPos + dlV * (((s + u + v) % 2) * 2 - 1);
									auto [nextLocalC, nextRegionC] = DecomposeGlobalCoordinate(nextGlobalPos);
									const Region* nextRegion = GetRegion(nextRegionC);
									if (nextRegion == nullptr)
									{
										corner.isEnd[s] = true;
										continue;
									}
									
									const uint64_t nextVoxel = nextRegion->data->voxels[nextLocalC.x][nextLocalC.y][nextLocalC.z];
									corner.isEnd[s] = ((nextVoxel >> gBit) & 1U) == 0;
								}
							}
						}
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

const GravityCorner* World::FindGravityCorner(const eg::AABB& aabb, glm::vec3 move, Dir currentDown) const
{
	glm::vec3 pos = (aabb.min + aabb.max) / 2.0f;
	
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
		if (glm::dot(move, otherDown) < 0.0001f)
			return;
		
		//Checks that the player is positioned correctly along the side of the corner
		glm::vec3 cornerVec = glm::cross(glm::vec3(DirectionVector(corner.down1)), glm::vec3(DirectionVector(corner.down2)));
		float t = glm::dot(cornerVec, pos - corner.position);
		
		if (t < 0.0f || t > 1.0f)
			return;
		
		//Checks that the player is positioned at the same height as the corner
		float h1 = glm::dot(currentDownDir, corner.position - aabb.min);
		float h2 = glm::dot(currentDownDir, corner.position - aabb.max);
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
	
	glm::ivec3 gMin = glm::ivec3(glm::floor(glm::min(aabb.min, aabb.min + move))) - 2;
	glm::ivec3 gMax = glm::ivec3(glm::ceil(glm::max(aabb.max, aabb.max + move))) + 1;
	
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

WallRayIntersectResult World::RayIntersectWall(const eg::Ray& ray) const
{
	float minDist = INFINITY;
	WallRayIntersectResult result;
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

RayIntersectResult World::RayIntersect(const eg::Ray& ray) const
{
	RayIntersectArgs intersectArgs;
	RayIntersectMessage message;
	message.rayIntersectArgs = &intersectArgs;
	
	intersectArgs.entity = nullptr;
	intersectArgs.distance = INFINITY;
	intersectArgs.ray = ray;
	
	WallRayIntersectResult wallResult = RayIntersectWall(ray);
	if (wallResult.intersected)
	{
		intersectArgs.distance = glm::dot(wallResult.intersectPosition - ray.GetStart(), ray.GetDirection());
	}
	
	m_entityManager->SendMessageToAll(message);
	
	RayIntersectResult result;
	result.entity = intersectArgs.entity;
	result.distance = intersectArgs.distance;
	result.intersected = intersectArgs.distance != INFINITY;
	return result;
}

void World::InitializeBulletPhysics()
{
	m_bulletBroadphase = std::make_unique<btDbvtBroadphase>();
	m_bulletWorld = std::make_unique<btDiscreteDynamicsWorld>(bullet::dispatcher, m_bulletBroadphase.get(),
		bullet::solver, bullet::collisionConfig);
	m_bulletWorld->setGravity({ 0, -bullet::GRAVITY, 0 });
	
	PrepareRegionMeshes(false);
	
	m_wallsBulletMesh = std::make_unique<btTriangleMesh>();
	for (Region& region : m_regions)
	{
		if (region.canDraw)
		{
			bullet::AddCollisionMesh(*m_wallsBulletMesh, region.data->collisionMesh);
		}
	}
	
	m_wallsBulletShape = std::make_unique<btBvhTriangleMeshShape>(m_wallsBulletMesh.get(), true);
	
	//Creates the wall rigid body and adds it to the world
	btTransform startTransform;
	startTransform.setIdentity();
	m_wallsMotionState = std::make_unique<btDefaultMotionState>(startTransform);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(0.0f, m_wallsMotionState.get(), m_wallsBulletShape.get());
	m_wallsRigidBody = std::make_unique<btRigidBody>(rbInfo);
	m_wallsRigidBody->setFriction(1.0f);
	m_bulletWorld->addRigidBody(m_wallsRigidBody.get());
	
	//Adds rigid bodies to the world
	static eg::EntitySignature rigidBodySignature = eg::EntitySignature::Create<ECRigidBody>();
	for (eg::Entity& entity : m_entityManager->GetEntitySet(rigidBodySignature))
	{
		InitRigidBodyEntity(entity);
	}
}

void World::InitRigidBodyEntity(eg::Entity& entity)
{
	ECRigidBody& ecRigidBody = entity.GetComponent<ECRigidBody>();
	
	if (ecRigidBody.m_rigidBody.has_value() && ecRigidBody.m_physicsWorld == nullptr)
	{
		m_bulletWorld->addRigidBody(&*ecRigidBody.m_rigidBody);
		ecRigidBody.m_physicsWorld = m_bulletWorld.get();
	}
}

void World::CalcClipping(ClippingArgs& args, Dir currentDown) const
{
	for (const Region& region : m_regions)
	{
		if (region.canDraw)
		{
			eg::CheckEllipsoidMeshCollision(args.collisionInfo, args.ellipsoid, args.move,
				region.data->collisionMesh, glm::mat4(1.0f));
		}
	}
	
	CalculateCollisionMessage message;
	message.currentDown = currentDown;
	message.clippingArgs = &args;
	m_entityManager->SendMessageToAll(message);
}

bool World::HasCollision(const glm::ivec3& pos, Dir side) const
{
	if (IsAir(pos) == IsAir(pos + DirectionVector(side)))
		return false;
	
	glm::ivec3 sideNormal = -DirectionVector(side);
	for (const Door& door : m_doors)
	{
		if (std::abs(glm::dot(door.normal, glm::vec3(sideNormal))) < 0.5f)
			continue;
		
		float radSq = door.radius * door.radius;
		
		const glm::ivec3 faceCenter2 = pos * 2 + 1 - sideNormal;
		const glm::ivec3 tangent = voxel::tangents[(int)side];
		const glm::ivec3 biTangent = voxel::biTangents[(int)side];
		for (int fx = 0; fx < 2; fx++)
		{
			for (int fy = 0; fy < 2; fy++)
			{
				const glm::ivec3 tDir = tangent * (fy * 2 - 1);
				const glm::ivec3 bDir = biTangent * (fx * 2 - 1);
				const glm::ivec3 diagDir = tDir + bDir;
				const glm::vec3 vertexPos = glm::vec3(faceCenter2 + diagDir) * 0.5f;
				if (glm::distance2(door.position, vertexPos) < radSq)
					return false;
			}
		}
	}
	
	return true;
}
