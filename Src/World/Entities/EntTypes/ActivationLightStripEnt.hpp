#pragma once

#include <future>
#include "../Entity.hpp"
#include "../../../Graphics/Materials/LightStripMaterial.hpp"
#include "../../VoxelBuffer.hpp"

class ActivationLightStripEnt : public Ent
{
public:
	ActivationLightStripEnt() = default;
	
	static constexpr EntTypeID TypeID = EntTypeID::ActivationLightStrip;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable | EntTypeFlags::EditorInvisible | EntTypeFlags::DisableClone;
	
	void Serialize(std::ostream& stream) const override;
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void CommonDraw(const EntDrawArgs& args) override;
	
	glm::vec3 GetPosition() const override;
	
	struct WayPoint
	{
		Dir wallNormal;
		glm::vec3 position;
	};
	
	static void GenerateForActivator(World& world, Ent& activatorEntity);
	static void GenerateAll(World& world);
	
	void Update(const struct WorldUpdateArgs& args) override;
	
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
	
	bool regenerate = false;
	
private:
	enum MeshVariants
	{
		MV_Straight,
		MV_Bend,
		MV_Corner,
		MV_Count
	};
	
	struct GenerateResult
	{
		std::vector<PathEntry> path;
		std::array<std::vector<LightStripMaterial::InstanceData>, MV_Count> instances;
		float maxTransitionProgress;
	};
	
	static GenerateResult Generate(const VoxelBuffer& voxels, eg::Span<const WayPoint> points);
	
	std::vector<WayPoint> GetWayPointsWithStartAndEnd() const;
	
	std::future<GenerateResult> m_generationFuture;
	
	static const eg::Model* s_models[MV_Count];
	static const eg::IMaterial* s_materials[MV_Count];
	
	std::weak_ptr<Ent> m_activator;
	std::weak_ptr<Ent> m_activatable;
	
	std::vector<PathEntry> m_path;
	
	std::array<std::vector<LightStripMaterial::InstanceData>, MV_Count> m_instances;
	
	int m_transitionDirection = 0;
	float m_maxTransitionProgress = 0;
	float m_transitionProgress = -1;
	
	LightStripMaterial m_material;
};
