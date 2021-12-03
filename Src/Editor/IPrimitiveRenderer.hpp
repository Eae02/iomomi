#pragma once

class IPrimitiveRenderer
{
public:
	virtual void AddVertex(const glm::vec3& position, const eg::ColorSRGB& color) = 0;
	virtual void AddTriangle(uint32_t v0, uint32_t v1, uint32_t v2) = 0;
	virtual void AddTriangle(const glm::vec3 positions[3], const eg::ColorSRGB& color) = 0;
	virtual void AddQuad(const glm::vec3 positions[4], const eg::ColorSRGB& color) = 0;
	virtual void AddLine(const glm::vec3& a, const glm::vec3& b, const eg::ColorSRGB& color, float width = 0.01f) = 0;
	virtual void AddCollisionMesh(const eg::CollisionMesh& mesh, const eg::ColorSRGB& color) = 0;
};
