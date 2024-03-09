#include "WaterCollisionData.hpp"
#include "../World/VoxelBuffer.hpp"
#include "WaterConstants.hpp"
#include "WaterSimulator.hpp"

struct CollisionQuadForGPU
{
	glm::vec3 center;
	uint32_t blockedGravities;
	uint32_t normal;  // normalized, encoded with packSnorm4x8
	uint32_t tangent; // normalized, encoded with packSnorm4x8
	float tangentLen;
	float bitangentLen;
};

static_assert(sizeof(CollisionQuadForGPU) == 4 * 4 * 2); // assert that there is no padding

static constexpr eg::BufferFlags STAGING_BUFFER_FLAGS = eg::BufferFlags::HostAllocate | eg::BufferFlags::MapWrite |
                                                        eg::BufferFlags::CopySrc | eg::BufferFlags::ManualBarrier;

static constexpr size_t MAX_TOTAL_COLLISION_QUADS = 255;

// The maximum number of collision quads that can influence a single voxel
static constexpr size_t MAX_VOXEL_COLLISION_QUADS = 3;

WaterCollisionData WaterCollisionData::Create(
	eg::CommandContext& cc, const VoxelBuffer& voxelBuffer, std::vector<WaterCollisionQuad> collisionQuads
)
{
	size_t cappedNumCollisionQuads = std::min<size_t>(collisionQuads.size(), MAX_TOTAL_COLLISION_QUADS);
	if (cappedNumCollisionQuads < collisionQuads.size())
	{
		eg::Log(eg::LogLevel::Error, "water", "Too many water collision quads: {0}", collisionQuads.size());
	}

	auto [minBounds, maxBounds] = voxelBuffer.CalculateBounds();

	glm::ivec3 voxelsSize = maxBounds - minBounds;
	voxelsSize.x = eg::RoundToNextMultiple(voxelsSize.x, 4);
	voxelsSize.y = eg::RoundToNextMultiple(voxelsSize.y, 4);
	voxelsSize.z = eg::RoundToNextMultiple(voxelsSize.z, 4);

	const uint32_t numVoxels = voxelsSize.x * voxelsSize.y * voxelsSize.z;
	eg::Buffer voxelDataUploadBuffer(STAGING_BUFFER_FLAGS, numVoxels * sizeof(uint32_t), nullptr);

	std::span<uint32_t> voxelDataSpan(static_cast<uint32_t*>(voxelDataUploadBuffer.Map()), numVoxels);

	uint32_t maxNumCollisionQuadsSingleVoxel = 0;

	for (int z = 0; z < voxelsSize.z; z++)
		for (int y = 0; y < voxelsSize.y; y++)
			for (int x = 0; x < voxelsSize.x; x++)
			{
				glm::ivec3 voxelCoord = minBounds + glm::ivec3(x, y, z);

				uint8_t solidMask = 0;
				for (int dx = 0; dx <= 1; dx++)
					for (int dy = 0; dy <= 1; dy++)
						for (int dz = 0; dz <= 1; dz++)
						{
							if (!voxelBuffer.IsAir(voxelCoord + glm::ivec3(dx, dy, dz)))
							{
								uint32_t bit = dx + dy * 2 + dz * 4;
								solidMask |= 1 << bit;
							}
						}

				uint32_t voxelData = solidMask;

				constexpr float QUAD_COLLISION_AABB_EXPAND = 0.2f;
				constexpr float QUAD_COLLISION_MAX_DIST = 0.2f;
				const eg::AABB quadTestAABB(
					glm::vec3(voxelCoord) - QUAD_COLLISION_AABB_EXPAND,
					glm::vec3(voxelCoord) + 1.0f + QUAD_COLLISION_AABB_EXPAND
				);
				uint32_t numCollisionQuads = 0;
				for (uint32_t i = 0; i < cappedNumCollisionQuads; i++)
				{
					std::optional<float> dist = collisionQuads[i].NormalDistanceToAABB(quadTestAABB);
					if (dist.has_value() && *dist < QUAD_COLLISION_MAX_DIST)
					{
						if (numCollisionQuads < MAX_VOXEL_COLLISION_QUADS)
							voxelData |= (i + 1) << (8 + 8 * numCollisionQuads);
						numCollisionQuads++;
					}
				}
				maxNumCollisionQuadsSingleVoxel = std::max(maxNumCollisionQuadsSingleVoxel, numCollisionQuads);

				size_t outIdx = x + y * voxelsSize.x + z * voxelsSize.x * voxelsSize.y;
				voxelDataSpan[outIdx] = voxelData;
			}

	voxelDataUploadBuffer.Flush();

	if (maxNumCollisionQuadsSingleVoxel > MAX_VOXEL_COLLISION_QUADS)
	{
		eg::Log(
			eg::LogLevel::Error, "water", "Too many water collision quads for a single voxel: {0}",
			maxNumCollisionQuadsSingleVoxel
		);
	}

	WaterCollisionData result;
	// ** Creates and uploads data into the collision quads buffer **
	uint64_t collisionQuadsBufferSize = std::max<size_t>(cappedNumCollisionQuads, 1) * sizeof(CollisionQuadForGPU);
	result.collisionQuadsBuffer = eg::Buffer(eg::BufferCreateInfo{
		.flags = eg::BufferFlags::StorageBuffer | eg::BufferFlags::CopyDst | eg::BufferFlags::ManualBarrier,
		.size = collisionQuadsBufferSize,
		.label = "WaterCollisionQuads",
	});

	if (!collisionQuads.empty())
	{
		eg::Buffer collisionQuadsUploadBuffer(STAGING_BUFFER_FLAGS, collisionQuadsBufferSize, nullptr);
		auto* collisionQuadsUploadPtr = static_cast<CollisionQuadForGPU*>(collisionQuadsUploadBuffer.Map());
		for (size_t i = 0; i < cappedNumCollisionQuads; i++)
		{
			collisionQuadsUploadPtr[i] = CollisionQuadForGPU{
				.center = collisionQuads[i].center,
				.blockedGravities = static_cast<uint32_t>(collisionQuads[i].blockedGravities),
				.normal = glm::packSnorm4x8(glm::vec4(glm::normalize(collisionQuads[i].normal), 0.0f)),
				.tangent = glm::packSnorm4x8(glm::vec4(glm::normalize(collisionQuads[i].tangent), 0.0f)),
				.tangentLen = collisionQuads[i].tangentLen,
				.bitangentLen = collisionQuads[i].bitangentLen,
			};
		}
		collisionQuadsUploadBuffer.Flush();

		cc.CopyBuffer(collisionQuadsUploadBuffer, result.collisionQuadsBuffer, 0, 0, collisionQuadsBufferSize);

		cc.Barrier(
			collisionQuadsUploadBuffer,
			{
				.oldUsage = eg::BufferUsage::CopyDst,
				.newUsage = eg::BufferUsage::StorageBufferRead,
				.newAccess = eg::ShaderAccessFlags::Compute,
			}
		);
	}

	// Creates the voxel collision ranges texture
	result.voxelDataTexture = eg::Texture::Create3D(eg::TextureCreateInfo{
		.flags = eg::TextureFlags::CopyDst | eg::TextureFlags::ManualBarrier | eg::TextureFlags::ShaderSample,
		.mipLevels = 1,
		.width = static_cast<uint32_t>(voxelsSize.x),
		.height = static_cast<uint32_t>(voxelsSize.y),
		.depth = static_cast<uint32_t>(voxelsSize.z),
		.format = eg::Format::R32_UInt,
		.label = "VoxelSolidMasks",
	});

	// ** Uploads data into the voxel collision ranges texture **

	cc.Barrier(result.voxelDataTexture, eg::TextureBarrier{ .newUsage = eg::TextureUsage::CopyDst });

	cc.SetTextureData(
		result.voxelDataTexture,
		eg::TextureRange{
			.sizeX = eg::ToUnsigned(voxelsSize.x),
			.sizeY = eg::ToUnsigned(voxelsSize.y),
			.sizeZ = eg::ToUnsigned(voxelsSize.z),
		},
		voxelDataUploadBuffer, 0
	);

	eg::TextureBarrier barrier = {
		.oldUsage = eg::TextureUsage::CopyDst,
		.newUsage = eg::TextureUsage::ShaderSample,
		.newAccess = eg::ShaderAccessFlags::Compute,
		.subresource = eg::TextureSubresource(),
	};
	cc.Barrier(result.voxelDataTexture, barrier);

	result.collisionQuads = std::move(collisionQuads);

	return result;
}

