#pragma once

#include "../../Entity.hpp"
#include "../../Components/ActivatorComp.hpp"
#include "../../EntInteractable.hpp"

class PushButtonEnt : public Ent, public EntInteractable
{
public:
	PushButtonEnt() = default;
	
	static constexpr EntTypeID TypeID = EntTypeID::PushButton;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable |
	                                         EntTypeFlags::EditorWallMove | EntTypeFlags::HasPhysics | EntTypeFlags::Interactable;
	
	void RenderSettings() override;
	
	Dir GetFacingDirection() const override { return m_direction; }
	glm::vec3 GetPosition() const override { return m_position; }
	void EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;
	
	void CommonDraw(const EntDrawArgs& args) override;
	
	void Update(const struct WorldUpdateArgs& args) override;
	
	void Interact(class Player& player) override;
	int CheckInteraction(const class Player& player, const PhysicsEngine& physicsEngine) const override;
	std::optional<InteractControlHint> GetInteractControlHint() const override;
	
	const void* GetComponent(const std::type_info& type) const override;
	
	void Serialize(std::ostream& stream) const override;
	void Deserialize(std::istream& stream) override;
	
private:
	glm::mat4 GetTransform() const;
	
	Dir m_direction {};
	glm::vec3 m_position;
	ActivatorComp m_activator;
	int m_rotation = 0;
	
	float m_activationDelay = 0;
	float m_activationDuration = 0.5f;
	
	float m_timeSincePressed = INFINITY;
};
