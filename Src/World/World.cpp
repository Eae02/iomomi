#include "World.hpp"

#include "../../Protobuf/Build/World.pb.h"
#include "../Graphics/Materials/GravityCornerLightMaterial.hpp"
#include "../Graphics/WallShader.hpp"
#include "Entities/Components/ActivatorComp.hpp"
#include "Entities/EntTypes/Activation/CubeEnt.hpp"
#include "Entities/EntTypes/Activation/CubeSpawnerEnt.hpp"
#include "Entities/EntTypes/EntranceExitEnt.hpp"
#include "PrepareDrawArgs.hpp"

World::World()
{
	ssrFallbackColor = eg::ColorSRGB::FromHex(0x625F46);

	m_physicsObject.canBePushed = false;
	m_physicsObject.shape = &m_collisionMesh;
	m_physicsObject.owner = this;

	m_voxelVertexBuffer.flags = eg::BufferFlags::VertexBuffer | eg::BufferFlags::CopyDst;
	m_voxelIndexBuffer.flags = eg::BufferFlags::IndexBuffer | eg::BufferFlags::CopyDst;
	m_borderVertexBuffer.flags = eg::BufferFlags::VertexBuffer | eg::BufferFlags::CopyDst;

	thumbnailCameraPos = glm::vec3(0, 0, 0);
	thumbnailCameraDir = glm::vec3(0, 0, 1);
}

static const uint32_t CURRENT_VERSION = 9;
static char MAGIC[] = { (char)0xFF, 'G', 'W', 'D' };

struct __attribute__((__packed__, __may_alias__)) VoxelData
{
	int32_t x;
	int32_t y;
	int32_t z;
	uint8_t materials[6];
	uint16_t hasGravityCorner;
};

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
	if (version != 9)
	{
		eg::Log(eg::LogLevel::Error, "wd", "Unsupported world format");
		return nullptr;
	}

	// Reads voxel data
	uint32_t numVoxels = eg::BinRead<uint32_t>(stream);
	std::vector<VoxelData> voxelData(numVoxels);
	eg::ReadCompressedSection(stream, voxelData.data(), numVoxels * sizeof(VoxelData));

	// Parses voxel data
	for (const VoxelData& data : voxelData)
	{
		VoxelBuffer::AirVoxel& voxel =
			world->voxels.m_voxels.emplace(glm::ivec3(data.x, data.y, data.z), VoxelBuffer::AirVoxel()).first->second;
		std::copy_n(data.materials, 6, voxel.materials);
		for (int i = 0; i < 6; i++)
		{
			if (voxel.materials[i] >= MAX_WALL_MATERIALS || !wallMaterials[voxel.materials[i]].initialized)
				voxel.materials[i] = 0;
		}
		voxel.hasGravityCorner = std::bitset<12>(data.hasGravityCorner);
	}

	// Parses protobuf data
	uint64_t dataSize = eg::BinRead<uint64_t>(stream);
	std::vector<char> data(dataSize);
	stream.read(data.data(), dataSize);
	iomomi_pb::World worldPB;
	worldPB.ParseFromArray(data.data(), eg::ToInt(dataSize));

	// Writes protobuf data to the world's fields
	world->thumbnailCameraPos =
		glm::vec3(worldPB.thumbnail_camera_x(), worldPB.thumbnail_camera_y(), worldPB.thumbnail_camera_z());
	world->thumbnailCameraDir = glm::normalize(
		glm::vec3(worldPB.thumbnail_camera_dx(), worldPB.thumbnail_camera_dy(), worldPB.thumbnail_camera_dz()));
	world->extraWaterParticles = worldPB.extra_water_particles();
	world->playerHasGravityGun = worldPB.player_has_gravity_gun();
	world->title = worldPB.title();
	if (worldPB.water_presim_iterations() != 0)
		world->waterPresimIterations = worldPB.water_presim_iterations();
	for (int i = 0; i < std::min(worldPB.control_hints_size(), NUM_OPTIONAL_CONTROL_HINTS); i++)
	{
		world->showControlHint[i] = worldPB.control_hints(i);
	}
	if (worldPB.has_ssr_fallback())
	{
		world->ssrFallbackColor.r = worldPB.ssr_fallback_r();
		world->ssrFallbackColor.g = worldPB.ssr_fallback_g();
		world->ssrFallbackColor.b = worldPB.ssr_fallback_b();
		if (worldPB.ssr_intensity() > 0)
			world->ssrIntensity = worldPB.ssr_intensity();
	}

	world->entManager = EntityManager::Deserialize(stream);

	world->voxels.m_modified = true;
	world->m_isLatestVersion = version == CURRENT_VERSION;

	// Post-load initialization
	ActivatorComp::Initialize(world->entManager);
	if (!isEditor)
	{
		world->entManager.ForEachOfType<EntranceExitEnt>([&](EntranceExitEnt& ent)
		                                                 { world->m_doors.push_back(ent.GetDoorDescription()); });

		world->entManager.ForEachOfType<CubeSpawnerEnt>(
			[&](CubeSpawnerEnt& ent)
			{ world->cubeSpawnerPositions2.emplace_back(glm::round(ent.GetPosition() * 2.0f)); });
	}
	ActivationLightStripEnt::GenerateAll(*world);

	return world;
}