void WaterCollisionData::SetCollisionQuadBlockedGravities(
	eg::CommandContext& cc, uint32_t collisionQuadIndex, DirFlags blockedGravities
)
{
	if (collisionQuadIndex >= collisionQuads.size() ||
	    collisionQuads[collisionQuadIndex].blockedGravities == blockedGravities)
	{
		return;
	}

	collisionQuads[collisionQuadIndex].blockedGravities = blockedGravities;

	if (!collisionQuadsBufferNeedsBarrier)
	{
		cc.Barrier(
			collisionQuadsBuffer,
			eg::BufferBarrier{
				.oldUsage = eg::BufferUsage::StorageBufferRead,
				.newUsage = eg::BufferUsage::CopyDst,
				.oldAccess = eg::ShaderAccessFlags::Compute,
			}
		);

		collisionQuadsBufferNeedsBarrier = true;
	}

	const uint64_t writeOffset =
		collisionQuadIndex * sizeof(CollisionQuadForGPU) + offsetof(CollisionQuadForGPU, blockedGravities);
	const uint32_t blockedGravitiesU32 = static_cast<uint32_t>(blockedGravities);
	cc.UpdateBuffer(collisionQuadsBuffer, writeOffset, sizeof(uint32_t), &blockedGravitiesU32);
}

