#pragma once

class LiquidPlaneRenderer
{
public:
	LiquidPlaneRenderer();
	
	void Prepare(const class World& world, eg::MeshBatchOrdered& meshBatch, const glm::vec3& cameraPos);
	
	void Render() const;
	
private:
	std::vector<std::pair<float, const class ECLiquidPlane*>> m_planes;
	
	eg::Pipeline m_pipeline;
};
