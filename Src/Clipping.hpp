#pragma once

std::optional<glm::vec3> CollisionCorrection(const eg::AABB& aabb, const glm::vec3* triangleVertices);

std::optional<glm::vec3> CollisionCorrection(const eg::AABB& aabb, const eg::CollisionMesh& mesh, const glm::mat4& meshTransform);
