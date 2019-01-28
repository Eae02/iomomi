#pragma once

class PrimitiveRenderer
{
public:
	PrimitiveRenderer() = default;
	
	void Begin(const glm::mat4& viewProjection);
	
	void End();
	
	void Draw() const;
	
	uint32_t NextIndex() const
	{
		return (uint32_t)m_vertices.size();
	}
	
	void AddVertex(const glm::vec3& position, const eg::ColorSRGB& color);
	
	void AddTriangle(uint32_t v0, uint32_t v1, uint32_t v2);
	
	void AddTriangle(const glm::vec3 positions[3], const eg::ColorSRGB& color);
	
	void AddQuad(const glm::vec3 positions[4], const eg::ColorSRGB& color);
	
	static void OnInit();
	static void OnShutdown();
	
private:
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
