#pragma once

#include "Messages.hpp"
#include "ECEditorVisible.hpp"
#include "../../Graphics/Materials/LightStripMaterial.hpp"

class ECActivationLightStrip
{
public:
	ECActivationLightStrip();
	
	void HandleMessage(eg::Entity& entity, const DrawMessage& message);
	void HandleMessage(eg::Entity& entity, const EditorDrawMessage& message);
	
	struct WayPoint
	{
		Dir wallNormal;
		glm::vec3 position;
	};
	
	static void GenerateForActivator(const World& world, eg::Entity& activatorEntity);
	static void GenerateAll(const class World& world);
	
	static void Update(eg::EntityManager& entityManager, float dt);
	
	void Generate(const class World& world, eg::Span<const WayPoint> points);
	
	void ClearGeneratedMesh();
	
	struct PathEntry
	{
		Dir wallNormal;
		glm::vec3 position;
		int prevWayPoint;
	};
	
	const std::vector<PathEntry>& Path() const
	{
		return m_path;
	}
	
	static eg::MessageReceiver MessageReceiver;
	
	static eg::EntitySignature EntitySignature;
	
	static void OnInit();
	static void OnShutdown();
	
private:
	void Draw(eg::MeshBatch& meshBatch);
	
	enum MeshVariants
	{
		MV_Straight,
		MV_Bend,
		MV_Count
	};
	
	static const eg::Model* s_models[MV_Count];
	static const eg::IMaterial* s_materials[MV_Count];
	
	std::vector<PathEntry> m_path;
	
	std::vector<LightStripMaterial::InstanceData> m_instances[MV_Count];
	
	int m_transitionDirection = 0;
	float m_maxTransitionProgress = 0;
	float m_transitionProgress = -1;
	
	LightStripMaterial m_material;
};
