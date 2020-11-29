#include "World.hpp"
#include "PrepareDrawArgs.hpp"
#include "Entities/EntTypes/EntranceExitEnt.hpp"
#include "Entities/Components/ActivatorComp.hpp"
#include "Entities/EntTypes/CubeEnt.hpp"
#include "Entities/EntTypes/WallLightEnt.hpp"
#include "../Graphics/Materials/GravityCornerLightMaterial.hpp"
#include "../Graphics/WallShader.hpp"
#include "../Graphics/RenderSettings.hpp"

#include <yaml-cpp/yaml.h>

World::World()
{
	m_physicsObject.canBePushed = false;
	m_physicsObject.shape = &m_collisionMesh;
	
	m_voxelVertexBuffer.flags = eg::BufferFlags::VertexBuffer | eg::BufferFlags::CopyDst;
	m_voxelIndexBuffer.flags = eg::BufferFlags::IndexBuffer | eg::BufferFlags::CopyDst;
	m_borderVertexBuffer.flags = eg::BufferFlags::VertexBuffer | eg::BufferFlags::CopyDst;
	
	thumbnailCameraPos = glm::vec3(0, 0, 0);
	thumbnailCameraDir = glm::vec3(0, 0, 1);
}

static const uint32_t CURRENT_VERSION = 8;
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
	
	std::unique_ptr<World> world = std::make_unique<World>();
	
	const uint32_t version = eg::BinRead<uint32_t>(stream);
	if (version >= 1 && version <= 4)
	{
		world->LoadFormatSub5(stream, version, isEditor);
	}
	else if (version >= 5)
	{
		world->LoadFormatSup5(stream, version, isEditor);
	}
	else
	{
		eg::Log(eg::LogLevel::Error, "wd", "Unsupported world format");
		return nullptr;
	}
	
	if (version < 8)
	{
		world->entManager.ForEachOfType<EntranceExitEnt>([&] (EntranceExitEnt& ent)
		{
			if (ent.m_type == EntranceExitEnt::Type::Entrance)
			{
				world->thumbnailCameraDir = glm::vec3(DirectionVector(ent.GetFacingDirection()));
				world->thumbnailCameraPos = ent.GetPosition() + world->thumbnailCameraDir * 0.1f;
			}
		});
	}
	
	ActivatorComp::Initialize(world->entManager);
	
	if (!isEditor)
	{
		world->entManager.ForEachOfType<EntranceExitEnt>([&] (EntranceExitEnt& ent)
		{
			world->m_doors.push_back(ent.GetDoorDescription());
		});
	}
	
	world->m_isLatestVersion = version == CURRENT_VERSION;
	
	return world;
}

void World::LoadFormatSub5(std::istream& stream, uint32_t version, bool isEditor)
{
	const uint32_t numRegions = eg::BinRead<uint32_t>(stream);
	for (uint32_t i = 0; i < numRegions; i++)
	{
		glm::ivec3 coordinate;
		coordinate.x = eg::BinRead<int32_t>(stream);
		coordinate.y = eg::BinRead<int32_t>(stream);
		coordinate.z = eg::BinRead<int32_t>(stream);
		
		constexpr uint32_t REGION_SIZE = 16;
		constexpr uint64_t IS_AIR_MASK = (uint64_t)1 << (uint64_t)60;
		
		uint64_t voxels[REGION_SIZE][REGION_SIZE][REGION_SIZE];
		
		if (!eg::ReadCompressedSection(stream, voxels, sizeof(voxels)))
		{
			EG_PANIC("Could not decompress voxels")
		}
		
		glm::ivec3 regionMin = coordinate * (int)REGION_SIZE;
		for (uint32_t x = 0; x < REGION_SIZE; x++)
		{
			for (uint32_t y = 0; y < REGION_SIZE; y++)
			{
				for (uint32_t z = 0; z < REGION_SIZE; z++)
				{
					if (!(voxels[x][y][z] & IS_AIR_MASK))
						continue;
					AirVoxel& voxel = m_voxels.emplace(regionMin + glm::ivec3(x, y, z), AirVoxel()).first->second;
					for (int f = 0; f < 6; f++)
					{
						voxel.materials[f] = (voxels[x][y][z] >> (f * 8)) & 0xFF;
					}
					voxel.hasGravityCorner = (voxels[x][y][z] >> 48) & 0xFFF;
				}
			}
		}
	}
	
	playerHasGravityGun = version >= 2 && (bool)eg::BinRead<uint8_t>(stream);
	
	bool halfLightIntensity = version < 4 || (bool)eg::BinRead<uint8_t>(stream);
	
	title = version >= 3 ? eg::BinReadString(stream) : "Untitled";
	
	entManager = EntityManager::Deserialize(stream);
	
	if (halfLightIntensity)
	{
		entManager.ForEachOfType<WallLightEnt>([&] (WallLightEnt& ent)
		{
			ent.HalfLightIntensity();
		});
	}
}

