#pragma once

#include "IPrimitiveRenderer.hpp"

class PrimitiveRenderer : public IPrimitiveRenderer
{
public:
	PrimitiveRenderer() = default;

	void Begin(const glm::mat4& viewProjection, const glm::vec3& cameraPos);

	void End();

	void Draw() const;

	uint32_t NextIndex() const { return eg::UnsignedNarrow<uint32_t>(m_vertices.size()); }

	void AddVertex(const glm::vec3& position, const eg::ColorSRGB& color) override;

	void AddTriangle(uint32_t v0, uint32_t v1, uint32_t v2) override;

	void AddTriangle(const glm::vec3 positions[3], const eg::ColorSRGB& color) override;

	void AddQuad(const glm::vec3 positions[4], const eg::ColorSRGB& color) override;

	void AddLine(const glm::vec3& a, const glm::vec3& b, const eg::ColorSRGB& color, float width = 0.01f) override;

	void AddCollisionMesh(const eg::CollisionMesh& mesh, const eg::ColorSRGB& color) override;

	static void OnInit();
	static void OnShutdown();

private:
	glm::vec3 m_cameraPos;
	glm::mat4 m_viewProjection;

	struct Triangle
	{
		float depth;
		uint32_t indices[3];
	};

	std::vector<Triangle> m_triangles;

	struct Vertex
	{
		glm::vec3 position;
		uint8_t color[4];
	};

	std::vector<Vertex> m_vertices;

	uint64_t m_vertexBufferCapacity = 0;
	uint64_t m_indexBufferCapacity = 0;
	eg::Buffer m_vertexBuffer;
	eg::Buffer m_indexBuffer;
};
