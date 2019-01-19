#pragma once

#include "Materials/ObjectMaterial.hpp"

class ObjectRenderer
{
public:
	ObjectRenderer() = default;
	
	void Begin();
	
	void Add(const eg::Model& model, const ObjectMaterial& material, const glm::mat4& transform);
	
	void End();
	
	void Draw(const class RenderSettings& renderSettings);
	
private:
	eg::LinearAllocator m_allocator;
	
	struct Instance
	{
		glm::mat4 transform;
		Instance* next;
	};
	
	struct ModelBucket
	{
		const eg::Model* model;
		Instance* firstInstance = nullptr;
		Instance* lastInstance = nullptr;
		uint32_t numInstances = 0;
		uint32_t instanceBufferOffset = 0;
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
	
	PipelineBucket* m_drawList = nullptr;
	uint32_t m_totalInstances = 0;
	
	uint32_t m_matricesBufferCapacity = 0;
	eg::Buffer m_matricesBuffer;
};