#pragma pack(push, 1)
struct VoxelData
{
	int32_t x;
	int32_t y;
	int32_t z;
	uint8_t materials[6];
	uint16_t hasGravityCorner;
};
#pragma pack(pop)

void World::LoadFormatSup5(std::istream& stream, uint32_t version, bool isEditor)
{
	uint32_t numVoxels = eg::BinRead<uint32_t>(stream);
	std::vector<VoxelData> voxelData(numVoxels);
	
	if (version == 5)
	{
		stream.read(reinterpret_cast<char*>(voxelData.data()), numVoxels * sizeof(VoxelData));
	}
	else
	{
		eg::ReadCompressedSection(stream, voxelData.data(), numVoxels * sizeof(VoxelData));
	}
	
	for (const VoxelData& data : voxelData)
	{
		AirVoxel& voxel = m_voxels.emplace(glm::ivec3(data.x, data.y, data.z), AirVoxel()).first->second;
		std::copy_n(data.materials, 6, voxel.materials);
		for (int i = 0; i < 6; i++)
		{
			if (voxel.materials[i] >= MAX_WALL_MATERIALS || !wallMaterials[voxel.materials[i]].initialized)
				voxel.materials[i] = 0;
		}
		voxel.hasGravityCorner = std::bitset<12>(data.hasGravityCorner);
	}
	
	if (version >= 8)
	{
		thumbnailCameraPos.x = eg::BinRead<float>(stream);
		thumbnailCameraPos.y = eg::BinRead<float>(stream);
		thumbnailCameraPos.z = eg::BinRead<float>(stream);
		thumbnailCameraDir.x = eg::BinRead<float>(stream);
		thumbnailCameraDir.y = eg::BinRead<float>(stream);
		thumbnailCameraDir.z = eg::BinRead<float>(stream);
		thumbnailCameraDir = glm::normalize(thumbnailCameraDir);
	}
	if (version >= 7)
	{
		extraWaterParticles = eg::BinRead<uint32_t>(stream);
	}
	playerHasGravityGun = eg::BinRead<uint8_t>(stream);
	title = eg::BinReadString(stream);
	
	entManager = EntityManager::Deserialize(stream);
}

void World::Save(std::ostream& outStream) const
{
	outStream.write(MAGIC, sizeof(MAGIC));
	eg::BinWrite(outStream, CURRENT_VERSION);
	
	eg::BinWrite(outStream, (uint32_t)m_voxels.size());
	std::vector<VoxelData> voxelData;
	voxelData.reserve(m_voxels.size());
	
	for (const auto& voxel : m_voxels)
	{
		VoxelData& data = voxelData.emplace_back();
		data.x = voxel.first.x;
		data.y = voxel.first.y;
		data.z = voxel.first.z;
		std::copy_n(voxel.second.materials, 6, data.materials);
		data.hasGravityCorner = voxel.second.hasGravityCorner.to_ulong();
	}
	
	eg::WriteCompressedSection(outStream, voxelData.data(), voxelData.size() * sizeof(VoxelData));
	
	eg::BinWrite<float>(outStream, thumbnailCameraPos.x);
	eg::BinWrite<float>(outStream, thumbnailCameraPos.y);
	eg::BinWrite<float>(outStream, thumbnailCameraPos.z);
	eg::BinWrite<float>(outStream, thumbnailCameraDir.x);
	eg::BinWrite<float>(outStream, thumbnailCameraDir.y);
	eg::BinWrite<float>(outStream, thumbnailCameraDir.z);
	
	eg::BinWrite<uint32_t>(outStream, extraWaterParticles);
	eg::BinWrite<uint8_t>(outStream, playerHasGravityGun);
	eg::BinWriteString(outStream, title);
	
	entManager.Serialize(outStream);
}

std::pair<glm::ivec3, glm::ivec3> World::CalculateBounds() const
{
	glm::ivec3 boundsMin(INT_MAX);
	glm::ivec3 boundsMax(INT_MIN);
	for (const auto& voxel : m_voxels)
	{
		boundsMin = glm::min(boundsMin, voxel.first);
		boundsMax = glm::max(boundsMax, voxel.first);
	}
	boundsMin -= 1;
	boundsMax += 2;
	return std::make_pair(boundsMin, boundsMax);
}

