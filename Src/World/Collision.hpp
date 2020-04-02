#pragma once

struct CollisionResponseCombiner
{
public:
	CollisionResponseCombiner() = default;
	
	void Update(const glm::vec3& correction);
	
	void Update(const std::optional<glm::vec3>& newCorrection)
	{
		if (newCorrection)
			Update(*newCorrection);
	}
	
	const std::optional<glm::vec3>& GetCorrection() const
	{
		return m_correction;
	}

private:
	std::optional<glm::vec3> m_correction;
	float m_correctionMag2 = INFINITY;
};

std::optional<glm::vec3> CheckCollisionAABBPolygon(const eg::AABB& aabb, eg::Span<const glm::vec3> polyVertices,
	const glm::vec3& moveDir, float shiftAmount = 0.01f);

std::optional<glm::vec3> CheckCollisionAABBTriangleMesh(const eg::AABB& aabb, const glm::vec3& moveDir,
	const eg::CollisionMesh& mesh, const glm::mat4& meshTransform);
