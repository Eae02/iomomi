#pragma once

struct CollisionResponseCombiner
{
public:
	CollisionResponseCombiner() = default;
	
	bool Update(const glm::vec3& correction);
	
	bool Update(const std::optional<glm::vec3>& newCorrection)
	{
		if (newCorrection)
			return Update(*newCorrection);
		return false;
	}
	
	const std::optional<glm::vec3>& GetCorrection() const
	{
		return m_correction;
	}

private:
	std::optional<glm::vec3> m_correction;
	float m_correctionMag2 = INFINITY;
};

struct OrientedBox
{
	glm::vec3 radius;
	glm::vec3 center;
	glm::quat rotation;
};

std::optional<glm::vec3> CheckCollisionAABBPolygon(const eg::AABB& aabb, eg::Span<const glm::vec3> polyVertices,
	const glm::vec3& moveDir, float shiftAmount = 0.01f);

std::optional<glm::vec3> CheckCollisionAABBTriangleMesh(const eg::AABB& aabb, const glm::vec3& moveDir,
	const eg::CollisionMesh& mesh, const glm::mat4& meshTransform, bool flipWinding = false);

std::optional<glm::vec3> CheckCollisionAABBOrientedBox(const eg::AABB& aabb, const OrientedBox& orientedBox,
	const glm::vec3& moveDir, float shiftAmount = 0.01f);

std::optional<float> RayIntersectOrientedBox(const eg::Ray& ray, const OrientedBox& box);
