#pragma once

#include "Materials/ObjectMaterial.hpp"

class ObjectRenderer
{
public:
	ObjectRenderer() = default;
	
	void Begin(ObjectMaterial::PipelineType pipeline);
	
	void Add(const eg::Model& model, const ObjectMaterial& material, const glm::mat4& transform)
	{
		const ObjectMaterial* materials[] = { &material };
		Add(model, materials, transform);
	}
	
	void Add(const eg::Model& model, eg::Span<const ObjectMaterial* const> materials, const glm::mat4& transform);
	
	void End();
	
	void Draw();
	
private:
	eg::LinearAllocator m_allocator;
	
	struct Instance
	{
		glm::mat4 transform;
		Instance* next;
	};
	
	struct MeshBucket
	{
		uint32_t firstVertex;
		uint32_t firstIndex;
		uint32_t numIndices;
		Instance* firstInstance = nullptr;
		Instance* lastInstance = nullptr;
		uint32_t numInstances = 0;
		uint32_t instanceBufferOffset = 0;
		MeshBucket* next = nullptr;
	};
	
	struct ModelBucket
	{
		const eg::Model* model;
		MeshBucket* meshes;
		ModelBucket* next = nullptr;
	};
	
	struct MaterialBucket
	{
		const ObjectMaterial* material;
		ModelBucket* models;
		MaterialBucket* next;
	};
	
	struct PipelineBucket
	{
		eg::PipelineRef pipeline;
		MaterialBucket* materials;
		PipelineBucket* next;
	};
	
	ObjectMaterial::PipelineType m_pipelineType;
	
	PipelineBucket* m_drawList = nullptr;
	uint32_t m_totalInstances = 0;
	
	uint32_t m_matricesBufferCapacity = 0;
	eg::Buffer m_matricesBuffer;
};
