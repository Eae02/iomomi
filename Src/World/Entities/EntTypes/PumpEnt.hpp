#pragma once

#include "../../../Graphics/Materials/PumpScreenMaterial.hpp"
#include "../../../Graphics/Water/WaterPumpDescription.hpp"
#include "../../PhysicsEngine.hpp"
#include "../EntInteractable.hpp"
#include "../Entity.hpp"

class PumpEnt : public Ent, public EntInteractable
{
public:
	PumpEnt();

	static constexpr EntTypeID TypeID = EntTypeID::Pump;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable |
	                                         EntTypeFlags::DisableClone | EntTypeFlags::Interactable |
	                                         EntTypeFlags::ShadowDrawableS | EntTypeFlags::EditorRotatable |
	                                         EntTypeFlags::HasPhysics;

	static constexpr const char* EntPrettyName = "Water Pump";

	void Serialize(std::ostream& stream) const override;

	void Deserialize(std::istream& stream) override;

	void RenderSettings() override;

	void CommonDraw(const EntDrawArgs& args) override;

	void Update(const struct WorldUpdateArgs& args) override;

	void Interact(class Player& player) override;
	int CheckInteraction(const class Player& player, const PhysicsEngine& physicsEngine) const override;
	std::optional<InteractControlHint> GetInteractControlHint() const override;

	glm::vec3 GetPosition() const override { return m_position; }
	void EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;

	glm::quat EdGetRotation() const override;
	void EdRotated(const glm::quat& newRotation) override;

	void CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt) override;

	std::span<const EditorSelectionMesh> EdGetSelectionMeshes() const override;

	std::optional<WaterPumpDescription> GetPumpDescription() const;

private:
	void UpdateTransform();

	glm::vec3 m_position;
	glm::quat m_rotation;

	glm::mat4 m_transform;

	glm::vec3 m_leftPumpPoint;
	glm::vec3 m_rightPumpPoint;

	float m_particlesPerSecond = 0;
	float m_maxInputDistance = 0;
	float m_maxOutputDistance = 0;

	PumpDirection GetHoveredButton(const Player& player, const PhysicsEngine* physicsEngine) const;

	PumpDirection m_currentDirection = PumpDirection::None;

	float m_buttonLPushTimer = 0;
	float m_buttonRPushTimer = 0;

	float m_buttonLActivationGlow = 0;
	float m_buttonRActivationGlow = 0;

	EditorSelectionMesh m_editorSelectionMesh;

	PhysicsObject m_physicsObject;

	PumpScreenMaterial m_screenMaterial;
};