void World::SetIsAir(const glm::ivec3& pos, bool air)
{
	auto voxelIt = m_voxels.find(pos);
	bool alreadyAir = voxelIt != m_voxels.end();
	
	if (alreadyAir == air)
		return;
	
	if (alreadyAir)
	{
		m_voxels.erase(voxelIt);
	}
	else
	{
		m_voxels.emplace(pos, AirVoxel());
	}
}

void World::SetMaterialSafe(const glm::ivec3& pos, Dir side, int material)
{
	auto voxelIt = m_voxels.find(pos);
	if (voxelIt != m_voxels.end())
	{
		voxelIt->second.materials[(int)side] = material;
		m_buffersOutOfDate = true;
	}
}

void World::SetMaterial(const glm::ivec3& pos, Dir side, int material)
{
	auto voxelIt = m_voxels.find(pos);
	EG_ASSERT(voxelIt != m_voxels.end())
	voxelIt->second.materials[(int)side] = material;
	m_buffersOutOfDate = true;
}

int World::GetMaterial(const glm::ivec3& pos, Dir side) const
{
	auto voxelIt = m_voxels.find(pos);
	if (voxelIt == m_voxels.end())
		return 0;
	return voxelIt->second.materials[(int)side];
}

glm::mat3 GravityCorner::MakeRotationMatrix() const
{
	const glm::vec3 r1 = -DirectionVector(down1);
	const glm::vec3 r2 = -DirectionVector(down2);
	return glm::mat3(r1, r2, glm::cross(r1, r2));
}

void World::CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt)
{
	physicsEngine.RegisterObject(&m_physicsObject);
	
	entManager.ForEachWithFlag(EntTypeFlags::HasPhysics, [&] (Ent& entity)
	{
		entity.CollectPhysicsObjects(physicsEngine, dt);
	});
}

void World::Update(const WorldUpdateArgs& args)
{
	entManager.Update(args);
}

void World::PrepareForDraw(PrepareDrawArgs& args)
{
	if (m_buffersOutOfDate)
		PrepareMeshes(args.isEditor);
	
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
	
	for (const GravityCorner& corner : m_gravityCorners)
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
			args.meshBatch->AddModelMesh(gravityCornerModel, meshIndex, GravityCornerLightMaterial::instance,
				StaticPropMaterial::InstanceData(transformLight));
			args.meshBatch->AddModelMesh(gravityCornerModel, meshIndex, gravityCornerMat,
				StaticPropMaterial::InstanceData(transform));
		}
	}
	
	if (!args.isEditor)
	{
		EntGameDrawArgs entDrawArgs;
		entDrawArgs.world = this;
		entDrawArgs.meshBatch = args.meshBatch;
		entDrawArgs.transparentMeshBatch = args.transparentMeshBatch;
		entDrawArgs.pointLights = &args.pointLights;
		entManager.ForEachWithFlag(EntTypeFlags::Drawable, [&] (Ent& entity) { entity.GameDraw(entDrawArgs); });
	}
}

void World::Draw()
{
	if (!m_canDraw)
		return;
	
	BindWallShaderGame();
	
	eg::DC.BindVertexBuffer(0, m_voxelVertexBuffer.buffer, 0);
	eg::DC.BindIndexBuffer(eg::IndexType::UInt32, m_voxelIndexBuffer.buffer, 0);
	eg::DC.DrawIndexed(0, m_numVoxelIndices, 0, 0, 1);
}

void World::DrawPointLightShadows(const struct PointLightShadowRenderArgs& renderArgs)
{
	if (!m_canDraw)
		return;
	
	BindWallShaderPointLightShadow(renderArgs);
	
	eg::DC.BindVertexBuffer(0, m_voxelVertexBuffer.buffer, 0);
	eg::DC.BindIndexBuffer(eg::IndexType::UInt32, m_voxelIndexBuffer.buffer, 0);
	eg::DC.DrawIndexed(0, m_numVoxelIndices, 0, 0, 1);
}

void World::DrawEditor()
{
	if (!m_canDraw)
		return;
	
	BindWallShaderEditor();
	
	eg::DC.BindVertexBuffer(0, m_voxelVertexBuffer.buffer, 0);
	eg::DC.BindIndexBuffer(eg::IndexType::UInt32, m_voxelIndexBuffer.buffer, 0);
	eg::DC.DrawIndexed(0, m_numVoxelIndices, 0, 0, 1);
	
	if (m_numBorderVertices > 0)
	{
		DrawWallBordersEditor(m_borderVertexBuffer.buffer, m_numBorderVertices);
	}
}

