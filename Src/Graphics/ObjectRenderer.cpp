#include "ObjectRenderer.hpp"
#include "RenderSettings.hpp"

void ObjectRenderer::Begin(ObjectMaterial::PipelineType pipelineType)
{
	m_allocator.Reset();
	m_pipelineType = pipelineType;
	m_drawList = nullptr;
	m_totalInstances = 0;
}

void ObjectRenderer::Add(const eg::Model& model, const ObjectMaterial& material, const glm::mat4& transform)
{
	eg::PipelineRef pipeline = material.GetPipeline(m_pipelineType);
	
	//Selects a pipeline bucket
	PipelineBucket* pipelineBucket = m_drawList;
	for (; pipelineBucket != nullptr; pipelineBucket = pipelineBucket->next)
	{
		if (pipelineBucket->pipeline.handle == pipeline.handle)
			break;
	}
	if (pipelineBucket == nullptr)
	{
		pipelineBucket = m_allocator.New<PipelineBucket>();
		pipelineBucket->pipeline = pipeline;
		pipelineBucket->materials = nullptr;
		pipelineBucket->next = m_drawList;
		m_drawList = pipelineBucket;
	}
	
	//Selects a material bucket
	MaterialBucket* materialBucket = pipelineBucket->materials;
	for (; materialBucket != nullptr; materialBucket = materialBucket->next)
	{
		if (materialBucket->material == &material)
			break;
	}
	if (materialBucket == nullptr)
	{
		materialBucket = m_allocator.New<MaterialBucket>();
		materialBucket->material = &material;
		materialBucket->models = nullptr;
		materialBucket->next = pipelineBucket->materials;
		pipelineBucket->materials = materialBucket;
	}
	
	//Selects a model bucket
	ModelBucket* modelBucket = materialBucket->models;
	for (; modelBucket != nullptr; modelBucket = modelBucket->next)
	{
		if (modelBucket->model == &model)
			break;
	}
	if (modelBucket == nullptr)
	{
		modelBucket = m_allocator.New<ModelBucket>();
		modelBucket->model = &model;
		modelBucket->next = materialBucket->models;
		materialBucket->models = modelBucket;
	}
	
	Instance* instance = m_allocator.New<Instance>();
	instance->transform = transform;
	instance->next = nullptr;
	
	if (modelBucket->firstInstance == nullptr)
	{
		modelBucket->firstInstance = modelBucket->lastInstance = instance;
	}
	else
	{
		modelBucket->lastInstance->next = instance;
		modelBucket->lastInstance = instance;
	}
	
	modelBucket->numInstances++;
	m_totalInstances++;
}

void ObjectRenderer::End()
{
	if (m_totalInstances == 0)
		return;
	
	const uint64_t bytesToUpload = m_totalInstances * sizeof(glm::mat4);
	eg::UploadBuffer uploadBuffer = eg::GetTemporaryUploadBuffer(bytesToUpload);
	
	glm::mat4* matricesOut = static_cast<glm::mat4*>(uploadBuffer.Map());
	uint32_t matricesPos = 0;
	for (PipelineBucket* pipeline = m_drawList; pipeline; pipeline =  pipeline->next)
	{
		for (MaterialBucket* material = pipeline->materials; material; material = material->next)
		{
			for (ModelBucket* model = material->models; model; model = model->next)
			{
				model->instanceBufferOffset = matricesPos;
				for (Instance* instance = model->firstInstance; instance; instance = instance->next)
				{
					matricesOut[matricesPos] = instance->transform;
					matricesPos++;
				}
			}
		}
	}
	
	uploadBuffer.Unmap();
	
	if (m_totalInstances > m_matricesBufferCapacity)
	{
		m_matricesBufferCapacity = eg::RoundToNextMultiple(m_totalInstances, 1024);
		m_matricesBuffer = eg::Buffer(eg::BufferFlags::CopyDst | eg::BufferFlags::VertexBuffer,
			m_matricesBufferCapacity * sizeof(glm::mat4), nullptr);
	}
	
	eg::DC.CopyBuffer(uploadBuffer.buffer, m_matricesBuffer, uploadBuffer.offset, 0, bytesToUpload);
	
	m_matricesBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
}

void ObjectRenderer::Draw()
{
	if (m_totalInstances == 0)
		return;
	
	eg::DC.BindVertexBuffer(1, m_matricesBuffer, 0);
	
	for (PipelineBucket* pipeline = m_drawList; pipeline; pipeline =  pipeline->next)
	{
		eg::DC.BindPipeline(pipeline->pipeline);
		eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
		
		for (MaterialBucket* material = pipeline->materials; material; material = material->next)
		{
			material->material->Bind(m_pipelineType);
			
			for (ModelBucket* model = material->models; model; model = model->next)
			{
				model->model->Bind();
				
				for (size_t m = 0; m < model->model->NumMeshes(); m++)
				{
					const eg::Model::Mesh& mesh = model->model->GetMesh(m);
					eg::DC.DrawIndexed(mesh.firstIndex, mesh.numIndices, mesh.firstVertex,
						model->instanceBufferOffset, model->numInstances);
				}
			}
		}
	}
}