void World::Save(std::ostream& outStream) const
{
	outStream.write(MAGIC, sizeof(MAGIC));
	eg::BinWrite(outStream, CURRENT_VERSION);

	eg::BinWrite(outStream, eg::UnsignedNarrow<uint32_t>(voxels.m_voxels.size()));
	std::vector<VoxelData> voxelData;
	voxelData.reserve(voxels.m_voxels.size());

	for (const auto& voxel : voxels.m_voxels)
	{
		VoxelData& data = voxelData.emplace_back();
		data.x = voxel.first.x;
		data.y = voxel.first.y;
		data.z = voxel.first.z;
		std::copy_n(voxel.second.materials, 6, data.materials);
		data.hasGravityCorner = static_cast<uint16_t>(voxel.second.hasGravityCorner.to_ulong());
	}

	eg::WriteCompressedSection(outStream, voxelData.data(), voxelData.size() * sizeof(VoxelData));

	iomomi_pb::World worldPB;
	worldPB.set_thumbnail_camera_x(thumbnailCameraPos.x);
	worldPB.set_thumbnail_camera_y(thumbnailCameraPos.y);
	worldPB.set_thumbnail_camera_z(thumbnailCameraPos.z);
	worldPB.set_thumbnail_camera_dx(thumbnailCameraDir.x);
	worldPB.set_thumbnail_camera_dy(thumbnailCameraDir.y);
	worldPB.set_thumbnail_camera_dz(thumbnailCameraDir.z);
	worldPB.set_title(title);
	worldPB.set_extra_water_particles(extraWaterParticles);
	worldPB.set_player_has_gravity_gun(playerHasGravityGun);
	worldPB.set_has_ssr_fallback(true);
	worldPB.set_ssr_fallback_r(ssrFallbackColor.r);
	worldPB.set_ssr_fallback_g(ssrFallbackColor.g);
	worldPB.set_ssr_fallback_b(ssrFallbackColor.b);
	worldPB.set_ssr_intensity(ssrIntensity);
	worldPB.set_water_presim_iterations(waterPresimIterations);
	for (bool controlHint : showControlHint)
	{
		worldPB.add_control_hints(controlHint);
	}

	eg::BinWrite<uint64_t>(outStream, worldPB.ByteSizeLong());
	worldPB.SerializeToOstream(&outStream);

	entManager.Serialize(outStream);
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

	entManager.ForEachWithFlag(
		EntTypeFlags::HasPhysics, [&](Ent& entity) { entity.CollectPhysicsObjects(physicsEngine, dt); });
}

void World::Update(const WorldUpdateArgs& args)
{
	entManager.Update(args);
}

void World::UpdateAfterPhysics(const WorldUpdateArgs& args)
{
	entManager.ForEachOfType<CubeEnt>([&](CubeEnt& cube) { cube.UpdateAfterSimulation(args); });
}

