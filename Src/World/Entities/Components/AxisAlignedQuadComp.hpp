#pragma once

struct EditorSelectionMesh;

struct AxisAlignedQuadComp
{
	int upPlane = 0;
	glm::vec2 radius{ 1.0f };

	glm::vec3 GetNormal() const;

	std::tuple<glm::vec3, glm::vec3> GetTangents(float rotation) const;

	struct CollisionGeometry
	{
		glm::vec3 vertices[6][4];

		std::optional<glm::vec3> CheckCollision(const eg::AABB& aabb, const glm::vec3& moveDir) const;
	};

	CollisionGeometry GetCollisionGeometry(const glm::vec3& center, float depth) const;

	EditorSelectionMesh GetEditorSelectionMesh(const glm::vec3& center, float depth) const;

	// Convenience wrapper around GetEditorSelectionMesh which returns a reference to a common EditorSelectionMesh.
	std::span<const EditorSelectionMesh> GetEditorSelectionMeshes(const glm::vec3& center, float depth) const;
};