void World::PrepareMeshes(bool isEditor)
{
	std::vector<WallVertex> wallVertices;
	std::vector<uint32_t> wallIndices;
	m_collisionMesh = BuildMesh(wallVertices, wallIndices, isEditor);
	
	m_gravityCorners.clear();
	std::vector<WallBorderVertex> borderVertices;
	BuildBorderMesh(isEditor ? &borderVertices : nullptr, m_gravityCorners);
	
	const uint64_t wallVerticesBytes = wallVertices.size() * sizeof(WallVertex);
	const uint64_t wallIndicesBytes = wallIndices.size() * sizeof(uint32_t);
	const uint64_t borderVerticesBytes = borderVertices.size() * sizeof(WallBorderVertex);
	
	//Creates an upload buffer and copies data to it
	eg::UploadBuffer uploadBuffer = eg::GetTemporaryUploadBuffer(wallVerticesBytes + wallIndicesBytes + borderVerticesBytes);
	char* uploadBufferMem = reinterpret_cast<char*>(uploadBuffer.Map());
	std::memcpy(uploadBufferMem, wallVertices.data(), wallVerticesBytes);
	std::memcpy(uploadBufferMem + wallVerticesBytes, wallIndices.data(), wallIndicesBytes);
	std::memcpy(uploadBufferMem + wallVerticesBytes + wallIndicesBytes, borderVertices.data(), borderVerticesBytes);
	uploadBuffer.Flush();
	
	m_voxelVertexBuffer.EnsureSize(wallVertices.size(), sizeof(WallVertex));
	m_voxelIndexBuffer.EnsureSize(wallIndices.size(), sizeof(uint32_t));
	m_borderVertexBuffer.EnsureSize(borderVertices.size(), sizeof(WallBorderVertex));
	
	//Uploads data to the vertex and index buffers
	eg::DC.CopyBuffer(uploadBuffer.buffer, m_voxelVertexBuffer.buffer, uploadBuffer.offset, 0, wallVerticesBytes);
	eg::DC.CopyBuffer(uploadBuffer.buffer, m_voxelIndexBuffer.buffer, uploadBuffer.offset + wallVerticesBytes, 0, wallIndicesBytes);
	if (!borderVertices.empty())
	{
		eg::DC.CopyBuffer(uploadBuffer.buffer, m_borderVertexBuffer.buffer,
		                  uploadBuffer.offset + wallVerticesBytes + wallIndicesBytes, 0, borderVerticesBytes);
		m_borderVertexBuffer.buffer.UsageHint(eg::BufferUsage::VertexBuffer);
	}
	
	m_voxelVertexBuffer.buffer.UsageHint(eg::BufferUsage::VertexBuffer);
	m_voxelIndexBuffer.buffer.UsageHint(eg::BufferUsage::IndexBuffer);
	
	m_numVoxelIndices = wallIndices.size();
	m_numBorderVertices = borderVertices.size();
	m_buffersOutOfDate = false;
	m_canDraw = true;
}