void WaterCollisionData::MaybeInsertCollisionQuadsBufferBarrier(eg::CommandContext& cc)
{
	if (collisionQuadsBufferNeedsBarrier)
	{
		cc.Barrier(
			collisionQuadsBuffer,
			eg::BufferBarrier{
				.oldUsage = eg::BufferUsage::CopyDst,
				.newUsage = eg::BufferUsage::StorageBufferRead,
				.newAccess = eg::ShaderAccessFlags::Compute,
			}
		);

		collisionQuadsBufferNeedsBarrier = false;
	}
}

std::optional<float> WaterCollisionQuad::NormalDistanceToAABB(const eg::AABB& aabb) const
{
	auto GetAABBAxisRange = [&](glm::vec3 axis) -> std::pair<float, float>
	{
		float axisMin = INFINITY;
		float axisMax = -INFINITY;
		for (int n = 0; n < 8; n++)
		{
			glm::vec3 vertex = aabb.NthVertex(n);
			float dist = glm::dot(vertex, axis);
			axisMin = std::min(axisMin, dist);
			axisMax = std::max(axisMax, dist);
		}
		return { axisMin, axisMax };
	};

	auto [tangentAABBMin, tangentAABBMax] = GetAABBAxisRange(tangent);
	float centerTangentDist = glm::dot(center, tangent);
	if (tangentAABBMax < centerTangentDist - tangentLen || tangentAABBMin > centerTangentDist + tangentLen)
		return std::nullopt;

	glm::vec3 bitangent = Bitangent();
	auto [bitangentAABBMin, bitangentAABBMax] = GetAABBAxisRange(bitangent);
	float centerBitangentDist = glm::dot(center, bitangent);
	if (bitangentAABBMax < centerBitangentDist - bitangentLen || bitangentAABBMin > centerBitangentDist + bitangentLen)
		return std::nullopt;

	auto [normalAABBMin, normalAABBMax] = GetAABBAxisRange(normal);
	float centerNormalDist = glm::dot(center, normal);

	float d1 = std::max(centerNormalDist - normalAABBMax, 0.0f);
	float d2 = std::max(normalAABBMin - centerNormalDist, 0.0f);
	return std::max(d1, d2);
}