void World::PrepareForDraw(PrepareDrawArgs& args)
{
	if (voxels.m_modified)
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
				transform = transform * glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 1)) *
				            glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, -1));
			}

			const int meshIndex = corner.isEnd[s] ? endMeshIndex : midMeshIndex;

			const eg::AABB aabb = gravityCornerModel.GetMesh(meshIndex).boundingAABB->TransformedBoundingBox(transform);
			if (args.frustum->Intersects(aabb))
			{
				const glm::mat4 transformLight =
					transform * glm::translate(glm::mat4(1.0f), glm::vec3(0.01f, 0.01f, 0));
				args.meshBatch->AddModelMesh(
					gravityCornerModel, meshIndex, GravityCornerLightMaterial::instance,
					StaticPropMaterial::InstanceData(transformLight));

				args.meshBatch->AddModelMesh(
					gravityCornerModel, meshIndex, gravityCornerMat, StaticPropMaterial::InstanceData(transform));
			}
		}
	}

	if (!args.isEditor)
	{
		EntGameDrawArgs entDrawArgs = {};
		entDrawArgs.world = this;
		entDrawArgs.meshBatch = args.meshBatch;
		entDrawArgs.transparentMeshBatch = args.transparentMeshBatch;
		entDrawArgs.frustum = args.frustum;
		entManager.ForEachWithFlag(EntTypeFlags::Drawable, [&](Ent& entity) { entity.GameDraw(entDrawArgs); });
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

void World::DrawPointLightShadows(const struct PointLightShadowDrawArgs& renderArgs)
{
	if (!m_canDraw)
		return;

	BindWallShaderPointLightShadow(renderArgs);

	eg::DC.BindVertexBuffer(0, m_voxelVertexBuffer.buffer, 0);
	eg::DC.BindIndexBuffer(eg::IndexType::UInt32, m_voxelIndexBuffer.buffer, 0);
	eg::DC.DrawIndexed(0, m_numVoxelIndices, 0, 0, 1);
}

void World::DrawEditor(bool drawGrid)
{
	if (!m_canDraw)
		return;

	BindWallShaderEditor(drawGrid);

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

	// Creates an upload buffer and copies data to it
	eg::UploadBuffer uploadBuffer =
		eg::GetTemporaryUploadBuffer(wallVerticesBytes + wallIndicesBytes + borderVerticesBytes);
	char* uploadBufferMem = reinterpret_cast<char*>(uploadBuffer.Map());
	std::memcpy(uploadBufferMem, wallVertices.data(), wallVerticesBytes);
	std::memcpy(uploadBufferMem + wallVerticesBytes, wallIndices.data(), wallIndicesBytes);
	std::memcpy(uploadBufferMem + wallVerticesBytes + wallIndicesBytes, borderVertices.data(), borderVerticesBytes);
	uploadBuffer.Flush();

	m_voxelVertexBuffer.EnsureSize(eg::UnsignedNarrow<uint32_t>(wallVertices.size()), sizeof(WallVertex));
	m_voxelIndexBuffer.EnsureSize(eg::UnsignedNarrow<uint32_t>(wallIndices.size()), sizeof(uint32_t));
	m_borderVertexBuffer.EnsureSize(eg::UnsignedNarrow<uint32_t>(borderVertices.size()), sizeof(WallBorderVertex));

	// Uploads data to the vertex and index buffers
	eg::DC.CopyBuffer(uploadBuffer.buffer, m_voxelVertexBuffer.buffer, uploadBuffer.offset, 0, wallVerticesBytes);
	eg::DC.CopyBuffer(
		uploadBuffer.buffer, m_voxelIndexBuffer.buffer, uploadBuffer.offset + wallVerticesBytes, 0, wallIndicesBytes);
	if (!borderVertices.empty())
	{
		eg::DC.CopyBuffer(
			uploadBuffer.buffer, m_borderVertexBuffer.buffer,
			uploadBuffer.offset + wallVerticesBytes + wallIndicesBytes, 0, borderVerticesBytes);
		m_borderVertexBuffer.buffer.UsageHint(eg::BufferUsage::VertexBuffer);
	}

	m_voxelVertexBuffer.buffer.UsageHint(eg::BufferUsage::VertexBuffer);
	m_voxelIndexBuffer.buffer.UsageHint(eg::BufferUsage::IndexBuffer);

	m_numVoxelIndices = eg::UnsignedNarrow<uint32_t>(wallIndices.size());
	m_numBorderVertices = eg::UnsignedNarrow<uint32_t>(borderVertices.size());
	voxels.m_modified = false;
	m_canDraw = true;
}

eg::CollisionMesh World::BuildMesh(
	std::vector<WallVertex>& vertices, std::vector<uint32_t>& indices, bool includeNoDraw) const
{
	std::vector<glm::vec3> collisionVertices;
	std::vector<uint32_t> collisionIndices;

	std::vector<WallVertex> pendingVertices;

	for (const auto& voxel : voxels.m_voxels)
	{
		for (int s = 0; s < 6; s++)
		{
			glm::ivec3 normal = DirectionVector(static_cast<Dir>(s));
			const glm::ivec3 nPos = voxel.first - normal;

			// Only emit triangles for voxels that face into a solid voxel
			if (voxels.IsAir(nPos))
				continue;
			int materialIndex = voxel.second.materials[s];
			if (materialIndex == 0 && !includeNoDraw)
				continue;

			// The center of the face to be emitted
			const glm::ivec3 faceCenter2 = voxel.first * 2 + 1 - normal;

			bool shouldDraw = !eg::Contains(cubeSpawnerPositions2, faceCenter2);
			bool hasCollision = true;

			const glm::ivec3 tangent = voxel::tangents[s];
			const glm::ivec3 biTangent = voxel::biTangents[s];
			struct QuadVertex
			{
				int fx;
				int fy;
				glm::ivec3 tDir;
				glm::ivec3 bDir;
				glm::vec3 pos;
				float doorDist;
			};
			QuadVertex quadVertices[4] = {
				{ 0, 0, -tangent, -biTangent },
				{ 1, 0, tangent, -biTangent },
				{ 1, 1, tangent, biTangent },
				{ 0, 1, -tangent, biTangent },
			};
			for (QuadVertex& vertex : quadVertices)
			{
				vertex.pos = glm::vec3(faceCenter2 + vertex.tDir + vertex.bDir) * 0.5f;

				vertex.doorDist = 100;
				for (const Door& door : m_doors)
				{
					if (std::abs(glm::dot(door.normal, glm::vec3(normal))) > 0.5f)
					{
						vertex.doorDist =
							std::min(vertex.doorDist, glm::distance(vertex.pos, door.position) - door.radius);
					}
				}

				if (vertex.doorDist < 0.0f)
					hasCollision = false;
			}

			pendingVertices.clear();
			auto PushVertex = [&](const glm::vec3& pos)
			{
				WallVertex& vertex = pendingVertices.emplace_back();
				for (int i = 0; i < 3; i++)
					vertex.position[i] = pos[i];

				vertex.texCoord[0] = glm::dot(glm::vec3(biTangent), pos) / wallMaterials[materialIndex].textureScale;
				vertex.texCoord[1] = -glm::dot(glm::vec3(tangent), pos) / wallMaterials[materialIndex].textureScale;
				vertex.texCoord[2] = static_cast<float>(materialIndex - 1);
				vertex.normalAndRoughnessLo[3] = eg::FloatToSNorm(wallMaterials[materialIndex].minRoughness * 2 - 1);
				vertex.tangentAndRoughnessHi[3] = eg::FloatToSNorm(wallMaterials[materialIndex].maxRoughness * 2 - 1);

				vertex.SetNormal(normal);
				vertex.SetTangent(tangent);
			};

			for (int i = 0; i < 4; i++)
			{
				// Adds this vertex if it's not hidden by a door
				if (quadVertices[i].doorDist > 0)
				{
					PushVertex(quadVertices[i].pos);
				}

				// Adds a border vertex if one of this or the next vertex are hidden by a door
				int nextI = (i + 1) % 4;
				if ((quadVertices[i].doorDist > 0) != (quadVertices[nextI].doorDist > 0))
				{
					float a = -quadVertices[nextI].doorDist / (quadVertices[i].doorDist - quadVertices[nextI].doorDist);
					PushVertex(glm::mix(quadVertices[nextI].pos, quadVertices[i].pos, glm::clamp(a, 0.0f, 1.0f)));
				}
			}

			if (pendingVertices.size() < 3)
				continue;

			auto PushIndices = [&](std::vector<uint32_t>& indicesOut, uint32_t baseIndex)
			{
				for (uint32_t i = 2; i < pendingVertices.size(); i++)
				{
					indicesOut.push_back(baseIndex);
					indicesOut.push_back(baseIndex + i - 1);
					indicesOut.push_back(baseIndex + i);
				}
			};

			if (shouldDraw)
			{
				PushIndices(indices, eg::UnsignedNarrow<uint32_t>(vertices.size()));
				for (const WallVertex& vertex : pendingVertices)
					vertices.push_back(vertex);
			}

			if (hasCollision)
			{
				PushIndices(collisionIndices, eg::UnsignedNarrow<uint32_t>(collisionVertices.size()));
				for (const WallVertex& vertex : pendingVertices)
					collisionVertices.emplace_back(vertex.position[0], vertex.position[1], vertex.position[2]);
			}
		}
	}

	eg::CollisionMesh collisionMesh(collisionVertices, collisionIndices);
	collisionMesh.FlipWinding();
	return collisionMesh;
}

void World::BuildBorderMesh(
	std::vector<WallBorderVertex>* borderVertices, std::vector<GravityCorner>& gravityCorners) const
{
	for (const auto& voxel : voxels.m_voxels)
	{
		const glm::vec3 cPos = glm::vec3(voxel.first) + 0.5f;

		for (int dl = 0; dl < 3; dl++)
		{
			glm::ivec3 dlV, uV, vV;
			dlV[dl] = 1;
			uV[(dl + 1) % 3] = 1;
			vV[(dl + 2) % 3] = 1;
			const Dir uDir = static_cast<Dir>(((dl + 1) % 3) * 2);
			const Dir vDir = static_cast<Dir>(((dl + 2) % 3) * 2);

			for (int u = 0; u < 2; u++)
			{
				const glm::ivec3 uSV = uV * (u * 2 - 1);
				for (int v = 0; v < 2; v++)
				{
					const glm::ivec3 vSV = vV * (v * 2 - 1);
					auto CouldHaveGravityCorner = [&](const glm::ivec3& pos)
					{
						const bool uAir = voxels.IsAir(pos + uSV);
						const bool vAir = voxels.IsAir(pos + vSV);
						const bool diagAir = voxels.IsAir(pos + uSV + vSV);
						return !(diagAir || uAir != vAir);
					};

					if (!CouldHaveGravityCorner(voxel.first))
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
							auto nextVoxelIt = voxels.m_voxels.find(nextGlobalPos);
							corner.isEnd[s] = !CouldHaveGravityCorner(nextGlobalPos) ||
							                  nextVoxelIt == voxels.m_voxels.end() ||
							                  !nextVoxelIt->second.hasGravityCorner[gBit];
						}
					}
				}
			}
		}
	}
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

		// Checks that the player is moving towards the corner
		if (glm::dot(move, otherDown) < 0.0001f)
			continue;

		// Checks that the player is positioned correctly along the side of the corner
		glm::vec3 cornerVec =
			glm::cross(glm::vec3(DirectionVector(corner.down1)), glm::vec3(DirectionVector(corner.down2)));
		float t = glm::dot(cornerVec, pos - corner.position);

		if (t < 0.0f || t > 1.0f)
			continue;

		// Checks that the player is positioned at the same height as the corner
		float h1 = glm::dot(currentDownDir, corner.position - aabb.min);
		float h2 = glm::dot(currentDownDir, corner.position - aabb.max);
		if (std::abs(h1) > MAX_HEIGHT_DIFF && std::abs(h2) > MAX_HEIGHT_DIFF)
			continue;

		// Checks that the player is within range of the corner
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

bool World::HasCollision(const glm::ivec3& pos, Dir side) const
{
	if (voxels.IsAir(pos) == voxels.IsAir(pos + DirectionVector(side)))
		return false;

	glm::ivec3 sideNormal = -DirectionVector(side);
	for (const Door& door : m_doors)
	{
		if (std::abs(glm::dot(door.normal, glm::vec3(sideNormal))) < 0.5f)
			continue;

		float radSq = door.radius * door.radius;

		const glm::ivec3 faceCenter2 = pos * 2 + 1 - sideNormal;
		const glm::ivec3 tangent = voxel::tangents[static_cast<int>(side)];
		const glm::ivec3 biTangent = voxel::biTangents[static_cast<int>(side)];
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

float World::MaxDistanceToWallVertex(const glm::vec3& pos) const
{
	float maxDist = 0;
	for (const glm::vec3& v : m_collisionMesh.Vertices())
	{
		maxDist = std::max(maxDist, glm::distance2(pos, v));
	}
	return std::sqrt(maxDist);
}