eg::CollisionMesh World::BuildMesh(std::vector<WallVertex>& vertices, std::vector<uint32_t>& indices, bool includeNoDraw) const
{
	std::vector<glm::vec3> collisionVertices;
	std::vector<uint32_t> collisionIndices;
	
	for (const auto& voxel : m_voxels)
	{
		for (int s = 0; s < 6; s++)
		{
			glm::ivec3 normal = DirectionVector((Dir)s);
			const glm::ivec3 nPos = voxel.first - normal;
			
			//Only emit triangles for voxels that face into a solid voxel
			if (IsAir(nPos))
				continue;
			int texture = voxel.second.materials[s];
			if (texture == 0 && !includeNoDraw)
				continue;
			
			//The center of the face to be emitted
			const glm::ivec3 faceCenter2 = voxel.first * 2 + 1 - normal;
			
			//Emits indices
			const uint32_t baseIndex = (uint32_t)vertices.size();
			static const uint32_t indicesRelative[] = { 0, 1, 2, 2, 1, 3 };
			for (uint32_t i : indicesRelative)
				indices.push_back(baseIndex + i);
			
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
					
					WallVertex& vertex = vertices.emplace_back();
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
					
					if (!IsAir(voxel.first + tDir))
						vertex.misc[WallVertex::M_AO] |= fy + 1;
					if (!IsAir(voxel.first + bDir))
						vertex.misc[WallVertex::M_AO] |= (fx + 1) << 2;
					if (!IsAir(voxel.first + diagDir) && vertex.misc[WallVertex::M_AO] == 0)
						vertex.misc[WallVertex::M_AO] = 1 << (fx + 2 * fy);
				}
			}
			
			if (hasCollision)
			{
				for (uint32_t i : indicesRelative)
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
	
	eg::CollisionMesh collisionMesh = eg::CollisionMesh::CreateV3<uint32_t>(collisionVertices, collisionIndices);
	collisionMesh.FlipWinding();
	return collisionMesh;
}

void World::BuildBorderMesh(std::vector<WallBorderVertex>* borderVertices, std::vector<GravityCorner>& gravityCorners) const
{
	for (const auto& voxel : m_voxels)
	{
		const glm::vec3 cPos = glm::vec3(voxel.first) + 0.5f;
		
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
					const bool uAir = IsAir(voxel.first + uSV);
					const bool vAir = IsAir(voxel.first + vSV);
					const bool diagAir = IsAir(voxel.first + uSV + vSV);
					if (diagAir || uAir != vAir)
						continue;
					
					if (borderVertices != nullptr)
					{
						WallBorderVertex& v1 = borderVertices->emplace_back();
						v1.position = glm::vec4(cPos + 0.5f * glm::vec3(uSV + vSV + dlV), 0.0f);
						VecEncode(-uSV, v1.normal1);
						VecEncode(-vSV, v1.normal2);
						
						WallBorderVertex& v2 = borderVertices->emplace_back(v1);
						v2.position -= glm::vec4(dlV, -1);
					}
					
					const uint64_t gBit = (dl * 4 + (1 - v) * 2 + (1 - u));
					if (voxel.second.hasGravityCorner[gBit])
					{
						GravityCorner& corner = gravityCorners.emplace_back();
						corner.position = cPos + 0.5f * glm::vec3(uSV + vSV - dlV);
						corner.down1 = u ? uDir : OppositeDir(uDir);
						corner.down2 = v ? vDir : OppositeDir(vDir);
						if (u != v)
							corner.position += dlV;
						
						for (int s = 0; s < 2; s++)
						{
							glm::ivec3 nextGlobalPos = voxel.first + dlV * (((s + u + v) % 2) * 2 - 1);
							auto nextVoxelIt = m_voxels.find(nextGlobalPos);
							corner.isEnd[s] = nextVoxelIt == m_voxels.end() || !nextVoxelIt->second.hasGravityCorner[gBit];
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
	
	auto voxelIt = m_voxels.find(glm::ivec3(voxelPos));
	if (voxelIt == m_voxels.end())
		return false;
	
	return voxelIt->second.hasGravityCorner[voxelPos.w];
}

void World::SetIsGravityCorner(const glm::ivec3& cornerPos, Dir cornerDir, bool value)
{
	glm::ivec4 voxelPos = GetGravityCornerVoxelPos(cornerPos, cornerDir);
	if (voxelPos.w == -1)
		return;
	
	auto voxelIt = m_voxels.find(glm::ivec3(voxelPos));
	if (voxelIt == m_voxels.end())
		return;
	
	voxelIt->second.hasGravityCorner[voxelPos.w] = value;
	m_buffersOutOfDate = true;
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
	
	for (const GravityCorner& corner : m_gravityCorners)
	{
		glm::vec3 otherDown;
		if (corner.down1 == currentDown)
			otherDown = DirectionVector(corner.down2);
		else if (corner.down2 == currentDown)
			otherDown = DirectionVector(corner.down1);
		else
			continue;
		
		//Checks that the player is moving towards the corner
		if (glm::dot(move, otherDown) < 0.0001f)
			continue;
		
		//Checks that the player is positioned correctly along the side of the corner
		glm::vec3 cornerVec = glm::cross(glm::vec3(DirectionVector(corner.down1)), glm::vec3(DirectionVector(corner.down2)));
		float t = glm::dot(cornerVec, pos - corner.position);
		
		if (t < 0.0f || t > 1.0f)
			continue;
		
		//Checks that the player is positioned at the same height as the corner
		float h1 = glm::dot(currentDownDir, corner.position - aabb.min);
		float h2 = glm::dot(currentDownDir, corner.position - aabb.max);
		if (std::abs(h1) > MAX_HEIGHT_DIFF && std::abs(h2) > MAX_HEIGHT_DIFF)
			continue;
		
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

bool World::IsAir(const glm::ivec3& pos) const
{
	return m_voxels.count(pos) != 0;
}
