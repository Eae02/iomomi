#pragma once

#include "../../../Graphics/Materials/GravityBarrierMaterial.hpp"
#include "../../PhysicsEngine.hpp"
#include "../Components/ActivatableComp.hpp"
#include "../Components/AxisAlignedQuadComp.hpp"
#include "../Components/WaterBlockComp.hpp"
#include "../Entity.hpp"

struct GravityBarrierInteractableComp
{
	Dir currentDown;
};

class GravityBarrierEnt : public Ent
{
public:
	static constexpr EntTypeID TypeID = EntTypeID::GravityBarrier;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable |
	                                         EntTypeFlags::ShadowDrawableS | EntTypeFlags::HasPhysics |
	                                         EntTypeFlags::DisableClone;

	GravityBarrierEnt();

	void Serialize(std::ostream& stream) const override;

	void Deserialize(std::istream& stream) override;

	void RenderSettings() override;

	void CommonDraw(const EntDrawArgs& args) override;

	void Update(const WorldUpdateArgs& args) override;

	void Spawned(bool isEditor) override;

	const void* GetComponent(const std::type_info& type) const override;

	void CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt) override;

	int BlockedAxis() const;

	glm::mat4 GetTransform() const;
	glm::vec3 GetPosition() const override { return m_position; }
	void EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;

	int EdGetIconIndex() const override;

	std::span<const EditorSelectionMesh> EdGetSelectionMeshes() const override;

	bool NeverBlockWater() const { return m_neverBlockWater; }
	bool RedFromWater() const { return m_redFromWater; }

	enum class ActivateAction
	{
		Disable,
		Enable,
		Rotate
	};

	int flowDirection = 0;
	ActivateAction activateAction = ActivateAction::Rotate;

	void SetWaterDistanceTexture(eg::TextureRef waterDistanceTexture)
	{
		m_material.SetWaterDistanceTexture(waterDistanceTexture);
	}

private:
	glm::vec3 m_position;

	static void UpdateNearEntities(const Player* player, EntityManager& entityManager);

	static std::vector<glm::vec3> GetConnectionPoints(const Ent& entity);

	static bool ShouldCollide(const PhysicsObject& self, const PhysicsObject& other);

	std::tuple<glm::vec3, glm::vec3> GetTangents() const;

	void UpdateWaterBlockedGravities();

	ActivatableComp m_activatable;
	AxisAlignedQuadComp m_aaQuad;
	WaterBlockComp m_waterBlockComp;

	float m_opacity = 1;
	int m_flowDirectionOffset = 0;
	float m_noiseSampleOffset;
	bool m_enabled = true;
	bool m_blockFalling = false;
	bool m_waterBlockComponentOutOfDate = true;
	bool m_neverBlockWater = false;
	bool m_redFromWater = true;

	PhysicsObject m_physicsObject;

	GravityBarrierMaterial m_material;
};
