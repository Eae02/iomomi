#pragma once

#include "../Entity.hpp"
#include "../EntInteractable.hpp"
#include "../../../Graphics/Materials/GravitySwitchVolLightMaterial.hpp"
#include "../Components/ActivatableComp.hpp"
#include "../../../Graphics/Materials/GravitySwitchMaterial.hpp"

class GravitySwitchEnt : public Ent, public EntInteractable
{
public:
	GravitySwitchEnt();
	
	static constexpr EntTypeID TypeID = EntTypeID::GravitySwitch;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable |
		EntTypeFlags::Interactable | EntTypeFlags::EditorWallMove;
	
	void Serialize(std::ostream& stream) const override;
	
	void Deserialize(std::istream& stream) override;
	
	void EditorDraw(const EntEditorDrawArgs& args) override;
	void GameDraw(const EntGameDrawArgs& args) override;
	
	void Update(const struct WorldUpdateArgs& args) override;
	
	void Interact(class Player& player) override;
	
	int CheckInteraction(const class Player& player, const class PhysicsEngine& physicsEngine) const override;
	
	std::optional<InteractControlHint> GetInteractControlHint() const override;
	
	const void* GetComponent(const std::type_info& type) const override;
	
	inline eg::AABB GetAABB() const
	{
		return Ent::GetAABB(1.0f, 0.2f, m_up);
	}
	
	glm::vec3 GetPosition() const override { return m_position; }
	Dir GetFacingDirection() const override { return m_up; }
	void EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;
	
private:
	void Draw(eg::MeshBatch& meshBatch) const;
	
	static std::vector<glm::vec3> GetConnectionPoints(const Ent& entity);
	
	Dir m_up;
	glm::vec3 m_position;
	
	float m_enableAnimationTime = 1;
	
	ActivatableComp m_activatable;
	GravitySwitchVolLightMaterial m_volLightMaterial;
	GravitySwitchMaterial m_centerMaterial;
};


