#pragma once

#include "../Entity.hpp"
#include "../../../Graphics/Materials/LightStripMaterial.hpp"

class ActivationLightStripEnt : public Ent
{
public:
	ActivationLightStripEnt();
	
	static constexpr EntTypeID TypeID = EntTypeID::ActivationLightStrip;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable | EntTypeFlags::EditorInvisible;
	
	void Serialize(std::ostream& stream) const override;
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void CommonDraw(const EntDrawArgs& args) override;
	
	struct WayPoint
	{
		Dir wallNormal;
		glm::vec3 position;
	};
	
	static void GenerateForActivator(World& world, Ent& activatorEntity);
	static void GenerateAll(World& world);
	
	void Update(const struct WorldUpdateArgs& args) override;
	
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
	
	const std::weak_ptr<Ent>& ActivatorEntity() const
	{
		return m_activator;
	}
	
	static void OnInit();
	
	static const eg::ColorLin ACTIVATED_COLOR;
	static const eg::ColorLin DEACTIVATED_COLOR;
	
private:
	void Draw(eg::MeshBatch& meshBatch);
	
	enum MeshVariants
	{
		MV_Straight,
		MV_Bend,
		MV_Corner,
		MV_Count
	};
	
	static const eg::Model* s_models[MV_Count];
	static const eg::IMaterial* s_materials[MV_Count];
	
	std::weak_ptr<Ent> m_activator;
	
	std::vector<PathEntry> m_path;
	
	std::vector<LightStripMaterial::InstanceData> m_instances[MV_Count];
	
	int m_transitionDirection = 0;
	float m_maxTransitionProgress = 0;
	float m_transitionProgress = -1;
	
	LightStripMaterial m_material;
};
